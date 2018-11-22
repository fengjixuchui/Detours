//////////////////////////////////////////////////////////////////////////////
//
//  Common version parameters.
//
//  Microsoft Research Detours Package, Version 4.0.1
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//


#if __has_include(<Windows.h>)
#   define DetoursUserMode      1
#else
#   define DetoursKernelMode    1

typedef void                *HMODULE;
typedef const void          *PCVOID, *LPCVOID;
typedef void                *PVOID, *LPVOID;

typedef int                 BOOL;   typedef BOOL    *PBOOL, *LPBOOL;
typedef unsigned char       BYTE;   typedef BYTE    *PBYTE, *LPBYTE;
typedef unsigned short      WORD;   typedef WORD    *PWORD, *LPWORD;
typedef unsigned long       DWORD;  typedef DWORD   *PDWORD, *LPDWORD;
typedef unsigned __int64    QWORD;  typedef QWORD   *PQWORD, *LPQWORD;

typedef short               SHORT;  typedef SHORT   *PSHORT, *LPSHORT;
typedef int                 INT;    typedef INT     *PINT, *LPINT;
typedef long                LONG;   typedef LONG    *PLONG, *LPLONG;

typedef unsigned short      USHORT; typedef USHORT  *PUSHORT, *LPUSHORT;
typedef unsigned int        UINT;   typedef UINT    *PUINT, *LPUINT;
typedef unsigned long       ULONG;  typedef ULONG   *PULONG, *LPULONG;
#endif


#define DETOURS_CALLBACK    __stdcall
#define DETOURS_API         __stdcall


#ifdef _DEBUG
#   define DETOUR_DEBUG 1
#endif


#ifdef DetoursUserMode
// If you want to support XP, please open it
// #define _USING_V110_SDK71_ 1

#   include "winver.h"
#else
#   include <ntimage.h>
#endif


#if 0
#   ifdef DetoursUserMode
#       include <Windows.h>
#   else
#       include <ntddk.h>
#       include <wdm.h>
#   endif
#endif


#ifndef DETOURS_STRINGIFY
#define DETOURS_STRINGIFY(x)    DETOURS_STRINGIFY_(x)
#define DETOURS_STRINGIFY_(x)   #x
#endif

#define VER_DETOURS_BITS        DETOUR_STRINGIFY(DETOURS_BITS)

#define VER_FILEFLAGSMASK       0x3fL
#define VER_FILEFLAGS           0x0L
#define VER_FILEOS              0x00040004L
#define VER_FILETYPE            0x00000002L
#define VER_FILESUBTYPE         0x00000000L

#define DETOURS_VERSION_4       0x4c0c1     // 0xMAJORcMINORcPATCH
#define DETOURS_VERSION         DETOURS_VERSION_4
