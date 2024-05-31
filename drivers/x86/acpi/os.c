
#include <log.h>
#include <fcntl.h>
#include <vfs.h>
#include <string.h>
#include <common.h>
#include <irql.h>
#include <heap.h>
#include <virtual.h>
#include <physical.h>
#include <thread.h>
#include <cpu.h>
#include <spinlock.h>
#include <string.h>
#include <semaphore.h>
#include <irq.h>
#include <timer.h>
#include <panic.h>
#include <sys/stat.h>
#include <machine/regs.h>
#include <machine/pic.h>
#include <machine/portio.h>
#include <machine/interrupt.h>

/*
 * TODO: some bad news ... 
 * "The one who holds a spinlock must be locked into virtual memory" (otherwise code at IRQL_SCHEDULER+ i.e. the critical section
 * could be paged out...).
 * 
 * Holding mutexes should be fine, but spinlocks are a big no no. How often does ACPICA use spinlocks? Is it controlled
 * by AML? Can it get away with a less strong locking mechanism??
 * 
 * Or, the ACPICA spec seems to think a single threaded mode is okay and means spinlocks et. al. can just be no-ops.
 * How would that work?
 */

#include "acpi.h"

ACPI_STATUS AcpiOsInitialize()
{
    LogWriteSerial("AcpiOsInitialize\n");
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate()
{
    LogWriteSerial("AcpiOsTerminate\n");
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
    LogWriteSerial("AcpiOsGetRootPointer\n");
    ACPI_PHYSICAL_ADDRESS Ret = 0;
    AcpiFindRootPointer(&Ret);
    return Ret;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* PredefinedObject, ACPI_STRING* NewValue)
{    
    LogWriteSerial("AcpiOsPredefinedOverride\n");

    if (!PredefinedObject || !NewValue) {
        return AE_BAD_PARAMETER;
    }

    *NewValue = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_TABLE_HEADER** NewTable)
{
    LogWriteSerial("AcpiOsTableOverride\n");

    *NewTable = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_PHYSICAL_ADDRESS* NewAddress, UINT32* NewTableLength)
{
    LogWriteSerial("AcpiOsPhysicalTableOverride\n");

    *NewAddress = 0;
    return AE_OK;
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32* Value, UINT32 Width)
{
    LogWriteSerial("AcpiOsReadPort\n");

    if (Width == 8) *Value = inb(Address);
    else if (Width == 16) *Value = inw(Address);
    else if (Width == 32) *Value = inl(Address);
    else return AE_BAD_PARAMETER;
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    LogWriteSerial("AcpiOsWritePort\n");

    if (Width == 8) outb(Address, (uint8_t) Value);
    else if (Width == 16) outw(Address, (uint16_t) Value);
    else if (Width == 32) outl(Address, Value);
    else return AE_BAD_PARAMETER;
    return AE_OK;
}

UINT64 AcpiOsGetTimer(void)
{
    LogWriteSerial("AcpiOsGetTimer\n");

    /* returns it in 100 nanosecond units */
    return GetSystemTimer() / 100;
}

void AcpiOsWaitEventsComplete(void)
{
    PanicEx(PANIC_NOT_IMPLEMENTED, "acpi: AcpiOsWaitEventsComplete");
}

//https://github.com/no92/vineyard/blob/dev/kernel/driver/acpi/osl/pci.c
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* PciId, UINT32 Register, UINT64* Value, UINT32 Width)
{
    LogWriteSerial("AcpiOsReadPciConfiguration\n");

    uint32_t regAligned = Register & ~0x03U;
    uint32_t offset = Register & 0x03U;
    uint32_t addr = (uint32_t) ((uint32_t) (PciId->Bus << 16) | (uint32_t) (PciId->Device << 11) | (uint32_t) (PciId->Function << 8) | ((uint32_t) 0x80000000) | regAligned);

    outl(0xCF8, addr);

    uint32_t ret = inl(0xCFC);

    void* res = (char*) &ret + offset;
    size_t count = Width >> 3;
    *Value = 0;

    memcpy(Value, res, count);

    return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* PciId, UINT32 Register, UINT64 Value, UINT32 Width)
{
    LogWriteSerial("AcpiOsWritePciConfiguration\n");

    uint32_t addr = (uint32_t) ((uint32_t) (PciId->Bus << 16) | (uint32_t) (PciId->Device << 11) | (uint32_t) (PciId->Function << 8) | ((uint32_t) 0x80000000));
    addr += Register;

    outl(0xCF8, addr);

    switch (Width) {
    case 8:
        outb(0xCFC, Value & 0xFF);
        break;
    case 16:
        outw(0xCFC, Value & 0xFFFF);
        break;
    case 32:
        outl(0xCFC, Value & 0xFFFFFFFF);
        break;
    default:
        PanicEx(PANIC_DRIVER_FAULT, "acpi: AcpiOsWritePciConfiguration bad width!");
        break;
    }

    return AE_OK;

}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void* Info)
{
    LogWriteSerial("AcpiOsSignal\n");

    switch (Function) {
    case ACPI_SIGNAL_FATAL:
        PanicEx(PANIC_ACPI_AML, "acpi: AML fatal opcode");
        break;
    case ACPI_SIGNAL_BREAKPOINT:
        PanicEx(PANIC_ACPI_AML, "acpi: AML breakpoint\n");
        break;
    default:
        PanicEx(PANIC_DRIVER_FAULT, "acpi: AcpiOsSignal");
        return AE_BAD_PARAMETER;
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64* Value, UINT32 Width)
{
    LogWriteSerial("acpi: TODO: read memory...");
    if (Width == 8) {
        uint8_t* a = (uint8_t*) (size_t) Address;
        *Value = *a;
    }
    else if (Width == 16) {
        uint16_t* a = (uint16_t*) (size_t) Address;
        *Value = *a;
    }
    else if (Width == 32) {
        uint32_t* a = (uint32_t*) (size_t) Address;
        *Value = *a;
    }
    else if (Width == 64) {
        uint64_t* a = (uint64_t*) (size_t) Address;
        *Value = *a;
    }
    else return AE_BAD_PARAMETER;
    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue)
{
    LogWriteSerial("AcpiOsEnterSleep\n");
    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
    LogWriteSerial("acpi: TODO: write memory...");

    if (Width == 8) {
        uint8_t* a = (uint8_t*) (size_t) Address;
        *a = Value;
    }
    else if (Width == 16) {
        uint16_t* a = (uint16_t*) (size_t) Address;
        *a = Value;
    }
    else if (Width == 32) {
        uint32_t* a = (uint32_t*) (size_t) Address;
        *a = Value;
    }
    else if (Width == 64) {
        uint64_t* a = (uint64_t*) (size_t) Address;
        *a = Value;
    }
    else return AE_BAD_PARAMETER;
    return AE_OK;
}

void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{	
    LogWriteSerial("AcpiOsMapMemory 0x%X 0x%X\n", (uint32_t) PhysicalAddress, (uint32_t) Length);
    
    int extra = (PhysicalAddress & (ARCH_PAGE_SIZE - 1));
    size_t virt = MapVirt(PhysicalAddress & ~(ARCH_PAGE_SIZE - 1), 0, Length + extra, VM_READ | VM_WRITE | VM_LOCK | VM_MAP_HARDWARE, NULL, 0);
    LogWriteSerial("Got virt: 0x%X\nReturning: 0x%X\n", virt, virt | (PhysicalAddress & (ARCH_PAGE_SIZE - 1)));
    return (void*) (size_t) (virt | (PhysicalAddress & (ARCH_PAGE_SIZE - 1)));
}

void AcpiOsUnmapMemory(void* where, ACPI_SIZE length)
{
    LogWriteSerial("AcpiOsUnmapMemory\n");

    UnmapVirt(((size_t) where) & ~(ARCH_PAGE_SIZE - 1), length);
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void* LogicalAddress, ACPI_PHYSICAL_ADDRESS* PhysicalAddress)
{
    LogWriteSerial("AcpiOsGetPhysicalAddress\n");
    *PhysicalAddress = GetPhysFromVirt((size_t) LogicalAddress);
    return AE_OK;
}

void* AcpiOsAllocate(ACPI_SIZE Size)
{
    return AllocHeap(Size);
}

void AcpiOsFree(void* Memory)
{
    FreeHeap(Memory);
}

BOOLEAN AcpiOsReadable(void* Memory, ACPI_SIZE Length)
{
    LogWriteSerial("AcpiOsReadable 0x%X\n", Memory);
    return true;
}

BOOLEAN AcpiOsWritable(void* Memory, ACPI_SIZE Length)
{
    LogWriteSerial("AcpiOsWritable 0x%X\n", Memory);
    return true;
}

ACPI_THREAD_ID AcpiOsGetThreadId()
{
    return GetThread()->thread_id;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void* Context)
{
    LogWriteSerial("AcpiOsExecute %d\n", Type);

    CreateThread(Function, Context, GetVas(), "acpica");
    return AE_OK;
}

void AcpiOsSleep(UINT64 Milliseconds)
{
    LogWriteSerial("AcpiOsSleep\n");
    SleepMilli(Milliseconds);
}

void AcpiOsStall(UINT32 Microseconds)
{
    LogWriteSerial("AcpiOsStall\n");

    uint64_t end = GetSystemTimer() + Microseconds * 1000 + 1;
    while (GetSystemTimer() < end) {
        ;
    }
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE* OutHandle)
{
    struct semaphore* sem = CreateSemaphore("acpica", MaxUnits, MaxUnits - InitialUnits);    
    *OutHandle = (void*) sem;
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    DestroySemaphore((struct semaphore*) Handle, SEM_DONT_CARE);
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
    if (Units > 1) {
        LogDeveloperWarning("AcpiOsWaitSemaphore units > 1\n");
        return AE_SUPPORT;
    }
    
    if (Timeout == 0xFFFF) {
        AcquireSemaphore((struct semaphore*) Handle, -1);
        return;
    }

    int res = AcquireSemaphore((struct semaphore*) Handle, 0);
    if (res == 0) {
        return AE_OK;
    } else {
        return AE_TIME;
    }
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    if (Units > 1) {
        LogDeveloperWarning("AcpiOsSignalSemaphore units > 1\n");
        return AE_SUPPORT;
    }

    ReleaseSemaphore((struct semaphore*) Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* OutHandle)
{
    LogWriteSerial("AcpiOsCreateLock\n");

    struct spinlock* lock = AllocHeap(sizeof(struct spinlock));
    InitSpinlock(lock, "acpica", IRQL_SCHEDULER);

    *OutHandle = (ACPI_SPINLOCK*) lock;

    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_HANDLE Handle)
{
    FreeHeap(Handle);
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
    LogDeveloperWarning("AcpiOsAcquireLock\n");
    //AcquireSpinlock((struct spinlock*) Handle);
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    //ReleaseSpinlock((struct spinlock*) Handle);
    LogDeveloperWarning("AcpiOsReleaseLock\n");
}

struct acpica_interrupt_handler {
    ACPI_OSD_HANDLER handler;
    void* context;
    bool valid;
};

static struct acpica_interrupt_handler acpica_interrupt_handlers[32];

static int HandleAcpicaInterrupt(int num) {
    MAX_IRQL(IRQL_PAGE_FAULT);

    if (num >= 32) {
        return 1;
    }

    if (acpica_interrupt_handlers[num].valid) {
        LogWriteSerial("running an acpica interrupt handler! (IRQ %d)\n", num);
        UINT32 res = acpica_interrupt_handlers[num].handler(acpica_interrupt_handlers[num].context);
        if (res == ACPI_INTERRUPT_HANDLED) {
            return 0;
        } else {
            return 1;
        }

    } else {
        return 1;
    }
}

static int LOCKED_DRIVER_DATA acpica_caught_irq = -1;

static int LOCKED_DRIVER_CODE AcpicaInterruptCatcher(struct x86_regs* regs) {
    /*
     * We can't actually run the handler right now, as we're in IRQL_DRIVER or above.
     * And ACPICA has no concept of IRQL, so it could do whatever. Instead, we'll just set a flag,
     * and the mainloop of the ACPICA thread can poll for it (every 200ms). 
     */
    int num = regs->int_no - PIC_IRQ_BASE;
    if (acpica_caught_irq == -1) {
        acpica_caught_irq = num;
    } else {
        PanicEx(PANIC_DRIVER_FAULT, "second acpica interrupt caught before first one done");
    }
} 

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void* Context)
{
    LogWriteSerial("AcpiOsInstallInterruptHandler\n");

    if (InterruptLevel >= 32 || Handler == NULL) {
        return AE_BAD_PARAMETER;
    }

    if (acpica_interrupt_handlers[InterruptLevel].valid) {
        return AE_ALREADY_EXISTS;
    }

    LogWriteSerial("ACPICA is installing an interrupt handler for IRQ %d\n", InterruptLevel);

    acpica_interrupt_handlers[InterruptLevel].handler = Handler;
    acpica_interrupt_handlers[InterruptLevel].context = Context;
    acpica_interrupt_handlers[InterruptLevel].valid = true;

    RegisterIrqHandler(PIC_IRQ_BASE + InterruptLevel, AcpicaInterruptCatcher);

    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler)
{
    LogWriteSerial("AcpiOsRemoveInterruptHandler\n");

    if (InterruptNumber >= 32 || Handler == NULL) {
        return AE_BAD_PARAMETER;
    }

    if (!acpica_interrupt_handlers[InterruptNumber].valid) {
        return AE_NOT_EXIST;
    }

    if (acpica_interrupt_handlers[InterruptNumber].handler != Handler) {
        return AE_BAD_PARAMETER;
    }

    acpica_interrupt_handlers[InterruptNumber].valid = false;
    return AE_OK;
}

void AcpiOsVprintf(const char* format, va_list list)
{
    if (format != NULL) {
        LogWriteSerial("AcpiOsVprintf: %s\n", format);
    }
}

void AcpiOsPrintf(const char* format, ...)
{
    if (format != NULL) {
        LogWriteSerial("AcpiOsPrintf: %s\n", format);
    }
}

void AcpicaSleep(void) {
	AcpiEnterSleepStatePrep(2);
	AcpiEnterSleepState(2);

	//the computer sleeps here (execution stops here until awoken)

	AcpiLeaveSleepStatePrep(2);
	AcpiLeaveSleepState(2);
}

void AcpicaShutdown(void) {
    ACPI_STATUS a = AcpiEnterSleepStatePrep(5);
	if (a != AE_OK) {
		return;
	}
	asm volatile ("cli");
	a = AcpiEnterSleepState(5);
}

static ACPI_STATUS AcpicaPowerButtonHandler(void*) {
    ArchSetPowerState(ARCH_POWER_STATE_SHUTDOWN);
}

static ACPI_STATUS AcpicaPowerNotifyHandler(void*) {
    ArchSetPowerState(ARCH_POWER_STATE_SHUTDOWN);
}

static ACPI_STATUS AcpicaLidNotifyHandler(void*) {
    LogWriteSerial("AcpicaLidNotifyHandler\n");
}

static ACPI_STATUS AcpicaSleepNotifyHandler(void*) {
    ArchSetPowerState(ARCH_POWER_STATE_SLEEP);
}

static void AcpicaGlobalEventHandler(uint32_t type, ACPI_HANDLE device, uint32_t number, void* context) {
    LogWriteSerial("AcpicaGlobalEventHandler\n");
    
	if (type == ACPI_EVENT_TYPE_FIXED && number == ACPI_EVENT_POWER_BUTTON) {
		ArchSetPowerState(ARCH_POWER_STATE_SLEEP);
	}
	if (type == ACPI_EVENT_TYPE_FIXED && number == ACPI_EVENT_SLEEP_BUTTON) {
	    ArchSetPowerState(ARCH_POWER_STATE_SHUTDOWN);
	}
}

static ACPI_STATUS RegisterAcpicaObject(ACPI_HANDLE parent, ACPI_HANDLE obj, ACPI_DEVICE_INFO* info) {
    (void) parent;
    (void) obj;
    (void) info;

    ACPI_STATUS res;

    if (info == NULL) {
        return AE_OK;
    }

    LogWriteSerial("found an ACPI object...\n");

    if (info->Type == ACPI_TYPE_DEVICE) {
        const char* hid = (info->Valid & ACPI_VALID_HID) ? info->HardwareId.String : NULL;
    
        if (hid != NULL) {
            int hid_length = info->HardwareId.Length;

            if (!strncmp(hid, "PNP0C0C", hid_length)) {
                res = AcpiInstallNotifyHandler(obj, ACPI_ALL_NOTIFY, AcpicaPowerNotifyHandler, NULL);
                if (res != AE_OK) {
                    LogDeveloperWarning("FAILURE AcpiInstallNotifyHandler(PNP0C0C)");
                    if (res == AE_NO_MEMORY) {
                        return AE_NO_MEMORY;
                    }
                }
            }

            if (!strncmp(hid, "PNP0C0D", hid_length)) {
                res = AcpiInstallNotifyHandler(obj, ACPI_ALL_NOTIFY, AcpicaLidNotifyHandler, NULL);
                if (res != AE_OK) {
                    LogDeveloperWarning("FAILURE AcpicaLidNotifyHandler(PNP0C0D)");
                    if (res == AE_NO_MEMORY) {
                        return AE_NO_MEMORY;
                    }
                }
            }

            if (!strncmp(hid, "PNP0C0E", hid_length)) {
                res = AcpiInstallNotifyHandler(obj, ACPI_ALL_NOTIFY, AcpicaSleepNotifyHandler, NULL);
                if (res != AE_OK) {
                    LogDeveloperWarning("FAILURE AcpicaLidNotifyHandler(PNP0C0E)");
                    if (res == AE_NO_MEMORY) {
                        return AE_NO_MEMORY;
                    }
                }
            }
        }
    }

    return AE_OK;
}

// *********************************************************************************************
//
// From here:
// https://github.com/vvaltchev/tilck/blob/master/modules/acpi/acpi_module.c#L392
//
// *********************************************************************************************

static ACPI_STATUS AcpicaWalkSingleObject(ACPI_HANDLE parent, ACPI_HANDLE obj) {
    ACPI_DEVICE_INFO* info;
    ACPI_STATUS res = AcpiGetObjectInfo(obj, &info);

    if (ACPI_FAILURE(res)) {
        return res;
    }

    res = RegisterAcpicaObject(parent, obj, info);

    ACPI_FREE(info);

    if (res == AE_NO_MEMORY) {
        return res;
    }

    return AE_OK;
}

static ACPI_STATUS AcpicaWalkNamespace(void)
{
    ACPI_HANDLE parent = NULL;
    ACPI_HANDLE child = NULL;
    ACPI_STATUS res;

    while (true) {
        res = AcpiGetNextObject(ACPI_TYPE_ANY, parent, child, &child);

        if (ACPI_FAILURE(res)) {
            /*
             * No more children. If this is root, we must stop. Otherwise, go back upwards.
             */
            if (parent == NULL) {
                break;
            }

            child = parent;
            AcpiGetParent(parent, &parent);
            continue;
        }

        res = AcpicaWalkSingleObject(parent, child);
        if (ACPI_FAILURE(res)) {
            return res;
        }

        res = AcpiGetNextObject(ACPI_TYPE_ANY, child, NULL, NULL);
        if (ACPI_SUCCESS(res)) {
            parent = child;
            child = NULL;
        }
    }

    return AE_OK;
}

// *********************************************************************************************
//  END 
// *********************************************************************************************


/*
 * "He who acquires a spinlock must be locked"
 */
static void LOCKED_DRIVER_CODE PollIrqs() {
    struct spinlock irq_caught_lock;
    InitSpinlock(&irq_caught_lock, "acpica irq", IRQL_HIGH);

    /*
     * Poll for any IRQs that have been raised, and execute the ACPICA handler if needed.
     * We do this as we can't run the handlers from the IRQ context for IRQL reasons.
     */
    while (true) {
        int irql = AcquireSpinlock(&irq_caught_lock);
        if (acpica_caught_irq != -1) {
            int irq = acpica_caught_irq;
            acpica_caught_irq = -1;
            ReleaseSpinlock(&irq_caught_lock);
            HandleAcpicaInterrupt(irq);

        } else {
            ReleaseSpinlock(&irq_caught_lock);
        }

        SleepMilli(200);
    }
}

void AcpicaThread(void*) {
    LogWriteSerial("ACPI.SYS loaded\n");

    AcpiDbgLevel = (ACPI_NORMAL_DEFAULT | ACPI_LV_EVENTS) & ~ACPI_LV_REPAIR;
    AcpiGbl_DisableAutoRepair = true;

    for (int i = 0; i < 32; ++i) {
        acpica_interrupt_handlers[i].valid = false;
    }

    ACPI_STATUS a = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiInitializeSubsystem");
        return;
    }

	a = AcpiInitializeTables(NULL, 16, true);
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiInitializeTables");
        return;
    }

	a = AcpiLoadTables();
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiLoadTables");
        return;
    }

    InitCenturyRegister();

    // For some PCs, may need to add this to get power button to work, 
    // according to https://forum.osdev.org/viewtopic.php?f=1&t=33640
    // The handler itself (`acpi_ec_handler`) can just return AE_OK.
    // status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT, ACPI_ADR_SPACE_EC, &acpi_ec_handler, NULL, NULL)

	a = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiEnableSubsystem");
        return;
    }

    a = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
									   ACPI_ADR_SPACE_SYSTEM_MEMORY, ACPI_DEFAULT_HANDLER, NULL, NULL);

	a = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
									   ACPI_ADR_SPACE_SYSTEM_IO, ACPI_DEFAULT_HANDLER, NULL, NULL);

	a = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
									   ACPI_ADR_SPACE_PCI_CONFIG, ACPI_DEFAULT_HANDLER, NULL, NULL);

	a = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiInitializeObjects");
        return;
    }

    a = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, AcpicaPowerButtonHandler, NULL);
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON)");
    }

    a = AcpicaWalkNamespace();
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpicaWalkNamespace");
        return;
    }

    a = AcpiInstallGlobalEventHandler(AcpicaGlobalEventHandler, NULL);
	if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiInstallGlobalEventHandler");
        return;
    }

    a = AcpiUpdateAllGpes();
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiUpdateAllGpes");
        return;
    }

    ACPI_BUFFER buffer;
    ACPI_SYSTEM_INFO sysinfo;
    buffer.Length = sizeof(ACPI_SYSTEM_INFO);
    buffer.Pointer = &sysinfo;
    a = AcpiGetSystemInfo(&buffer);
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiGetSystemInfo");
    } else {
        if (sysinfo.Flags == ACPI_SYS_MODE_ACPI) {
            LogWriteSerial("This system supports ACPI mode.\n");
        } else {
            LogWriteSerial("This system does not support ACPI mode.\n");
        }
    }

    bool using_apic = false;
    ACPI_STATUS status;
    ACPI_OBJECT_LIST params;
    ACPI_OBJECT arg;

    params.Count = 1;
    params.Pointer = &arg;

    arg.Type = ACPI_TYPE_INTEGER;
    arg.Integer.Value = using_apic ? 1 : 0;

    status = AcpiEvaluateObject(NULL, (ACPI_STRING) "\\_PIC", &params, NULL);
    if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
        LogWriteSerial("status = 0x%X\n", status);
        LogDeveloperWarning("ACPI failure AcpiEvaluateObject(\\_PIC)");

    } else if (status == AE_NOT_FOUND) {
        LogWriteSerial("\\_PIC method not found.\n");

    } else {
        LogWriteSerial("\\_PIC method success.\n");
    }

	a = AcpiWriteBitRegister(ACPI_BITREG_SCI_ENABLE, 1);
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiWriteBitRegister(ACPI_BITREG_SCI_ENABLE, 1)");
        return;
    }

	a = AcpiWriteBitRegister(ACPI_BITREG_POWER_BUTTON_ENABLE, 1);
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiWriteBitRegister(ACPI_BITREG_POWER_BUTTON_ENABLE, 1)");
        return;
    }

	a = AcpiWriteBitRegister(ACPI_BITREG_SLEEP_BUTTON_ENABLE, 1);
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiWriteBitRegister(ACPI_BITREG_SLEEP_BUTTON_ENABLE, 1)");
        return;
    }

	a = AcpiEnableEvent(ACPI_EVENT_SLEEP_BUTTON, 0);
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiEnableEvent(ACPI_EVENT_SLEEP_BUTTON)");
        return;
    }

	a = AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON)");
        return;
    }
    
    ACPI_EVENT_STATUS event_status;
    a = AcpiGetEventStatus(ACPI_EVENT_POWER_BUTTON, &event_status);
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiGetEventStatus(ACPI_EVENT_POWER_BUTTON)");
    } else {
        if (event_status & ACPI_EVENT_FLAG_STATUS_SET) {
            LogWriteSerial("Event ACPI_EVENT_POWER_BUTTON has occured.\n");
        } else if (event_status & ACPI_EVENT_FLAG_ENABLED) {
            LogWriteSerial("Event ACPI_EVENT_POWER_BUTTON is enabled.\n");
        } else if (event_status & ACPI_EVENT_FLAG_HAS_HANDLER) {
            LogWriteSerial("Event ACPI_EVENT_POWER_BUTTON has a handler.\n");
        }
    }

    UINT32 retv;
    a = AcpiReadBitRegister(ACPI_BITREG_SCI_ENABLE, &retv);
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiReadBitRegister(ACPI_BITREG_SCI_ENABLE)");
    } else {
        LogWriteSerial("Register ACPI_BITREG_SCI_ENABLE has value: 0x%X\n", retv);
    }

    a = AcpiReadBitRegister(ACPI_BITREG_POWER_BUTTON_ENABLE, &retv);
    if (a != AE_OK) {
        LogDeveloperWarning("FAILURE AcpiReadBitRegister(ACPI_BITREG_POWER_BUTTON_ENABLE)");
    } else {
        LogWriteSerial("Register ACPI_BITREG_POWER_BUTTON_ENABLE has value: 0x%X\n", retv);
    }

    LogWriteSerial("ACPI.SYS fully initialised\n");

    InitSimpleBootFlag();
    PollIrqs();
}

void InitAcpica(void) {
    CreateThread(AcpicaThread, NULL, GetVas(), "acpcia");
}