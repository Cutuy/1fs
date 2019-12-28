#include "pch.h"
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>

#include "projfscb.h"

LPCWSTR srcName = LR"(B:\content\)";
wchar_t pathBuff[256];

GUID lastSession;
BOOL isNewSession;
BOOL isGetEnumComplete;
wchar_t lastFilePath[256];


void _BuildFilePath(PCWSTR relativePath)
{
	memset(pathBuff, 0, sizeof(pathBuff));
	lstrcatW(pathBuff, srcName);
	if (lstrlenW(relativePath))
	{
		lstrcatW(pathBuff, relativePath);
	}
	lstrcatW(pathBuff, L"\\\*");
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
	if (0 != memcmp(&lastSession, enumerationId, sizeof(GUID)))
	{
		isNewSession = true;
		memcpy(&lastSession, enumerationId, sizeof(GUID));
	}
	else
	{
		isNewSession = false;
	}

	return S_OK;
}

HRESULT MyEndEnumCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData,
	_In_ const GUID* enumerationId
)
{
	isNewSession = false;
	isGetEnumComplete = false;
	return S_OK;
}

HRESULT MyGetEnumCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData,
	_In_ const GUID* enumerationId,
	_In_opt_ PCWSTR searchExpression,
	_In_ PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle
)
{
	if (isGetEnumComplete)
	{
		return S_OK;
	}
	BOOL isEnumFile = (callbackData->Flags & PRJ_CB_DATA_FLAG_ENUM_RETURN_SINGLE_ENTRY) != 0;

	/*
	if (nullptr != searchExpression && PrjDoesNameContainWildCards(searchExpression))
	{
		throw ERROR_INVALID_DATA;
	}
	*/
	LPCWSTR filePath = callbackData->FilePathName;
	if (isNewSession || (callbackData->Flags & PRJ_CB_DATA_FLAG_ENUM_RESTART_SCAN) != 0)
	{
		memset(lastFilePath, 0, sizeof(lastFilePath));
		if (nullptr != filePath)
		{
			wmemcpy(lastFilePath, filePath, max(lstrlenW(filePath), lstrlenW(lastFilePath)));
		}
	}

	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA ffd;

	_BuildFilePath(filePath);
	hFind = FindFirstFile(pathBuff, &ffd);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		throw ERROR_INVALID_HANDLE;
	}

	HRESULT hr;
	DWORD dwError = 0;

	do
	{
		if (searchExpression == nullptr || PrjFileNameMatch(ffd.cFileName, searchExpression))
		{
			PRJ_FILE_BASIC_INFO fileBasicInfo = {};
			fileBasicInfo.IsDirectory = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			fileBasicInfo.FileSize = (((INT64)ffd.nFileSizeHigh) << 32) | ffd.nFileSizeLow;
			fileBasicInfo.FileAttributes = ffd.dwFileAttributes;

			if (S_OK != (hr = PrjFillDirEntryBuffer(ffd.cFileName, &fileBasicInfo, dirEntryBufferHandle))) {
				// return hr;
				// Workarounds...
				OutputDebugStringW(L"GG my friend in PrjFillDirEntryBuffer");
				isGetEnumComplete = true;
				return S_OK;
			}
			else if (isEnumFile)
			{
				isGetEnumComplete = true;
				return S_OK;
			}
		}
		else
		{
			OutputDebugStringW(L"unmatched ");
			OutputDebugStringW(filePath);
			OutputDebugStringW(L"\t");
			OutputDebugStringW(ffd.cFileName);
			OutputDebugStringW(L"\n");
		}
	} while (FindNextFile(hFind, &ffd) != 0);

	dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES)
	{
		throw ERROR;
	}

	isGetEnumComplete = true;

	return S_OK;
}

HRESULT MyGetPlaceholderCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData
)
{
	LPCWSTR filePath = callbackData->FilePathName;
	BOOL isFileFound = false;

	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA ffd;

	_BuildFilePath(lastFilePath);
	hFind = FindFirstFile(pathBuff, &ffd);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		throw ERROR_INVALID_HANDLE;
	}

	DWORD dwError = 0;

	do
	{
		
		memset(pathBuff, 0, sizeof(pathBuff));
		lstrcatW(pathBuff, lastFilePath);
		lstrcatW(pathBuff, L"\\");
		lstrcatW(pathBuff, ffd.cFileName);
		BOOL isFileSame = (0 == PrjFileNameCompare(filePath, pathBuff));

		if (isFileSame)
		{
			PRJ_FILE_BASIC_INFO fileBasicInfo = {};
			fileBasicInfo.IsDirectory = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			fileBasicInfo.FileSize = (((INT64)ffd.nFileSizeHigh) << 32) | ffd.nFileSizeLow;
			fileBasicInfo.FileAttributes = ffd.dwFileAttributes;

			PRJ_PLACEHOLDER_INFO placeholderInfo = {};
			placeholderInfo.FileBasicInfo = fileBasicInfo;

			PrjWritePlaceholderInfo(callbackData->NamespaceVirtualizationContext,
				filePath,
				&placeholderInfo,
				sizeof(placeholderInfo));
			isFileFound = true;
			break;
		}
	} while (FindNextFile(hFind, &ffd) != 0);

	if (!isFileFound)
	{
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}

	dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES)
	{
		return ERROR;
	}

	return S_OK;
}

HRESULT MyGetFileDataCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData,
	_In_ UINT64 byteOffset,
	_In_ UINT32 length
)
{


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

	OVERLAPPED ol = { 0 };
	HANDLE hFile;
	LPCWSTR filePath = callbackData->FilePathName;
	memset(pathBuff, 0, sizeof(pathBuff));
	lstrcatW(pathBuff, srcName);
	lstrcatW(pathBuff, filePath);
	hFile = CreateFile(pathBuff,               // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // normal file
		NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		PrjFreeAlignedBuffer(rwBuffer);
		return ERROR_OPEN_FAILED;
	}
	if (FALSE == ReadFileEx(hFile, rwBuffer, length, &ol, FileIOCompletionRoutine))
	{
		return ERROR;
		CloseHandle(hFile);
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