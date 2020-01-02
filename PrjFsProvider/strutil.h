#pragma once
#include <windows.h>
#include <tchar.h> 

#include "const.h"

BOOL EndsWith(LPCWSTR str, LPCWSTR suffix);

#ifdef __DIRECTORY_WORKAROUND__
BOOL IsShadowFile(LPCWSTR fileName);
#endif

// This function does not guarantee a valid path compare
void ReplacePrefix(__in LPCWSTR prefixToReplace, __in LPCWSTR prefixToSet, __inout LPWSTR inOutBuff);

void GetPathLastComponent(__in LPCWSTR path, __out LPWSTR last);

/*
	Compare the path depth difference
	return false if paths not comparable (partial order cannot established)
	out pLess: depth(lpcSub) - depth(lpcObj)
*/
BOOL lpathcmpW(__in LPCWSTR lpcSub, __in LPCWSTR lpcObj, __out INT* pLess);