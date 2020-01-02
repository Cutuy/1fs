// PrjFsProvider.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>
#include <assert.h>

#include "strutil.h"
#include "PrjFsProvider.h"

extern PrjFsSessionStore gSessStore;



#ifdef __DIRECTORY_WORKAROUND__
inline BOOL IsShadowFile(LPCWSTR fileName)
{
	return EndsWith(fileName, SHADOW_FILE_SUFFIX);
}
#endif


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

int PrjFsSessionStore::AddRemap(PCWSTR from, PCWSTR to)
{
	// Let's forget the loops & priorities for remaps now; assuming they do not have conflict of scopes at all
	PrjFsMap map(from, to);

	this->remaps.push_back(map);
	// TODO check dup
	// printf_s("[%s] key %ls already exists\n", __func__, from);
	this->AddRepath(to, from);
	return 0;
}

void PrjFsSessionStore::ReplayProjections(
	__in PCWSTR dir, 
	__out std::vector<LPCWSTR> *inclusions,
	__out std::vector<LPCWSTR> *exclusions
)
{
	for (auto it = this->remaps.begin(); it != this->remaps.end(); ++it)
	{
		PrjFsMap proj = *it;
		BOOL hr;
		
		int less;

		if (hr = lpathcmpW(proj.ToPath, dir, &less))
		{
			if (less <= 0)
			{
				// add to repath dict 
				//this->AddRepath(proj.ToPath, proj.FromPath);
				
			}
			else if (1 == less)
			{
				wchar_t pathBuff[PATH_BUFF_LEN];
				GetPathLastComponent(proj.ToPath, pathBuff);
				inclusions->push_back(pathBuff);
				// add to repath dict 
				//this->AddRepath(proj.ToPath, proj.FromPath);
			}
		}
		// no else!
		if (hr = lpathcmpW(proj.FromPath, dir, &less))
		{
			if (1 == less)
			{
				wchar_t pathBuff[PATH_BUFF_LEN];
				GetPathLastComponent(proj.FromPath, pathBuff);
				exclusions->push_back(pathBuff);
				// ignore (since the repath will be added by rule #1 or #2 at some dir)
			}
		}
	}
	return;
}

void PrjFsSessionStore::AddRepath(LPCWSTR virtPath, LPCWSTR possiblePhysPath)
{
	// This function ensures any repath, at time of insertion, is "fully expanded"
	// to its oldest path possible; and updates if an old virtPath already exists.
	// This results in values of repaths are always physical paths

	if (nullptr == virtPath || 0 == lstrlenW(virtPath))
	{
		printf_s("[%s] virtPath cannot be empty\n", __func__);
		return;
	}
	
	INT less;
	PrjFsMap map(virtPath, possiblePhysPath); // force mem alloc

	// Check dup
	if (this->repaths.count(std::wstring(map.FromPath)))
	{
		printf_s("[%s] Key %ls will be replaced\n", __func__, map.FromPath);
	}

	// Expand (theoretically a new repath should expand at most once)
	for (auto it = this->repaths.begin(); it != this->repaths.end(); ++it)
	{
		std::pair<std::wstring, std::wstring> pair = *it;
		// If the newer toPath is deeper or equal to the older virtPath
		if (lpathcmpW(map.ToPath, pair.first.data(), &less))
		{
			if (less < 0)
			{
				printf_s("[%s] Possible illegal repath insertion due to collision of\n \
				(new) %ls -> %ls\n \
				(old) %ls -> %ls\n",
				__func__, map.FromPath, map.ToPath, pair.first, pair.second);
				return;
			}
			else
			{
				// Replace the prefix (pair.first) part of map.ToPath to pair.second
				// Then set it to map.ToPath
				ReplacePrefix(pair.first.data(), pair.second.data(), map.ToPath);
				goto insert_one;
				// Just to make sure...
				break;
			}
		}
	}
insert_one:
	// Insert, may overwrite existing key
	this->repaths[std::wstring(map.FromPath)] = std::wstring(map.ToPath);
	return;
}

void PrjFsSessionStore::GetRepath(__in LPCWSTR virtPath, __out LPWSTR physPath)
{
	INT least = 0;
	lstrcpyW(physPath, virtPath);

	INT less;
	for (auto it = this->repaths.begin(); it != this->repaths.end(); ++it)
	{
		auto remap = *it;
		if (lpathcmpW(remap.first.data(), virtPath, &less))
		{
			if (less <= least)
			{
				least = less;
				ReplacePrefix(remap.first.data(), remap.second.data(), physPath);
			}
		}
	}
}

/*
	Search for includes and excludes for directory at depth 1 of directory

int PrjFsSessionStore::FilterRemaps(__in PCWSTR directory, __out std::vector<PCWSTR>* pIncludePaths, __out std::vector<PCWSTR>* pExcludePaths)
{
	auto pIncludePaths = new std::vector<PCWSTR>();
	auto pExcludePaths = new std::vector<PCWSTR>();

	// Requires directory to contain "/*"
	PCWCHAR pch = wcsstr(directory, ENTER_DIRECTORY_PATH);
	BOOL isDirectoryEntered = (nullptr != pch);
	if (!isDirectoryEntered)
	{

	}
	else
	{
		for (auto it = this->remaps.begin(); it != this->remaps.end(); ++it)
		{
			PrjFsMap map = *it;
			// when "root/b/*" is accessed...
			// Include when dir itself is projected from elsewhere
			if (lstrcmpW(directory, map.ToPath))
			{
				pIncludePaths->push_back(map.FromPath);
			}
			// Exclude (root/b/a/* or root/b/c) from directory
			if (PrjFileNameMatch(directory, map.FromPath)
				&& IsAtRootDirectory(directory, map.FromPath))// TODO remap can be "copy" or "cut"
			{
				pExcludePaths->push_back(map.FromPath);
			}
		}
	}

}
*/

LPCWSTR PrjFsSessionStore::GetRoot()
{
	return srcName;
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

	// Check repath of directory or file
	//gSessStore.GetRepath(callbackData->FilePathName);


	// Apply remaps
	std::vector<LPCWSTR> inclusions;
	std::vector<LPCWSTR> exclusions;
	gSessStore.ReplayProjections(callbackData->FilePathName, &inclusions, &exclusions);

	// Build absolute path
	wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
	swprintf_s(pathBuff, L"%ls%ls%ls", gSessStore.GetRoot(), callbackData->FilePathName, ENTER_DIRECTORY_PATH);
	// "C:\root\" -> "C:\root\folder" -> "C:\root\folder\*"

	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA ffd;
	hFind = FindFirstFile(pathBuff, &ffd);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		printf_s("[%s] Open hFile failed for %ls\n", __func__, pathBuff);
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

			hr = PrjFillDirEntryBuffer(ffd.cFileName, &fileBasicInfo, dirEntryBufferHandle);

#ifdef __DIRECTORY_WORKAROUND__
			// Create shadow file for directory
			if (fileBasicInfo.IsDirectory 
				&& 0 != lstrcmpW(ffd.cFileName, L".") 
				&& 0 != lstrcmpW(ffd.cFileName, L".."))
			{
				wmemset(pathBuff, 0, PATH_BUFF_LEN);
				swprintf_s(pathBuff, L"%ls%ls", ffd.cFileName, SHADOW_FILE_SUFFIX);
				fileBasicInfo.IsDirectory = false;
				fileBasicInfo.FileSize = 0;
				fileBasicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;
				hr = PrjFillDirEntryBuffer(pathBuff, &fileBasicInfo, dirEntryBufferHandle);

				printf_s("[%s] Shadow file created for %ls\n", __func__, pathBuff);
			}
#endif
			
			if (FAILED(hr)) {
				printf_s("[%s] PrjFillDirEntryBuffer failed %ld\n", __func__, hr);
				return hr;
			}
			else if (isSingleEntry)
			{
				goto l_getEnumComplete;
			}
		}
	} while (FindNextFile(hFind, &ffd) != 0);

	dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES)
	{
		printf_s("[%s] Unexpected last error\n", __func__);
		return ERROR;
	}

l_getEnumComplete:
	// Search into a directory will come 2 requests, the first will be singleEntry with "*" as searchExp
	// Avoid blocking the second call by testing isSingleEntry
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

	// Build absolute path
	wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
	lstrcatW(pathBuff, gSessStore.GetRoot());
	lstrcatW(pathBuff, DIRECTORY_SEP_PATH);
	lstrcatW(pathBuff, callbackData->FilePathName);

	UINT32 fileAttr = GetFileAttributes(pathBuff);

#ifdef __DIRECTORY_WORKAROUND__
	if (IsShadowFile(pathBuff))
	{
		printf_s("[%s] found shadow file %ls\n", __func__, pathBuff);
		fileAttr = FILE_ATTRIBUTE_NORMAL;
	}
#endif
	
	if (INVALID_FILE_ATTRIBUTES == fileAttr)
	{
		printf_s("[%s] file not found for %ls\n", __func__, pathBuff);
		// When renaming/moving, this cb will be called (not desired) unless
		// PRJ_QUERY_FILE_NAME_CB is provided
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

	PRJ_FILE_BASIC_INFO fileBasicInfo = {};
	fileBasicInfo.IsDirectory = (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	fileBasicInfo.FileAttributes = fileAttr;
	fileBasicInfo.FileSize = fileSize.QuadPart;
	PRJ_PLACEHOLDER_INFO placeholderInfo = {};
	placeholderInfo.FileBasicInfo = fileBasicInfo;

	PrjWritePlaceholderInfo(callbackData->NamespaceVirtualizationContext,
		callbackData->FilePathName,
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

	wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
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

HRESULT MyQueryFileNameCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData
)
{
	printf_s("[%s] %ls\n", __func__, callbackData->FilePathName);

	// Build absolute path
	wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
	lstrcatW(pathBuff, gSessStore.GetRoot());
	lstrcatW(pathBuff, DIRECTORY_SEP_PATH);
	lstrcatW(pathBuff, callbackData->FilePathName);

	UINT32 fileAttr = GetFileAttributes(pathBuff);
	if (INVALID_FILE_ATTRIBUTES == fileAttr)
	{
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}
	else
	{
		return S_OK;
	}

}

HRESULT MyNotificationCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData,
	_In_ BOOLEAN isDirectory,
	_In_ PRJ_NOTIFICATION notification,
	_In_opt_ PCWSTR destinationFileName,
	_Inout_ PRJ_NOTIFICATION_PARAMETERS* operationParameters
)
{
	printf_s("[%s] dir %d notif %ld from %ls to %ls\n", __func__, isDirectory, notification, callbackData->FilePathName, destinationFileName);
	if (notification == PRJ_NOTIFICATION_FILE_RENAMED)
	{
		if (wcslen(callbackData->FilePathName) == 0
			|| nullptr == destinationFileName 
			|| wcslen(destinationFileName) == 0
		)
		{
			return ERROR_NOT_SUPPORTED;
		}

#ifdef __DIRECTORY_WORKAROUND__
		// Note that Windows Shell does not allow folder and file has the same under
		// under same directry, whereas WinAPI supports it
		// We follow WinShell, for now...
		assert(!isDirectory);

		if (IsShadowFile(callbackData->FilePathName))
		{
			// Convert "/someDir_MOVE" to "/someDir"
			// The special workaround on acting on the remap should not be done here
			wchar_t srcBuff[PATH_BUFF_LEN] = { 0 };
			wmemcpy(srcBuff, callbackData->FilePathName, lstrlenW(callbackData->FilePathName) - lstrlenW(SHADOW_FILE_SUFFIX));
			
			wchar_t dstBuff[PATH_BUFF_LEN] = { 0 };
			wmemcpy(dstBuff, destinationFileName, lstrlenW(destinationFileName) - lstrlenW(SHADOW_FILE_SUFFIX));
			
			gSessStore.AddRemap(srcBuff, dstBuff);
			printf_s("[%s] Added remap %ls %ls\n", __func__, srcBuff, dstBuff);
		}
		else
		{
			gSessStore.AddRemap(callbackData->FilePathName, destinationFileName);
			printf_s("[%s] Added remap %ls %ls\n", __func__, callbackData->FilePathName, destinationFileName);
		}
#else
		throw ERROR_NOT_SUPPORTED;
#endif
	}
	return S_OK;
}


