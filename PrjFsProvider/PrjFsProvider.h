#pragma once
#include <projectedfslib.h>
#include <vector>
#include <set>

#include "const.h"


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
		int isFromPathLessThan = lstrcmpW(lhs.FromPath, rhs.FromPath);
		if (isFromPathLessThan < 0)
		{
			return true;
		}
		else if (isFromPathLessThan == 0)
		{
			return lstrcmpW(lhs.ToPath, rhs.ToPath) < 0;
		}
		else
		{
			return false;
		}
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

// TODO make sure the stored/accessed order follows FIFO of add time!
//typedef std::set<PrjFsMap, PrjFsMapComparator> Remap;
typedef std::vector<PrjFsMap> Remap;

// Expects singleton
class PrjFsSessionStore
{
private:
	LPCWSTR srcName;
	std::vector<PrjFsSessionRuntime*> sessions;
	Remap remaps; // user ops as well as backtracks
public:
	PrjFsSessionStore() = delete;
	PrjFsSessionStore(LPCWSTR root);
	LPCWSTR GetRoot();
	LPPrjFsSessionRuntime GetSession(LPCGUID lpcGuid);
	void FreeSession(LPCGUID lpcGuid);
	int AddRemap(PCWSTR from, PCWSTR to);
	//int FilterRemaps(__in PCWSTR directory, __inout Remap* remaps);
	void ReplayProjections(
		__in PCWSTR dir,
		__out std::vector<LPCWSTR> *inclusions,
		__out std::vector<LPCWSTR> *exclusions
	);
};
