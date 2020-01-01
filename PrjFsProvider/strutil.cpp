#include "pch.h"
#include <string>
#include <algorithm>

#include "strutil.h"

BOOL EndsWith(LPCWSTR str, LPCWSTR suffix)
{
	int nStr = lstrlenW(str);
	int nSuffix = lstrlenW(suffix);
	return nStr >= nSuffix && 0 == lstrcmpW(str + nStr - nSuffix, suffix);
}

void GetPathLastComponent(__in LPCWSTR path, __out LPWSTR last)
{
	std::wstring str(path);
	int i = (int)str.rfind('\\');
	if (i < 0)
	{
		lstrcpyW(last, str.substr(0).data());
	}
	else
	{
		// skipping the backslash
		lstrcpyW(last, str.substr(i + 1).data());
	}
	
	return;
}

BOOL lpathcmpW(__in LPCWSTR lpcSub, __in LPCWSTR lpcObj, __out INT* pLess)
{
	// No path shall contain "/*" or ends with "/"
	if (wcsstr(lpcSub, ENTER_DIRECTORY_PATH)
		|| wcsstr(lpcObj, ENTER_DIRECTORY_PATH)
		|| EndsWith(lpcSub, DIRECTORY_SEP_PATH)
		|| EndsWith(lpcObj, DIRECTORY_SEP_PATH))
	{
		return false;
	}

	if (0 == lstrcmpW(lpcSub, lpcObj))
	{
		*pLess = 0;
		return true;
	}

	PCWSTR pch;

	if (nullptr != (pch = wcsstr(lpcObj, lpcSub)))
	{
		if (pch == lpcObj)
		{
			std::wstring str(lpcObj + lstrlenW(lpcSub));
			// when lpcSub is empty, lpcObj will not have the leading backslash
			// when lpcSub simply prefixes lpcObj but not aligned to the last directory, str[0] will not be backslash
			if (0 == lstrlenW(lpcSub) || str.at(0) == '\\')
			{
				*pLess = -(0 == lstrlenW(lpcSub)) - (int)std::count(str.begin(), str.end(), '\\');
				return true;
			}

		}
	}
	else if (nullptr != (pch = wcsstr(lpcSub, lpcObj)))
	{
		if (pch == lpcSub)
		{
			std::wstring str(lpcSub + lstrlenW(lpcObj));
			if (0 == lstrlenW(lpcObj) || str.at(0) == '\\')
			{
				*pLess = (0 == lstrlenW(lpcObj)) + (int)std::count(str.begin(), str.end(), '\\');
				return true;
			}
		}
	}
	return false;
}

/*
BOOL IsAtRootDirectory(LPCWSTR rootDir, LPCWSTR path)
{
	// Test closure
	if (!PrjFileNameMatch(path, rootDir))
	{
		return false;
	}

	PCWCHAR pch;
	BOOL isPathFile;

	// Test rootDir
	pch = wcsstr(rootDir, ENTER_DIRECTORY_PATH);
	if (nullptr == pch)
	{
		return false;
	}

	// a/b/* <-> a/b/c, a/b/c/d (x), a/b/c/*, a/b/c/d/* (x)
	wchar_t pathBuff[PATH_BUFF_LEN];
	// copy "c", "c/d", "c/*", "c/d/*" to pathBuff
	lstrcpyW(pathBuff, path + lstrlenW(rootDir) - 1);

	isPathFile = (nullptr == (pch = wcsstr(path, ENTER_DIRECTORY_PATH)));
	pch = wcsstr(pathBuff, DIRECTORY_SEP_PATH);
	if (isPathFile)
	{
		// test if "c/d" contains "/"
		return nullptr == pch;
	}
	else
	{
		// pch now at the start of "c[/]*" or "c[/]d/*"
		pch = wcsstr(pch + 1, ENTER_DIRECTORY_PATH);
		// pch now at null or "c/d[/*]"
		return nullptr == pch;
	}
}
*/