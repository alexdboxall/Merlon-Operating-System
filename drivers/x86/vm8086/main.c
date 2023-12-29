#include <heap.h>
#include <assert.h>
#include <string.h>
#include <virtual.h>
#include <errno.h>
#include <log.h>
#include <fcntl.h>
#include <transfer.h>
#include <panic.h>
#include <vfs.h>
#include <thread.h>
#include <physical.h>
#include <arch.h>
#include <common.h>
#include <machine/portio.h>
#include <machine/regs.h>

#define LOW_PAGES_TO_RESERVE    4

#define HOST_COMMS_PORT_CMD     0xFEFD
#define HOST_COMMS_PORT_DATA    0xFEFE

static size_t low_pages[LOW_PAGES_TO_RESERVE];
static int num_low_pages = 0;
static size_t past_a20_virt;

struct vm8086_monitor {
    uint8_t host_comms_index;
    uint8_t host_comms_data[256];

    size_t code_phys;
    size_t stack_phys;

    bool a20_enabled;
    bool interrupts_enabled;

    uint16_t ip;
    uint16_t sp;
    uint16_t cs;
    uint16_t ss;

    struct thread* thr;

    bool killed;
    int exit_code;
};

/*static*/ uint8_t LOCKED_DRIVER_CODE VmInb(struct vm8086_monitor* mntr, uint16_t port) {
    if (port == HOST_COMMS_PORT_CMD) {
        return mntr->host_comms_index;

    } else if (port == HOST_COMMS_PORT_DATA) {
        return mntr->host_comms_data[mntr->host_comms_index];
    
    } else {
        return inb(port);
    }
}

/*static*/ void LOCKED_DRIVER_CODE VmOutb(struct vm8086_monitor* mntr, uint16_t port, uint8_t value) {
    if (port == HOST_COMMS_PORT_CMD) {
        mntr->host_comms_index = value;

    } else if (port == HOST_COMMS_PORT_DATA) {
        mntr->host_comms_data[mntr->host_comms_index] = value;

    } else {
        outb(port, value);
    }
}

static size_t LOCKED_DRIVER_CODE VmLinearToVirtual(uint32_t addr) {
    if (addr <= 0xFFFFF) {
        return 0xC0000000 + addr;
    } else {
        return past_a20_virt + (addr - 0x100000);
    }
}

static uint32_t LOCKED_DRIVER_CODE VmRealToLinear(struct vm8086_monitor* mntr, uint16_t seg, uint16_t off) {
	uint32_t raw = (((uint32_t) seg) << 4) + ((uint32_t) off);
    if (mntr->a20_enabled) {
        return raw & 0xFFFFF;
    } else {
        return raw;
    }
}

static size_t LOCKED_DRIVER_CODE VmRealToVirtual(struct vm8086_monitor* mntr, uint16_t seg, uint16_t off) {
    return VmLinearToVirtual(VmRealToLinear(mntr, seg, off));
}

static uint16_t LOCKED_DRIVER_CODE VmLinearToSegment(uint32_t linear) {
	return (linear >> 4) & 0xF000;
}

static uint16_t LOCKED_DRIVER_CODE VmLinearToOffset(uint32_t linear) {
	return linear & 0xFFFF;
}

static void LOCKED_DRIVER_CODE AddLowPage(size_t phys) {
    for (int i = 0; i < LOW_PAGES_TO_RESERVE; ++i) {
        if (low_pages[i] == 0) {
            low_pages[i] = phys;
            ++num_low_pages;
            return;
        }
    }
}

static void LOCKED_DRIVER_CODE TryReserveLowPages(void) { 
    for (int i = 0; i < LOW_PAGES_TO_RESERVE && num_low_pages < LOW_PAGES_TO_RESERVE; ++i) {
        size_t low_phys_mem = AllocPhysContiguous(ARCH_PAGE_SIZE, 0x1000, 0xA0000, 0);
        if (low_phys_mem != 0) {
            AddLowPage(low_phys_mem);
        } else {
            break;
        }
    }
}

static size_t LOCKED_DRIVER_CODE AllocLowPage(void) {
    if (num_low_pages == 0) {
        TryReserveLowPages();
        if (num_low_pages == 0) {
            PanicEx(PANIC_NO_LOW_MEMORY, "no low memory for vm8086");
        }
    }

    for (int i = 0; i < LOW_PAGES_TO_RESERVE; ++i) {
        if (low_pages[i] != 0) {
            size_t addr = low_pages[i];
            low_pages[i] = 0;
            --num_low_pages;
            return addr;
        }
    }

    Panic(PANIC_IMPOSSIBLE_RETURN);
}

static uint8_t LOCKED_DRIVER_CODE VmReadRealByte(struct vm8086_monitor* mntr, uint16_t seg, uint16_t offset) {
    return *((uint8_t*) VmRealToVirtual(mntr, seg, offset));
}

uint16_t LOCKED_DRIVER_CODE VmReadRealWord(struct vm8086_monitor* mntr, uint16_t seg, uint16_t offset) {
    return *((uint16_t*) VmRealToVirtual(mntr, seg, offset));
}

uint32_t LOCKED_DRIVER_CODE VmReadRealDword(struct vm8086_monitor* mntr, uint16_t seg, uint16_t offset) {
    return *((uint32_t*) VmRealToVirtual(mntr, seg, offset));
}

static void LOCKED_DRIVER_CODE VmWriteRealByte(struct vm8086_monitor* mntr, uint16_t seg, uint16_t offset, uint8_t value) {
    *((uint8_t*) VmRealToVirtual(mntr, seg, offset)) = value;
}

static void LOCKED_DRIVER_CODE VmWriteRealWord(struct vm8086_monitor* mntr, uint16_t seg, uint16_t offset, uint16_t value) {
    *((uint16_t*) VmRealToVirtual(mntr, seg, offset)) = value;
}

void LOCKED_DRIVER_CODE VmWriteRealDword(struct vm8086_monitor* mntr, uint16_t seg, uint16_t offset, uint32_t value) {
    *((uint32_t*) VmRealToVirtual(mntr, seg, offset)) = value;
}

static uint32_t LOCKED_DRIVER_CODE VmGetAdjustedFlags(uint32_t flags, bool enable_irqs) {
    if (enable_irqs) {
        return flags | 0x200;
    } else {
        return flags & ~0x200;
    }
}

#pragma GCC push_options
#pragma GCC optimize ("O0")
static volatile uint16_t* LOCKED_DRIVER_CODE VmGetIvt(void) {
    return (uint16_t*) (size_t) 0;     // whoa!!
}
#pragma GCC pop_options

static void LOCKED_DRIVER_CODE VmKillTask(struct vm8086_monitor* mntr, int status) {
    if (GetThread() != mntr->thr) {
        PanicEx(PANIC_DRIVER_FAULT, "vm8086 thread assertion failure");
    }

    mntr->exit_code = status;
    mntr->killed = true;
    TerminateThread(GetThread());
}

void LOCKED_DRIVER_CODE VmDoInterrupt(struct vm8086_monitor* mntr, struct x86_regs* regs, int irq_num, uint16_t ss, uint16_t sp, int instruction_length) {
    if (irq_num == 0xFE) {
        if ((regs->ebx & 0xFFFF) == 0x5555 && (regs->ecx & 0xFFFF) == 0xAAAA) {
            VmKillTask(mntr, regs->eax & 0xFFFF);
        }
    }
    
    volatile uint16_t* ivt = VmGetIvt();

    sp -= 6;
    regs->useresp = (regs->useresp - 6) & 0xFFFF;

    VmWriteRealWord(mntr, ss, sp, regs->eip + instruction_length);
    VmWriteRealWord(mntr, ss, sp + 2, regs->cs);
    VmWriteRealWord(mntr, ss, sp + 4, VmGetAdjustedFlags(regs->eflags, mntr->interrupts_enabled));

    mntr->interrupts_enabled = false;

    regs->cs = ivt[irq_num * 2 + 1];
	regs->eip = ivt[irq_num * 2 + 0];
}

static void LOCKED_DRIVER_CODE VmIncrementEip(struct x86_regs* regs) {
    regs->eip = (regs->eip + 1) & 0xFFFF;
}

int LOCKED_DRIVER_CODE VmFaultHandler(struct vm8086_monitor* mntr, struct x86_regs* regs) {
    volatile uint16_t* ivt = VmGetIvt();

    bool operand_32 = false;
    bool address_32 = false;
    int segment_override = 6;       //0 = CS, 1 = DS, 2 = ES, 3 = FS, 4 = GS, 5 = SS, 6 = DEFAULT

	while (true) {
        (void) ivt;
        (void) operand_32;
        (void) address_32;
        (void) segment_override;

        uint8_t opcode = VmReadRealByte(mntr, regs->cs, regs->eip);

        switch (opcode) {
        case 0x26:      
            // ES segment override prefix
            segment_override = 2;
            VmIncrementEip(regs);
            break;

        case 0x2E:      
            // CS segment override prefix
            segment_override = 0;
            VmIncrementEip(regs);
            break;

        case 0x36:      
            // SS segment override prefix
            segment_override = 5;
            VmIncrementEip(regs);
            break;

        case 0x3E:      
            // DS segment override prefix
            segment_override = 1;
            VmIncrementEip(regs);
            break;

        case 0x64:      
            // FS segment override prefix
            segment_override = 3;
            VmIncrementEip(regs);
            break;

        case 0x65:      
            // GS segment override prefix
            segment_override = 4;
            VmIncrementEip(regs);
            break;

        case 0x66:      
            // operand 32 prefix
            operand_32 = true;
            VmIncrementEip(regs);
            break;

        case 0x67:      
            // address 32 prefix
            operand_32 = true;
            VmIncrementEip(regs);
            break;

        case 0xF3:
            // REPeat prefix
            PanicEx(PANIC_DRIVER_FAULT, "[vm8086] REP prefix not supported!");
            VmIncrementEip(regs);
            return EINVAL;

        case 0x9C:
            // pushf
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] PUSHF");
            return EINVAL;

        case 0x9D:
            // popf
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] POPF");
            return EINVAL;

        case 0xCD:
            // INT 
            VmDoInterrupt(mntr, regs, VmReadRealByte(mntr, regs->cs, regs->eip + 1), regs->ss, regs->useresp, 2);
            return 0;

        case 0xFA:
            // CLI 
            mntr->interrupts_enabled = false;
            VmIncrementEip(regs);
            return 0;
        
        case 0xFB:
            // STI
            mntr->interrupts_enabled = true;
            VmIncrementEip(regs);
            return 0;

        case 0xCF:
            // IRET 
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] IRET");
            return EINVAL;

        case 0x6C:
            // INS 
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] INS");
            return EINVAL;

        case 0x6D:
            // INS 
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] INS");
            return EINVAL;

        case 0x6E:
            // OUTS 
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] OUTS");
            return EINVAL;

        case 0x6F:
            // OUTS 
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] OUTS");
            return EINVAL;

        case 0xE4:
            // IN
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] INx");
            return EINVAL;

        case 0xE5:
            // IN
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] INx");
            return EINVAL;

        case 0xE6:
            // OUT
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] OUTx");
            return EINVAL;

        case 0xE7:
            // OUT
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] OUTx");
            return EINVAL;

        case 0xEC:
            // IN
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] INx");
            return EINVAL;

        case 0xED:
            // IN
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] INx");
            return EINVAL;

        case 0xEE:
            // OUT
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] OUTx");
            return EINVAL;

        case 0xEF:
            // OUT
            PanicEx(PANIC_NOT_IMPLEMENTED, "[vm8086] OUTx");
            return EINVAL;

        case 0xCC:
            // INT3
            VmDoInterrupt(mntr, regs, 3, regs->ss, regs->useresp, 1);
            return 0;

        case 0xCE:
            // INTO
            if (regs->eflags & 0x800) {
                VmDoInterrupt(mntr, regs, 4, regs->ss, regs->useresp, 1);
            } else {
                VmIncrementEip(regs);
            }
            return 0;

        case 0xF1:
            // INT1
            VmDoInterrupt(mntr, regs, 1, regs->ss, regs->useresp, 1);
            return 0;

        default:
            return EINVAL;
        }
    }

    return EINVAL;
}

void Vm8086EntryPoint(void* m) {
    struct vm8086_monitor* mntr = m;

    (void) mntr;

    LogWriteSerial("TODO: enter into VM8086 mode here...\n");

    while (true) {
        ;
    
    }
}

struct vm8086_monitor* CreateVm8086Monitor(uint8_t* code, int code_size) {
    if (code_size >= ARCH_PAGE_SIZE) {
        return NULL;
    }

    struct vm8086_monitor* mntr = AllocHeap(sizeof(struct vm8086_monitor));
    mntr->host_comms_index = 0;
    memset(mntr->host_comms_data, 0, 256);

    size_t code_phys = AllocLowPage();
    size_t stack_phys = AllocLowPage();

    mntr->cs = VmLinearToSegment(code_phys);
    mntr->ip = VmLinearToOffset(code_phys);

    size_t stack_top = stack_phys + ARCH_PAGE_SIZE;
    mntr->ss = stack_top >> 4;
    mntr->sp = 0;

    mntr->code_phys = code_phys;    
    mntr->stack_phys = stack_phys;
    mntr->a20_enabled = false;
    mntr->interrupts_enabled = true;
    mntr->killed = false;
    mntr->exit_code = 0;

    mntr->thr = CreateThread(Vm8086EntryPoint, mntr, GetVas(), "vm8086");

    for (int i = 0; i < code_size; ++i) {
        VmWriteRealByte(mntr, mntr->cs, mntr->ip + i, code[i]);
    }

    return mntr;
}

void DestroyVm8086Task(struct vm8086_monitor* mntr) {
    LockScheduler();
    if (!mntr->killed) {
        assert(mntr->thr != GetThread());
        TerminateThreadLockHeld(mntr->thr);
    }
    UnlockScheduler();
    AddLowPage(mntr->code_phys);
    AddLowPage(mntr->stack_phys);
    FreeHeap(mntr);
}

struct vm8086_monitor* CreateVm8086Task(const char* path) {
    if (path == NULL) {
        return NULL;
    }

    struct open_file* file;
    int res = OpenFile(path, O_RDONLY, 0, &file);
    if (res != 0) {
        return NULL;
    }

    off_t file_size;
    res = GetFileSize(file, &file_size);
    if (res != 0 || file_size >= ARCH_PAGE_SIZE) {
        CloseFile(file);
        return NULL;
    }

    uint8_t* data = AllocHeap(file_size);
    struct transfer io = CreateKernelTransfer(data, file_size, 0, TRANSFER_READ);
    res = ReadFile(file, &io);
    if (res != 0 || io.length_remaining != 0) {
        FreeHeap(data);
        CloseFile(file);
        return NULL;
    }
    
    struct vm8086_monitor* mntr = CreateVm8086Monitor(data, file_size);
    FreeHeap(data);
    CloseFile(file);
    return mntr;
}

void InitVm8086(void) {
    past_a20_virt = MapVirt(0x100000, 0, 64 * 1024, VM_LOCK | VM_MAP_HARDWARE | VM_READ | VM_WRITE, NULL, 0);

    memset(low_pages, 0, sizeof(low_pages));
    num_low_pages = 0;
    TryReserveLowPages();
}