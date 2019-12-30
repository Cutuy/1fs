// PrjFsProviderTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <projectedfslib.h>
#include "PrjFsProvider.h"

#pragma comment(lib, "ProjectedFSLib.lib")

#define SrcName LR"(B:\content\)"
#define DstName LR"(A:\root\)"

const GUID instanceId = { 0xA2299C9C, 0x7832, 0x4CBA, {0xA0, 0x22, 0x60, 0x75, 0x2A, 0x7E, 0x3E, 0x7F} };
//const GUID instanceId = { 0xA2299C8C, 0x7832, 0x4CBA, {0xA0, 0x22, 0x60, 0x75, 0x2A, 0x7E, 0x3E, 0x7F} };

PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT instanceHandle;
PRJ_CALLBACKS callbackTable2 = {
    MyStartEnumCallback,
    MyEndEnumCallback,
    MyGetEnumCallback,
    MyGetPlaceholderCallback,
    MyGetFileDataCallback,
    MyQueryFileNameCallback,
    MyNotificationCallback,
    nullptr
};
extern PrjFsSessionStore gSessStore = PrjFsSessionStore(SrcName);

int main()
{
    std::cout << "Hello World!\n";

    // Mark as placeholder, or the root would not be a reparse point
    HRESULT hr;
    hr = PrjMarkDirectoryAsPlaceholder(DstName, nullptr, nullptr, &instanceId);
    
    PRJ_NOTIFICATION_MAPPING notificationMappings[1];
    notificationMappings[0].NotificationRoot = L"";
    notificationMappings[0].NotificationBitMask = 
        PRJ_NOTIFY_PRE_RENAME | PRJ_NOTIFY_FILE_RENAMED | PRJ_NOTIFY_NEW_FILE_CREATED | PRJ_NOTIFY_PRE_DELETE | PRJ_NOTIFY_PRE_SET_HARDLINK | PRJ_NOTIFY_FILE_PRE_CONVERT_TO_FULL
        ;

    PRJ_STARTVIRTUALIZING_OPTIONS options = {};
    options.NotificationMappings = notificationMappings;
    options.NotificationMappingsCount = ARRAYSIZE(notificationMappings);

    hr = PrjStartVirtualizing(DstName,
        &callbackTable2,
        nullptr,
        &options,
        &instanceHandle);
    if (FAILED(hr)) 
    {
        printf_s("[%s] PrjStartVirtualizing failed with %ld", __func__, hr);
    }

    // TODO Entering "tracking" mode, where any file moves within src
    // will be tracked and persisted for future virtualizations.

    while (1);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
