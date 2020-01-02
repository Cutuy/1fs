#include "pch.h"

#include "WinFileProvider.h"
HRESULT winFileDir
(
	__in LPCWSTR lpcPhysDirAbsEntered, 
	__in LPCWSTR searchExpression,
	__in BOOL isSingleEntry,
	__inout PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle,
	__inout LPPrjFsSessionRuntime lpSess,
	__out std::vector<PRJ_FILE_BASIC_INFO>* lpFiles
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

			lpFiles->push_back(fileBasicInfo);
			// TODO remove
			hr = PrjFillDirEntryBuffer(ffd.cFileName, &fileBasicInfo, dirEntryBufferHandle);

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

				lpFiles->push_back(fileBasicInfo);
				// TODO remove
				hr = PrjFillDirEntryBuffer(pathBuff, &fileBasicInfo, dirEntryBufferHandle);

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