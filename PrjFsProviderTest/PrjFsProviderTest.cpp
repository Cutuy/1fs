// PrjFsProviderTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <projectedfslib.h>
#include <assert.h>

#include "strutil.h"
#include "PrjFsProvider.h"

#pragma comment(lib, "ProjectedFSLib.lib")

#define SrcName LR"(B:\content\)"
#define DstName LR"(A:\root\)"

const GUID instanceId = { 0xA2299CAC, 0x7832, 0x4CBA, {0xA0, 0x22, 0x60, 0x75, 0x2A, 0x7E, 0x3E, 0x7F} };
//const GUID instanceId = { 0xA2299C9C, 0x7832, 0x4CBA, {0xA0, 0x22, 0x60, 0x75, 0x2A, 0x7E, 0x3E, 0x7F} };
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

void PrjFsSessionStore::TEST_ClearProjections() {
    this->repaths.clear();
    this->remaps.clear();
}

void PrjFsSessionStore::TEST_GetRepath(__in LPCWSTR virtPath, __out LPWSTR physPath)
{
    if (this->repaths.count(virtPath))
    {
        lstrcpyW(physPath, this->repaths[std::wstring(virtPath)].data());
    }
    else
    {
        lstrcpyW(physPath, virtPath);
    }
    return;
}

void testStringUtils()
{
    LPCWSTR str1 = LR"(aaa\bbbb\c\d)";
    LPCWSTR str2 = LR"(aaa\b)";
    LPCWSTR str3 = LR"(b\c\d)";
    LPCWSTR str4 = LR"(aaa\bbbb\c\d)";
    LPCWSTR str5 = LR"(aaa\bbbb\c\)";
    LPCWSTR str6 = LR"(aaa\bbbb\c)";
    LPCWSTR str7 = LR"(aaa\bbbb\d)";
    LPCWSTR str8 = LR"(a)";
    LPCWSTR str9 = LR"(aaa)";
    LPCWSTR str10 = LR"()";
    LPCWSTR str11 = LR"(b)";


    printf_s("%ls\n", L"EndsWith");
    assert(EndsWith(str1, str3));
    assert(!EndsWith(str1, str2));
    assert(EndsWith(str1, str4));
    assert(EndsWith(str4, str3));

    printf_s("%ls\n", L"lpathcmpW");
    int less;
    
    assert(lpathcmpW(str1, str1, &less));
    assert(0 == less);
    assert(!lpathcmpW(str1, str5, &less));
    assert(lpathcmpW(str1, str6, &less));
    assert(1 == less);
    assert(!lpathcmpW(str1, str8, &less));
    assert(!lpathcmpW(str8, str1, &less));
    assert(lpathcmpW(str1, str10, &less));
    assert(4 == less);
    assert(lpathcmpW(str10, str1, &less));
    assert(-4 == less);
    assert(lpathcmpW(str10, str10, &less));
    assert(0 == less);
    assert(!lpathcmpW(str8, str11, &less));

    printf_s("%ls\n", L"GetPathLastComponent");
    wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
    GetPathLastComponent(str10, pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"()"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);
    GetPathLastComponent(str9, pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(aaa)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);
    GetPathLastComponent(str7, pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(d)"));
    while (1);
}

void testProjectionReplays()
{
    gSessStore.AddRemap(LR"(a\g\h)", LR"(a\j)");
    gSessStore.AddRemap(LR"(a\f)", LR"(a\j\p)");
    std::vector<LPCWSTR> inclusions;
    std::vector<LPCWSTR> exclusions;
    // view at a
    gSessStore.ReplayProjections(LR"(a)", &inclusions, &exclusions);
    assert(0 == lstrcmpW(inclusions.at(0), LR"(j)"));
    assert(0 == lstrcmpW(exclusions.at(0), LR"(f)"));
    inclusions.clear();
    exclusions.clear();
    // view at j
    gSessStore.ReplayProjections(LR"(a\j)", &inclusions, &exclusions);
    assert(0 == lstrcmpW(inclusions.at(0), LR"(p)"));
    assert(exclusions.size() == 0);
    inclusions.clear();
    exclusions.clear();
    // view at p
    gSessStore.ReplayProjections(LR"(a\j\p)", &inclusions, &exclusions);
    assert(inclusions.size() == 0);
    assert(exclusions.size() == 0);
    inclusions.clear();
    exclusions.clear();

    gSessStore.TEST_ClearProjections();
}

void testRepathExpansions()
{

}

int main()
{
    std::cout << "Hello World!\n";

    //testStringUtils();
    testProjectionReplays();

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
    
    /*
    Sleep(2000);
    wchar_t src[PATH_BUFF_LEN] = LR"(A:\root\f1-visa-renewal\test)";
    wchar_t dst[PATH_BUFF_LEN] = LR"(A:\root\test)";

    if (!MoveFileEx(src, dst, MOVEFILE_WRITE_THROUGH))
    {
        printf("MoveFileEx failed with error %d\n", GetLastError());
    }
    */

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
