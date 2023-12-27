
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
#include <machine/portio.h>
#include <machine/interrupt.h>

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
        PanicEx(PANIC_DRIVER_FAULT, "acpi: AML fatal opcode");
        break;
    case ACPI_SIGNAL_BREAKPOINT:
        PanicEx(PANIC_DRIVER_FAULT, "acpi: AML breakpoint\n");
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
    
    size_t virt = MapVirt(PhysicalAddress & ~(ARCH_PAGE_SIZE - 1), 0, Length, VM_READ | VM_WRITE | VM_LOCK, NULL, 0);
    return (void*) (size_t) (virt | (PhysicalAddress & (ARCH_PAGE_SIZE - 1)));
}

void AcpiOsUnmapMemory(void* where, ACPI_SIZE length)
{
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
    return AllocHeap(Size);;
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
    DestroySemaphore((struct semaphore*) Handle);
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
    AcquireSpinlockIrql((struct spinlock*) Handle);
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    ReleaseSpinlockIrql((struct spinlock*) Handle);
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void* Context)
{
    PanicEx(PANIC_NOT_IMPLEMENTED, "acpi: AcpiOsInstallInterruptHandler - how are we going to do context??");
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler)
{
    PanicEx(PANIC_NOT_IMPLEMENTED, "acpi: AcpiOsRemoveInterruptHandler");
    return AE_OK;
}

void AcpiOsVprintf(const char* format, va_list list)
{

}

void AcpiOsPrintf(const char* format, ...)
{
    
}

void InitAcpica() {
    LogWriteSerial("ACPICA.SYS loaded\n");

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

	a = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
									   ACPI_ADR_SPACE_SYSTEM_MEMORY, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiInstallAddressSpaceHandler ACPI_ADR_SPACE_SYSTEM_MEMORY");
        return;
    }

	a = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
									   ACPI_ADR_SPACE_SYSTEM_IO, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiInstallAddressSpaceHandler ACPI_ADR_SPACE_SYSTEM_IO");
        return;
    }

	a = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
									   ACPI_ADR_SPACE_PCI_CONFIG, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiInstallAddressSpaceHandler ACPI_ADR_SPACE_PCI_CONFIG");
        return;
    }

	a = AcpiLoadTables();
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiLoadTables");
        return;
    }

	a = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiEnableSubsystem");
        return;
    }
    
	a = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(a)) {
        LogDeveloperWarning("FAILURE AcpiInitializeObjects");
        return;
    }

    LogWriteSerial("ACPICA.SYS fully initialised\n");
}