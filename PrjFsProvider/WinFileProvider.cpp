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
	// Shadow file is a pure virtual file that does not exist in ntfs
	if (IsShadowFile(lpcPhysAbs))
	{		
		// phys file non-exist -> fil not found
		// virt path not valid (even if phys file exist) -> use IsVirtPathValid to protect in caller's code
		wchar_t unsuffixPhysAbs[PATH_BUFF_LEN] = { 0 };
		wmemcpy(unsuffixPhysAbs, lpcPhysAbs, lstrlenW(lpcPhysAbs) - lstrlenW(SHADOW_FILE_SUFFIX));
		fileAttr = GetFileAttributes(unsuffixPhysAbs);
		if (INVALID_FILE_ATTRIBUTES == fileAttr || !(FILE_ATTRIBUTE_DIRECTORY & fileAttr))
		{
			printf_s("[%s] file not found for %ls\n", __func__, unsuffixPhysAbs);
			return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		}
		
		// Now treat the shadow file as a normal file
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
		// https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea#directories
		(fileAttr & FILE_ATTRIBUTE_DIRECTORY)? FILE_FLAG_BACKUP_SEMANTICS : FILE_FLAG_OVERLAPPED, // normal file
		NULL);
	// Consider FILE_FLAG_NO_BUFFERING?
	// 	((FILE_ATTRIBUTE_DIRECTORY & fileAttr)? FILE_FLAG_BACKUP_SEMANTICS: 0);


	LARGE_INTEGER fileSize;
	if (hFile == INVALID_HANDLE_VALUE || !GetFileSizeEx(hFile, &fileSize))
	{
		fileSize.LowPart = 0;
		fileSize.HighPart = 0;
	}
	FILETIME ft1 = {}, ft2 = {}, ft3 = {};
	GetFileTime(hFile, &ft1, &ft2, &ft3);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
	}

	lpFile->IsDirectory = (fileAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	lpFile->FileAttributes = fileAttr;
	lpFile->FileSize = fileSize.QuadPart;
	lpFile->CreationTime.HighPart = ft1.dwHighDateTime;
	lpFile->CreationTime.LowPart = ft1.dwLowDateTime;
	lpFile->LastAccessTime.HighPart = ft2.dwHighDateTime;
	lpFile->LastAccessTime.LowPart = ft2.dwLowDateTime;
	lpFile->ChangeTime.HighPart = ft3.dwHighDateTime;
	lpFile->ChangeTime.LowPart = ft3.dwLowDateTime;
	lpFile->LastWriteTime.HighPart = ft3.dwHighDateTime;
	lpFile->LastWriteTime.LowPart = ft3.dwLowDateTime;

	return S_OK;
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

	DWORD dwError = 0;

	do
	{
		if (nullptr == searchExpression || PrjFileNameMatch(ffd.cFileName, searchExpression))
		{
			PRJ_FILE_BASIC_INFO fileBasicInfo = {};
			fileBasicInfo.IsDirectory = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			fileBasicInfo.FileSize = (((INT64)ffd.nFileSizeHigh) << 32) | ffd.nFileSizeLow;
			fileBasicInfo.FileAttributes = ffd.dwFileAttributes;
			fileBasicInfo.CreationTime.HighPart = ffd.ftCreationTime.dwHighDateTime;
			fileBasicInfo.CreationTime.LowPart = ffd.ftCreationTime.dwLowDateTime;
			fileBasicInfo.LastAccessTime.HighPart = ffd.ftLastAccessTime.dwHighDateTime;
			fileBasicInfo.LastAccessTime.LowPart = ffd.ftLastAccessTime.dwLowDateTime;
			fileBasicInfo.ChangeTime.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
			fileBasicInfo.ChangeTime.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
			fileBasicInfo.LastWriteTime.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
			fileBasicInfo.LastWriteTime.LowPart = ffd.ftLastWriteTime.dwLowDateTime;

			if (lpFiles->count(ffd.cFileName))
			{
				printf_s("[%s] file name conflict %ls", __func__, ffd.cFileName);
			}
			(*lpFiles)[std::wstring(ffd.cFileName)] = fileBasicInfo;
			
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
				fileBasicInfo.CreationTime.HighPart = ffd.ftCreationTime.dwHighDateTime;
				fileBasicInfo.CreationTime.LowPart = ffd.ftCreationTime.dwLowDateTime;
				fileBasicInfo.LastAccessTime.HighPart = ffd.ftLastAccessTime.dwHighDateTime;
				fileBasicInfo.LastAccessTime.LowPart = ffd.ftLastAccessTime.dwLowDateTime;
				fileBasicInfo.ChangeTime.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
				fileBasicInfo.ChangeTime.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
				fileBasicInfo.LastWriteTime.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
				fileBasicInfo.LastWriteTime.LowPart = ffd.ftLastWriteTime.dwLowDateTime;

				if (lpFiles->count(pathBuff))
				{
					printf_s("[%s] file name conflict %ls", __func__, pathBuff);
				}
				(*lpFiles)[std::wstring(pathBuff)] = fileBasicInfo;

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