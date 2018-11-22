//////////////////////////////////////////////////////////////////////////////
//
//  Module Enumeration Functions (modules.cpp of detours.lib)
//
//  Microsoft Research Detours Package, Version 4.0.1
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  Module enumeration functions.
//

#include "internal.h"


#ifdef DetoursUserMode

//////////////////////////////////////////////////////////////////////////
//
#define CLR_DIRECTORY OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR]
#define IAT_DIRECTORY OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT]

//////////////////////////////////////////////////////////////////////////////
//
const GUID DETOUR_EXE_RESTORE_GUID = {
    0x2ed7a3ff, 0x3339, 0x4a8d,
    { 0x80, 0x5c, 0xd4, 0x98, 0x15, 0x3f, 0xc2, 0x8f }};

//////////////////////////////////////////////////////////////////////////////
//

PDETOUR_SYM_INFO DetourLoadImageHlp(VOID)
{
    static DETOUR_SYM_INFO symInfo;
    static PDETOUR_SYM_INFO pSymInfo = nullptr;
    static BOOL failed = false;

    if (failed) {
        return nullptr;
    }
    if (pSymInfo != nullptr) {
        return pSymInfo;
    }

    RtlSecureZeroMemory(&symInfo, sizeof(symInfo));
    // Create a real handle to the process.
#if 0
    DuplicateHandle(
        DetoursCurrentProcess(),
        DetoursCurrentProcess(),
        DetoursCurrentProcess(),
        &symInfo.hProcess,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS);
#else
    symInfo.hProcess = DetoursCurrentProcess();
#endif

    symInfo.hDbgHelp = LoadLibraryExW(L"dbghelp.dll", nullptr, 0);
    if (symInfo.hDbgHelp == nullptr) {
      abort:
        failed = true;
        if (symInfo.hDbgHelp != nullptr) {
            FreeLibrary(symInfo.hDbgHelp);
        }
        symInfo.pfImagehlpApiVersionEx = nullptr;
        symInfo.pfSymInitialize = nullptr;
        symInfo.pfSymSetOptions = nullptr;
        symInfo.pfSymGetOptions = nullptr;
        symInfo.pfSymLoadModule64 = nullptr;
        symInfo.pfSymGetModuleInfo64 = nullptr;
        symInfo.pfSymFromName = nullptr;
        return nullptr;
    }

    symInfo.pfImagehlpApiVersionEx
        = (PF_ImagehlpApiVersionEx)GetProcAddress(symInfo.hDbgHelp,
                                                  "ImagehlpApiVersionEx");
    symInfo.pfSymInitialize
        = (PF_SymInitialize)GetProcAddress(symInfo.hDbgHelp, "SymInitialize");
    symInfo.pfSymSetOptions
        = (PF_SymSetOptions)GetProcAddress(symInfo.hDbgHelp, "SymSetOptions");
    symInfo.pfSymGetOptions
        = (PF_SymGetOptions)GetProcAddress(symInfo.hDbgHelp, "SymGetOptions");
    symInfo.pfSymLoadModule64
        = (PF_SymLoadModule64)GetProcAddress(symInfo.hDbgHelp, "SymLoadModule64");
    symInfo.pfSymGetModuleInfo64
        = (PF_SymGetModuleInfo64)GetProcAddress(symInfo.hDbgHelp, "SymGetModuleInfo64");
    symInfo.pfSymFromName
        = (PF_SymFromName)GetProcAddress(symInfo.hDbgHelp, "SymFromName");

    API_VERSION av;
    RtlSecureZeroMemory(&av, sizeof(av));
    av.MajorVersion = API_VERSION_NUMBER;

    if (symInfo.pfImagehlpApiVersionEx == nullptr ||
        symInfo.pfSymInitialize == nullptr ||
        symInfo.pfSymLoadModule64 == nullptr ||
        symInfo.pfSymGetModuleInfo64 == nullptr ||
        symInfo.pfSymFromName == nullptr) {
        goto abort;
    }

    symInfo.pfImagehlpApiVersionEx(&av);
    if (av.MajorVersion < API_VERSION_NUMBER) {
        goto abort;
    }

    if (!symInfo.pfSymInitialize(symInfo.hProcess, nullptr, FALSE)) {
        // We won't retry the initialize if it fails.
        goto abort;
    }

    if (symInfo.pfSymGetOptions != nullptr && symInfo.pfSymSetOptions != nullptr) {
        DWORD dw = symInfo.pfSymGetOptions();

        dw &= ~(SYMOPT_CASE_INSENSITIVE |
                SYMOPT_UNDNAME |
                SYMOPT_DEFERRED_LOADS |
                0);
        dw |= (
#if defined(SYMOPT_EXACT_SYMBOLS)
               SYMOPT_EXACT_SYMBOLS |
#endif
#if defined(SYMOPT_NO_UNQUALIFIED_LOADS)
               SYMOPT_NO_UNQUALIFIED_LOADS |
#endif
               SYMOPT_DEFERRED_LOADS |
#if defined(SYMOPT_FAIL_CRITICAL_ERRORS)
               SYMOPT_FAIL_CRITICAL_ERRORS |
#endif
#if defined(SYMOPT_INCLUDE_32BIT_MODULES)
               SYMOPT_INCLUDE_32BIT_MODULES |
#endif
               0);
        symInfo.pfSymSetOptions(dw);
    }

    pSymInfo = &symInfo;
    return pSymInfo;
}

PVOID DETOURS_API DetourFindFunction(_In_ LPCSTR pszModule,
                                _In_ LPCSTR pszFunction)
{
    /////////////////////////////////////////////// First, try GetProcAddress.
    //
#pragma prefast(suppress:28752, "We don't do the unicode conversion for LoadLibraryExA.")
    HMODULE hModule = LoadLibraryExA(pszModule, nullptr, 0);
    if (hModule == nullptr) {
        return nullptr;
    }

    PBYTE pbCode = (PBYTE)GetProcAddress(hModule, pszFunction);
    if (pbCode) {
        return pbCode;
    }

    ////////////////////////////////////////////////////// Then try ImageHelp.
    //
    DETOUR_TRACE(("DetourFindFunction(%hs, %hs)\n", pszModule, pszFunction));
    PDETOUR_SYM_INFO pSymInfo = DetourLoadImageHlp();
    if (pSymInfo == nullptr) {
        DETOUR_TRACE(("DetourLoadImageHlp failed: %d\n",
                      GetLastError()));
        return nullptr;
    }

    if (pSymInfo->pfSymLoadModule64(pSymInfo->hProcess, nullptr,
                                    (PCHAR)pszModule, nullptr,
                                    (DWORD64)hModule, 0) == 0) {
        if (ERROR_SUCCESS != GetLastError()) {
            DETOUR_TRACE(("SymLoadModule64(%p) failed: %d\n",
                          pSymInfo->hProcess, GetLastError()));
            return nullptr;
        }
    }

    HRESULT hrRet;
    CHAR szFullName[512];
    IMAGEHLP_MODULE64 modinfo;
    RtlSecureZeroMemory(&modinfo, sizeof(modinfo));
    modinfo.SizeOfStruct = sizeof(modinfo);
    if (!pSymInfo->pfSymGetModuleInfo64(pSymInfo->hProcess, (DWORD64)hModule, &modinfo)) {
        DETOUR_TRACE(("SymGetModuleInfo64(%p, %p) failed: %d\n",
                      pSymInfo->hProcess, hModule, GetLastError()));
        return nullptr;
    }

    hrRet = StringCchCopyA(szFullName, sizeof(szFullName)/sizeof(CHAR), modinfo.ModuleName);
    if (FAILED(hrRet)) {
        DETOUR_TRACE(("StringCchCopyA failed: %08x\n", hrRet));
        return nullptr;
    }
    hrRet = StringCchCatA(szFullName, sizeof(szFullName)/sizeof(CHAR), "!");
    if (FAILED(hrRet)) {
        DETOUR_TRACE(("StringCchCatA failed: %08x\n", hrRet));
        return nullptr;
    }
    hrRet = StringCchCatA(szFullName, sizeof(szFullName)/sizeof(CHAR), pszFunction);
    if (FAILED(hrRet)) {
        DETOUR_TRACE(("StringCchCatA failed: %08x\n", hrRet));
        return nullptr;
    }

    struct CFullSymbol : SYMBOL_INFO {
        CHAR szRestOfName[512];
    } symbol;
    RtlSecureZeroMemory(&symbol, sizeof(symbol));
    //symbol.ModBase = (ULONG64)hModule;
    symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
#ifdef DBHLPAPI
    symbol.MaxNameLen = sizeof(symbol.szRestOfName)/sizeof(symbol.szRestOfName[0]);
#else
    symbol.MaxNameLength = sizeof(symbol.szRestOfName)/sizeof(symbol.szRestOfName[0]);
#endif

    if (!pSymInfo->pfSymFromName(pSymInfo->hProcess, szFullName, &symbol)) {
        DETOUR_TRACE(("SymFromName(%hs) failed: %d\n", szFullName, GetLastError()));
        return nullptr;
    }

#if defined(DETOURS_IA64)
    // On the IA64, we get a raw code pointer from the symbol engine
    // and have to convert it to a wrapped [code pointer, global pointer].
    //
    PPLABEL_DESCRIPTOR pldEntry = (PPLABEL_DESCRIPTOR)DetourGetEntryPoint(hModule);
    PPLABEL_DESCRIPTOR pldSymbol = new PLABEL_DESCRIPTOR;

    pldSymbol->EntryPoint = symbol.Address;
    pldSymbol->GlobalPointer = pldEntry->GlobalPointer;
    return (PBYTE)pldSymbol;
#elif defined(DETOURS_ARM)
    // On the ARM, we get a raw code pointer, which we must convert into a
    // valied Thumb2 function pointer.
    return DETOURS_PBYTE_TO_PFUNC(symbol.Address);
#else
    return (PBYTE)symbol.Address;
#endif
}

//////////////////////////////////////////////////// Module Image Functions.
//

HMODULE DETOURS_API DetourEnumerateModules(_In_opt_ HMODULE hModuleLast)
{
    PBYTE pbLast = (PBYTE)hModuleLast + MM_ALLOCATION_GRANULARITY;

    MEMORY_BASIC_INFORMATION mbi;
    RtlSecureZeroMemory(&mbi, sizeof(mbi));

    // Find the next memory region that contains a mapped PE image.
    //
    for (;; pbLast = (PBYTE)mbi.BaseAddress + mbi.RegionSize) {
        if (VirtualQuery(pbLast, &mbi, sizeof(mbi)) <= 0) {
            break;
        }

        // Skip uncommitted regions and guard pages.
        //
        if ((mbi.State != MEM_COMMIT) ||
            ((mbi.Protect & 0xff) == PAGE_NOACCESS) ||
            (mbi.Protect & PAGE_GUARD)) {
            continue;
        }

        __try {
            PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pbLast;
            if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE ||
                (DWORD)pDosHeader->e_lfanew > mbi.RegionSize ||
                (DWORD)pDosHeader->e_lfanew < sizeof(*pDosHeader)) {
                continue;
            }

            PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)pDosHeader +
                                                              pDosHeader->e_lfanew);
            if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
                continue;
            }

            return (HMODULE)pDosHeader;
        }
#pragma prefast(suppress:28940, "A bad pointer means this probably isn't a PE header.")
        __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
                 EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            continue;
        }
    }
    return nullptr;
}

PVOID DETOURS_API DetourGetEntryPoint(_In_opt_ HMODULE hModule)
{
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (hModule == nullptr) {
        pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleW(nullptr);
    }

    __try {
#pragma warning(suppress:6011) // GetModuleHandleW(nullptr) never returns nullptr.
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return nullptr;
        }

        PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)pDosHeader +
                                                          pDosHeader->e_lfanew);
        if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_INVALID_EXE_SIGNATURE);
            return nullptr;
        }
        if (pNtHeader->FileHeader.SizeOfOptionalHeader == 0) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return nullptr;
        }

        PDETOUR_CLR_HEADER pClrHeader = nullptr;
        if (pNtHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            if (((PIMAGE_NT_HEADERS32)pNtHeader)->CLR_DIRECTORY.VirtualAddress != 0 &&
                ((PIMAGE_NT_HEADERS32)pNtHeader)->CLR_DIRECTORY.Size != 0) {
                pClrHeader = (PDETOUR_CLR_HEADER)
                    (((PBYTE)pDosHeader)
                     + ((PIMAGE_NT_HEADERS32)pNtHeader)->CLR_DIRECTORY.VirtualAddress);
            }
        }
        else if (pNtHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            if (((PIMAGE_NT_HEADERS64)pNtHeader)->CLR_DIRECTORY.VirtualAddress != 0 &&
                ((PIMAGE_NT_HEADERS64)pNtHeader)->CLR_DIRECTORY.Size != 0) {
                pClrHeader = (PDETOUR_CLR_HEADER)
                    (((PBYTE)pDosHeader)
                     + ((PIMAGE_NT_HEADERS64)pNtHeader)->CLR_DIRECTORY.VirtualAddress);
            }
        }

        if (pClrHeader != nullptr) {
            // For MSIL assemblies, we want to use the _Cor entry points.

            HMODULE hClr = GetModuleHandleW(L"MSCOREE.DLL");
            if (hClr == nullptr) {
                return nullptr;
            }

            DetoursSetLastError(DETOURS_STATUS_SUCCESS);
            return GetProcAddress(hClr, "_CorExeMain");
        }

        DetoursSetLastError(DETOURS_STATUS_SUCCESS);

        // Pure resource DLLs have neither an entry point nor CLR information
        // so handle them by returning nullptr (LastError is DETOURS_STATUS_SUCCESS)
        if (pNtHeader->OptionalHeader.AddressOfEntryPoint == 0) {
            return nullptr;
        }

        return ((PBYTE)pDosHeader) +
            pNtHeader->OptionalHeader.AddressOfEntryPoint;
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return nullptr;
    }
}

ULONG DETOURS_API DetourGetModuleSize(_In_opt_ HMODULE hModule)
{
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (hModule == nullptr) {
        pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleW(nullptr);
    }

    __try {
#pragma warning(suppress:6011) // GetModuleHandleW(nullptr) never returns nullptr.
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return 0;
        }

        PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)pDosHeader +
                                                          pDosHeader->e_lfanew);
        if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_INVALID_EXE_SIGNATURE);
            return 0;
        }
        if (pNtHeader->FileHeader.SizeOfOptionalHeader == 0) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return 0;
        }
        DetoursSetLastError(DETOURS_STATUS_SUCCESS);

        return (pNtHeader->OptionalHeader.SizeOfImage);
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return 0;
    }
}

HMODULE DETOURS_API DetourGetContainingModule(_In_ PVOID pvAddr)
{
    MEMORY_BASIC_INFORMATION mbi;
    RtlSecureZeroMemory(&mbi, sizeof(mbi));

    __try {
        if (VirtualQuery(pvAddr, &mbi, sizeof(mbi)) <= 0) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return nullptr;
        }

        // Skip uncommitted regions and guard pages.
        //
        if ((mbi.State != MEM_COMMIT) ||
            ((mbi.Protect & 0xff) == PAGE_NOACCESS) ||
            (mbi.Protect & PAGE_GUARD)) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return nullptr;
        }

        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)mbi.AllocationBase;
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return nullptr;
        }

        PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)pDosHeader +
                                                          pDosHeader->e_lfanew);
        if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_INVALID_EXE_SIGNATURE);
            return nullptr;
        }
        if (pNtHeader->FileHeader.SizeOfOptionalHeader == 0) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return nullptr;
        }
        DetoursSetLastError(DETOURS_STATUS_SUCCESS);

        return (HMODULE)pDosHeader;
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        DetoursSetLastError(DETOURS_STATUS_INVALID_EXE_SIGNATURE);
        return nullptr;
    }
}


static inline PBYTE RvaAdjust(_Pre_notnull_ PIMAGE_DOS_HEADER pDosHeader, _In_ DWORD raddr)
{
    if (raddr != 0) {
        return ((PBYTE)pDosHeader) + raddr;
    }
    return nullptr;
}

BOOL DETOURS_API DetourEnumerateExports(_In_ HMODULE hModule,
                                   _In_opt_ PVOID pContext,
                                   _In_ PF_DETOUR_ENUMERATE_EXPORT_CALLBACK pfExport)
{
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (hModule == nullptr) {
        pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleW(nullptr);
    }

    __try {
#pragma warning(suppress:6011) // GetModuleHandleW(nullptr) never returns nullptr.
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return FALSE;
        }

        PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)pDosHeader +
                                                          pDosHeader->e_lfanew);
        if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_INVALID_EXE_SIGNATURE);
            return FALSE;
        }
        if (pNtHeader->FileHeader.SizeOfOptionalHeader == 0) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return FALSE;
        }

        PIMAGE_EXPORT_DIRECTORY pExportDir
            = (PIMAGE_EXPORT_DIRECTORY)
            RvaAdjust(pDosHeader,
                      pNtHeader->OptionalHeader
                      .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

        if (pExportDir == nullptr) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return FALSE;
        }

        PBYTE pExportDirEnd = (PBYTE)pExportDir + pNtHeader->OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        PDWORD pdwFunctions = (PDWORD)RvaAdjust(pDosHeader, pExportDir->AddressOfFunctions);
        PDWORD pdwNames = (PDWORD)RvaAdjust(pDosHeader, pExportDir->AddressOfNames);
        PWORD pwOrdinals = (PWORD)RvaAdjust(pDosHeader, pExportDir->AddressOfNameOrdinals);

        for (DWORD nFunc = 0; nFunc < pExportDir->NumberOfFunctions; nFunc++) {
            PBYTE pbCode = (pdwFunctions != nullptr)
                ? (PBYTE)RvaAdjust(pDosHeader, pdwFunctions[nFunc]) : nullptr;
            PCHAR pszName = nullptr;

            // if the pointer is in the export region, then it is a forwarder.
            if (pbCode > (PBYTE)pExportDir && pbCode < pExportDirEnd) {
                pbCode = nullptr;
            }

            for (DWORD n = 0; n < pExportDir->NumberOfNames; n++) {
                if (pwOrdinals[n] == nFunc) {
                    pszName = (pdwNames != nullptr)
                        ? (PCHAR)RvaAdjust(pDosHeader, pdwNames[n]) : nullptr;
                    break;
                }
            }
            ULONG nOrdinal = pExportDir->Base + nFunc;

            if (!pfExport(pContext, nOrdinal, pszName, pbCode)) {
                break;
            }
        }
        DetoursSetLastError(DETOURS_STATUS_SUCCESS);
        return TRUE;
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return FALSE;
    }
}

BOOL DETOURS_API DetourEnumerateImportsEx(_In_opt_ HMODULE hModule,
                                     _In_opt_ PVOID pContext,
                                     _In_opt_ PF_DETOUR_IMPORT_FILE_CALLBACK pfImportFile,
                                     _In_opt_ PF_DETOUR_IMPORT_FUNC_CALLBACK_EX pfImportFunc)
{
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (hModule == nullptr) {
        pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleW(nullptr);
    }

    __try {
#pragma warning(suppress:6011) // GetModuleHandleW(nullptr) never returns nullptr.
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return FALSE;
        }

        PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)pDosHeader +
                                                          pDosHeader->e_lfanew);
        if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_INVALID_EXE_SIGNATURE);
            return FALSE;
        }
        if (pNtHeader->FileHeader.SizeOfOptionalHeader == 0) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return FALSE;
        }

        PIMAGE_IMPORT_DESCRIPTOR iidp
            = (PIMAGE_IMPORT_DESCRIPTOR)
            RvaAdjust(pDosHeader,
                      pNtHeader->OptionalHeader
                      .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

        if (iidp == nullptr) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return FALSE;
        }

        for (; iidp->OriginalFirstThunk != 0; iidp++) {

            PCSTR pszName = (PCHAR)RvaAdjust(pDosHeader, iidp->Name);
            if (pszName == nullptr) {
                DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
                return FALSE;
            }

            PIMAGE_THUNK_DATA pThunks = (PIMAGE_THUNK_DATA)
                RvaAdjust(pDosHeader, iidp->OriginalFirstThunk);
            PVOID * pAddrs = (PVOID *)
                RvaAdjust(pDosHeader, iidp->FirstThunk);

            HMODULE hFile = DetourGetContainingModule(pAddrs[0]);

            if (pfImportFile != nullptr) {
                if (!pfImportFile(pContext, hFile, pszName)) {
                    break;
                }
            }

            DWORD nNames = 0;
            if (pThunks) {
                for (; pThunks[nNames].u1.Ordinal; nNames++) {
                    DWORD nOrdinal = 0;
                    PCSTR pszFunc = nullptr;

                    if (IMAGE_SNAP_BY_ORDINAL(pThunks[nNames].u1.Ordinal)) {
                        nOrdinal = (DWORD)IMAGE_ORDINAL(pThunks[nNames].u1.Ordinal);
                    }
                    else {
                        pszFunc = (PCSTR)RvaAdjust(pDosHeader,
                                                   (DWORD)pThunks[nNames].u1.AddressOfData + 2);
                    }

                    if (pfImportFunc != nullptr) {
                        if (!pfImportFunc(pContext,
                                          nOrdinal,
                                          pszFunc,
                                          &pAddrs[nNames])) {
                            break;
                        }
                    }
                }
                if (pfImportFunc != nullptr) {
                    pfImportFunc(pContext, 0, nullptr, nullptr);
                }
            }
        }
        if (pfImportFile != nullptr) {
            pfImportFile(pContext, nullptr, nullptr);
        }
        DetoursSetLastError(DETOURS_STATUS_SUCCESS);
        return TRUE;
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return FALSE;
    }
}

// Context for DetourEnumerateImportsThunk, which adapts "regular" callbacks for use with "Ex".
struct _DETOUR_ENUMERATE_IMPORTS_THUNK_CONTEXT
{
    PVOID pContext;
    PF_DETOUR_IMPORT_FUNC_CALLBACK pfImportFunc;
};

// Callback for DetourEnumerateImportsEx that adapts DetourEnumerateImportsEx
// for use with a DetourEnumerateImports callback -- derefence the IAT and pass the value on.

static
BOOL
DETOURS_CALLBACK
DetourEnumerateImportsThunk(_In_ PVOID VoidContext,
                            _In_ DWORD nOrdinal,
                            _In_opt_ PCSTR pszFunc,
                            _In_opt_ PVOID* ppvFunc)
{
    _DETOUR_ENUMERATE_IMPORTS_THUNK_CONTEXT const * const
        pContext = (_DETOUR_ENUMERATE_IMPORTS_THUNK_CONTEXT*)VoidContext;
    return pContext->pfImportFunc(pContext->pContext, nOrdinal, pszFunc, ppvFunc ? *ppvFunc : nullptr);
}

BOOL DETOURS_API DetourEnumerateImports(_In_opt_ HMODULE hModule,
                                   _In_opt_ PVOID pContext,
                                   _In_opt_ PF_DETOUR_IMPORT_FILE_CALLBACK pfImportFile,
                                   _In_opt_ PF_DETOUR_IMPORT_FUNC_CALLBACK pfImportFunc)
{
    _DETOUR_ENUMERATE_IMPORTS_THUNK_CONTEXT const context = { pContext, pfImportFunc };

    return DetourEnumerateImportsEx(hModule,
                                    (PVOID)&context,
                                    pfImportFile,
                                    &DetourEnumerateImportsThunk);
}

static PDETOUR_LOADED_BINARY DETOURS_API GetPayloadSectionFromModule(HMODULE hModule)
{
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (hModule == nullptr) {
        pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandleW(nullptr);
    }

    __try {
#pragma warning(suppress:6011) // GetModuleHandleW(nullptr) never returns nullptr.
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return nullptr;
        }

        PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)pDosHeader +
                                                          pDosHeader->e_lfanew);
        if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
            DetoursSetLastError(DETOURS_STATUS_INVALID_EXE_SIGNATURE);
            return nullptr;
        }
        if (pNtHeader->FileHeader.SizeOfOptionalHeader == 0) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return nullptr;
        }

        PIMAGE_SECTION_HEADER pSectionHeaders
            = (PIMAGE_SECTION_HEADER)((PBYTE)pNtHeader
                                      + sizeof(pNtHeader->Signature)
                                      + sizeof(pNtHeader->FileHeader)
                                      + pNtHeader->FileHeader.SizeOfOptionalHeader);

        for (DWORD n = 0; n < pNtHeader->FileHeader.NumberOfSections; n++) {
            if (strcmp((PCHAR)pSectionHeaders[n].Name, ".detour") == 0) {
                if (pSectionHeaders[n].VirtualAddress == 0 ||
                    pSectionHeaders[n].SizeOfRawData == 0) {

                    break;
                }

                PBYTE pbData = (PBYTE)pDosHeader + pSectionHeaders[n].VirtualAddress;
                DETOUR_SECTION_HEADER *pHeader = (DETOUR_SECTION_HEADER *)pbData;
                if (pHeader->cbHeaderSize < sizeof(DETOUR_SECTION_HEADER) ||
                    pHeader->nSignature != DETOUR_SECTION_HEADER_SIGNATURE) {

                    break;
                }

                if (pHeader->nDataOffset == 0) {
                    pHeader->nDataOffset = pHeader->cbHeaderSize;
                }
                DetoursSetLastError(DETOURS_STATUS_SUCCESS);
                return (PBYTE)pHeader;
            }
        }
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return nullptr;
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return nullptr;
    }
}

DWORD DETOURS_API DetourGetSizeOfPayloads(_In_opt_ HMODULE hModule)
{
    PDETOUR_LOADED_BINARY pBinary = GetPayloadSectionFromModule(hModule);
    if (pBinary == nullptr) {
        // Error set by GetPayloadSectionFromModule.
        return 0;
    }

    __try {
        DETOUR_SECTION_HEADER *pHeader = (DETOUR_SECTION_HEADER *)pBinary;
        if (pHeader->cbHeaderSize < sizeof(DETOUR_SECTION_HEADER) ||
            pHeader->nSignature != DETOUR_SECTION_HEADER_SIGNATURE) {

            DetoursSetLastError(DETOURS_STATUS_INVALID_HANDLE);
            return 0;
        }
        DetoursSetLastError(DETOURS_STATUS_SUCCESS);
        return pHeader->cbDataSize;
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        DetoursSetLastError(DETOURS_STATUS_INVALID_HANDLE);
        return 0;
    }
}

_Writable_bytes_(*pcbData)
_Readable_bytes_(*pcbData)
_Success_(return != nullptr)
PVOID DETOURS_API DetourFindPayload(_In_opt_ HMODULE hModule,
                               _In_ REFGUID rguid,
                               _Out_ DWORD *pcbData)
{
    PBYTE pbData = nullptr;
    if (pcbData) {
        *pcbData = 0;
    }

    PDETOUR_LOADED_BINARY pBinary = GetPayloadSectionFromModule(hModule);
    if (pBinary == nullptr) {
        // Error set by GetPayloadSectionFromModule.
        return nullptr;
    }

    __try {
        DETOUR_SECTION_HEADER *pHeader = (DETOUR_SECTION_HEADER *)pBinary;
        if (pHeader->cbHeaderSize < sizeof(DETOUR_SECTION_HEADER) ||
            pHeader->nSignature != DETOUR_SECTION_HEADER_SIGNATURE) {

            DetoursSetLastError(DETOURS_STATUS_INVALID_EXE_SIGNATURE);
            return nullptr;
        }

        PBYTE pbBeg = ((PBYTE)pHeader) + pHeader->nDataOffset;
        PBYTE pbEnd = ((PBYTE)pHeader) + pHeader->cbDataSize;

        for (pbData = pbBeg; pbData < pbEnd;) {
            DETOUR_SECTION_RECORD *pSection = (DETOUR_SECTION_RECORD *)pbData;

            if (pSection->guid.Data1 == rguid.Data1 &&
                pSection->guid.Data2 == rguid.Data2 &&
                pSection->guid.Data3 == rguid.Data3 &&
                pSection->guid.Data4[0] == rguid.Data4[0] &&
                pSection->guid.Data4[1] == rguid.Data4[1] &&
                pSection->guid.Data4[2] == rguid.Data4[2] &&
                pSection->guid.Data4[3] == rguid.Data4[3] &&
                pSection->guid.Data4[4] == rguid.Data4[4] &&
                pSection->guid.Data4[5] == rguid.Data4[5] &&
                pSection->guid.Data4[6] == rguid.Data4[6] &&
                pSection->guid.Data4[7] == rguid.Data4[7]) {

                if (pcbData) {
                    *pcbData = pSection->cbBytes - sizeof(*pSection);
                    DetoursSetLastError(DETOURS_STATUS_SUCCESS);
                    return (PBYTE)(pSection + 1);
                }
            }

            pbData = (PBYTE)pSection + pSection->cbBytes;
        }
        DetoursSetLastError(DETOURS_STATUS_INVALID_HANDLE);
        return nullptr;
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        DetoursSetLastError(DETOURS_STATUS_INVALID_HANDLE);
        return nullptr;
    }
}

_Writable_bytes_(*pcbData)
_Readable_bytes_(*pcbData)
_Success_(return != nullptr)
PVOID DETOURS_API DetourFindPayloadEx(_In_ REFGUID rguid,
                                 _Out_ DWORD * pcbData)
{
    for (HMODULE hMod = nullptr; (hMod = DetourEnumerateModules(hMod)) != nullptr;) {
        PVOID pvData;

        pvData = DetourFindPayload(hMod, rguid, pcbData);
        if (pvData != nullptr) {
            return pvData;
        }
    }
    DetoursSetLastError(DETOURS_STATUS_MOD_NOT_FOUND);
    return nullptr;
}

BOOL DETOURS_API DetourRestoreAfterWithEx(_In_reads_bytes_(cbData) PVOID pvData,
                                     _In_ DWORD cbData)
{
    PDETOUR_EXE_RESTORE pder = (PDETOUR_EXE_RESTORE)pvData;

    if (pder->cb != sizeof(*pder) || pder->cb > cbData) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return FALSE;
    }

    DWORD dwPermIdh = ~0u;
    DWORD dwPermInh = ~0u;
    DWORD dwPermClr = ~0u;
    DWORD dwIgnore;
    BOOL fSucceeded = FALSE;
    BOOL fUpdated32To64 = FALSE;

    if (pder->pclr != nullptr && pder->clr.Flags != ((PDETOUR_CLR_HEADER)pder->pclr)->Flags) {
        // If we had to promote the 32/64-bit agnostic IL to 64-bit, we can't restore
        // that.
        fUpdated32To64 = TRUE;
    }

    if (DetourVirtualProtectSameExecute(pder->pidh, pder->cbidh,
                                        PAGE_EXECUTE_READWRITE, &dwPermIdh)) {
        if (DetourVirtualProtectSameExecute(pder->pinh, pder->cbinh,
                                            PAGE_EXECUTE_READWRITE, &dwPermInh)) {

            memcpy(pder->pidh, &pder->idh, pder->cbidh);
            memcpy(pder->pinh, &pder->inh, pder->cbinh);

            if (pder->pclr != nullptr && !fUpdated32To64) {
                if (DetourVirtualProtectSameExecute(pder->pclr, pder->cbclr,
                                                    PAGE_EXECUTE_READWRITE, &dwPermClr)) {
                    memcpy(pder->pclr, &pder->clr, pder->cbclr);
                    VirtualProtect(pder->pclr, pder->cbclr, dwPermClr, &dwIgnore);
                    fSucceeded = TRUE;
                }
            }
            else {
                fSucceeded = TRUE;
            }
            VirtualProtect(pder->pinh, pder->cbinh, dwPermInh, &dwIgnore);
        }
        VirtualProtect(pder->pidh, pder->cbidh, dwPermIdh, &dwIgnore);
    }
    return fSucceeded;
}

BOOL DETOURS_API DetourRestoreAfterWith()
{
    PVOID pvData;
    DWORD cbData;

    pvData = DetourFindPayloadEx(DETOUR_EXE_RESTORE_GUID, &cbData);

    if (pvData != nullptr && cbData != 0) {
        return DetourRestoreAfterWithEx(pvData, cbData);
    }
    DetoursSetLastError(DETOURS_STATUS_MOD_NOT_FOUND);
    return FALSE;
}

#endif // DetoursUserMode

//  End of File
//////////////////////////////////////////////////////////////////////////
