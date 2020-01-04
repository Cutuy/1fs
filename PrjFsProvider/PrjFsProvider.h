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
//PRJ_QUERY_FILE_NAME_CB MyQueryFileNameCallback;
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

struct ProjectionComparator
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
typedef std::map<std::wstring, std::wstring, ProjectionComparator> proj_t;
// Expects singleton
class PrjFsSessionStore
{
private:
	LPCWSTR srcName;
	std::vector<PrjFsSessionRuntime*> sessions;
	
	// Virtual to *physical* path mapping, without guaranteeing existence of virtual path
	proj_t repaths;
	
	// Physical to physical path mapping for entry visibility calculation
	std::vector<PrjFsMap> remaps; // user ops as well as backtracks

	void AddRepath(LPCWSTR virtPath, LPCWSTR possiblePhysPath);
public:
	PrjFsSessionStore() = delete;
	PrjFsSessionStore(LPCWSTR root);
	LPCWSTR GetRoot();
	LPPrjFsSessionRuntime GetSession(LPCGUID lpcGuid);
	void FreeSession(LPCGUID lpcGuid);
	int AddRemap(PCWSTR from, PCWSTR to);

	BOOL IsVirtPathValid(LPCWSTR virtPath);

	// Find path component at depth of 1 done without accessing the FS on disk
	void ReplayProjections(
		__in PCWSTR virtDir,
		__out std::vector<std::wstring>& virtInclusions,
		__out std::vector<std::wstring>& virtExclusions
	);

	// Find a repath that has the longest matching prefix path
	void GetRepath(__in LPCWSTR virtPath, __out LPWSTR physPath) const;

	void TEST_ClearProjections();
};
