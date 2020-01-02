#include "pch.h"
#include "strutil.h"
#include "WinFileProvider.h"

HRESULT winFileScan
(
	__in LPCWSTR lpcPhysAbs,
	__out PRJ_FILE_BASIC_INFO* lpFile
)
{
	UINT32 fileAttr = GetFileAttributes(lpcPhysAbs);
#ifdef __DIRECTORY_WORKAROUND__
	if (IsShadowFile(lpcPhysAbs))
	{
		printf_s("[%s] found shadow file %ls\n", __func__, lpcPhysAbs);
		fileAttr = FILE_ATTRIBUTE_NORMAL;
	}
#endif

	if (INVALID_FILE_ATTRIBUTES == fileAttr)
	{
		printf_s("[%s] file not found for %ls\n", __func__, lpcPhysAbs);
		// When renaming/moving, this cb will be called (not desired) unless
		// PRJ_QUERY_FILE_NAME_CB is provided
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}

	HANDLE hFile;
	hFile = CreateFile(lpcPhysAbs,               // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // normal file
		NULL);
	// Consider FILE_FLAG_NO_BUFFERING?
	// 	((FILE_ATTRIBUTE_DIRECTORY & fileAttr)? FILE_FLAG_BACKUP_SEMANTICS: 0);


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

	lpFile->IsDirectory = (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	lpFile->FileAttributes = fileAttr;
	lpFile->FileSize = fileSize.QuadPart;
}

HRESULT winFileDirScan
(
	__in LPCWSTR lpcPhysDirAbsEntered, 
	__in LPCWSTR searchExpression,
	__in BOOL isSingleEntry,
	__inout PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle,
	__inout LPPrjFsSessionRuntime lpSess,
	__out std::map<std::wstring, PRJ_FILE_BASIC_INFO>* lpFiles
)
{
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA ffd;
	hFind = FindFirstFile(lpcPhysDirAbsEntered, &ffd);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		printf_s("[%s] Open hFile failed for %ls\n", __func__, lpcPhysDirAbsEntered);
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
			
			if (lpFiles->count(ffd.cFileName))
			{
				printf_s("[%s] file name conflict %ls", __func__, ffd.cFileName);
			}
			(*lpFiles)[std::wstring(ffd.cFileName)] = fileBasicInfo;
			
			// TODO remove
			//hr = PrjFillDirEntryBuffer(ffd.cFileName, &fileBasicInfo, dirEntryBufferHandle);

#ifdef __DIRECTORY_WORKAROUND__
			// Create shadow file for directory
			if (fileBasicInfo.IsDirectory
				&& 0 != lstrcmpW(ffd.cFileName, L".")
				&& 0 != lstrcmpW(ffd.cFileName, L".."))
			{
				wchar_t pathBuff[PATH_BUFF_LEN] = { 0 };
				swprintf_s(pathBuff, L"%ls%ls", ffd.cFileName, SHADOW_FILE_SUFFIX);
				fileBasicInfo.IsDirectory = false;
				fileBasicInfo.FileSize = 0;
				fileBasicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;

				if (lpFiles->count(pathBuff))
				{
					printf_s("[%s] file name conflict %ls", __func__, pathBuff);
				}
				(*lpFiles)[std::wstring(pathBuff)] = fileBasicInfo;

				// TODO remove
				//hr = PrjFillDirEntryBuffer(pathBuff, &fileBasicInfo, dirEntryBufferHandle);

				printf_s("[%s] Shadow file created for %ls\n", __func__, pathBuff);
			}
#endif

			if (isSingleEntry)
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