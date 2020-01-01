#pragma once
#include <projectedfslib.h>
#include <vector>
#include <map>
#include <string>

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
	PrjFsMap(PCWSTR lpcFromPath, PCWSTR lpcToPath)
	{
		if (lpcFromPath)
		{
			lstrcpyW(FromPath, lpcFromPath);
		}
		if (lpcToPath)
		{
			lstrcpyW(ToPath, lpcToPath);
		}
	}
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

struct RepathComparator
{
	bool operator() (const std::wstring& lhs, const std::wstring& rhs) const {
		return -1 == lstrcmpW(lhs.data(), rhs.data());
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
	
	std::map<std::wstring, std::wstring, RepathComparator> repaths;
	
	// Make sure the stored / accessed order follows FIFO of add time!
	std::vector<PrjFsMap> remaps; // user ops as well as backtracks
public:
	PrjFsSessionStore() = delete;
	PrjFsSessionStore(LPCWSTR root);
	LPCWSTR GetRoot();
	LPPrjFsSessionRuntime GetSession(LPCGUID lpcGuid);
	void FreeSession(LPCGUID lpcGuid);
	int AddRemap(PCWSTR from, PCWSTR to);
	void ReplayProjections(
		__in PCWSTR dir,
		__out std::vector<LPCWSTR> *inclusions,
		__out std::vector<LPCWSTR> *exclusions
	);
	void AddRepath(LPCWSTR virtPath, LPCWSTR possiblePhysPath);
	void TEST_ClearProjections();
	void TEST_GetRepath(__in LPCWSTR virtPath, __out LPWSTR physPath);
};
