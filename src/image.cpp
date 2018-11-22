//////////////////////////////////////////////////////////////////////////////
//
//  Image manipulation functions (image.cpp of detours.lib)
//
//  Microsoft Research Detours Package, Version 4.0.1
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  Used for for payloads, byways, and imports.
//

#include "internal.h"


#ifdef DetoursUserMode

namespace Detour
{
//////////////////////////////////////////////////////////////////////////////
//
#ifndef _STRSAFE_H_INCLUDED_
_Must_inspect_result_
static inline HRESULT StringCchLengthA(
    _In_reads_or_z_(cchMax) LPCSTR psz,
    _In_
    _In_range_(1, STRSAFE_MAX_CCH) size_t cchMax,
    _Out_opt_
    _Deref_out_range_(<, cchMax)
    _Deref_out_range_(<=, _String_length_(psz))
    _Out_ size_t* pcch)
{
    HRESULT hr = S_OK;
    size_t cchMaxPrev = cchMax;

    if (cchMax > 2147483647) {
        return ERROR_INVALID_PARAMETER;
    }

    while (cchMax && (*psz != '\0')) {
        psz++;
        cchMax--;
    }

    if (cchMax == 0) {
        // the string is longer than cchMax
        hr = ERROR_INVALID_PARAMETER;
    }

    if (SUCCEEDED(hr) && pcch) {
        *pcch = cchMaxPrev - cchMax;
    }

    return hr;
}

_Must_inspect_result_
static inline HRESULT StringCchCopyA(
    _Out_writes_(cchDest) _Always_(_Post_z_) LPSTR pszDest,
    _In_ size_t cchDest,
    _In_ LPCSTR pszSrc)
{
    HRESULT hr = S_OK;

    if (cchDest == 0) {
        // can not null terminate a zero-byte dest buffer
        hr = ERROR_INVALID_PARAMETER;
    }
    else {
        while (cchDest && (*pszSrc != '\0')) {
            *pszDest++ = *pszSrc++;
            cchDest--;
        }

        if (cchDest == 0) {
            // we are going to truncate pszDest
            pszDest--;
            hr = ERROR_INVALID_PARAMETER;
        }

        *pszDest= '\0';
    }

    return hr;
}

_Must_inspect_result_
static inline HRESULT StringCchCatA(
    _Out_writes_(cchDest) _Always_(_Post_z_) LPSTR pszDest,
    _In_ size_t cchDest,
    _In_ LPCSTR pszSrc)
{
    HRESULT hr;
    size_t cchDestCurrent;

    if (cchDest > 2147483647){
        return ERROR_INVALID_PARAMETER;
    }

    hr = StringCchLengthA(pszDest, cchDest, &cchDestCurrent);

    if (SUCCEEDED(hr) && cchDestCurrent < cchDest) {
        hr = StringCchCopyA(pszDest + cchDestCurrent,
                            cchDest - cchDestCurrent,
                            pszSrc);
    }

    return hr;
}

#endif

///////////////////////////////////////////////////////////////////////////////
//

class CImageData
{
    friend class CImage;

public:
    CImageData(PBYTE pbData, DWORD cbData);
    ~CImageData();

    PBYTE                   Enumerate(GUID *pGuid, DWORD *pcbData, DWORD *pnIterator);
    PBYTE                   Find(REFGUID rguid, DWORD *pcbData);
    PBYTE                   Set(REFGUID rguid, PBYTE pbData, DWORD cbData);

    BOOL                    Delete(REFGUID rguid);
    BOOL                    Purge();

    BOOL                    IsEmpty()           { return m_cbData == 0; }
    BOOL                    IsValid();

protected:
    BOOL                    SizeTo(DWORD cbData);

protected:
    _Field_size_(m_cbAlloc)
    PBYTE                   m_pbData    = nullptr;
    DWORD                   m_cbData    = 0;
    DWORD                   m_cbAlloc   = 0;
};

class CImageImportFile
{
    friend class CImage;
    friend class CImageImportName;

public:
    CImageImportFile();
    ~CImageImportFile();

public:
    CImageImportFile *      m_pNextFile = nullptr;
    BOOL                    m_fByway    = FALSE;

    _Field_size_(m_nImportNames)
    CImageImportName *      m_pImportNames = nullptr;
    DWORD                   m_nImportNames = 0;

    DWORD                   m_rvaOriginalFirstThunk = 0;
    DWORD                   m_rvaFirstThunk         = 0;

    DWORD                   m_nForwarderChain   = 0;
    LPCSTR                  m_pszOrig           = nullptr;
    LPCSTR                  m_pszName           = nullptr;
};

class CImageImportName
{
    friend class CImage;
    friend class CImageImportFile;

public:
    CImageImportName();
    ~CImageImportName();

public:
    WORD        m_nHint     = 0;
    ULONG       m_nOrig     = 0;
    ULONG       m_nOrdinal  = 0;
    LPCSTR      m_pszOrig   = nullptr;
    LPCSTR      m_pszName   = nullptr;
};

class CImage
{
    friend class CImageThunks;
    friend class CImageChars;
    friend class CImageImportFile;
    friend class CImageImportName;

public:
    CImage();
    ~CImage();

    static CImage *         IsValid(PDETOUR_BINARY pBinary);

public:                                                 // File Functions
    BOOL                    Read(HANDLE hFile);
    BOOL                    Write(HANDLE hFile);
    BOOL                    Close();

public:                                                 // Manipulation Functions
    PBYTE                   DataEnum(GUID *pGuid, DWORD *pcbData, DWORD *pnIterator);
    PBYTE                   DataFind(REFGUID rguid, DWORD *pcbData);
    PBYTE                   DataSet(REFGUID rguid, PBYTE pbData, DWORD cbData);
    BOOL                    DataDelete(REFGUID rguid);
    BOOL                    DataPurge();

    BOOL                    EditImports(PVOID pContext,
                                        PF_DETOUR_BINARY_BYWAY_CALLBACK pfBywayCallback,
                                        PF_DETOUR_BINARY_FILE_CALLBACK pfFileCallback,
                                        PF_DETOUR_BINARY_SYMBOL_CALLBACK pfSymbolCallback,
                                        PF_DETOUR_BINARY_COMMIT_CALLBACK pfCommitCallback);

protected:
    BOOL                    WriteFile(HANDLE hFile,
                                      LPCVOID lpBuffer,
                                      DWORD nNumberOfBytesToWrite,
                                      LPDWORD lpNumberOfBytesWritten);
    BOOL                    CopyFileData(HANDLE hFile, DWORD nOldPos, DWORD cbData);
    BOOL                    ZeroFileData(HANDLE hFile, DWORD cbData);
    BOOL                    AlignFileData(HANDLE hFile);

    BOOL                    SizeOutputBuffer(DWORD cbData);
    PBYTE                   AllocateOutput(DWORD cbData, DWORD *pnVirtAddr);

    PVOID                   RvaToVa(ULONG_PTR nRva);
    DWORD                   RvaToFileOffset(DWORD nRva);

    DWORD                   FileAlign(DWORD nAddr);
    DWORD                   SectionAlign(DWORD nAddr);

    BOOL                    CheckImportsNeeded(DWORD *pnTables,
                                               DWORD *pnThunks,
                                               DWORD *pnChars);

    CImageImportFile *      NewByway(_In_ LPCSTR pszName);

private:
    DWORD                   m_dwValidSignature  = 0;
    CImageData *            m_pImageData        = nullptr;  // Read & Write

    HANDLE                  m_hMap              = nullptr;  // Read & Write
    PBYTE                   m_pMap              = nullptr;  // Read & Write

    DWORD                   m_nNextFileAddr     = 0;        // Write
    DWORD                   m_nNextVirtAddr     = 0;        // Write

    IMAGE_DOS_HEADER        m_DosHeader         = { 0 };    // Read & Write
    IMAGE_NT_HEADERS        m_NtHeader          = { 0 };    // Read & Write
    IMAGE_SECTION_HEADER    m_SectionHeaders[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = { 0 };

    DWORD                   m_nPrePE    = 0;
    DWORD                   m_cbPrePE   = 0;
    DWORD                   m_cbPostPE  = 0;

    DWORD                   m_nPeOffset = 0;
    DWORD                   m_nSectionsOffset   = 0;
    DWORD                   m_nExtraOffset      = 0;
    DWORD                   m_nFileSize         = 0;

    DWORD                   m_nOutputVirtAddr   = 0;
    DWORD                   m_nOutputVirtSize   = 0;
    DWORD                   m_nOutputFileAddr   = 0;

    _Field_size_(m_cbOutputBuffer)
    PBYTE                   m_pbOutputBuffer    = nullptr;
    DWORD                   m_cbOutputBuffer    = 0;

    CImageImportFile *      m_pImportFiles      = nullptr;
    DWORD                   m_nImportFiles      = 0;

    BOOL                    m_fHadDetourSection = FALSE;

private:
    enum {
        DETOUR_IMAGE_VALID_SIGNATURE = 0xfedcba01,      // "Dtr\0"
    };
};

//////////////////////////////////////////////////////////////////////////////
//
static BYTE s_rbDosCode[0x10] = {
    0x0E,0x1F,0xBA,0x0E,0x00,0xB4,0x09,0xCD,
    0x21,0xB8,0x01,0x4C,0xCD,0x21,'*','*'
};

static inline DWORD Max(DWORD a, DWORD b)
{
    return a > b ? a : b;
}

static inline DWORD Align(DWORD a, DWORD size)
{
    size--;
    return (a + size) & ~size;
}

static inline DWORD QuadAlign(DWORD a)
{
    return Align(a, 8);
}

static LPCSTR DuplicateString(_In_ LPCSTR pszIn)
{
    if (pszIn == nullptr) {
        return nullptr;
    }

    size_t cch;
    HRESULT hr = StringCchLengthA(pszIn, 8192, &cch);
    if (FAILED(hr)) {
        DetoursSetLastError(DETOURS_STATUS_INVALID_PARAMETER);
        return nullptr;
    }

    PCHAR pszOut = new NOTHROW CHAR [cch + 1];
    if (pszOut == nullptr) {
        DetoursSetLastError(DETOURS_STATUS_OUTOFMEMORY);
        return nullptr;
    }

    hr = StringCchCopyA(pszOut, cch + 1, pszIn);
    if (FAILED(hr)) {
        delete[] pszOut;
        return nullptr;
    }

    return pszOut;
}

static VOID ReleaseString(_In_opt_ LPCSTR psz)
{
    if (psz != nullptr) {
        delete[] psz;
    }
}

//////////////////////////////////////////////////////////////////////////////
//
CImageImportFile::CImageImportFile()
{
    m_pNextFile = nullptr;
    m_fByway = FALSE;

    m_pImportNames = nullptr;
    m_nImportNames = 0;

    m_rvaOriginalFirstThunk = 0;
    m_rvaFirstThunk = 0;

    m_nForwarderChain = (UINT)0;
    m_pszName = nullptr;
    m_pszOrig = nullptr;
}

CImageImportFile::~CImageImportFile()
{
    if (m_pNextFile) {
        delete m_pNextFile;
        m_pNextFile = nullptr;
    }
    if (m_pImportNames) {
        delete[] m_pImportNames;
        m_pImportNames = nullptr;
        m_nImportNames = 0;
    }
    if (m_pszName) {
        delete[] m_pszName;
        m_pszName = nullptr;
    }
    if (m_pszOrig) {
        delete[] m_pszOrig;
        m_pszOrig = nullptr;
    }
}

CImageImportName::CImageImportName()
{
    m_nOrig = 0;
    m_nOrdinal = 0;
    m_nHint = 0;
    m_pszName = nullptr;
    m_pszOrig = nullptr;
}

CImageImportName::~CImageImportName()
{
    if (m_pszName) {
        delete[] m_pszName;
        m_pszName = nullptr;
    }
    if (m_pszOrig) {
        delete[] m_pszOrig;
        m_pszOrig = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////////
//
CImageData::CImageData(PBYTE pbData, DWORD cbData)
{
    m_pbData = pbData;
    m_cbData = cbData;
    m_cbAlloc = 0;
}

CImageData::~CImageData()
{
    IsValid();

    if (m_cbAlloc == 0) {
        m_pbData = nullptr;
    }
    if (m_pbData) {
        delete[] m_pbData;
        m_pbData = nullptr;
    }
    m_cbData = 0;
    m_cbAlloc = 0;
}

BOOL CImageData::SizeTo(DWORD cbData)
{
    IsValid();

    if (cbData <= m_cbAlloc) {
        return TRUE;
    }

    PBYTE pbNew = new NOTHROW BYTE [cbData];
    if (pbNew == nullptr) {
        DetoursSetLastError(DETOURS_STATUS_OUTOFMEMORY);
        return FALSE;
    }

    if (m_pbData) {
        memcpy(pbNew, m_pbData, m_cbData);
        if (m_cbAlloc > 0) {
            delete[] m_pbData;
        }
        m_pbData = nullptr;
    }
    m_pbData = pbNew;
    m_cbAlloc = cbData;

    IsValid();

    return TRUE;
}

BOOL CImageData::Purge()
{
    m_cbData = 0;

    IsValid();

    return TRUE;
}

BOOL CImageData::IsValid()
{
    if (m_pbData == nullptr) {
        return TRUE;
    }

    PBYTE pbBeg = m_pbData;
    PBYTE pbEnd = m_pbData + m_cbData;

    for (PBYTE pbIter = pbBeg; pbIter < pbEnd;) {
        PDETOUR_SECTION_RECORD pRecord = (PDETOUR_SECTION_RECORD)pbIter;

        if (pRecord->cbBytes < sizeof(DETOUR_SECTION_RECORD)) {
            return FALSE;
        }
        if (pRecord->nReserved != 0) {
            return FALSE;
        }

        pbIter += pRecord->cbBytes;
    }
    return TRUE;
}

PBYTE CImageData::Enumerate(GUID *pGuid, DWORD *pcbData, DWORD *pnIterator)
{
    IsValid();

    if (pnIterator == nullptr ||
        m_cbData < *pnIterator + sizeof(DETOUR_SECTION_RECORD)) {

        if (pcbData) {
            *pcbData = 0;
        }
        if (pGuid) {
            RtlSecureZeroMemory(pGuid, sizeof(*pGuid));
        }
        return nullptr;
    }

    PDETOUR_SECTION_RECORD pRecord = (PDETOUR_SECTION_RECORD)(m_pbData + *pnIterator);

    if (pGuid) {
        *pGuid = pRecord->guid;
    }
    if (pcbData) {
        *pcbData = pRecord->cbBytes - sizeof(DETOUR_SECTION_RECORD);
    }
    *pnIterator = (LONG)(((PBYTE)pRecord - m_pbData) + pRecord->cbBytes);

    return (PBYTE)(pRecord + 1);
}

PBYTE CImageData::Find(REFGUID rguid, DWORD *pcbData)
{
    IsValid();

    DWORD cbBytes = sizeof(DETOUR_SECTION_RECORD);
    for (DWORD nOffset = 0; nOffset < m_cbData; nOffset += cbBytes) {
        PDETOUR_SECTION_RECORD pRecord = (PDETOUR_SECTION_RECORD)(m_pbData + nOffset);

        cbBytes = pRecord->cbBytes;
        if (cbBytes > m_cbData) {
            break;
        }
        if (cbBytes < sizeof(DETOUR_SECTION_RECORD)) {
            continue;
        }

        if (pRecord->guid.Data1 == rguid.Data1 &&
            pRecord->guid.Data2 == rguid.Data2 &&
            pRecord->guid.Data3 == rguid.Data3 &&
            pRecord->guid.Data4[0] == rguid.Data4[0] &&
            pRecord->guid.Data4[1] == rguid.Data4[1] &&
            pRecord->guid.Data4[2] == rguid.Data4[2] &&
            pRecord->guid.Data4[3] == rguid.Data4[3] &&
            pRecord->guid.Data4[4] == rguid.Data4[4] &&
            pRecord->guid.Data4[5] == rguid.Data4[5] &&
            pRecord->guid.Data4[6] == rguid.Data4[6] &&
            pRecord->guid.Data4[7] == rguid.Data4[7]) {

            *pcbData = cbBytes - sizeof(DETOUR_SECTION_RECORD);
            return (PBYTE)(pRecord + 1);
        }
    }

    if (pcbData) {
        *pcbData = 0;
    }
    return nullptr;
}

BOOL CImageData::Delete(REFGUID rguid)
{
    IsValid();

    PBYTE pbFound = nullptr;
    DWORD cbFound = 0;

    pbFound = Find(rguid, &cbFound);
    if (pbFound == nullptr) {
        DetoursSetLastError(DETOURS_STATUS_MOD_NOT_FOUND);
        return FALSE;
    }

    pbFound -= sizeof(DETOUR_SECTION_RECORD);
    cbFound += sizeof(DETOUR_SECTION_RECORD);

    PBYTE pbRestData = pbFound + cbFound;
    DWORD cbRestData = m_cbData - (LONG)(pbRestData - m_pbData);

    if (cbRestData) {
        memmove(pbFound, pbRestData, cbRestData);
    }
    m_cbData -= cbFound;

    IsValid();
    return TRUE;
}

PBYTE CImageData::Set(REFGUID rguid, PBYTE pbData, DWORD cbData)
{
    IsValid();
    Delete(rguid);

    DWORD cbAlloc = QuadAlign(cbData);

    if (!SizeTo(m_cbData + cbAlloc + sizeof(DETOUR_SECTION_RECORD))) {
        return nullptr;
    }

    PDETOUR_SECTION_RECORD pRecord = (PDETOUR_SECTION_RECORD)(m_pbData + m_cbData);
    pRecord->cbBytes = cbAlloc + sizeof(DETOUR_SECTION_RECORD);
    pRecord->nReserved = 0;
    pRecord->guid = rguid;

    PBYTE pbDest = (PBYTE)(pRecord + 1);
    if (pbData) {
        memcpy(pbDest, pbData, cbData);
        if (cbData < cbAlloc) {
            RtlSecureZeroMemory(pbDest + cbData, cbAlloc - cbData);
        }
    }
    else {
        if (cbAlloc > 0) {
            RtlSecureZeroMemory(pbDest, cbAlloc);
        }
    }

    m_cbData += cbAlloc + sizeof(DETOUR_SECTION_RECORD);

    IsValid();
    return pbDest;
}

//////////////////////////////////////////////////////////////////////////////
//
class CImageThunks
{
private:
    CImage *            m_pImage        = nullptr;
    PIMAGE_THUNK_DATA   m_pThunks       = nullptr;
    DWORD               m_nThunks       = 0;
    DWORD               m_nThunksMax    = 0;
    DWORD               m_nThunkVirtAddr= 0;

public:
    CImageThunks(CImage *pImage, DWORD nThunksMax, DWORD *pnAddr)
    {
        m_pImage = pImage;
        m_nThunks = 0;
        m_nThunksMax = nThunksMax;
        m_pThunks = (PIMAGE_THUNK_DATA)
            m_pImage->AllocateOutput(sizeof(IMAGE_THUNK_DATA) * nThunksMax,
                                     &m_nThunkVirtAddr);
        *pnAddr = m_nThunkVirtAddr;
    }

    PIMAGE_THUNK_DATA Current(DWORD *pnVirtAddr)
    {
        if (m_nThunksMax > 1) {
            *pnVirtAddr = m_nThunkVirtAddr;
            return m_pThunks;
        }
        *pnVirtAddr = 0;
        return nullptr;
    }

    PIMAGE_THUNK_DATA Allocate(ULONG_PTR nData, DWORD *pnVirtAddr)
    {
        if (m_nThunks < m_nThunksMax) {
            *pnVirtAddr = m_nThunkVirtAddr;

            m_nThunks++;
            m_nThunkVirtAddr += sizeof(IMAGE_THUNK_DATA);
            m_pThunks->u1.Ordinal = nData;
            return m_pThunks++;
        }
        *pnVirtAddr = 0;
        return nullptr;
    }

    DWORD   Size()
    {
        return m_nThunksMax * sizeof(IMAGE_THUNK_DATA);
    }
};

//////////////////////////////////////////////////////////////////////////////
//
class CImageChars
{
private:
    CImage *        m_pImage    = nullptr;
    PCHAR           m_pChars    = nullptr;
    DWORD           m_nChars    = 0;
    DWORD           m_nCharsMax = 0;
    DWORD           m_nCharVirtAddr = 0;

public:
    CImageChars(CImage *pImage, _In_ DWORD nCharsMax, _Out_ DWORD *pnAddr)
    {
        m_pImage = pImage;
        m_nChars = 0;
        m_nCharsMax = nCharsMax;
        m_pChars = (PCHAR)m_pImage->AllocateOutput(nCharsMax, &m_nCharVirtAddr);
        *pnAddr = m_nCharVirtAddr;
    }

    LPCSTR Allocate(_In_ LPCSTR pszString, _Out_ DWORD *pnVirtAddr)
    {
        DWORD nLen = (DWORD)strlen(pszString) + 1;
        nLen += (nLen & 1);

        if (m_nChars + nLen > m_nCharsMax) {
            *pnVirtAddr = 0;
            return nullptr;
        }

        *pnVirtAddr = m_nCharVirtAddr;
        HRESULT hrRet = StringCchCopyA(m_pChars, m_nCharsMax, pszString);

        if (FAILED(hrRet)) {
            return nullptr;
        }

        pszString = m_pChars;

        m_pChars += nLen;
        m_nChars += nLen;
        m_nCharVirtAddr += nLen;

        return pszString;
    }

    LPCSTR Allocate(_In_ LPCSTR pszString, _In_ DWORD nHint, _Out_ DWORD *pnVirtAddr)
    {
        DWORD nLen = (DWORD)strlen(pszString) + 1 + sizeof(USHORT);
        nLen += (nLen & 1);

        if (m_nChars + nLen > m_nCharsMax) {
            *pnVirtAddr = 0;
            return nullptr;
        }

        *pnVirtAddr = m_nCharVirtAddr;
        *(USHORT *)m_pChars = (USHORT)nHint;

        HRESULT hrRet = StringCchCopyA(m_pChars + sizeof(USHORT), m_nCharsMax, pszString);
        if (FAILED(hrRet)) {
            return nullptr;
        }

        pszString = m_pChars + sizeof(USHORT);

        m_pChars += nLen;
        m_nChars += nLen;
        m_nCharVirtAddr += nLen;

        return pszString;
    }

    DWORD Size()
    {
        return m_nChars;
    }
};

//////////////////////////////////////////////////////////////////////////////
//
CImage * CImage::IsValid(PDETOUR_BINARY pBinary)
{
    if (pBinary) {
        CImage *pImage = (CImage *)pBinary;

        if (pImage->m_dwValidSignature == DETOUR_IMAGE_VALID_SIGNATURE) {
            return pImage;
        }
    }
    DetoursSetLastError(DETOURS_STATUS_INVALID_HANDLE);
    return nullptr;
}

CImage::CImage()
{
    m_dwValidSignature = (DWORD)DETOUR_IMAGE_VALID_SIGNATURE;

    m_hMap = nullptr;
    m_pMap = nullptr;

    m_nPeOffset = 0;
    m_nSectionsOffset = 0;

    m_pbOutputBuffer = nullptr;
    m_cbOutputBuffer = 0;

    m_pImageData = nullptr;

    m_pImportFiles = nullptr;
    m_nImportFiles = 0;

    m_fHadDetourSection = FALSE;
}

CImage::~CImage()
{
    Close();
    m_dwValidSignature = 0;
}

BOOL CImage::Close()
{
    if (m_pImportFiles) {
        delete m_pImportFiles;
        m_pImportFiles = nullptr;
        m_nImportFiles = 0;
    }

    if (m_pImageData) {
        delete m_pImageData;
        m_pImageData = nullptr;
    }

    if (m_pMap != nullptr) {
        UnmapViewOfFile(m_pMap);
        m_pMap = nullptr;
    }

    if (m_hMap) {
        CloseHandle(m_hMap);
        m_hMap = nullptr;
    }

    if (m_pbOutputBuffer) {
        delete[] m_pbOutputBuffer;
        m_pbOutputBuffer = nullptr;
        m_cbOutputBuffer = 0;
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
//
PBYTE CImage::DataEnum(GUID *pGuid, DWORD *pcbData, DWORD *pnIterator)
{
    if (m_pImageData == nullptr) {
        return nullptr;
    }
    return m_pImageData->Enumerate(pGuid, pcbData, pnIterator);
}

PBYTE CImage::DataFind(REFGUID rguid, DWORD *pcbData)
{
    if (m_pImageData == nullptr) {
        return nullptr;
    }
    return m_pImageData->Find(rguid, pcbData);
}

PBYTE CImage::DataSet(REFGUID rguid, PBYTE pbData, DWORD cbData)
{
    if (m_pImageData == nullptr) {
        return nullptr;
    }
    return m_pImageData->Set(rguid, pbData, cbData);
}

BOOL CImage::DataDelete(REFGUID rguid)
{
    if (m_pImageData == nullptr) {
        return FALSE;
    }
    return m_pImageData->Delete(rguid);
}

BOOL CImage::DataPurge()
{
    if (m_pImageData == nullptr) {
        return TRUE;
    }
    return m_pImageData->Purge();
}

//////////////////////////////////////////////////////////////////////////////
//
BOOL CImage::SizeOutputBuffer(DWORD cbData)
{
    if (m_cbOutputBuffer < cbData) {
        if (cbData < 1024) {//65536
            cbData = 1024;
        }
        cbData = FileAlign(cbData);

        PBYTE pOutput = new NOTHROW BYTE [cbData];
        if (pOutput == nullptr) {
            DetoursSetLastError(DETOURS_STATUS_OUTOFMEMORY);
            return FALSE;
        }

        if (m_pbOutputBuffer) {
            memcpy(pOutput, m_pbOutputBuffer, m_cbOutputBuffer);

            delete[] m_pbOutputBuffer;
            m_pbOutputBuffer = nullptr;
        }

        RtlSecureZeroMemory(pOutput + m_cbOutputBuffer, cbData - m_cbOutputBuffer),

        m_pbOutputBuffer = pOutput;
        m_cbOutputBuffer = cbData;
    }
    return TRUE;
}

PBYTE CImage::AllocateOutput(DWORD cbData, DWORD *pnVirtAddr)
{
    cbData = QuadAlign(cbData);

    PBYTE pbData = m_pbOutputBuffer + m_nOutputVirtSize;

    *pnVirtAddr = m_nOutputVirtAddr + m_nOutputVirtSize;
    m_nOutputVirtSize += cbData;

    if (m_nOutputVirtSize > m_cbOutputBuffer) {
        DetoursSetLastError(DETOURS_STATUS_OUTOFMEMORY);
        return nullptr;
    }

    RtlSecureZeroMemory(pbData, cbData);

    return pbData;
}

//////////////////////////////////////////////////////////////////////////////
//
DWORD CImage::FileAlign(DWORD nAddr)
{
    return Align(nAddr, m_NtHeader.OptionalHeader.FileAlignment);
}

DWORD CImage::SectionAlign(DWORD nAddr)
{
    return Align(nAddr, m_NtHeader.OptionalHeader.SectionAlignment);
}

//////////////////////////////////////////////////////////////////////////////
//
PVOID CImage::RvaToVa(ULONG_PTR nRva)
{
    if (nRva == 0) {
        return nullptr;
    }

    for (DWORD n = 0; n < m_NtHeader.FileHeader.NumberOfSections; n++) {
        DWORD vaStart = m_SectionHeaders[n].VirtualAddress;
        DWORD vaEnd = vaStart + m_SectionHeaders[n].SizeOfRawData;

        if (nRva >= vaStart && nRva < vaEnd) {
            return (PBYTE)m_pMap
                + m_SectionHeaders[n].PointerToRawData
                + nRva - m_SectionHeaders[n].VirtualAddress;
        }
    }
    return nullptr;
}

DWORD CImage::RvaToFileOffset(DWORD nRva)
{
    DWORD n;
    for (n = 0; n < m_NtHeader.FileHeader.NumberOfSections; n++) {
        DWORD vaStart = m_SectionHeaders[n].VirtualAddress;
        DWORD vaEnd = vaStart + m_SectionHeaders[n].SizeOfRawData;

        if (nRva >= vaStart && nRva < vaEnd) {
            return m_SectionHeaders[n].PointerToRawData
                + nRva - m_SectionHeaders[n].VirtualAddress;
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
BOOL CImage::WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                       LPDWORD lpNumberOfBytesWritten)
{
    return ::WriteFile(hFile,
                       lpBuffer,
                       nNumberOfBytesToWrite,
                       lpNumberOfBytesWritten,
                       nullptr);
}


BOOL CImage::CopyFileData(HANDLE hFile, DWORD nOldPos, DWORD cbData)
{
    DWORD cbDone = 0;
    return WriteFile(hFile, m_pMap + nOldPos, cbData, &cbDone);
}

BOOL CImage::ZeroFileData(HANDLE hFile, DWORD cbData)
{
    if (!SizeOutputBuffer(4096)) {
        return FALSE;
    }

    RtlSecureZeroMemory(m_pbOutputBuffer, 4096);

    for (DWORD cbLeft = cbData; cbLeft > 0;) {
        DWORD cbStep = cbLeft > sizeof(m_pbOutputBuffer)
            ? sizeof(m_pbOutputBuffer) : cbLeft;
        DWORD cbDone = 0;

        if (!WriteFile(hFile, m_pbOutputBuffer, cbStep, &cbDone)) {
            return FALSE;
        }
        if (cbDone == 0) {
            break;
        }

        cbLeft -= cbDone;
    }
    return TRUE;
}

BOOL CImage::AlignFileData(HANDLE hFile)
{
    DWORD nLastFileAddr = m_nNextFileAddr;

    m_nNextFileAddr = FileAlign(m_nNextFileAddr);
    m_nNextVirtAddr = SectionAlign(m_nNextVirtAddr);

    if (hFile != INVALID_HANDLE_VALUE) {
        if (m_nNextFileAddr > nLastFileAddr) {
            if (SetFilePointer(hFile, nLastFileAddr, nullptr, FILE_BEGIN) == ~0u) {
                return FALSE;
            }
            return ZeroFileData(hFile, m_nNextFileAddr - nLastFileAddr);
        }
    }
    return TRUE;
}

BOOL CImage::Read(HANDLE hFile)
{
    DWORD n;
    PBYTE pbData = nullptr;
    DWORD cbData = 0;

    if (hFile == INVALID_HANDLE_VALUE) {
        DetoursSetLastError(DETOURS_STATUS_INVALID_HANDLE);
        return FALSE;
    }

    ///////////////////////////////////////////////////////// Create mapping.
    //
    m_nFileSize = GetFileSize(hFile, nullptr);
    if (m_nFileSize == (DWORD)-1) {
        return FALSE;
    }

    m_hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (m_hMap == nullptr) {
        return FALSE;
    }

    m_pMap = (PBYTE)MapViewOfFileEx(m_hMap, FILE_MAP_READ, 0, 0, 0, nullptr);
    if (m_pMap == nullptr) {
        return FALSE;
    }

    ////////////////////////////////////////////////////// Process DOS Header.
    //
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)m_pMap;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return FALSE;
    }
    m_nPeOffset = pDosHeader->e_lfanew;
    m_nPrePE = 0;
    m_cbPrePE = QuadAlign(pDosHeader->e_lfanew);

    if (m_nPeOffset > m_nFileSize ||
        m_nPeOffset + sizeof(m_NtHeader) > m_nFileSize) {

        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return FALSE;
    }

    memcpy(&m_DosHeader, m_pMap + m_nPrePE, sizeof(m_DosHeader));

    /////////////////////////////////////////////////////// Process PE Header.
    //
    memcpy(&m_NtHeader, m_pMap + m_nPeOffset, sizeof(m_NtHeader));
    if (m_NtHeader.Signature != IMAGE_NT_SIGNATURE) {
        DetoursSetLastError(DETOURS_STATUS_INVALID_EXE_SIGNATURE);
        return FALSE;
    }
    if (m_NtHeader.FileHeader.SizeOfOptionalHeader == 0) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return FALSE;
    }
    m_nSectionsOffset = m_nPeOffset
        + sizeof(m_NtHeader.Signature)
        + sizeof(m_NtHeader.FileHeader)
        + m_NtHeader.FileHeader.SizeOfOptionalHeader;

    ///////////////////////////////////////////////// Process Section Headers.
    //
    if (m_NtHeader.FileHeader.NumberOfSections > ARRAYSIZE(m_SectionHeaders)) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return FALSE;
    }
    memcpy(&m_SectionHeaders,
               m_pMap + m_nSectionsOffset,
               sizeof(m_SectionHeaders[0]) * m_NtHeader.FileHeader.NumberOfSections);

    /////////////////////////////////////////////////// Parse .detour Section.
    //
    DWORD rvaOriginalImageDirectory = 0;
    DWORD rvaDetourBeg = 0;
    DWORD rvaDetourEnd = 0;

    _Analysis_assume_(m_NtHeader.FileHeader.NumberOfSections <= ARRAYSIZE(m_SectionHeaders));

    for (n = 0; n < m_NtHeader.FileHeader.NumberOfSections; n++) {
        if (strcmp((PCHAR)m_SectionHeaders[n].Name, ".detour") == 0) {
            DETOUR_SECTION_HEADER dh;
            memcpy(&dh,
                       m_pMap + m_SectionHeaders[n].PointerToRawData,
                       sizeof(dh));

            rvaOriginalImageDirectory = dh.nOriginalImportVirtualAddress;
            if (dh.cbPrePE != 0) {
                m_nPrePE = m_SectionHeaders[n].PointerToRawData + sizeof(dh);
                m_cbPrePE = dh.cbPrePE;
            }
            rvaDetourBeg = m_SectionHeaders[n].VirtualAddress;
            rvaDetourEnd = rvaDetourBeg + m_SectionHeaders[n].SizeOfRawData;
        }
    }

    //////////////////////////////////////////////////////// Get Import Table.
    //
    DWORD rvaImageDirectory = m_NtHeader.OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    PIMAGE_IMPORT_DESCRIPTOR iidp
        = (PIMAGE_IMPORT_DESCRIPTOR)RvaToVa(rvaImageDirectory);
    PIMAGE_IMPORT_DESCRIPTOR oidp
        = (PIMAGE_IMPORT_DESCRIPTOR)RvaToVa(rvaOriginalImageDirectory);

    if (oidp == nullptr) {
        oidp = iidp;
    }
    if (iidp == nullptr || oidp == nullptr) {
        DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
        return FALSE;
    }

    DWORD nFiles = 0;
    for (; iidp[nFiles].OriginalFirstThunk != 0 || iidp[nFiles].FirstThunk != 0; nFiles++) {
    }

    CImageImportFile **ppLastFile = &m_pImportFiles;
    m_pImportFiles = nullptr;

    for (n = 0; n < nFiles; n++, iidp++) {
        ULONG_PTR rvaName = iidp->Name;
        PCHAR pszName = (PCHAR)RvaToVa(rvaName);
        if (pszName == nullptr) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            goto fail;
        }

        CImageImportFile *pImportFile = new NOTHROW CImageImportFile;
        if (pImportFile == nullptr) {
            DetoursSetLastError(DETOURS_STATUS_INSUFFICIENT_RESOURCES);
            goto fail;
        }

        *ppLastFile = pImportFile;
        ppLastFile = &pImportFile->m_pNextFile;
        m_nImportFiles++;

        pImportFile->m_pszName = DuplicateString(pszName);
        if (pImportFile->m_pszName == nullptr) {
            goto fail;
        }

        pImportFile->m_rvaOriginalFirstThunk = iidp->OriginalFirstThunk;
        pImportFile->m_rvaFirstThunk = iidp->FirstThunk;
        pImportFile->m_nForwarderChain = iidp->ForwarderChain;
        pImportFile->m_pImportNames = nullptr;
        pImportFile->m_nImportNames = 0;
        pImportFile->m_fByway = FALSE;

        if ((ULONG)iidp->FirstThunk >= rvaDetourBeg &&
            (ULONG)iidp->FirstThunk < rvaDetourEnd) {

            pImportFile->m_pszOrig = nullptr;
            pImportFile->m_fByway = TRUE;
            continue;
        }

        rvaName = oidp->Name;
        pszName = (PCHAR)RvaToVa(rvaName);
        if (pszName == nullptr) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            goto fail;
        }
        pImportFile->m_pszOrig = DuplicateString(pszName);
        if (pImportFile->m_pszOrig == nullptr) {
            goto fail;
        }

        DWORD rvaThunk = iidp->OriginalFirstThunk;
        if( !rvaThunk ) {
            rvaThunk = iidp->FirstThunk;
        }
        PIMAGE_THUNK_DATA pAddrThunk = (PIMAGE_THUNK_DATA)RvaToVa(rvaThunk);
        rvaThunk = oidp->OriginalFirstThunk;
        if( !rvaThunk ) {
            rvaThunk = oidp->FirstThunk;
        }
        PIMAGE_THUNK_DATA pLookThunk = (PIMAGE_THUNK_DATA)RvaToVa(rvaThunk);

        DWORD nNames = 0;
        if (pAddrThunk) {
            for (; pAddrThunk[nNames].u1.Ordinal; nNames++) {
            }
        }

        if (pAddrThunk && nNames) {
            pImportFile->m_nImportNames = nNames;
            pImportFile->m_pImportNames = new NOTHROW CImageImportName [nNames];
            if (pImportFile->m_pImportNames == nullptr) {
                DetoursSetLastError(DETOURS_STATUS_INSUFFICIENT_RESOURCES);
                goto fail;
            }

            CImageImportName *pImportName = &pImportFile->m_pImportNames[0];

            for (DWORD f = 0; f < nNames; f++, pImportName++) {
                pImportName->m_nOrig = 0;
                pImportName->m_nOrdinal = 0;
                pImportName->m_nHint = 0;
                pImportName->m_pszName = nullptr;
                pImportName->m_pszOrig = nullptr;

                rvaName = pAddrThunk[f].u1.Ordinal;
                if (rvaName & IMAGE_ORDINAL_FLAG) {
                    pImportName->m_nOrig = (ULONG)IMAGE_ORDINAL(rvaName);
                    pImportName->m_nOrdinal = pImportName->m_nOrig;
                }
                else {
                    PIMAGE_IMPORT_BY_NAME pName
                        = (PIMAGE_IMPORT_BY_NAME)RvaToVa(rvaName);
                    if (pName) {
                        pImportName->m_nHint = pName->Hint;
                        pImportName->m_pszName = DuplicateString((PCHAR)pName->Name);
                        if (pImportName->m_pszName == nullptr) {
                            goto fail;
                        }
                    }

                    rvaName = pLookThunk[f].u1.Ordinal;
                    if (rvaName & IMAGE_ORDINAL_FLAG) {
                        pImportName->m_nOrig = (ULONG)IMAGE_ORDINAL(rvaName);
                        pImportName->m_nOrdinal = (ULONG)IMAGE_ORDINAL(rvaName);
                    }
                    else {
                        pName = (PIMAGE_IMPORT_BY_NAME)RvaToVa(rvaName);
                        if (pName) {
                            pImportName->m_pszOrig
                                = DuplicateString((PCHAR)pName->Name);
                            if (pImportName->m_pszOrig == nullptr) {
                                goto fail;
                            }
                        }
                    }
                }
            }
        }
        oidp++;
    }

    ////////////////////////////////////////////////////////// Parse Sections.
    //
    m_nExtraOffset = 0;
    for (n = 0; n < m_NtHeader.FileHeader.NumberOfSections; n++) {
        m_nExtraOffset = Max(m_SectionHeaders[n].PointerToRawData +
                             m_SectionHeaders[n].SizeOfRawData,
                             m_nExtraOffset);

        if (strcmp((PCHAR)m_SectionHeaders[n].Name, ".detour") == 0) {
            DETOUR_SECTION_HEADER dh;
            memcpy(&dh,
                       m_pMap + m_SectionHeaders[n].PointerToRawData,
                       sizeof(dh));

            if (dh.nDataOffset == 0) {
                dh.nDataOffset = dh.cbHeaderSize;
            }

            cbData = dh.cbDataSize - dh.nDataOffset;
            pbData = (m_pMap +
                      m_SectionHeaders[n].PointerToRawData +
                      dh.nDataOffset);

            m_nExtraOffset = Max(m_SectionHeaders[n].PointerToRawData +
                                 m_SectionHeaders[n].SizeOfRawData,
                                 m_nExtraOffset);

            m_NtHeader.FileHeader.NumberOfSections--;

            m_NtHeader.OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress
                = dh.nOriginalImportVirtualAddress;
            m_NtHeader.OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size
                = dh.nOriginalImportSize;

            m_NtHeader.OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress
                = dh.nOriginalBoundImportVirtualAddress;
            m_NtHeader.OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size
                = dh.nOriginalBoundImportSize;

            m_NtHeader.OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress
                = dh.nOriginalIatVirtualAddress;
            m_NtHeader.OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size
                = dh.nOriginalIatSize;

            m_NtHeader.OptionalHeader.CheckSum = 0;
            m_NtHeader.OptionalHeader.SizeOfImage
                = dh.nOriginalSizeOfImage;

            m_fHadDetourSection = TRUE;
        }
    }

    m_pImageData = new NOTHROW CImageData(pbData, cbData);
    if (m_pImageData == nullptr) {
        DetoursSetLastError(DETOURS_STATUS_INSUFFICIENT_RESOURCES);
    }
    return TRUE;

fail:
    return FALSE;
}

static inline BOOL strneq(_In_ LPCSTR pszOne, _In_ LPCSTR pszTwo)
{
    if (pszOne == pszTwo) {
        return FALSE;
    }
    if (!pszOne || !pszTwo) {
        return TRUE;
    }
    return (strcmp(pszOne, pszTwo) != 0);
}

BOOL CImage::CheckImportsNeeded(DWORD *pnTables, DWORD *pnThunks, DWORD *pnChars)
{
    DWORD nTables = 0;
    DWORD nThunks = 0;
    DWORD nChars = 0;
    BOOL fNeedDetourSection = FALSE;

    for (CImageImportFile *pImportFile = m_pImportFiles;
         pImportFile != nullptr; pImportFile = pImportFile->m_pNextFile) {

        nChars += (int)strlen(pImportFile->m_pszName) + 1;
        nChars += nChars & 1;

        if (pImportFile->m_fByway) {
            fNeedDetourSection = TRUE;
            nThunks++;
        }
        else {
            if (!fNeedDetourSection &&
                strneq(pImportFile->m_pszName, pImportFile->m_pszOrig)) {

                fNeedDetourSection = TRUE;
            }
            for (DWORD n = 0; n < pImportFile->m_nImportNames; n++) {
                CImageImportName *pImportName = &pImportFile->m_pImportNames[n];

                if (!fNeedDetourSection &&
                    strneq(pImportName->m_pszName, pImportName->m_pszOrig)) {

                    fNeedDetourSection = TRUE;
                }

                if (pImportName->m_pszName) {
                    nChars += sizeof(WORD);             // Hint
                    nChars += (int)strlen(pImportName->m_pszName) + 1;
                    nChars += nChars & 1;
                }
                nThunks++;
            }
        }
        nThunks++;
        nTables++;
    }
    nTables++;

    *pnTables = nTables;
    *pnThunks = nThunks;
    *pnChars = nChars;

    return fNeedDetourSection;
}

//////////////////////////////////////////////////////////////////////////////
//
CImageImportFile * CImage::NewByway(_In_ LPCSTR pszName)
{
    CImageImportFile *pImportFile = new NOTHROW CImageImportFile;
    if (pImportFile == nullptr) {
        DetoursSetLastError(DETOURS_STATUS_INSUFFICIENT_RESOURCES);
        goto fail;
    }

    pImportFile->m_pNextFile = nullptr;
    pImportFile->m_fByway = TRUE;

    pImportFile->m_pszName = DuplicateString(pszName);
    if (pImportFile->m_pszName == nullptr) {
        goto fail;
    }

    pImportFile->m_rvaOriginalFirstThunk = 0;
    pImportFile->m_rvaFirstThunk = 0;
    pImportFile->m_nForwarderChain = (UINT)0;
    pImportFile->m_pImportNames = nullptr;
    pImportFile->m_nImportNames = 0;

    m_nImportFiles++;
    return pImportFile;

fail:
    if (pImportFile) {
        delete pImportFile;
        pImportFile = nullptr;
    }
    return nullptr;
}

BOOL CImage::EditImports(PVOID pContext,
                         PF_DETOUR_BINARY_BYWAY_CALLBACK pfBywayCallback,
                         PF_DETOUR_BINARY_FILE_CALLBACK pfFileCallback,
                         PF_DETOUR_BINARY_SYMBOL_CALLBACK pfSymbolCallback,
                         PF_DETOUR_BINARY_COMMIT_CALLBACK pfCommitCallback)
{
    CImageImportFile *pImportFile = nullptr;
    CImageImportFile **ppLastFile = &m_pImportFiles;

    DetoursSetLastError(DETOURS_STATUS_CALL_NOT_IMPLEMENTED);

    while ((pImportFile = *ppLastFile) != nullptr) {

        if (pfBywayCallback != nullptr) {
            LPCSTR pszFile = nullptr;
            if (!(*pfBywayCallback)(pContext, nullptr, &pszFile)) {
                goto fail;
            }

            if (pszFile != nullptr) {
                // Insert a new Byway.
                CImageImportFile *pByway = NewByway(pszFile);
                if (pByway == nullptr) {
                    return FALSE;
                }

                pByway->m_pNextFile = pImportFile;
                *ppLastFile = pByway;
                ppLastFile = &pByway->m_pNextFile;
                continue;                               // Retry after Byway.
            }
        }

        if (pImportFile->m_fByway) {
            if (pfBywayCallback != nullptr) {
                LPCSTR pszFile = nullptr;

                if (!(*pfBywayCallback)(pContext, pImportFile->m_pszName, &pszFile)) {
                    goto fail;
                }

                if (pszFile != nullptr) {
                    // Replace? Byway
                    if (pszFile != pImportFile->m_pszName) {
                        LPCSTR pszLast = pImportFile->m_pszName;
                        pImportFile->m_pszName = DuplicateString(pszFile);
                        ReleaseString(pszLast);

                        if (pImportFile->m_pszName == nullptr) {
                            goto fail;
                        }
                    }
                }
                else {                                  // Delete Byway
                    *ppLastFile = pImportFile->m_pNextFile;
                    pImportFile->m_pNextFile = nullptr;
                    delete pImportFile;
                    m_nImportFiles--;
                    continue;                           // Retry after delete.
                }
            }
        }
        else {
            if (pfFileCallback != nullptr) {
                LPCSTR pszFile = nullptr;

                if (!(*pfFileCallback)(pContext,
                                       pImportFile->m_pszOrig,
                                       pImportFile->m_pszName,
                                       &pszFile)) {
                    goto fail;
                }

                if (pszFile != nullptr) {
                    if (pszFile != pImportFile->m_pszName) {
                        LPCSTR pszLast = pImportFile->m_pszName;
                        pImportFile->m_pszName = DuplicateString(pszFile);
                        ReleaseString(pszLast);

                        if (pImportFile->m_pszName == nullptr) {
                            goto fail;
                        }
                    }
                }
            }

            if (pfSymbolCallback != nullptr) {
                for (DWORD n = 0; n < pImportFile->m_nImportNames; n++) {
                    CImageImportName *pImportName = &pImportFile->m_pImportNames[n];

                    LPCSTR pszName = nullptr;
                    ULONG nOrdinal = 0;
                    if (!(*pfSymbolCallback)(pContext,
                                             pImportName->m_nOrig,
                                             pImportName->m_nOrdinal,
                                             &nOrdinal,
                                             pImportName->m_pszOrig,
                                             pImportName->m_pszName,
                                             &pszName)) {
                        goto fail;
                    }

                    if (pszName != nullptr) {
                        if (pszName != pImportName->m_pszName) {
                            pImportName->m_nOrdinal = 0;

                            LPCSTR pszLast = pImportName->m_pszName;
                            pImportName->m_pszName = DuplicateString(pszName);
                            ReleaseString(pszLast);

                            if (pImportName->m_pszName == nullptr) {
                                goto fail;
                            }
                        }
                    }
                    else if (nOrdinal != 0) {
                        pImportName->m_nOrdinal = nOrdinal;

                        if (pImportName->m_pszName != nullptr) {
                            delete[] pImportName->m_pszName;
                            pImportName->m_pszName = nullptr;
                        }
                    }
                }
            }
        }

        ppLastFile = &pImportFile->m_pNextFile;
        pImportFile = pImportFile->m_pNextFile;
    }

    for (;;) {
        if (pfBywayCallback != nullptr) {
            LPCSTR pszFile = nullptr;
            if (!(*pfBywayCallback)(pContext, nullptr, &pszFile)) {
                goto fail;
            }
            if (pszFile != nullptr) {
                // Insert a new Byway.
                CImageImportFile *pByway = NewByway(pszFile);
                if (pByway == nullptr) {
                    return FALSE;
                }

                pByway->m_pNextFile = pImportFile;
                *ppLastFile = pByway;
                ppLastFile = &pByway->m_pNextFile;
                continue;                               // Retry after Byway.
            }
        }
        break;
    }

    if (pfCommitCallback != nullptr) {
        if (!(*pfCommitCallback)(pContext)) {
            goto fail;
        }
    }

    DetoursSetLastError(DETOURS_STATUS_SUCCESS);
    return TRUE;

  fail:
    return FALSE;
}

BOOL CImage::Write(HANDLE hFile)
{
    DWORD cbDone;

    if (hFile == INVALID_HANDLE_VALUE) {
        DetoursSetLastError(DETOURS_STATUS_INVALID_HANDLE);
        return FALSE;
    }

    m_nNextFileAddr = 0;
    m_nNextVirtAddr = 0;

    DWORD nTables = 0;
    DWORD nThunks = 0;
    DWORD nChars = 0;
    BOOL fNeedDetourSection = CheckImportsNeeded(&nTables, &nThunks, &nChars);

    //////////////////////////////////////////////////////////// Copy Headers.
    //
    if (SetFilePointer(hFile, 0, nullptr, FILE_BEGIN) == ~0u) {
        return FALSE;
    }
    if (!CopyFileData(hFile, 0, m_NtHeader.OptionalHeader.SizeOfHeaders)) {
        return FALSE;
    }

    if (fNeedDetourSection || !m_pImageData->IsEmpty()) {
        // Replace the file's DOS header with our own.
        m_nPeOffset = sizeof(m_DosHeader) + sizeof(s_rbDosCode);
        m_nSectionsOffset = m_nPeOffset
            + sizeof(m_NtHeader.Signature)
            + sizeof(m_NtHeader.FileHeader)
            + m_NtHeader.FileHeader.SizeOfOptionalHeader;
        m_DosHeader.e_lfanew = m_nPeOffset;

        if (SetFilePointer(hFile, 0, nullptr, FILE_BEGIN) == ~0u) {
            return FALSE;
        }
        if (!WriteFile(hFile, &m_DosHeader, sizeof(m_DosHeader), &cbDone)) {
            return FALSE;
        }
        if (!WriteFile(hFile, &s_rbDosCode, sizeof(s_rbDosCode), &cbDone)) {
            return FALSE;
        }
    }
    else {
        // Restore the file's original DOS header.
        if (m_nPrePE != 0) {
            m_nPeOffset = m_cbPrePE;
            m_nSectionsOffset = m_nPeOffset
                + sizeof(m_NtHeader.Signature)
                + sizeof(m_NtHeader.FileHeader)
                + m_NtHeader.FileHeader.SizeOfOptionalHeader;
            m_DosHeader.e_lfanew = m_nPeOffset;


            if (SetFilePointer(hFile, 0, nullptr, FILE_BEGIN) == ~0u) {
                return FALSE;
            }
            if (!CopyFileData(hFile, m_nPrePE, m_cbPrePE)) {
                return FALSE;
            }
        }
    }

    m_nNextFileAddr = m_NtHeader.OptionalHeader.SizeOfHeaders;
    m_nNextVirtAddr = 0;
    if (!AlignFileData(hFile)) {
        return FALSE;
    }

    /////////////////////////////////////////////////////////// Copy Sections.
    //
    DWORD n = 0;
    for (; n < m_NtHeader.FileHeader.NumberOfSections; n++) {
        if (m_SectionHeaders[n].SizeOfRawData) {
            if (SetFilePointer(hFile,
                               m_SectionHeaders[n].PointerToRawData,
                               nullptr, FILE_BEGIN) == ~0u) {
                return FALSE;
            }
            if (!CopyFileData(hFile,
                              m_SectionHeaders[n].PointerToRawData,
                              m_SectionHeaders[n].SizeOfRawData)) {
                return FALSE;
            }
        }
        m_nNextFileAddr = Max(m_SectionHeaders[n].PointerToRawData +
                              m_SectionHeaders[n].SizeOfRawData,
                              m_nNextFileAddr);
#if 0
        m_nNextVirtAddr = Max(m_SectionHeaders[n].VirtualAddress +
                              m_SectionHeaders[n].Misc.VirtualSize,
                              m_nNextVirtAddr);
#else
        m_nNextVirtAddr = Max(m_SectionHeaders[n].VirtualAddress +
                              (m_SectionHeaders[n].Misc.VirtualSize
                               ? m_SectionHeaders[n].Misc.VirtualSize
                               : SectionAlign(m_SectionHeaders[n].SizeOfRawData)),
                              m_nNextVirtAddr);
#endif

        m_nExtraOffset = Max(m_nNextFileAddr, m_nExtraOffset);

        if (!AlignFileData(hFile)) {
            return FALSE;
        }
    }

    if (fNeedDetourSection || !m_pImageData->IsEmpty()) {

        if (m_NtHeader.FileHeader.NumberOfSections >= ARRAYSIZE(m_SectionHeaders)) {
            DetoursSetLastError(DETOURS_STATUS_BAD_EXE_FORMAT);
            return FALSE;
        }

        ////////////////////////////////////////////// Insert .detour Section.
        //
        DWORD nSection = m_NtHeader.FileHeader.NumberOfSections++;
        DETOUR_SECTION_HEADER dh;

        RtlSecureZeroMemory(&dh, sizeof(dh));
        RtlSecureZeroMemory(&m_SectionHeaders[nSection], sizeof(m_SectionHeaders[nSection]));

        dh.cbHeaderSize = sizeof(DETOUR_SECTION_HEADER);
        dh.nSignature = DETOUR_SECTION_HEADER_SIGNATURE;

        dh.nOriginalImportVirtualAddress = m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        dh.nOriginalImportSize = m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;

        dh.nOriginalBoundImportVirtualAddress
            = m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress;
        dh.nOriginalBoundImportSize = m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size;

        dh.nOriginalIatVirtualAddress = m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress;
        dh.nOriginalIatSize = m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size;

        dh.nOriginalSizeOfImage = m_NtHeader.OptionalHeader.SizeOfImage;

        DWORD clrAddr = m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
        DWORD clrSize = m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].Size;
        if (clrAddr && clrSize) {
            PDETOUR_CLR_HEADER pHdr = (PDETOUR_CLR_HEADER)RvaToVa(clrAddr);
            if (pHdr != nullptr) {
                DETOUR_CLR_HEADER hdr;
                hdr = *pHdr;

                dh.nOriginalClrFlags = hdr.Flags;
            }
        }

        HRESULT hrRet = StringCchCopyA((PCHAR)m_SectionHeaders[nSection].Name, IMAGE_SIZEOF_SHORT_NAME , ".detour");
        if (FAILED(hrRet))
            return FALSE;

        m_SectionHeaders[nSection].Characteristics
            = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;

        m_nOutputVirtAddr = m_nNextVirtAddr;
        m_nOutputVirtSize = 0;
        m_nOutputFileAddr = m_nNextFileAddr;

        dh.nDataOffset = 0;                     // pbData
        dh.cbDataSize = m_pImageData->m_cbData;
        dh.cbPrePE = m_cbPrePE;

        //////////////////////////////////////////////////////////////////////////
        //

        DWORD rvaImportTable = 0;
        DWORD rvaLookupTable = 0;
        DWORD rvaBoundTable = 0;
        DWORD rvaNameTable = 0;
        DWORD nImportTableSize = nTables * sizeof(IMAGE_IMPORT_DESCRIPTOR);

        if (!SizeOutputBuffer(QuadAlign(sizeof(dh))
                              + m_cbPrePE
                              + QuadAlign(m_pImageData->m_cbData)
                              + QuadAlign(sizeof(IMAGE_THUNK_DATA) * nThunks)
                              + QuadAlign(sizeof(IMAGE_THUNK_DATA) * nThunks)
                              + QuadAlign(nChars)
                              + QuadAlign(nImportTableSize))) {
            return FALSE;
        }

        DWORD vaHead = 0;
        PBYTE pbHead = nullptr;
        DWORD vaPrePE = 0;
        PBYTE pbPrePE = nullptr;
        DWORD vaData = 0;
        PBYTE pbData = nullptr;

        if ((pbHead = AllocateOutput(sizeof(dh), &vaHead)) == nullptr) {
            return FALSE;
        }

        if ((pbPrePE = AllocateOutput(m_cbPrePE, &vaPrePE)) == nullptr) {
            return FALSE;
        }

        CImageThunks lookupTable(this, nThunks, &rvaLookupTable);
        CImageThunks boundTable(this, nThunks, &rvaBoundTable);
        CImageChars nameTable(this, nChars, &rvaNameTable);

        if ((pbData = AllocateOutput(m_pImageData->m_cbData, &vaData)) == nullptr) {
            return FALSE;
        }

        dh.nDataOffset = vaData - vaHead;
        dh.cbDataSize = dh.nDataOffset + m_pImageData->m_cbData;
        memcpy(pbHead, &dh, sizeof(dh));
        memcpy(pbPrePE, m_pMap + m_nPrePE, m_cbPrePE);
        memcpy(pbData, m_pImageData->m_pbData, m_pImageData->m_cbData);

        PIMAGE_IMPORT_DESCRIPTOR piidDst = (PIMAGE_IMPORT_DESCRIPTOR)
            AllocateOutput(nImportTableSize, &rvaImportTable);
        if (piidDst == nullptr) {
            return FALSE;
        }

        //////////////////////////////////////////////// Step Through Imports.
        //
        for (CImageImportFile *pImportFile = m_pImportFiles;
             pImportFile != nullptr; pImportFile = pImportFile->m_pNextFile) {

            RtlSecureZeroMemory(piidDst, sizeof(piidDst));
            nameTable.Allocate(pImportFile->m_pszName, (DWORD *)&piidDst->Name);
            piidDst->TimeDateStamp = 0;
            piidDst->ForwarderChain = pImportFile->m_nForwarderChain;

            if (pImportFile->m_fByway) {
                ULONG rvaIgnored;

                lookupTable.Allocate(IMAGE_ORDINAL_FLAG+1,
                                     (DWORD *)&piidDst->OriginalFirstThunk);
                boundTable.Allocate(IMAGE_ORDINAL_FLAG+1,
                                    (DWORD *)&piidDst->FirstThunk);

                lookupTable.Allocate(0, &rvaIgnored);
                boundTable.Allocate(0, &rvaIgnored);
            }
            else {
                ULONG rvaIgnored;

                piidDst->FirstThunk = (ULONG)pImportFile->m_rvaFirstThunk;
                lookupTable.Current((DWORD *)&piidDst->OriginalFirstThunk);

                for (n = 0; n < pImportFile->m_nImportNames; n++) {
                    CImageImportName *pImportName = &pImportFile->m_pImportNames[n];

                    if (pImportName->m_pszName) {
                        ULONG nDstName = 0;

                        nameTable.Allocate(pImportName->m_pszName,
                                           pImportName->m_nHint,
                                           &nDstName);
                        lookupTable.Allocate(nDstName, &rvaIgnored);
                    }
                    else {
                        lookupTable.Allocate(IMAGE_ORDINAL_FLAG + pImportName->m_nOrdinal,
                                             &rvaIgnored);
                    }
                }
                lookupTable.Allocate(0, &rvaIgnored);
            }
            piidDst++;
        }
        RtlSecureZeroMemory(piidDst, sizeof(piidDst));

        //////////////////////////////////////////////////////////////////////////
        //
        m_nNextVirtAddr += m_nOutputVirtSize;
        m_nNextFileAddr += FileAlign(m_nOutputVirtSize);

        if (!AlignFileData(hFile)) {
            return FALSE;
        }

        //////////////////////////////////////////////////////////////////////////
        //
        m_SectionHeaders[nSection].VirtualAddress = m_nOutputVirtAddr;
        m_SectionHeaders[nSection].Misc.VirtualSize = m_nOutputVirtSize;
        m_SectionHeaders[nSection].PointerToRawData = m_nOutputFileAddr;
        m_SectionHeaders[nSection].SizeOfRawData = FileAlign(m_nOutputVirtSize);

        m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress
            = rvaImportTable;
        m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size
            = nImportTableSize;

        m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress = 0;
        m_NtHeader.OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size = 0;

        //////////////////////////////////////////////////////////////////////////
        //
        if (SetFilePointer(hFile, m_SectionHeaders[nSection].PointerToRawData,
                           nullptr, FILE_BEGIN) == ~0u) {
            return FALSE;
        }
        if (!WriteFile(hFile, m_pbOutputBuffer, m_SectionHeaders[nSection].SizeOfRawData,
                       &cbDone)) {
            return FALSE;
        }
    }

    ///////////////////////////////////////////////////// Adjust Extra Data.
    //
    LONG nExtraAdjust = m_nNextFileAddr - m_nExtraOffset;
    for (n = 0; n < m_NtHeader.FileHeader.NumberOfSections; n++) {
        if (m_SectionHeaders[n].PointerToRawData > m_nExtraOffset) {
            m_SectionHeaders[n].PointerToRawData += nExtraAdjust;
        }
        if (m_SectionHeaders[n].PointerToRelocations > m_nExtraOffset) {
            m_SectionHeaders[n].PointerToRelocations += nExtraAdjust;
        }
        if (m_SectionHeaders[n].PointerToLinenumbers > m_nExtraOffset) {
            m_SectionHeaders[n].PointerToLinenumbers += nExtraAdjust;
        }
    }
    if (m_NtHeader.FileHeader.PointerToSymbolTable > m_nExtraOffset) {
        m_NtHeader.FileHeader.PointerToSymbolTable += nExtraAdjust;
    }

    m_NtHeader.OptionalHeader.CheckSum = 0;
    m_NtHeader.OptionalHeader.SizeOfImage = m_nNextVirtAddr;

    ////////////////////////////////////////////////// Adjust Debug Directory.
    //
    DWORD debugAddr = m_NtHeader.OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
    DWORD debugSize = m_NtHeader.OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
    if (debugAddr && debugSize) {
        DWORD nFileOffset = RvaToFileOffset(debugAddr);
        if (SetFilePointer(hFile, nFileOffset, nullptr, FILE_BEGIN) == ~0u) {
            return FALSE;
        }

        PIMAGE_DEBUG_DIRECTORY pDir = (PIMAGE_DEBUG_DIRECTORY)RvaToVa(debugAddr);
        if (pDir == nullptr) {
            return FALSE;
        }

        DWORD nEntries = debugSize / sizeof(*pDir);
        for (n = 0; n < nEntries; n++) {
            IMAGE_DEBUG_DIRECTORY dir = pDir[n];

            if (dir.PointerToRawData > m_nExtraOffset) {
                dir.PointerToRawData += nExtraAdjust;
            }
            if (!WriteFile(hFile, &dir, sizeof(dir), &cbDone)) {
                return FALSE;
            }
        }
    }

    /////////////////////////////////////////////////////// Adjust CLR Header.
    //
    DWORD clrAddr = m_NtHeader.OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
    DWORD clrSize = m_NtHeader.OptionalHeader
        .DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].Size;
    if (clrAddr && clrSize && fNeedDetourSection) {
        DWORD nFileOffset = RvaToFileOffset(clrAddr);
        if (SetFilePointer(hFile, nFileOffset, nullptr, FILE_BEGIN) == ~0u) {
            return FALSE;
        }

        PDETOUR_CLR_HEADER pHdr = (PDETOUR_CLR_HEADER)RvaToVa(clrAddr);
        if (pHdr == nullptr) {
            return FALSE;
        }

        DETOUR_CLR_HEADER hdr;
        hdr = *pHdr;
        hdr.Flags &= 0xfffffffe;    // Clear the IL_ONLY flag.

        if (!WriteFile(hFile, &hdr, sizeof(hdr), &cbDone)) {
            return FALSE;
        }
    }

    ///////////////////////////////////////////////// Copy Left-over Data.
    //
    if (m_nFileSize > m_nExtraOffset) {
        if (SetFilePointer(hFile, m_nNextFileAddr, nullptr, FILE_BEGIN) == ~0u) {
            return FALSE;
        }
        if (!CopyFileData(hFile, m_nExtraOffset, m_nFileSize - m_nExtraOffset)) {
            return FALSE;
        }
    }


    //////////////////////////////////////////////////// Finalize Headers.
    //

    if (SetFilePointer(hFile, m_nPeOffset, nullptr, FILE_BEGIN) == ~0u) {
        return FALSE;
    }
    if (!WriteFile(hFile, &m_NtHeader, sizeof(m_NtHeader), &cbDone)) {
        return FALSE;
    }

    if (SetFilePointer(hFile, m_nSectionsOffset, nullptr, FILE_BEGIN) == ~0u) {
        return FALSE;
    }
    if (!WriteFile(hFile, &m_SectionHeaders,
                   sizeof(m_SectionHeaders[0])
                   * m_NtHeader.FileHeader.NumberOfSections,
                   &cbDone)) {
        return FALSE;
    }

    m_cbPostPE = SetFilePointer(hFile, 0, nullptr, FILE_CURRENT);
    if (m_cbPostPE == ~0u) {
        return FALSE;
    }
    m_cbPostPE = m_NtHeader.OptionalHeader.SizeOfHeaders - m_cbPostPE;

    return TRUE;
}

};                                                      // namespace Detour

//////////////////////////////////////////////////////////////////////////////
//
PDETOUR_BINARY DETOURS_API DetourBinaryOpen(_In_ HANDLE hFile)
{
    Detour::CImage *pImage = new NOTHROW
        Detour::CImage;
    if (pImage == nullptr) {
        DetoursSetLastError(DETOURS_STATUS_INSUFFICIENT_RESOURCES);
        return FALSE;
    }

    if (!pImage->Read(hFile)) {
        delete pImage;
        return FALSE;
    }

    return (PDETOUR_BINARY)pImage;
}

BOOL DETOURS_API DetourBinaryWrite(_In_ PDETOUR_BINARY pdi,
                              _In_ HANDLE hFile)
{
    Detour::CImage *pImage = Detour::CImage::IsValid(pdi);
    if (pImage == nullptr) {
        return FALSE;
    }

    return pImage->Write(hFile);
}

_Writable_bytes_(*pcbData)
_Readable_bytes_(*pcbData)
_Success_(return != nullptr)
PVOID DETOURS_API DetourBinaryEnumeratePayloads(_In_ PDETOUR_BINARY pBinary,
                                           _Out_opt_ GUID *pGuid,
                                           _Out_ DWORD *pcbData,
                                           _Inout_ DWORD *pnIterator)
{
    Detour::CImage *pImage = Detour::CImage::IsValid(pBinary);
    if (pImage == nullptr) {
        return FALSE;
    }

    return pImage->DataEnum(pGuid, pcbData, pnIterator);
}

_Writable_bytes_(*pcbData)
_Readable_bytes_(*pcbData)
_Success_(return != nullptr)
PVOID DETOURS_API DetourBinaryFindPayload(_In_ PDETOUR_BINARY pBinary,
                                     _In_ REFGUID rguid,
                                     _Out_ DWORD *pcbData)
{
    Detour::CImage *pImage = Detour::CImage::IsValid(pBinary);
    if (pImage == nullptr) {
        return FALSE;
    }

    return pImage->DataFind(rguid, pcbData);
}

PVOID DETOURS_API DetourBinarySetPayload(_In_ PDETOUR_BINARY pBinary,
                                    _In_ REFGUID rguid,
                                    _In_reads_opt_(cbData) PVOID pvData,
                                    _In_ DWORD cbData)
{
    Detour::CImage *pImage = Detour::CImage::IsValid(pBinary);
    if (pImage == nullptr) {
        return nullptr;
    }

    return pImage->DataSet(rguid, (PBYTE)pvData, cbData);
}

BOOL DETOURS_API DetourBinaryDeletePayload(_In_ PDETOUR_BINARY pBinary,
                                      _In_ REFGUID rguid)
{
    Detour::CImage *pImage = Detour::CImage::IsValid(pBinary);
    if (pImage == nullptr) {
        return FALSE;
    }

    return pImage->DataDelete(rguid);
}

BOOL DETOURS_API DetourBinaryPurgePayloads(_In_ PDETOUR_BINARY pBinary)
{
    Detour::CImage *pImage = Detour::CImage::IsValid(pBinary);
    if (pImage == nullptr) {
        return FALSE;
    }

    return pImage->DataPurge();
}

//////////////////////////////////////////////////////////////////////////////
//
static BOOL DETOURS_CALLBACK ResetBywayCallback(_In_opt_ PVOID pContext,
                                        _In_opt_ LPCSTR pszFile,
                                        _Outptr_result_maybenull_ LPCSTR *ppszOutFile)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(pszFile);

    *ppszOutFile = nullptr;
    return TRUE;
}

static BOOL DETOURS_CALLBACK ResetFileCallback(_In_opt_ PVOID pContext,
                                       _In_ LPCSTR pszOrigFile,
                                       _In_ LPCSTR pszFile,
                                       _Outptr_result_maybenull_ LPCSTR *ppszOutFile)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(pszFile);

    *ppszOutFile = pszOrigFile;
    return TRUE;
}

static BOOL DETOURS_CALLBACK ResetSymbolCallback(_In_opt_ PVOID pContext,
                                         _In_ ULONG nOrigOrdinal,
                                         _In_ ULONG nOrdinal,
                                         _Out_ ULONG *pnOutOrdinal,
                                         _In_opt_ LPCSTR pszOrigSymbol,
                                         _In_opt_ LPCSTR pszSymbol,
                                         _Outptr_result_maybenull_ LPCSTR *ppszOutSymbol)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(nOrdinal);
    UNREFERENCED_PARAMETER(pszSymbol);

    *pnOutOrdinal = nOrigOrdinal;
    *ppszOutSymbol = pszOrigSymbol;
    return TRUE;
}

BOOL DETOURS_API DetourBinaryResetImports(_In_ PDETOUR_BINARY pBinary)
{
    Detour::CImage *pImage = Detour::CImage::IsValid(pBinary);
    if (pImage == nullptr) {
        return FALSE;
    }

    return pImage->EditImports(nullptr,
                               ResetBywayCallback,
                               ResetFileCallback,
                               ResetSymbolCallback,
                               nullptr);
}

//////////////////////////////////////////////////////////////////////////////
//
BOOL DETOURS_API DetourBinaryEditImports(_In_ PDETOUR_BINARY pBinary,
                                    _In_opt_ PVOID pContext,
                                    _In_opt_ PF_DETOUR_BINARY_BYWAY_CALLBACK pfByway,
                                    _In_opt_ PF_DETOUR_BINARY_FILE_CALLBACK pfFile,
                                    _In_opt_ PF_DETOUR_BINARY_SYMBOL_CALLBACK pfSymbol,
                                    _In_opt_ PF_DETOUR_BINARY_COMMIT_CALLBACK pfCommit)
{
    Detour::CImage *pImage = Detour::CImage::IsValid(pBinary);
    if (pImage == nullptr) {
        return FALSE;
    }

    return pImage->EditImports(pContext,
                               pfByway,
                               pfFile,
                               pfSymbol,
                               pfCommit);
}

BOOL DETOURS_API DetourBinaryClose(_In_ PDETOUR_BINARY pBinary)
{
    Detour::CImage *pImage = Detour::CImage::IsValid(pBinary);
    if (pImage == nullptr) {
        return FALSE;
    }

    BOOL bSuccess = pImage->Close();
    delete pImage;
    pImage = nullptr;

    return bSuccess;
}

#endif // DetoursUserMode

//
///////////////////////////////////////////////////////////////// End of File.
