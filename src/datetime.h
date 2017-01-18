#ifndef DATETIME_H
#define DATETIME_H

#include <string>
#include "types.h"

// Note: Epoch is January 1, 1601 (UTC)
class DateTime
{
	FILETIME filetime;

public:
	std::wstring format(bool localTime = true);
	std::wstring formatShort(bool localTime = true);
	std::wstring formatDateShort(bool localTime = true);

	bool operator < (const DateTime& dt) const;
	
	uint64_t getEpochSeconds() const;

	DateTime(uint64_t epochSeconds);
	DateTime();
};

#endif