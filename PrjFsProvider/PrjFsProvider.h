#pragma once
#include <projectedfslib.h>
#include <vector>
#include <set>

#define PATH_BUFF_LEN (256)
#define DIRECTORY_SEP_PATH L"\\"
#define ENTER_DIRECTORY_PATH L"\\*"

PRJ_START_DIRECTORY_ENUMERATION_CB MyStartEnumCallback;
PRJ_END_DIRECTORY_ENUMERATION_CB MyEndEnumCallback;
PRJ_GET_DIRECTORY_ENUMERATION_CB MyGetEnumCallback;
PRJ_GET_PLACEHOLDER_INFO_CB MyGetPlaceholderCallback;
PRJ_GET_FILE_DATA_CB MyGetFileDataCallback;
PRJ_QUERY_FILE_NAME_CB MyQueryFileNameCallback;
PRJ_NOTIFICATION_CB MyNotificationCallback;

struct PrjFsMap
{
	wchar_t FromPath[PATH_BUFF_LEN];
	wchar_t ToPath[PATH_BUFF_LEN];
};

struct PrjFsMapComparator
{
	bool operator() (const PrjFsMap& lhs, const PrjFsMap& rhs) const
	{
		return 0 == lstrcmpW(lhs.FromPath, rhs.FromPath) \
			&& 0 == lstrcmpW(lhs.ToPath, rhs.ToPath);
	}
};

class PrjFsSessionRuntime
{
private:
	GUID EnumId;
public:
	BOOL IsGetEnumComplete;
	friend class PrjFsSessionStore;
};

typedef PrjFsSessionRuntime* LPPrjFsSessionRuntime;

// Expects singleton
class PrjFsSessionStore
{
private:
	LPCWSTR srcName;
	std::vector<PrjFsSessionRuntime*> sessions;
	std::set<PrjFsMap, PrjFsMapComparator> remaps;
public:
	PrjFsSessionStore() = delete;
	PrjFsSessionStore(LPCWSTR root);
	LPCWSTR GetRoot();
	LPPrjFsSessionRuntime GetSession(LPCGUID lpcGuid);
	void FreeSession(LPCGUID lpcGuid);
	int AddRemap(PCWSTR from, PCWSTR to);
};
