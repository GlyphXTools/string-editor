#ifndef DIALOGS_H
#define DIALOGS_H

#include "types.h"
#include "document.h"

struct NEW_FILE_OPTIONS
{
	LANGID         language;
	Document::Type type;
};

struct VERSION_INFO
{
	unsigned int           version;
	std::set<std::wstring> authors;
	std::wstring           author;
	std::wstring	       notes;
};

struct FIND_INFO
{
	enum Method
	{
		FM_NONE,
		FM_FROMCURSOR,
		FM_FROMSTART,
		FM_SELECTION
	};

	std::wstring term;
	Method method;

	bool   matchCase;
	bool   matchWord;
	bool   searchName;
	bool   searchValue;
	bool   searchComment;
	bool   selectAll;

	FIND_INFO()
	{
		matchCase  = matchWord   = false;
		searchName = searchValue = searchComment = true;
		method     = FM_NONE;
	}
};

namespace Dialogs
{
	bool             NewFile   (HWND hParent, NEW_FILE_OPTIONS& options);
	Document::Method ImportFile(HWND hParent, bool byIndex);
	bool             GetVersionInfo(HWND hParent, VERSION_INFO& info);

	LANGID AddLanguage(HWND hParent, const std::set<LANGID>& languages);
	LANGID ChangeLanguage(HWND hParent, const std::set<LANGID>& languages);

	bool ExportFile(HWND hParent, std::map<LANGID,std::wstring>& postfixes, std::map<LANGID,bool>& enabled, std::wstring& basename);

	bool Find(HWND hParent, FIND_INFO& info);

	void FileInfo(HWND hParent, const Document* document);
}

#endif