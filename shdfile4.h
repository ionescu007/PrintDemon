/*++

    Copyright (C) Alex Ionescu. All rights reserved.
    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    shdfile4.h

Abstract:

    Contains the definition of SHADOWFILE_4 reverse engineered for Windows 10

Environment:

    User Mode

Revision History:

    Added definition for Windows 10 Shadow Files - ionescu007 - 12 May 20

--*/

#define SF_SIGNATURE_4      0x5123  /* 'Q#' is the signature value */
#define SF_VERSION_4        4
typedef struct _SHADOWFILE_4
{
    DWORD signature;
    DWORD HeaderSize;
    DWORD Status;
    DWORD JobId;
    DWORD Priority;
    LPWSTR pNotify;
    LPWSTR pUser;
    LPWSTR pDocument;
    LPWSTR pOutputFile;
    LPWSTR pPrinterName;
    LPWSTR pDriverName;
    LPDEVMODE pDevMode;
    LPWSTR pPrintProcName;
    LPWSTR pDatatype;
    LPWSTR pParameters;
    SYSTEMTIME Submitted;
    DWORD StartTime;
    DWORD UntilTime;
    DWORD Size;
    DWORD cPages;
    DWORD cbSecurityDescriptor;
    PSECURITY_DESCRIPTOR pSecurityDescriptor;
    DWORD NextJobId;
    DWORD Version;
    DWORD dwReboots;
    LPWSTR pMachineName;
    DWORD TotalSize;
    LPWSTR pUserSid;
    LPWSTR pFilePool;
    DWORD SizeHigh;
    DWORD TotalSizeHigh;
    DWORD NamedPropertiesSize;
    LPWSTR NamedProperties;
} SHADOWFILE_4, *PSHADOWFILE_4;
#ifdef _M_AMD64
C_ASSERT(sizeof(SHADOWFILE_4) == 0xE0);
#endif
