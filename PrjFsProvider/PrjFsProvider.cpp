// PrjFsProvider.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>
#include <assert.h>

#include "PrjFsProvider.h"

extern PrjFsSessionStore gSessStore;

PrjFsSessionStore::PrjFsSessionStore(LPCWSTR root): srcName(root)
{
}

LPPrjFsSessionRuntime PrjFsSessionStore::GetSession(LPCGUID lpcGuid)
{
	for (auto it = this->sessions.begin(); it != this->sessions.end(); ++it)
	{
		PrjFsSessionRuntime* sess = *it;
		if (0 == memcmp(&sess->EnumId, lpcGuid, sizeof(GUID)))
		{
			return sess;
		}
	}
	printf_s("[%s] session not found, creating new\n", __func__);

	PrjFsSessionRuntime *rt = new PrjFsSessionRuntime();
	memcpy(&rt->EnumId, lpcGuid, sizeof(GUID));
	this->sessions.push_back(rt);

	return rt;
}

void PrjFsSessionStore::FreeSession(LPCGUID lpcGuid)
{
	for (auto it = this->sessions.begin(); it != this->sessions.end(); ++it)
	{
		PrjFsSessionRuntime* sess = *it;
		if (0 == memcmp(&sess->EnumId, lpcGuid, sizeof(GUID)))
		{
			delete *it;
			this->sessions.erase(it);
			break;
		}
	}
}

LPCWSTR PrjFsSessionStore::GetRoot()
{
	return srcName;
}

void PrjFsSessionRuntime::ClearPathBuff()
{
	memset(this->_mPathBuff, 0, PATH_BUFF_LEN);
}

void PrjFsSessionRuntime::AppendPathBuff(PCWSTR relPath)
{
	if (lstrlenW(relPath))
	{
		assert(lstrlenW(relPath) + lstrlenW(this->_mPathBuff) <= PATH_BUFF_LEN);
		lstrcatW(this->_mPathBuff, relPath);
	}
}

LPCWSTR PrjFsSessionRuntime::GetPath()
{
	return this->_mPathBuff;
}


VOID CALLBACK FileIOCompletionRoutine(
	__in  DWORD dwErrorCode,
	__in  DWORD dwNumberOfBytesTransfered,
	__in  LPOVERLAPPED lpOverlapped)
{
	_tprintf(TEXT("Error code:\t%x\n"), dwErrorCode);
	_tprintf(TEXT("Number of bytes:\t%x\n"), dwNumberOfBytesTransfered);
}

HRESULT MyStartEnumCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData,
	_In_ const GUID* enumerationId
)
{
	gSessStore.GetSession(enumerationId);
	return S_OK;
}

HRESULT MyEndEnumCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData,
	_In_ const GUID* enumerationId
)
{
	gSessStore.FreeSession(enumerationId);
	return S_OK;
}

HRESULT MyGetEnumCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData,
	_In_ const GUID* enumerationId,
	_In_opt_ PCWSTR searchExpression,
	_In_ PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle
)
{
	printf_s("[%s] filePath %ls, searchExp %ls, flag %d\n", __func__, callbackData->FilePathName, searchExpression, callbackData->Flags);
	LPPrjFsSessionRuntime lpSess = gSessStore.GetSession(enumerationId);
	
	if ((callbackData->Flags & PRJ_CB_DATA_FLAG_ENUM_RESTART_SCAN) != 0)
	{
		lpSess->IsGetEnumComplete = false;
	}

	if (lpSess->IsGetEnumComplete)
	{
		return S_OK;
	}

	BOOL isSingleEntry = (callbackData->Flags & PRJ_CB_DATA_FLAG_ENUM_RETURN_SINGLE_ENTRY) != 0;
	// fileName will be in searchExp

	/*
	if (nullptr != searchExpression && PrjDoesNameContainWildCards(searchExpression))
	{
		throw ERROR_INVALID_DATA;
	}
	*/

	// Build absolute path
	lpSess->ClearPathBuff();
	lpSess->AppendPathBuff(gSessStore.GetRoot()); // "C:\root\"
	lpSess->AppendPathBuff(callbackData->FilePathName); // "C:\root\folder"
	lpSess->AppendPathBuff(ENTER_DIRECTORY_PATH); // "C:\root\folder\*"

	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA ffd;
	hFind = FindFirstFile(lpSess->GetPath(), &ffd);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		printf_s("[%s] Open hFile failed for %ls\n", __func__, lpSess->GetPath());
		return ERROR_INVALID_HANDLE;
	}

	HRESULT hr;
	DWORD dwError = 0;

	do
	{
		if (nullptr == searchExpression || PrjFileNameMatch(ffd.cFileName, searchExpression))
		{
			PRJ_FILE_BASIC_INFO fileBasicInfo = {};
			fileBasicInfo.IsDirectory = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			fileBasicInfo.FileSize = (((INT64)ffd.nFileSizeHigh) << 32) | ffd.nFileSizeLow;
			fileBasicInfo.FileAttributes = ffd.dwFileAttributes;

			if (S_OK != (hr = PrjFillDirEntryBuffer(ffd.cFileName, &fileBasicInfo, dirEntryBufferHandle))) {
				printf_s("[%s] PrjFillDirEntryBuffer failed %ld\n", __func__, hr);
				return hr;
			}
			else if (isSingleEntry)
			{
				goto l_getEnumComplete;
			}
		}
		else
		{
			printf_s("[%s] No match found for %ls, %ls\n", __func__, lpSess->GetPath(), searchExpression);
		}
	} while (FindNextFile(hFind, &ffd) != 0);

	dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES)
	{
		printf_s("[%s] Unexpected last error\n", __func__);
		return ERROR;
	}

l_getEnumComplete:
	if (!isSingleEntry)
	{
		lpSess->IsGetEnumComplete = true;
	}

	return S_OK;
}

HRESULT MyGetPlaceholderCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData
)
{
	printf_s("[%s] filePath %ls\n", __func__, callbackData->FilePathName);

	LPCWSTR filePath = callbackData->FilePathName;
	BOOL isFileFound = false;

	// Build absolute path
	LPWSTR pathBuff = new wchar_t[PATH_BUFF_LEN];
	memset(pathBuff, 0, PATH_BUFF_LEN);
	lstrcatW(pathBuff, gSessStore.GetRoot());
	lstrcatW(pathBuff, DIRECTORY_SEP_PATH);
	lstrcatW(pathBuff, callbackData->FilePathName);
	
	UINT32 fileAttr = GetFileAttributes(pathBuff);
	if (INVALID_FILE_ATTRIBUTES == fileAttr)
	{
		printf_s("[%s] file not found for %ls\n", __func__, pathBuff);
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}

	HANDLE hFile;
	hFile = CreateFile(pathBuff,               // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // normal file
		NULL);
	LARGE_INTEGER fileSize;
	if (hFile == INVALID_HANDLE_VALUE || !GetFileSizeEx(hFile, &fileSize))
	{
		printf_s("[%s] file size failed to get\n", __func__);
		fileSize.LowPart = 0;
		fileSize.HighPart = 0;
	}
	if (hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
	}

	delete[] pathBuff;

	PRJ_FILE_BASIC_INFO fileBasicInfo = {};
	fileBasicInfo.IsDirectory = (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	fileBasicInfo.FileAttributes = fileAttr;
	fileBasicInfo.FileSize = fileSize.QuadPart;
	PRJ_PLACEHOLDER_INFO placeholderInfo = {};
	placeholderInfo.FileBasicInfo = fileBasicInfo;

	PrjWritePlaceholderInfo(callbackData->NamespaceVirtualizationContext,
		filePath,
		&placeholderInfo,
		sizeof(placeholderInfo));

	return S_OK;
}

HRESULT MyGetFileDataCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData,
	_In_ UINT64 byteOffset,
	_In_ UINT32 length
)
{
	printf_s("[%s] filePath %ls length %lu\n", __func__, callbackData->FilePathName, length);
	
	if (0 != byteOffset)
	{
		return ERROR_IMPLEMENTATION_LIMIT;
	}

	void* rwBuffer;
	rwBuffer = PrjAllocateAlignedBuffer(callbackData->NamespaceVirtualizationContext, length);
	if (rwBuffer == NULL)
	{
		return E_OUTOFMEMORY;
	}

	LPWSTR pathBuff = new wchar_t[PATH_BUFF_LEN];
	memset(pathBuff, 0, PATH_BUFF_LEN);
	lstrcatW(pathBuff, gSessStore.GetRoot());
	lstrcatW(pathBuff, DIRECTORY_SEP_PATH);
	lstrcatW(pathBuff, callbackData->FilePathName);

	
	HANDLE hFile;

	hFile = CreateFile(pathBuff,               // file to open
		GENERIC_READ | GENERIC_WRITE,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // normal file
		NULL);
	
	delete[] pathBuff;

	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf_s("[%s] CreateFile failed with\n", __func__);
		PrjFreeAlignedBuffer(rwBuffer);
		return ERROR_OPEN_FAILED;
	}

	OVERLAPPED ol = { 0 };
	if (FALSE == ReadFileEx(hFile, rwBuffer, length, &ol, FileIOCompletionRoutine))
	{
		printf_s("[%s] ReadFileEx failed\n", __func__);
		CloseHandle(hFile);
		return ERROR;
	};

	HRESULT hr;
	hr = PrjWriteFileData(callbackData->NamespaceVirtualizationContext,
		&callbackData->DataStreamId,
		rwBuffer,
		byteOffset,
		length);
	PrjFreeAlignedBuffer(rwBuffer);
	return hr;
}



