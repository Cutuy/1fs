// PrjFsProvider.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>
#include <assert.h>
#include <algorithm>

#include "strutil.h"
#include "PrjFsProvider.h"
#include "WinFileProvider.h"

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
	// Erase while in enumeration
	this->sessions.erase(
		std::remove_if(
			this->sessions.begin(),
			this->sessions.end(),
			[lpcGuid](PrjFsSessionRuntime* const& sess) {
				if (0 == memcmp(&sess->EnumId, lpcGuid, sizeof(GUID)))
				{
					delete sess;
					return true;
				}
				else
				{
					return false;
				}
			}
		),
		this->sessions.end()
	);
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
	__in PCWSTR virtDir, 
	__out std::vector<std::wstring> *virtInclusions,
	__out std::vector<std::wstring> *virtExclusions
)
{
	for (auto it = this->remaps.begin(); it != this->remaps.end(); ++it)
	{
		PrjFsMap proj = *it;
		BOOL hr;
		
		int less;

		if (hr = lpathcmpW(proj.ToPath, virtDir, &less))
		{
			if (less <= 0)
			{
				// nop
			}
			else if (1 == less)
			{
				wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
				GetPathLastComponent(proj.ToPath, pathBuff);

				std::vector<std::wstring>::iterator it_f;
				if (virtExclusions->end() != (it_f = std::find(virtExclusions->begin(), virtExclusions->end(), pathBuff)))
				{
					virtExclusions->erase(it_f);
				}
				virtInclusions->push_back(pathBuff);
			}
		}
		// no else!
		if (hr = lpathcmpW(proj.FromPath, virtDir, &less))
		{
			if (1 == less)
			{
				wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
				GetPathLastComponent(proj.FromPath, pathBuff);

				std::vector<std::wstring>::iterator it_f;
				if (virtInclusions->end() != (it_f = std::find(virtInclusions->begin(), virtInclusions->end(), pathBuff)))
				{
					virtInclusions->erase(it_f);
				}
				virtExclusions->push_back(pathBuff);
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
				__func__, map.FromPath, map.ToPath, pair.first.data(), pair.second.data());
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
	INT most = MININT;
	lstrcpyW(physPath, virtPath);

	// Find and replace by the repath of the closest parent (or self)
	// Do not change physPath while in loop, otherwise it may affect later iterations
	INT less;
	std::pair<std::wstring, std::wstring> repath;
	for (auto it = this->repaths.begin(); it != this->repaths.end(); ++it)
	{
		if (lpathcmpW(it->first.data(), virtPath, &less))
		{
			// If is parent and is the most closest so far
			if (less <= 0 && less >= most)
			{
				most = less;
				repath.first = it->first;
				repath.second = it->second;
			}
		}
	}
	if (most <= 0)
	{
		ReplacePrefix(repath.first.data(), repath.second.data(), physPath);
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

	LPCWSTR virtDir = callbackData->FilePathName;

	// key: virtFile | virtDirectory
	std::map<std::wstring, PRJ_FILE_BASIC_INFO> files;

	// Find files by ascending priority

	// Fill files by NTFS
	HRESULT hr;

	wchar_t physDir[PATH_BUFF_LEN] = { 0 };
	wchar_t physDirAbsEntered[PATH_BUFF_LEN] = { 0 };
	gSessStore.GetRepath(virtDir, physDir);
	swprintf_s(physDirAbsEntered, L"%ls%ls%ls", gSessStore.GetRoot(), physDir, ENTER_DIRECTORY_PATH);
	// "C:\root\" -> "C:\root\folder" -> "C:\root\folder\*"
	hr = winFileDirScan(
		physDirAbsEntered,
		searchExpression,
		isSingleEntry,
		dirEntryBufferHandle,
		lpSess, // TODO remove session logic from dir scan
		&files
	);
	if (!SUCCEEDED(hr))
	{
		return hr;
	}

	// Find files by 1fs
	std::vector<std::wstring> exclusions;
	std::vector<std::wstring> inclusions;
	gSessStore.ReplayProjections(virtDir, &inclusions, &exclusions);

	// Take: (ntfs (already filtered by searchExp) \cup (inclusions filtered)) \diff (exclusions)
	// The searchExp filter thus can be inserted into inclusions

	for (auto it = inclusions.begin(); it != inclusions.end(); ++it)
	{
		// apply SearchExp
		wchar_t virtFile[PATH_BUFF_LEN] = { 0 };
		swprintf_s(virtFile, L"%ls%ls",
			virtDir, (*it).data());

		if (nullptr != searchExpression 
			&& !PrjFileNameMatch(virtFile, searchExpression))
		{
			continue;
		}

		// virt -> phys
		wchar_t physFile[PATH_BUFF_LEN] = { 0 };
		gSessStore.GetRepath(virtFile, physFile);

		// phys -> abs phys
		//wchar_t (&physFileAbs)[PATH_BUFF_LEN] = virtFile; // too sao
		wchar_t physFileAbs[PATH_BUFF_LEN] = { 0 };
		swprintf_s(physFileAbs, L"%ls%ls%ls",
			gSessStore.GetRoot(), DIRECTORY_SEP_PATH, physFile);
		
		PRJ_FILE_BASIC_INFO fbi = {};
		// No need to scan for dir, since the current "view" is the parent of physFileAbs
		winFileScan(physFileAbs, &fbi);

		if (files.count(virtFile))
		{
			printf_s("[%s] inclusion clash with ntfs scan at %ls", __func__, physFileAbs);
		}
		files[virtFile] = fbi;
	}
	for (auto it = exclusions.begin(); it != exclusions.end(); ++it)
	{
		wchar_t virtFile[PATH_BUFF_LEN] = { 0 };
		swprintf_s(virtFile, L"%ls%ls",
			virtDir, (*it).data());

		if (files.count(virtFile))
		{
			files.erase(virtFile);
		}
		else
		{
			printf_s("[%s] exclusion declared but entry not found %ls", __func__, (*it).data());
		}
	}

	std::map<std::wstring, PRJ_FILE_BASIC_INFO>::iterator it = files.begin();

	std::for_each(files.begin(), isSingleEntry? (++it) : files.end(), [dirEntryBufferHandle](std::pair<const std::wstring, PRJ_FILE_BASIC_INFO>& pair) {
		PrjFillDirEntryBuffer(pair.first.data(), &pair.second, dirEntryBufferHandle);
	});

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


