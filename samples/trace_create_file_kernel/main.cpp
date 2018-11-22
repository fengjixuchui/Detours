#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

#include <detours.h>


extern"C"
{
    NTSTATUS NTAPI
        NtCreateFile(
            _Out_ PHANDLE FileHandle,
            _In_ ACCESS_MASK DesiredAccess,
            _In_ POBJECT_ATTRIBUTES ObjectAttributes,
            _Out_ PIO_STATUS_BLOCK IoStatusBlock,
            _In_opt_ PLARGE_INTEGER AllocationSize,
            _In_ ULONG FileAttributes,
            _In_ ULONG ShareAccess,
            _In_ ULONG CreateDisposition,
            _In_ ULONG CreateOptions,
            _In_reads_bytes_opt_(EaLength) PVOID EaBuffer,
            _In_ ULONG EaLength
        );
}

extern"C" DRIVER_INITIALIZE DriverEntry;

void* NtCreateFileTrampoline = nullptr;
NTSTATUS
NTAPI
NtCreateFileDetour(
    _Out_ PHANDLE FileHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock,
    _In_opt_ PLARGE_INTEGER AllocationSize,
    _In_ ULONG FileAttributes,
    _In_ ULONG ShareAccess,
    _In_ ULONG CreateDisposition,
    _In_ ULONG CreateOptions,
    _In_reads_bytes_opt_(EaLength) PVOID EaBuffer,
    _In_ ULONG EaLength
)
{
    if (ObjectAttributes && NT_SUCCESS(RtlUnicodeStringValidate(ObjectAttributes->ObjectName)))
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
           __FUNCTION__ ": %wZ \n", ObjectAttributes->ObjectName);
    }

    return static_cast<decltype(NtCreateFile)*>(NtCreateFileTrampoline)(
        FileHandle,
        DesiredAccess,
        ObjectAttributes,
        IoStatusBlock,
        AllocationSize,
        FileAttributes,
        ShareAccess,
        CreateDisposition,
        CreateOptions,
        EaBuffer,
        EaLength);
}

static
VOID DriverUnload(PDRIVER_OBJECT /*aDriverObject*/)
{
    DetourTransactionBegin();
    DetourUpdateThread(ZwCurrentThread());
    DetourDetach(&(void*&)NtCreateFileTrampoline, NtCreateFileDetour);
    DetourTransactionCommit();
}

extern"C"
NTSTATUS DriverEntry(PDRIVER_OBJECT aDriverObject, PUNICODE_STRING /*aRegistryPath*/)
{
    auto vStatus = STATUS_SUCCESS;

    for (;;)
    {
        NtCreateFileTrampoline = NtCreateFile;

        DetourTransactionBegin();
        DetourUpdateThread(ZwCurrentThread());
        DetourAttach(&(void*&)NtCreateFileTrampoline, NtCreateFileDetour);
        DetourTransactionCommit();

        aDriverObject->DriverUnload = DriverUnload;
        break;
    }

    return vStatus;
}
