#pragma once
#include <projectedfslib.h>
#include <vector>

#define PATH_BUFF_LEN (256)
#define DIRECTORY_SEP_PATH L"\\"
#define ENTER_DIRECTORY_PATH L"\\*"

PRJ_START_DIRECTORY_ENUMERATION_CB MyStartEnumCallback;
PRJ_END_DIRECTORY_ENUMERATION_CB MyEndEnumCallback;
PRJ_GET_DIRECTORY_ENUMERATION_CB MyGetEnumCallback;
PRJ_GET_PLACEHOLDER_INFO_CB MyGetPlaceholderCallback;
PRJ_GET_FILE_DATA_CB MyGetFileDataCallback;



class PrjFsSessionRuntime
{
private:
	GUID EnumId;
	wchar_t _mPathBuff[PATH_BUFF_LEN]; // thread unsafe within the same session!

public:
	BOOL IsGetEnumComplete;
	void ClearPathBuff();
	void AppendPathBuff(PCWSTR relPath);
	LPCWSTR GetPath();
	friend class PrjFsSessionStore;
};

typedef PrjFsSessionRuntime* LPPrjFsSessionRuntime;

// Expects singleton
class PrjFsSessionStore
{
private:
	LPCWSTR srcName;
	std::vector<PrjFsSessionRuntime*> sessions;
public:
	PrjFsSessionStore() = delete;
	PrjFsSessionStore(LPCWSTR root);
	LPCWSTR GetRoot();
	LPPrjFsSessionRuntime GetSession(LPCGUID lpcGuid);
	void FreeSession(LPCGUID lpcGuid);
};
