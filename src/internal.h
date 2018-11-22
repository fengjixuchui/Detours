//////////////////////////////////////////////////////////////////////////////
//
//  Internal Detours Functionality (internal.h of internal.cpp)
//
//  Microsoft Research Detours Package, Version 4.0.1
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//


#pragma once

//////////////////////////////////////////////////////////////////////////
// Detours Import & Macro

#if __has_include(<Windows.h>)
#define DetoursUserMode     1
#else
#define DetoursKernelMode   1
#endif

#pragma warning(disable:4068)           // unknown pragma (suppress)
#pragma warning(disable:4710)           // function not inlined

#pragma warning(push)

#if _MSC_VER >= 1900
#pragma warning(disable:4091)           // empty typedef
#endif

#if _MSC_VER > 1400
#   pragma warning(disable:6102 6103)   // /analyze warnings
#endif

#define _CRT_STDIO_ARBITRARY_WIDE_SPECIFIERS 1

#ifdef DetoursUserMode
#   define _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE 1
#   include <windows.h>
#   include <strsafe.h>
#else
#   include <ntddk.h>
#   include <wdm.h>
#   include <ntstrsafe.h>
#endif

#include <stddef.h>
#include <limits.h>

#define DETOURS_INTERNAL 1
#include "detours.h"

#if DETOURS_VERSION != DETOURS_VERSION_4
#error detours.h version mismatch
#endif

#define NOTHROW

#include "native.h"

#pragma warning(pop)

//////////////////////////////////////////////////////////////////////////////
// Detours Exception


#ifdef DetoursUserMode
#else
#define EXCEPTION_ACCESS_VIOLATION      STATUS_ACCESS_VIOLATION
#endif


//////////////////////////////////////////////////////////////////////////////
// Detours Status

#ifdef DetoursUserMode
#define DETOURS_STATUS_SUCCESS                  NO_ERROR
#define DETOURS_STATUS_INVALID_ADDRESS          ERROR_INVALID_ADDRESS
#define DETOURS_STATUS_INVALID_OPERATION        ERROR_INVALID_OPERATION
#define DETOURS_STATUS_INSUFFICIENT_RESOURCES   ERROR_NOT_ENOUGH_MEMORY
#define DETOURS_STATUS_INVALID_PARAMETER        ERROR_INVALID_PARAMETER
#define DETOURS_STATUS_INVALID_HANDLE           ERROR_INVALID_HANDLE
#define DETOURS_STATUS_INVALID_BLOCK            ERROR_INVALID_BLOCK
#define DETOURS_STATUS_OUTOFMEMORY              ERROR_OUTOFMEMORY
#define DETOURS_STATUS_MOD_NOT_FOUND            ERROR_MOD_NOT_FOUND
#define DETOURS_STATUS_BAD_EXE_FORMAT           ERROR_BAD_EXE_FORMAT
#define DETOURS_STATUS_INVALID_EXE_SIGNATURE    ERROR_INVALID_EXE_SIGNATURE
#define DETOURS_STATUS_CALL_NOT_IMPLEMENTED     ERROR_CALL_NOT_IMPLEMENTED
#else
#define DETOURS_STATUS_SUCCESS                  STATUS_SUCCESS
#define DETOURS_STATUS_INVALID_ADDRESS          STATUS_INVALID_ADDRESS
#define DETOURS_STATUS_INVALID_OPERATION        STATUS_INVALID_DEVICE_REQUEST
#define DETOURS_STATUS_INSUFFICIENT_RESOURCES   STATUS_INSUFFICIENT_RESOURCES
#define DETOURS_STATUS_INVALID_PARAMETER        STATUS_INVALID_PARAMETER
#define DETOURS_STATUS_INVALID_HANDLE           STATUS_INVALID_HANDLE
#define DETOURS_STATUS_INVALID_BLOCK            STATUS_INVALID_ADDRESS
#define DETOURS_STATUS_OUTOFMEMORY              STATUS_BUFFER_OVERFLOW
#define DETOURS_STATUS_MOD_NOT_FOUND            STATUS_DLL_NOT_FOUND
#define DETOURS_STATUS_BAD_EXE_FORMAT           STATUS_INVALID_IMAGE_WIN_32
#define DETOURS_STATUS_INVALID_EXE_SIGNATURE    STATUS_INVALID_IMAGE_NOT_MZ
#define DETOURS_STATUS_CALL_NOT_IMPLEMENTED     STATUS_NOT_IMPLEMENTED
#endif


//////////////////////////////////////////////////////////////////////////////
// Detours Internal Functionality

LONG DETOURS_API
DetoursGetLastError();

VOID DETOURS_API
DetoursSetLastError(_In_ LONG Code);

LONG DETOURS_API
DetoursCurrentProcessId();

LONG DETOURS_API
DetoursCurrentThreadId();

HANDLE DETOURS_API
DetoursCurrentProcess();

HANDLE DETOURS_API
DetoursCurrentThread();

SIZE_T DETOURS_API
DetoursQueryModuleMemoryBaseInformationForAddress(
    _In_opt_ LPVOID lpAddress,
    _Out_writes_bytes_to_(dwLength, return) PMEMORY_BASIC_INFORMATION lpBuffer, 
    _In_ SIZE_T dwLength
);
