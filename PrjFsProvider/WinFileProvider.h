#pragma once
#include <windows.h>
#include <tchar.h> 
#include <projectedfslib.h>
#include <vector>
#include "PrjFsProvider.h"
#include "const.h"

HRESULT winFileScan
(
	__in LPCWSTR lpcPhysAbs,
	__out PRJ_FILE_BASIC_INFO* lpFile
);

HRESULT winFileDirScan
(
	__in LPCWSTR lpcPhysDirAbsEntered,
	__in LPCWSTR searchExpression,
	__in BOOL isSingleEntry,
	__inout PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle,
	__inout LPPrjFsSessionRuntime lpSess,
	__out std::map<std::wstring, PRJ_FILE_BASIC_INFO>* lpFiles
);