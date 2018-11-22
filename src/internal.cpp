//////////////////////////////////////////////////////////////////////////////
//
//  Internal Detours Functionality (internal.cpp of detours.lib)
//
//  Microsoft Research Detours Package, Version 4.0.1
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//

#include "internal.h"

static LONG DetoursLastErrorCode = DETOURS_STATUS_SUCCESS;


//////////////////////////////////////////////////////////////////////////////
// Detours Arch impl


//////////////////////////////////////////////////////////////////////////
// Detours Common impl

LONG DETOURS_API
DetoursGetLastError()
{
#ifdef DetoursUserMode
    DetoursLastErrorCode = ::GetLastError();
#endif

    return DetoursLastErrorCode;
}

VOID DETOURS_API
DetoursSetLastError(_In_ LONG Code)
{
    InterlockedExchange(&DetoursLastErrorCode, Code);

#ifdef DetoursUserMode
    ::SetLastError(Code);
#endif
}

LONG DETOURS_API
DetoursCurrentProcessId()
{
#ifdef DetoursUserMode
    return (LONG)::GetCurrentProcessId();
#else
    return (LONG)(SIZE_T)::PsGetCurrentProcessId();
#endif
}

LONG DETOURS_API
DetoursCurrentThreadId()
{
#ifdef DetoursUserMode
    return (LONG)::GetCurrentThreadId();
#else
    return (LONG)(SIZE_T)::PsGetCurrentThreadId();
#endif
}

HANDLE DETOURS_API
DetoursCurrentProcess()
{
    return ZwCurrentProcess();
}

HANDLE DETOURS_API
DetoursCurrentThread()
{
    return ZwCurrentThread();
}

SIZE_T DETOURS_API
DetoursQueryModuleMemoryBaseInformationForAddress(
    _In_opt_ LPVOID lpAddress,
    _Out_writes_bytes_to_(dwLength, return) PMEMORY_BASIC_INFORMATION lpBuffer,
    _In_ SIZE_T dwLength
)
{
#ifdef DetoursUserMode
    return ::VirtualQuery(lpAddress, lpBuffer, dwLength);
#else

    NTSTATUS Status         = STATUS_SUCCESS;
    SIZE_T   ReturnLength   = 0;
    PRTL_SYSTEM_MODULES Modules = nullptr;

    for (;;)
    {
        if (dwLength < sizeof(*lpBuffer))
        {
            break;
        }

        ULONG NeedBytes = 0;
        Status = ZwQuerySystemInformation(
            SystemModuleInformation,
            nullptr, 0,
            &NeedBytes);
        if (!NT_SUCCESS(Status) &&
            Status != STATUS_INFO_LENGTH_MISMATCH)
        {
            break;
        }

        NeedBytes *= 2;
        Modules = (PRTL_SYSTEM_MODULES)ExAllocatePoolWithTag(
            NonPagedPoolNx, NeedBytes, DETOURS_TAG);
        if (Modules == nullptr)
        {
            break;
        }

        Status = ZwQuerySystemInformation(
            SystemModuleInformation,
            Modules, NeedBytes,
            &NeedBytes);
        if (!NT_SUCCESS(Status))
        {
            break;
        }

        for (ULONG i = 0; i < Modules->NumberOfModules; ++i)
        {
            PRTL_SYSTEM_MODULE_INFORMATION ModInfo = &Modules->Modules[i];

            if (ModInfo->ImageBase <= lpAddress && lpAddress < ((PBYTE)ModInfo->ImageBase + ModInfo->ImageSize))
            {
                PMEMORY_BASIC_INFORMATION MemInfo = lpBuffer;

                MemInfo->AllocationBase = ModInfo->ImageBase;
                MemInfo->BaseAddress    = ModInfo->ImageBase;
                MemInfo->RegionSize     = ModInfo->ImageSize;
                MemInfo->State          = MEM_COMMIT;
                MemInfo->Protect        = PAGE_READONLY;
                MemInfo->Type           = MEM_MAPPED | MEM_IMAGE;
                MemInfo->AllocationProtect = PAGE_EXECUTE_READWRITE;

                ReturnLength = sizeof(*MemInfo);
                break;
            }
        }

        break;
    }
    if (Modules)
    {
        ExFreePoolWithTag(Modules, DETOURS_TAG);
    }

    return ReturnLength;
#endif
}

