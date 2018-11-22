#pragma once


//////////////////////////////////////////////////////////////////////////
// Struct

typedef enum _MEMORY_INFORMATION_CLASS
{
    MemoryBasicInformation,
    MemoryWorkingSetList,
    MemorySectionName,
    MemoryBasicVlmInformation,
    MemoryWorkingSetExList
} MEMORY_INFORMATION_CLASS;

#ifdef DetoursKernelMode
#define MEM_IMAGE 0x01000000  
typedef struct _MEMORY_BASIC_INFORMATION
{
    PVOID BaseAddress;
    PVOID AllocationBase;
    DWORD AllocationProtect;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;
#endif // DetoursUserMode

typedef enum _SYSTEM_INFORMATION_CLASS
{
    SystemBasicInformation,
    SystemProcessorInformation,             // obsolete...delete
    SystemPerformanceInformation,
    SystemTimeOfDayInformation,
    SystemPathInformation,
    SystemProcessInformation,
    SystemCallCountInformation,
    SystemDeviceInformation,
    SystemProcessorPerformanceInformation,
    SystemFlagsInformation,
    SystemCallTimeInformation,
    SystemModuleInformation,                // RTL_SYSTEM_MODULES
    // ...
} SYSTEM_INFORMATION_CLASS;

typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
    HANDLE  Section;                 // Not filled in
    PVOID   MappedBase;
    PVOID   ImageBase;
    ULONG   ImageSize;
    ULONG   Flags;
    USHORT  LoadOrderIndex;
    USHORT  InitOrderIndex;
    USHORT  LoadCount;
    USHORT  OffsetToFileName;
    UCHAR   FullPathName[256];
} 
RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION,
RTL_SYSTEM_MODULE_INFORMATION , *PRTL_SYSTEM_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES
{
    ULONG NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION Modules[1];
}
RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES,
RTL_SYSTEM_MODULES , *PRTL_SYSTEM_MODULES;


//////////////////////////////////////////////////////////////////////////
// Functional

#ifndef ZwCurrentProcess
#define ZwCurrentProcess()  ( (HANDLE)(LONG_PTR) -1 )  
#endif

#ifndef ZwCurrentThread
#define ZwCurrentThread()   ( (HANDLE)(LONG_PTR) -2 )   
#endif

#ifndef ZwCurrentSession
#define ZwCurrentSession()  ( (HANDLE)(LONG_PTR) -3 )  
#endif


extern"C"
{

    VOID NTAPI 
        RtlSetLastWin32ErrorAndNtStatusFromNtStatus(
        _In_ NTSTATUS aStatus
    );

    NTSTATUS NTAPI
        ZwQueryVirtualMemory(
            _In_ HANDLE ProcessHandle,
            _In_ PVOID BaseAddress,
            _In_ MEMORY_INFORMATION_CLASS MemoryInformationClass,
            _Out_bytecap_(MemoryInformationLength) PVOID MemoryInformation,
            _In_ SIZE_T MemoryInformationLength,
            _Out_opt_ PSIZE_T ReturnLength
        );

    NTSTATUS NTAPI
        ZwQuerySystemInformation(
            _In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
            _Out_bytecap_(SystemInformationLength) PVOID SystemInformation,
            _In_ ULONG SystemInformationLength,
            _Out_opt_ PULONG ReturnLength
        );
}
