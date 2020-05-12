#include <windows.h>
#include <stdio.h>
#include "..\shdfile4.h"

INT
main(
    _In_ INT ArgumentCount,
    _In_ PCHAR Arguments[]
    )
{
    PRINTER_DEFAULTS printerDefaults;
    BOOL bRes;
    HANDLE hPrinter;
    PJOB_INFO_4 jobInfo;
    DWORD dwNeeded, dwReturned;
    PPRINTER_INFO_4 printerInfo;
    PPRINTER_INFO_2 printerFullInfo;
    LPWSTR printerName;
    WCHAR jobName[64];
    HANDLE hJob;
    DWORD dwError;
    PCHAR printerData;
    WCHAR spoolDir[MAX_PATH];
    SHADOWFILE_4 shadowFileData;
    PSHADOWFILE_4 pShadowFileData;

    //
    // First see how much space we need
    //
    dwNeeded = 0;
    dwReturned = 0;
    bRes = EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 4, NULL, 0, &dwNeeded, &dwReturned);
    if ((bRes != FALSE) || (dwNeeded == 0))
    {
        printf("Error: %lx\n", GetLastError());
        return -1;
    }

    //
    // Allocate it
    //
    printerInfo = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwNeeded);
    if (printerInfo == NULL)
    {
        printf("Error: %lx\n", GetLastError());
        return NULL;
    }

    //
    // Now enumerate the printer
    //
    bRes = EnumPrinters(PRINTER_ENUM_LOCAL,
                        NULL,
                        4,
                        (LPBYTE)printerInfo,
                        dwNeeded,
                        &dwNeeded,
                        &dwReturned);
    if (bRes == FALSE)
    {
        printf("Error: %lx\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, printerInfo);
        return -1;
    }
    
    //
    // Enumerate all the printers
    //
    printerName = NULL;
    while (dwReturned--)
    {
        //
        // Check for attributes that indicate ours
        //
        if ((printerInfo->Attributes & (PRINTER_ATTRIBUTE_HIDDEN |
                                        PRINTER_ATTRIBUTE_RAW_ONLY |
                                        PRINTER_ATTRIBUTE_LOCAL)) ==
                                       (PRINTER_ATTRIBUTE_HIDDEN |
                                        PRINTER_ATTRIBUTE_RAW_ONLY |
                                        PRINTER_ATTRIBUTE_LOCAL))
        {
            //
            // We found it!
            //
            printf("[+] Found IPC Printer: %S (status = %lx)\n",
                    printerInfo->pPrinterName, printerInfo->Attributes);
            printerName = printerInfo->pPrinterName;
            break;
        }
        printerInfo++;
    }

    //
    // Check if we found our printer
    //
    if (printerName == NULL)
    {
        printf("[-] Couldn't find IPC printer!\n");
        return -1;
    }

    //
    // We did, go open it, and then free the name/info structure
    //
    printerDefaults.pDatatype = NULL;
    printerDefaults.pDevMode = NULL;
    printerDefaults.DesiredAccess = PRINTER_ALL_ACCESS;
    bRes = OpenPrinter(printerName, &hPrinter, &printerDefaults);
    HeapFree(GetProcessHeap(), 0, printerInfo);
    if (bRes == FALSE)
    {
        printf("Failed to open printer: %lx\n", GetLastError());
        return -1;
    }

    //
    // Check how much space we needed for the printer information
    //
    bRes = GetPrinter(hPrinter, 2, NULL, 0, &dwNeeded);
    if (bRes != FALSE)
    {
        printf("Unexpected success querying printer\n");
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // Allocate it
    //
    printerFullInfo = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwNeeded);
    if (printerFullInfo == NULL)
    {
        printf("Out of memory\n");
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // Now query it
    //
    bRes = GetPrinter(hPrinter,
                      2,
                      (LPBYTE)printerFullInfo,
                      dwNeeded,
                      &dwNeeded);
    if (bRes == FALSE)
    {
        printf("Failed to query printer: %lx\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, printerFullInfo);
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // Make sure someone published some data to it
    //
    if (printerFullInfo->cJobs != 1)
    {
        printf("Printer doesn't have an active job: %lx\n", printerFullInfo->cJobs);
        HeapFree(GetProcessHeap(), 0, printerFullInfo);
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // Enumerate the first (and only) job -- see how much space is needed for it
    //
    HeapFree(GetProcessHeap(), 0, printerFullInfo);
    bRes = EnumJobs(hPrinter, 0, 1, 4, NULL, 0, &dwNeeded, &dwReturned);
    if ((bRes != FALSE) && (dwReturned != 0))
    {
        printf("Failed to enumerate jobs: %lx\n", GetLastError());
        ClosePrinter(hPrinter);
        return -1;
    }
    else if (dwNeeded == 0)
    {
        printf("[-] No printer job active!\n");
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // Allocate space for it
    //
    jobInfo = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwNeeded);
    if (jobInfo == NULL)
    {
        printf("Out of memory\n");
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // Now enumerate information on this job
    //
    bRes = EnumJobs(hPrinter,
                    0,
                    1,
                    4,
                    (LPBYTE)jobInfo,
                    dwNeeded,
                    &dwNeeded,
                    &dwReturned);
    if (bRes == FALSE)
    {
        printf("Error enumerating job: %lx\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, jobInfo);
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // Print some information on the job based on the API
    //
    printf("[+] Found IPC Job\n");
    printf("[.]\tJob ID: %d\n", jobInfo->JobId);
    printf("[.]\tQueued by: %S on %S\n",
           jobInfo->pUserName, jobInfo->pMachineName);
    printf("[.]\tSD: %p\n", jobInfo->pSecurityDescriptor);
    printf("[.]\tDocument Name: %S and type: %S\n",
           jobInfo->pDocument, jobInfo->pDatatype);
    printf("[.]\tJob Status: %lx (%S)\n",
           jobInfo->Status, jobInfo->pStatus);
    printf("[.]\tPriority: %d Position: %d\n",
           jobInfo->Priority, jobInfo->Position);
    printf("[.]\tData Size: %lld bytes (%d pages total, %d printed so far)\n",
           (DWORD64)jobInfo->SizeHigh << 32ULL | jobInfo->Size,
           jobInfo->TotalPages, jobInfo->PagesPrinted);
    printf("[.]\tTime: %d Start Time: %d End Time: %d\n",
           jobInfo->Time, jobInfo->StartTime, jobInfo->UntilTime);
    printf("[.]\tSubmitted on %d/%d/%d at %d:%d:%d.%d\n",
           jobInfo->Submitted.wMonth, jobInfo->Submitted.wDay, jobInfo->Submitted.wYear,
           jobInfo->Submitted.wHour, jobInfo->Submitted.wMinute, jobInfo->Submitted.wSecond,
           jobInfo->Submitted.wMilliseconds);

    //
    // Get the spooler directory
    //
    dwError = GetPrinterData(hPrinter,
                             SPLREG_DEFAULT_SPOOL_DIRECTORY,
                             NULL,
                             (LPBYTE)spoolDir,
                             sizeof(spoolDir),
                             &dwNeeded);
    if (dwError != ERROR_SUCCESS)
    {
        printf("Failed getting spooler directory: %lx\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, jobInfo);
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // Done with the printer
    //
    ClosePrinter(hPrinter);

    //
    // Open the shadow file based on the expected name
    //
    HANDLE hShadowFile;
    WCHAR shadowFileName[MAX_PATH];
    wsprintf(shadowFileName, L"%s\\%05d.SHD", spoolDir, jobInfo->JobId);
    printf("[.] Opening %S\n", shadowFileName);
    hShadowFile = CreateFile(shadowFileName,
                             GENERIC_READ,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL);
    if (hShadowFile == INVALID_HANDLE_VALUE)
    {
        printf("[-] Couldn't find shadow file (it can have any name it wants): %lx\n",
                GetLastError());
        HeapFree(GetProcessHeap(), 0, jobInfo);
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // Read the shadow file
    //
    bRes = ReadFile(hShadowFile,
                    &shadowFileData,
                    sizeof(shadowFileData),
                    &dwReturned,
                    NULL);
    if ((bRes == FALSE) || (dwReturned != sizeof(shadowFileData)))
    {
        printf("[-] Error reading shadow file: %lx\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, jobInfo);
        CloseHandle(hShadowFile);
        ClosePrinter(hPrinter);
        return -1;
    }

    //
    // We only support Version 4
    //
    if ((shadowFileData.Version != SF_VERSION_4) ||
        (shadowFileData.HeaderSize != sizeof(shadowFileData)) ||
        (shadowFileData.signature != SF_SIGNATURE_4))
    {
        printf("[-] Unrecognized shadow file format\n");
        HeapFree(GetProcessHeap(), 0, jobInfo);
        CloseHandle(hShadowFile);
        return -1;
    }

    //
    // Make sure it's for this job
    //
    if (shadowFileData.JobId != jobInfo->JobId)
    {
        printf("[-] Job ID mismatch: %d\n", shadowFileData.JobId);
        HeapFree(GetProcessHeap(), 0, jobInfo);
        CloseHandle(hShadowFile);
        return -1;
    }
 
    //
    // Get the size of the shadow spool file
    //
    dwNeeded = GetFileSize(hShadowFile, NULL);
    if (dwNeeded == 0)
    {
        printf("Error getting file size: %lx\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, jobInfo);
        CloseHandle(hShadowFile);
        return -1;
    }

    //
    // Allocate data for it
    //
    pShadowFileData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwNeeded);
    if (pShadowFileData == NULL)
    {
        printf("Out of memory\n");
        HeapFree(GetProcessHeap(), 0, jobInfo);
        CloseHandle(hShadowFile);
        return -1;
    }

    //
    // Go back to the start
    //
    SetFilePointer(hShadowFile, 0, NULL, SEEK_SET);

    //
    // Read the full file this time around
    //
    bRes = ReadFile(hShadowFile,
                    pShadowFileData,
                    dwNeeded,
                    &dwReturned,
                    NULL);
    if ((bRes == FALSE) || (dwReturned != dwNeeded))
    {
        printf("[-] Error reading shadow file: %lx\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, jobInfo);
        CloseHandle(hShadowFile);
        return -1;
    }

    //
    // We no longer need the shadow file
    //
    CloseHandle(hShadowFile);

    //
    // Print the data from the shadow file
    //
    printf("[+] Found Shadow File Job\n");
    printf("[.]\tJob ID: %d\n", pShadowFileData->JobId);
    printf("[.]\tQueued by: %S on %S\n",
           (PWCHAR)((ULONG_PTR)pShadowFileData + (ULONG_PTR)pShadowFileData->pUser),
           (PWCHAR)((ULONG_PTR)pShadowFileData + (ULONG_PTR)pShadowFileData->pMachineName));
    printf("[.]\tSD: %p\n", (PWCHAR)((ULONG_PTR)pShadowFileData + (ULONG_PTR)pShadowFileData->pSecurityDescriptor));
    printf("[.]\tDocument Name: %S and type: %S\n",
           (PWCHAR)((ULONG_PTR)pShadowFileData + (ULONG_PTR)pShadowFileData->pDocument),
           (PWCHAR)((ULONG_PTR)pShadowFileData + (ULONG_PTR)pShadowFileData->pDatatype));
    printf("[.]\tJob Status: %lx\n",
           pShadowFileData->Status);
    printf("[.]\tPriority: %d\n",
           pShadowFileData->Priority);
    printf("[.]\tData Size: %lld bytes (%d pages total)\n",
           (DWORD64)pShadowFileData->SizeHigh << 32ULL | pShadowFileData->Size,
           pShadowFileData->cPages);
    printf("[.]\tStart Time: %d End Time: %d\n",
            pShadowFileData->StartTime, pShadowFileData->UntilTime);
    printf("[.]\tSubmitted on %d/%d/%d at %d:%d:%d.%d\n",
           pShadowFileData->Submitted.wMonth, pShadowFileData->Submitted.wDay, pShadowFileData->Submitted.wYear,
           pShadowFileData->Submitted.wHour, pShadowFileData->Submitted.wMinute, pShadowFileData->Submitted.wSecond,
           pShadowFileData->Submitted.wMilliseconds);

    //
    // Open the print job
    //
    wsprintf(jobName, L"%s,Job %d", jobInfo->pPrinterName, jobInfo->JobId);
    bRes = OpenPrinter(jobName, &hJob, NULL);
    if (bRes == FALSE)
    {
        printf("Failed to open printer job: %lx\n", GetLastError());
        return -1;
    }

    //
    // Allocate space for the printer data
    //
    printerData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, jobInfo->Size);
    if (printerData == NULL)
    {
        printf("Out of memory\n");
        HeapFree(GetProcessHeap(), 0, jobInfo);
        ClosePrinter(hJob);
        return -1;
    }

    //
    // Read printer data
    //
    printf("[.] Reading %d bytes of data from printer\n", jobInfo->Size);
    bRes = ReadPrinter(hJob, printerData, jobInfo->Size, &dwNeeded);
    if (bRes == FALSE)
    {
        printf("Failed to read printer data: %lx\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, printerData);
        HeapFree(GetProcessHeap(), 0, jobInfo);
        ClosePrinter(hJob);
        return -1;
    }

    //
    // Print it out (assume it's a string)
    //
    printf("[+] Printer Data: %s\n", printerData);

    //
    // All done here
    //
    HeapFree(GetProcessHeap(), 0, printerData);
    HeapFree(GetProcessHeap(), 0, jobInfo);
    ClosePrinter(hJob);
    return 0;
}

