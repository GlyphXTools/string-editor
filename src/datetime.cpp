#include "datetime.h"
using namespace std;

bool DateTime::operator < (const DateTime& dt) const
{
	return (CompareFileTime(&filetime, &dt.filetime) < 0);
}

wstring DateTime::formatDateShort(bool localTime)
{
	SYSTEMTIME time;
	if (localTime)
	{
		FILETIME localtime;
		FileTimeToLocalFileTime(&filetime, &localtime);
		FileTimeToSystemTime(&localtime, &time);
	}
	else
	{
		FileTimeToSystemTime(&filetime, &time);
	}

	int flags = LOCALE_NOUSEROVERRIDE | DATE_SHORTDATE;
	int len   = GetDateFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, NULL, 0);
	wchar_t* buf = new wchar_t[len];
	GetDateFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, buf, len);
	wstring str = buf;
	delete[] buf;
	return str;
}

wstring DateTime::formatShort(bool localTime)
{
	SYSTEMTIME time;
	if (localTime)
	{
		FILETIME localtime;
		FileTimeToLocalFileTime(&filetime, &localtime);
		FileTimeToSystemTime(&localtime, &time);
	}
	else
	{
		FileTimeToSystemTime(&filetime, &time);
	}

	int flags = LOCALE_NOUSEROVERRIDE | DATE_SHORTDATE;
	int len   = GetDateFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, NULL, 0);
	wchar_t* buf = new wchar_t[len+1];
	GetDateFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, buf, len);
	wstring str = buf;
	delete[] buf;

	flags = LOCALE_NOUSEROVERRIDE | TIME_NOSECONDS;
	len   = GetTimeFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, NULL, 0);
	buf   = new wchar_t[len+1];
	GetTimeFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, buf, len);
	str = str + L", " + buf;
	delete[] buf;

	return str;
}

wstring DateTime::format(bool localTime)
{
	SYSTEMTIME time;
	if (localTime)
	{
		FILETIME localtime;
		FileTimeToLocalFileTime(&filetime, &localtime);
		FileTimeToSystemTime(&localtime, &time);
	}
	else
	{
		FileTimeToSystemTime(&filetime, &time);
	}

	int flags = LOCALE_NOUSEROVERRIDE | DATE_LONGDATE;
	int len   = GetDateFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, NULL, 0);
	wchar_t* buf = new wchar_t[len];
	GetDateFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, buf, len);
	wstring str = buf;
	delete[] buf;

	flags = LOCALE_NOUSEROVERRIDE | TIME_NOSECONDS;
	len   = GetTimeFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, NULL, 0);
	buf   = new wchar_t[len];
	GetTimeFormat(LOCALE_USER_DEFAULT, flags, &time, NULL, buf, len);
	str = str + L", " + buf;
	delete[] buf;

	return str;
}

uint64_t DateTime::getEpochSeconds() const
{
	ULARGE_INTEGER value;
	value.LowPart  = filetime.dwLowDateTime;
	value.HighPart = filetime.dwHighDateTime;

	return value.QuadPart;
}

DateTime::DateTime(uint64_t epochSeconds)
{
	ULARGE_INTEGER value;
	value.QuadPart = epochSeconds;

	filetime.dwLowDateTime  = value.LowPart;
	filetime.dwHighDateTime = value.HighPart;
}

DateTime::DateTime()
{
	SYSTEMTIME systemtime;
	GetSystemTime(&systemtime);
	SystemTimeToFileTime(&systemtime, &filetime);
}
