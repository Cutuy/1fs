// PrjFsProviderTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <projectedfslib.h>
#include <assert.h>

#include "strutil.h"
#include "PrjFsProvider.h"

#pragma comment(lib, "ProjectedFSLib.lib")



PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT instanceHandle;
PRJ_CALLBACKS callbackTable2 = {
    MyStartEnumCallback,
    MyEndEnumCallback,
    MyGetEnumCallback,
    MyGetPlaceholderCallback,
    MyGetFileDataCallback,
    nullptr, //MyQueryFileNameCallback,
    MyNotificationCallback,
    nullptr
};
extern PrjFsSessionStore gSessStore = PrjFsSessionStore(SrcName);

void PrjFsSessionStore::TEST_ClearProjections() {
    this->repaths.clear();
    this->remaps.clear();
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
}

#define CLEAR_VIEW inclusions.clear(); exclusions.clear(); 
#define CLEAR_CASE gSessStore.TEST_ClearProjections();

#define VIEW_AT(VIEW_POINT)  gSessStore.ReplayProjections(VIEW_POINT, inclusions, exclusions)

void testProjectionReplays()
{
    std::vector<std::wstring> inclusions;
    std::vector<std::wstring> exclusions;

#pragma region Case 1
    CLEAR_CASE;
    gSessStore.AddRemap(LR"(a\g\h)", LR"(a\j)");
    gSessStore.AddRemap(LR"(a\f)", LR"(a\j\p)");

    // view at a
    CLEAR_VIEW;
    VIEW_AT(LR"(a)");
    assert(0 == lstrcmpW(inclusions.at(0).data(), LR"(j)"));
    assert(0 == lstrcmpW(exclusions.at(0).data(), LR"(f)"));
    // view at j
    CLEAR_VIEW;
    VIEW_AT(LR"(a\j)");
    assert(0 == lstrcmpW(inclusions.at(0).data(), LR"(p)"));
    assert(exclusions.size() == 0);
    // view at p
    CLEAR_VIEW;
    VIEW_AT(LR"(a\j\p)");
    assert(inclusions.size() == 0);
    assert(exclusions.size() == 0);
    // view at root
    CLEAR_VIEW;
    VIEW_AT(LR"()");
    assert(inclusions.size() == 0);
    assert(exclusions.size() == 0);
    // view at arbitrary invalid paths
    CLEAR_VIEW;
    VIEW_AT(LR"(a\m)");
    assert(inclusions.size() == 0);
    assert(exclusions.size() == 0);
    // view at g
    CLEAR_VIEW;
    VIEW_AT(LR"(a\g)");
    assert(inclusions.size() == 0);
    assert(0 == lstrcmpW(exclusions.at(0).data(), LR"(h)"));
#pragma endregion
#pragma region Case 2
    // A bug found while in dev - the TIME of rule taking effect matters!
    // Case 2
    CLEAR_CASE;
    gSessStore.AddRemap(LR"(f0)", LR"(f1)");
    gSessStore.AddRemap(LR"(f1\p)", LR"(f1\q)");
    gSessStore.AddRemap(LR"(f1)", LR"(f2)");

    // view at f2
    CLEAR_VIEW;
    VIEW_AT(LR"(f2)");
    assert(0 == lstrcmpW(inclusions.at(0).data(), LR"(q)"));
    assert(0 == lstrcmpW(exclusions.at(0).data(), LR"(p)"));
    assert(!gSessStore.IsVirtPathValid(LR"(f1)"));
    assert(!gSessStore.IsVirtPathValid(LR"(f1\p)"));
    assert(!gSessStore.IsVirtPathValid(LR"(f1\q)"));
#pragma endregion
#pragma region Case 3
    // Another bug... self cancelling rules
    // This case reveals that the redesigned remap
    // should be both ordered chronologically as well as unique in FromPath
    CLEAR_CASE;
    gSessStore.AddRemap(LR"(f0\p)", LR"(p)");
    gSessStore.AddRemap(LR"(p)", LR"(f0\p)");
    
    // view at root
    CLEAR_VIEW;
    VIEW_AT(LR"()");
    assert(inclusions.size() == 0);
#pragma endregion
    CLEAR_CASE;
}

void testUltimateBoss()
{
    wchar_t pathBuff[PATH_BUFF_LEN];

    CLEAR_CASE;
    gSessStore.AddRemap(LR"(x)", LR"(a)");
    gSessStore.AddRemap(LR"(q\b)", LR"(a\b)");

    gSessStore.GetRepath(LR"(a)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(x)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\b)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(q\b)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    assert(!gSessStore.IsVirtPathValid(LR"(x\b)"));

    CLEAR_CASE;
    gSessStore.AddRemap(LR"(q\b)", LR"(a\b)");
    gSessStore.AddRemap(LR"(x)", LR"(a)");

    gSessStore.GetRepath(LR"(a)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(x)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\b)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(x\b)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    assert(gSessStore.IsVirtPathValid(LR"(q)"));
    assert(!gSessStore.IsVirtPathValid(LR"(q\b)"));
    assert(!gSessStore.IsVirtPathValid(LR"(x)"));
    assert(!gSessStore.IsVirtPathValid(LR"(x\b)"));
    assert(gSessStore.IsVirtPathValid(LR"(a)"));
    assert(gSessStore.IsVirtPathValid(LR"(a\b)"));
    assert(gSessStore.IsVirtPathValid(LR"(a\b\c)"));
    assert(gSessStore.IsVirtPathValid(LR"(a\b\c\d)"));

    gSessStore.AddRemap(LR"(a\b)", LR"(b)");
    assert(gSessStore.IsVirtPathValid(LR"(b)"));
    assert(!gSessStore.IsVirtPathValid(LR"(a\b)"));

    CLEAR_CASE;
}

void testRepathExpansions()
{
    // AddRemap(oldAddress, newAddress)
    // repath: given newAddress, get oldAddress

    // Repath only handles path translation
    // without knowing inclusions/exclusions!
    printf_s("[%s]\n", __func__);
    gSessStore.AddRemap(LR"(a\g\h)", LR"(a\j)");
    gSessStore.AddRemap(LR"(a\f)", LR"(a\j\p)");
    gSessStore.AddRemap(LR"(a\j)", LR"(a\k)");
    gSessStore.AddRemap(LR"(a\j\c)", LR"(a\g\c)");
    gSessStore.AddRemap(LR"(a\m)", LR"(a\k\q)");

    wchar_t pathBuff[PATH_BUFF_LEN] = {0};

    gSessStore.GetRepath(LR"()", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"()"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);
    
    gSessStore.GetRepath(LR"(a)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(b)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(b)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\b)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\b)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);
    
    gSessStore.GetRepath(LR"(a\g\h)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\g\h)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\k)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\g\h)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\k\x)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\g\h\x)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\j\x)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\g\h\x)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\g)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\g)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\g\c)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\g\h\c)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\g\c\d)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\g\h\c\d)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\g\d)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\g\d)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\kkk)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\kkk)"));
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    gSessStore.GetRepath(LR"(a\k\q\d)", pathBuff);
    assert(0 == lstrcmpW(pathBuff, LR"(a\m\d)")); // yes!
    wmemset(pathBuff, 0, PATH_BUFF_LEN);

    CLEAR_CASE;
}

void applyTestRepaths()
{
    /*
    a\a.c\a.c.u     -> b\a.c.u
    b\b.k           -> a\a.c\b.k
    a\a.c\b.k\b.k.g -> b.k.g
    a               -> b.k.g\a

    b\a.c.u         -> a.c.u
    a.c.u           -> b\a.c.u
    b               -> b.k.g\b
    */
    gSessStore.AddRemap(LR"(a\a.c\a.c.u)", LR"(b\a.c.u)");
    gSessStore.AddRemap(LR"(b\b.k_MOVE)", LR"(a\a.c\b.k_MOVE)");
    gSessStore.AddRemap(LR"(b\b.k)", LR"(a\a.c\b.k)");
    gSessStore.AddRemap(LR"(a\a.c\b.k\b.k.g_MOVE)", LR"(b.k.g_MOVE)");
    gSessStore.AddRemap(LR"(a\a.c\b.k\b.k.g)", LR"(b.k.g)");
    gSessStore.AddRemap(LR"(a_MOVE)", LR"(b.k.g\a_MOVE)");
    gSessStore.AddRemap(LR"(a)", LR"(b.k.g\a)");
    /*
    gSessStore.AddRemap(LR"(b\a.c.u)", LR"(a.c.u)");
    gSessStore.AddRemap(LR"(a.c.u)", LR"(b\a.c.u)");
    gSessStore.AddRemap(LR"(b_MOVE)", LR"(b.k.g\b_MOVE)");
    gSessStore.AddRemap(LR"(b)", LR"(b.k.g\b)");
    */
}


int main()
{
    std::cout << "Hello World!\n";
    HRESULT hr;

    //testStringUtils();
    testProjectionReplays();
    testRepathExpansions();
    testUltimateBoss();

    //applyTestRepaths();

    GUID instanceId;
    hr = CoCreateGuid(&instanceId);
    if (FAILED(hr))
    {
        printf_s("[%s] GUID creation failed\n", __func__);
        return hr;
    }

    // Mark as placeholder, or the root would not be a reparse point
    hr = PrjMarkDirectoryAsPlaceholder(DstName, nullptr, nullptr, &instanceId);
    if (FAILED(hr))
    {
        printf_s("[%s] Failed to mark virtualization root (0x%08x)\n", __func__, hr);
        return hr;
    }
    
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
        return hr;
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
