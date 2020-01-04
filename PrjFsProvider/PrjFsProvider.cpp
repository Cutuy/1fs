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
	// TODO validate the parent of arg_to must exist 
	// Let's forget the loops & priorities for remaps now; assuming they do not have conflict of scopes at all

	
	// Get the physical path of remap before adding it
	// Any duplications or single-from_multi-to will be handled by GetRepath
	PrjFsMap map(from, to);

	this->remaps.erase(
		std::remove_if(
			this->remaps.begin(),
			this->remaps.end(),
			[map](PrjFsMap const& rm) {
				return 0 == lstrcmpW(rm.FromPath, map.FromPath);
			}
		),
		this->remaps.end()
	);
	// Filter identity projection (though it doesn't hurt)
	if (0 != lstrcmpW(map.FromPath, map.ToPath))
	{
		this->remaps.push_back(map);
		// TODO forbid any virt access to physFrom
		// by add a special repath: (from, NULL)
		// Currently invalid virtual path can still access the files
		// under the translated (i.e. no match) phys path on disk fs

		// Take effect of the new repath only after the remap has been added
		// otherwise the current remap cannot use repath translation (from always equals to to)
		this->AddRepath(to, from); // TODO: or to -> physFrom?

	}
	return 0;
}

void PrjFsSessionStore::ReplayProjections(
	__in PCWSTR virtDir, 
	__out std::vector<std::wstring>& virtInclusions,
	__out std::vector<std::wstring>& virtExclusions
)
{
	//wchar_t physDir[PATH_BUFF_LEN] = { 0 };
	//gSessStore.GetRepath(virtDir, physDir);

	// File moves are evaluated always against physical path
	wchar_t activeDir[PATH_BUFF_LEN] = { 0 };
	lstrcpyW(activeDir, virtDir);

	std::vector<PrjFsMap>::reverse_iterator it = this->remaps.rbegin();
	while (it != this->remaps.rend())
	{
		BOOL hr;
		int less;
		if (hr = lpathcmpW(it->ToPath, activeDir, &less))
		{
			if (0 == less)
			{
				wmemset(activeDir, 0, PATH_BUFF_LEN);
				lstrcpyW(activeDir, it->FromPath);
				goto next_item;
			}
			else if (1 == less)
			{
				wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
				GetPathLastComponent(it->ToPath, pathBuff);

				std::vector<std::wstring>::iterator it_f;
				if (virtExclusions.end() == (it_f = std::find(virtExclusions.begin(), virtExclusions.end(), pathBuff)))
				{
					virtInclusions.push_back(pathBuff);
				}
				
			}
		}
		// no else!
		if (hr = lpathcmpW(it->FromPath, activeDir, &less))
		{
			if (1 == less)
			{
				wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
				GetPathLastComponent(it->FromPath, pathBuff);

				std::vector<std::wstring>::iterator it_f;
				if (virtInclusions.end() == (it_f = std::find(virtInclusions.begin(), virtInclusions.end(), pathBuff)))
				{
					virtExclusions.push_back(pathBuff);
				}
			}
		}
next_item:
		++it;
	}
}

void PrjFsSessionStore::AddRepath(LPCWSTR virtPath, LPCWSTR possiblePhysPath)
{
	// Do not check validity/invalidity of virtPath here
	// Because only after this repath is added will virtPath be valid
	// and virtPath may be valid (so we are updating an existing repath)

	if (nullptr == virtPath || 0 == lstrlenW(virtPath))
	{
		printf_s("[%s] virtPath cannot be empty\n", __func__);
		return;
	}

	// All repaths should always be valid virt-to-phys paths
	// This means: 1) phys is latest, 2) existing & new repaths each are correct after insertion
	// This function maintains these properties.

	// No need to update ToPath of existing repaths
	// Because ToPath is physical, if that physical location (or its uplevel) shall be 
	// mapped to somewhere new, then AddRemap shall be called first, 
	// which in turn calls AddRepath with new location as arg_virtPath.
	// That call will replace all matching existing repaths by its *FromPath*.

	// Invalidate existing repaths (by FromPath only)
	proj_t::iterator repath_it = this->repaths.begin();
	proj_t::iterator repath_end = this->repaths.end();
	while (repath_it != repath_end)
	{
		pair_t pair = *repath_it;
		INT less;
		if (lpathcmpW(virtPath, pair.first.data(), &less))
		{
			if (less < 0)
			{
				PrjFsMap proj(pair.first.data(), pair.second.data());
				printf_s("[%s] Invalidated repath %ls", __func__, pair.first.data());
				repath_it = this->repaths.erase(repath_it);
				continue;
			}
		}
		++repath_it;
	}

	// Insert new repath at most once
	wchar_t physPath[PATH_BUFF_LEN] = { 0 };
	lstrcpyW(physPath, possiblePhysPath);

	proj_t::iterator it;
	if (this->repaths.end() != (it = std::find_if(this->repaths.begin(), this->repaths.end(),
		[possiblePhysPath](pair_t const& pair)
		{
			INT less;
			if (lpathcmpW(possiblePhysPath, pair.first.data(), &less))
			{
				if (less >= 0)
				{
					return true;
				}
			}
			return false;
		}
	)))
	{
		pair_t pair = *it;
		// Replace the prefix (pair.first) part of map.ToPath to pair.second
		// Then set it to map.ToPath; this cannot be done by GetRepath
		ReplacePrefix(pair.first.data(), pair.second.data(), physPath);
	}

	if (this->repaths.count(virtPath))
	{
		printf_s("[%s] Updating existing repath %ls", __func__, virtPath);
	}
	if (0 != lstrcmpW(virtPath, physPath))
	{
		this->repaths[virtPath] = std::wstring(physPath);
	}
	
	// Validate result
	assert(this->IsVirtPathValid(virtPath));
}

void PrjFsSessionStore::GetRepath(__in LPCWSTR virtPath, __out LPWSTR physPath) const
{
	INT most = MININT;
	lstrcpyW(physPath, virtPath);

	// Find and replace by the repath of the closest parent (or self)

	// Do not change physPath while in loop, otherwise it becomes too greedy 
	// to ignore closer (repaths, once added, are *all* valid w/o priorities) match 
	// in later iterations
	
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
	if (most != MININT)
	{
		ReplacePrefix(repath.first.data(), repath.second.data(), physPath);
	}
}

BOOL PrjFsSessionStore::IsVirtPathValid(LPCWSTR virtPath)
{
	wchar_t testPath[PATH_BUFF_LEN] = { 0 };
	wchar_t firstComp[PATH_BUFF_LEN] = { 0 };
	wchar_t remainPath[PATH_BUFF_LEN] = { 0 };
	lstrcpyW(remainPath, virtPath);

	std::vector<std::wstring> exclusions;
	std::vector<std::wstring> inclusions;

	while (lstrlenW(remainPath))
	{
		this->ReplayProjections(testPath, inclusions, exclusions);
		GetPathFirstComponent(remainPath, firstComp);

		if (exclusions.end() !=
			std::find(exclusions.begin(), exclusions.end(), firstComp))
		{
			return false;
		}

		inclusions.clear();
		exclusions.clear();
		
		// firstComp:  a     -> b   -> c   -> 
		// testPath:         -> a   -> a/b -> a/b/c
		// remainPath: a/b/c -> b/c -> c   -> 
		
		ReplacePrefix(firstComp, L"", remainPath);
		ReplacePrefix(DIRECTORY_SEP_PATH, L"", remainPath);
		if (lstrlenW(testPath))
		{
			lstrcatW(testPath, DIRECTORY_SEP_PATH);
		}
		lstrcatW(testPath, firstComp);
		wmemset(firstComp, 0, PATH_BUFF_LEN);
	}
	return true;
}

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
	BOOL isRestart = (callbackData->Flags & PRJ_CB_DATA_FLAG_ENUM_RESTART_SCAN) != 0;
	// fileName will be in searchExp
	printf_s("[%s] (%ls, %ls), single %d, restart %d\n", 
		__func__, 
		callbackData->FilePathName, searchExpression, 
		isSingleEntry ? 1: 0,
		isRestart ? 1: 0);

	LPCWSTR virtDir = callbackData->FilePathName;

	if (!gSessStore.IsVirtPathValid(virtDir))
	{
		return S_OK;
	}

	// key: virtFile | virtDirectory
	std::map<std::wstring, PRJ_FILE_BASIC_INFO> files;

	// Find files by ascending priority

	// Fill files by NTFS
	HRESULT hr;

	wchar_t physDir[PATH_BUFF_LEN] = { 0 };
	wchar_t physDirAbsEntered[PATH_BUFF_LEN] = { 0 };
	gSessStore.GetRepath(virtDir, physDir);
	// physDir may be empty, making the backslash redundant
	swprintf_s(physDirAbsEntered, L"%ls%ls%ls%ls", gSessStore.GetRoot(), 
		lstrlenW(physDir) ? DIRECTORY_SEP_PATH : L"", physDir, ENTER_DIRECTORY_PATH);
	// "C:\root" -> "C:\root\folder" -> "C:\root\folder\*"

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
	printf_s("[%s] Found %d entries from ntfs %ls\n", 
		__func__, (int)files.size(), physDirAbsEntered);
	

	// Find files by 1fs
	std::vector<std::wstring> exclusions;
	std::vector<std::wstring> inclusions;
	gSessStore.ReplayProjections(virtDir, inclusions, exclusions);

	// Take: (ntfs (already filtered by searchExp) \cup (inclusions filtered)) \diff (exclusions)
	// The searchExp filter thus can be inserted into inclusions

	for (auto it = inclusions.begin(); it != inclusions.end(); ++it)
	{
		// apply SearchExp
		wchar_t virtFile[PATH_BUFF_LEN] = { 0 };
		swprintf_s(virtFile, L"%ls%ls%ls",
			virtDir, lstrlenW(virtDir) ? DIRECTORY_SEP_PATH : L"", (*it).data());

		wchar_t virtFileLastComp[PATH_BUFF_LEN] = { 0 };
		GetPathLastComponent(virtFile, virtFileLastComp);

		if (nullptr != searchExpression 
			&& !PrjFileNameMatch(virtFileLastComp, searchExpression))
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
		
		HRESULT hr;

		PRJ_FILE_BASIC_INFO fbi = {};
		// No need to scan for dir, since the current "view" is the parent of physFileAbs
		hr = winFileScan(physFileAbs, &fbi);
		if (FAILED(hr))
		{
			continue;
		}

		auto entry = (*it).data();
		if (files.count(entry))
		{
			printf_s("[%s] inclusion clash with ntfs scan at %ls\n", __func__, physFileAbs);
		}
		files[entry] = fbi;
	}
	for (auto it = exclusions.begin(); it != exclusions.end(); ++it)
	{
		// Exclude item from enum results for the current call
		auto entry = (*it).data();

		if (files.count(entry))
		{
			files.erase(entry);
		}
		else
		{
			printf_s("[%s] exclusion declared but entry not found %ls\n", __func__, (*it).data());
		}
	}

	std::map<std::wstring, PRJ_FILE_BASIC_INFO>::iterator it = files.begin();


	if (files.size())
	{
		std::for_each(isSingleEntry ? files.begin() : (++it), isSingleEntry ? (++it) : files.end(), [dirEntryBufferHandle](std::pair<const std::wstring, PRJ_FILE_BASIC_INFO>& pair) {
			HRESULT hr;
			hr = PrjFillDirEntryBuffer(pair.first.data(), &pair.second, dirEntryBufferHandle);
			if (FAILED(hr)) {
				printf_s("[%s] PrjFillDirEntryBuffer failed for %ls due to 0x%08d\n", __func__, pair.first.data(), hr);
			}
		});
	}

	if (!isSingleEntry)
	{
		printf_s("\n[%s] (%ls, %ls), found %d entries\n",
			__func__,
			callbackData->FilePathName, 
			searchExpression,
			(int)files.size());
		std::for_each(files.begin(), files.end(), [](std::pair<const std::wstring, PRJ_FILE_BASIC_INFO>& pair) {
			printf_s("%ls\n", pair.first.data());
		});
		printf_s("\n");
	}

	return S_OK;
}

HRESULT MyGetPlaceholderCallback(
	_In_ const PRJ_CALLBACK_DATA* callbackData
)
{
	LPCWSTR virtFile = callbackData->FilePathName;

	// Do not assume virtFile is always valid
	if (!gSessStore.IsVirtPathValid(virtFile))
	{
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}

	wchar_t physFile[PATH_BUFF_LEN] = { 0 };
	wchar_t physFileAbs[PATH_BUFF_LEN] = { 0 };
	gSessStore.GetRepath(virtFile, physFile);
	swprintf_s(physFileAbs, L"%ls%ls%ls",
		gSessStore.GetRoot(), DIRECTORY_SEP_PATH, physFile);

	printf_s("[%s] physFileAbs %ls\n", __func__, physFileAbs);

	HRESULT hr;

	PRJ_FILE_BASIC_INFO fileBasicInfo = {};
	hr = winFileScan(physFileAbs, &fileBasicInfo);
	if (FAILED(hr))
	{
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}

	PRJ_PLACEHOLDER_INFO placeholderInfo = {};
	placeholderInfo.FileBasicInfo = fileBasicInfo;
	
	PrjWritePlaceholderInfo(callbackData->NamespaceVirtualizationContext,
		virtFile,
		&placeholderInfo,
		sizeof(placeholderInfo));
	printf_s("[%s] Placeholder written for %ls\n", __func__, virtFile);

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

	LPCWSTR virtFile = callbackData->FilePathName;
	wchar_t physFile[PATH_BUFF_LEN] = { 0 };
	wchar_t physFileAbs[PATH_BUFF_LEN] = { 0 };
	gSessStore.GetRepath(virtFile, physFile);
	swprintf_s(physFileAbs, L"%ls%ls%ls",
		gSessStore.GetRoot(), DIRECTORY_SEP_PATH, physFile);

	void* rwBuffer;
	rwBuffer = PrjAllocateAlignedBuffer(callbackData->NamespaceVirtualizationContext, length);
	if (rwBuffer == NULL)
	{
		return E_OUTOFMEMORY;
	}

	
	HANDLE hFile;

	hFile = CreateFile(physFileAbs,               // file to open
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

/*
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
*/

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

		PRJ_UPDATE_FAILURE_CAUSES causes;

		gSessStore.AddRemap(callbackData->FilePathName, destinationFileName);
		printf_s("[%s] Added remap %ls %ls\n", __func__, callbackData->FilePathName, destinationFileName);


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
#endif
	}
	return S_OK;
}


