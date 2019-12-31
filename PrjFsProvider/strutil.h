#pragma once
#include <windows.h>
#include <tchar.h> 

#include "const.h"

BOOL EndsWith(LPCWSTR str, LPCWSTR suffix);

/*
	Compare the path depth difference
	return false if paths not comparable (partial order cannot established)
	out pLess: depth(lpcSub) - depth(lpcObj)
*/
BOOL lpathcmpW(__in LPCWSTR lpcSub, __in LPCWSTR lpcObj, __out INT* pLess);