#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <set>
#include "types.h"

struct wcsless {
	// Compares two wchar_t* strings, for use with STL
	bool operator()(const wchar_t* const left, const wchar_t* const right) const {
		return wcscmp(left, right) < 0;
	}
};

// Returns GetWindowText as std::wstring
std::wstring GetWindowStr(HWND hWnd);
std::wstring GetDlgItemStr(HWND hWnd, int idItem);

// Convert an ANSI string to a wide (UCS-2) string
std::wstring AnsiToWide(const char* cstr);
static std::wstring AnsiToWide(const std::string& str)
{
	return AnsiToWide(str.c_str());
}

// Convert  a wide (UCS-2) string to an an ANSI string
std::string WideToAnsi(const wchar_t* cstr,     const char* defChar = " ");
static std::string WideToAnsi(const std::wstring& str, const char* defChar = " ")
{
	return WideToAnsi(str.c_str(), defChar);
}

void GetLanguageList(std::set<LANGID>& languages);
std::wstring GetLanguageName(LANGID language);
std::wstring GetEnglishLanguageName(LANGID language);

std::wstring FormatString(const wchar_t* format, ...);
std::wstring LoadString(UINT id, ...);

#endif