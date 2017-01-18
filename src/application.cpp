#include <algorithm>
#include <conio.h>
#include <cassert>
#include <fstream>
#include <string>
#include <sstream>

#define _WIN32_WINNT 0x501
#include "application.h"
#include "stringlist.h"
#include "dialogs.h"
#include "datetime.h"
#include "exceptions.h"
#include "utils.h"
#include "ui.h"

#include <commctrl.h>
#include <commdlg.h>
#include <afxres.h>
#include "resource.h"

using namespace std;

static const int MIN_WINDOW_WIDTH  = 750;
static const int MIN_WINDOW_HEIGHT = 400;
static const int INITIAL_WINDOW_WIDTH  = 860;
static const int INITIAL_WINDOW_HEIGHT = 600;

enum
{
	COLUMN_NAME = 1,
	COLUMN_VALUE,
	COLUMN_COMMENT,
	COLUMN_DATE,
};

//
// BusyCursor class.
// Sets the cursor during an operation and restores it afterwards
//
class BusyCursor
{
	HCURSOR hOldCursor;

public:
	BusyCursor(HCURSOR hCursor)
	{
		hOldCursor = GetCursor();
		SetCursor(hCursor);
	}

	BusyCursor(LPCWSTR lpCursorName)
	{
		hOldCursor = GetCursor();
		SetCursor(LoadCursor(NULL, lpCursorName));
	}

	~BusyCursor()
	{
		SetCursor(hOldCursor);
	}
};

//
// Some global functions
//

static LPARAM ListView_GetItemParam(HWND hwndLV, int id)
{
	LVITEM item;
	item.lParam = -1;
	if (id >= 0)
	{
		item.mask     = LVIF_PARAM;
		item.iSubItem = 0;
		item.iItem    = id;
		ListView_GetItem(hwndLV, &item);
	}
	return item.lParam;
}


//
// Interfaces and generic callbacks
//
static LRESULT CALLBACK GenericWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	IWindow* wnd = (IWindow*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (uMsg == WM_CREATE)
	{
		CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
		wnd = (IWindow*)pcs->lpCreateParams;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)wnd);
	}

	if (wnd != NULL)
	{
		return wnd->WindowProc(hWnd, uMsg, wParam, lParam);
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static INT_PTR CALLBACK GenericDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	IDialog* dlg = (IDialog*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (uMsg == WM_INITDIALOG)
	{
		dlg = (IDialog*)lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)dlg);
	}

	if (dlg != NULL)
	{
		return dlg->DialogProc(hWnd, uMsg, wParam, lParam);
	}
	return FALSE;
}

static int CALLBACK GenericStringCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const Application* app = (Application*)lParamSort;
	return app->StringCompareFunc(lParam1, lParam2);
}

static void EnableMenu(HMENU hMenu, UINT state)
{
	int count = GetMenuItemCount(hMenu);
	for (int i = 0; i < count; i++)
	{
		EnableMenuItem(hMenu, i, state | MF_BYPOSITION);
	}
}

//
// Application class
//
void Application::UI_OnDocumentChanged()
{
	bool enable = (document != NULL);
	if (enable)
	{
		hActiveListView = GetDlgItem(hContainer, (document->getType() == Document::DT_NAME) ? IDC_LIST2 : IDC_LIST3);
		ShowWindow(hActiveListView, SW_SHOW);
	}
	else
	{
		hActiveListView = NULL;
		ShowWindow(GetDlgItem(hContainer, IDC_LIST2), SW_HIDE);
		ShowWindow(GetDlgItem(hContainer, IDC_LIST3), SW_HIDE);
		SendMessage(hVersionSelect,  CB_RESETCONTENT, 0, 0);
		SendMessage(hLanguageSelect, CB_RESETCONTENT, 0, 0);
	}

	// Clear all windows
	ListView_DeleteAllItems(hActiveListView);
	ListView_DeleteAllItems(hHistory);
	SetDlgItemText(hContainer, IDC_EDIT1, L"");
	SetDlgItemText(hContainer, IDC_EDIT2, L"");
	SetDlgItemText(hContainer, IDC_EDIT3, L"");

	// Show/hide and enable/disable windows
	ShowWindow(hContainer, enable ? SW_SHOW : SW_HIDE);
	EnableWindow(hVersionSelect,  enable);
	EnableWindow(hLanguageSelect, enable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_FILE_SAVE,     enable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_FIND,     enable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_FINDNEXT, enable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_CUT,      enable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_COPY,     enable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_PASTE,    enable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_FINDNEXTINVALID, enable);

	bool updownEnable = enable && document->getType() == Document::DT_INDEX;
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_MOVESTRINGSUP,   updownEnable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_MOVESTRINGSDOWN, updownEnable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_MOVEVALUESUP,   updownEnable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_MOVEVALUESDOWN, updownEnable);

	// Toggle menu items
	UINT  state = (enable ? MF_ENABLED : MF_GRAYED);
	HMENU hMenu = GetMenu(hMainWnd);
	HMENU hFile = GetSubMenu(hMenu, 0);
	EnableMenuItem(hFile, ID_FILE_SAVE,    state | MF_BYCOMMAND);
	EnableMenuItem(hFile, ID_FILE_SAVEAS,  state | MF_BYCOMMAND);
	EnableMenuItem(hFile, ID_FILE_CLOSE,   state | MF_BYCOMMAND);
	EnableMenuItem(hFile, ID_FILE_DETAILS, state | MF_BYCOMMAND);
	EnableMenuItem(hFile, ID_FILE_IMPORT,  state | MF_BYCOMMAND);
	EnableMenuItem(hFile, ID_FILE_EXPORT,  state | MF_BYCOMMAND);
	EnableMenu(GetSubMenu(hMenu, 1), state);
	EnableMenu(GetSubMenu(hMenu, 2), state);

	UI_OnFocusChanged();
}

void Application::OnInitMenu(HMENU hMenu)
{
	if (document != NULL)
	{
		vector<Document::VersionInfo> versions;
		document->getVersions(versions);
		bool isCurrent = (document->getActiveVersion() == versions.size() - 1);

		// Edit menu
		bool canPaste = IsClipboardFormatAvailable(CF_UNICODETEXT) || IsClipboardFormatAvailable(CF_TEXT);
		UINT state;
		
		HMENU hEditMenu = GetSubMenu(hMenu, 1);
		state = isCurrent ? MF_ENABLED : MF_GRAYED;	
		EnableMenuItem(hEditMenu, ID_EDIT_ADDSTRING, state | MF_BYCOMMAND);
		EnableMenuItem(hEditMenu, ID_EDIT_DELETE, state | MF_BYCOMMAND);

		state = isCurrent && canPaste ? MF_ENABLED : MF_GRAYED;
		EnableMenuItem(hEditMenu, ID_EDIT_PASTE, state | MF_BYCOMMAND);

		state = (isCurrent && canPaste && document->getType() == Document::DT_NAME) ? MF_ENABLED : MF_GRAYED;
		EnableMenuItem(hEditMenu, 5, state | MF_BYPOSITION);

		state = (isCurrent && document->getType() == Document::DT_INDEX) ? MF_ENABLED : MF_GRAYED;
		EnableMenuItem(hEditMenu, ID_EDIT_MOVESTRINGSUP,   state | MF_BYCOMMAND);
		EnableMenuItem(hEditMenu, ID_EDIT_MOVESTRINGSDOWN, state | MF_BYCOMMAND);
		EnableMenuItem(hEditMenu, ID_EDIT_MOVEVALUESUP,   state | MF_BYCOMMAND);
		EnableMenuItem(hEditMenu, ID_EDIT_MOVEVALUESDOWN, state | MF_BYCOMMAND);

		// Language menu
		HMENU hLanguageMenu = GetSubMenu(hMenu, 2);
		set<LANGID> languages;
		document->getLanguages(languages);

		state = isCurrent ? MF_ENABLED : MF_GRAYED;

		EnableMenuItem(hLanguageMenu, ID_LANGUAGE_ADD,    state | MF_BYCOMMAND);
		EnableMenuItem(hLanguageMenu, ID_LANGUAGE_CHANGE, state | MF_BYCOMMAND);

		if (state == MF_ENABLED && languages.size() <= 1) state = MF_GRAYED;
		EnableMenuItem(hLanguageMenu, ID_LANGUAGE_DELETE,   state | MF_BYCOMMAND);
		EnableMenuItem(hLanguageMenu, ID_LANGUAGE_PREVIOUS, state | MF_BYCOMMAND);
		EnableMenuItem(hLanguageMenu, ID_LANGUAGE_NEXT,     state | MF_BYCOMMAND);
	}
}

void Application::UI_OnFocusChanged()
{
	int  item     = document == NULL ? -1 : (int)ListView_GetItemParam(hActiveListView, ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED));
	bool current  = (document != NULL && ListView_GetNextItem(hHistory, -1, LVNI_SELECTED) == 0);

	EnableWindow(GetDlgItem(hContainer, IDC_EDIT1), item != -1);
	EnableWindow(GetDlgItem(hContainer, IDC_EDIT2), item != -1);
	EnableWindow(GetDlgItem(hContainer, IDC_EDIT3), item != -1);
	EnableWindow(GetDlgItem(hContainer, IDC_LIST1), item != -1);

	bool updownEnable = item >= 0 && document->getType() == Document::DT_INDEX;
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_MOVESTRINGSUP,   updownEnable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_MOVESTRINGSDOWN, updownEnable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_MOVEVALUESUP,   updownEnable);
	SendMessage(hToolbar, TB_ENABLEBUTTON, ID_EDIT_MOVEVALUESDOWN, updownEnable);
}

void Application::OnHistoryFocused()
{
	if (document != NULL)
	{
		// The user has selected a string in the history listview
		int id      = (int)ListView_GetItemParam(hActiveListView, ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED));
		int version = (int)ListView_GetItemParam(hHistory,        ListView_GetNextItem(hHistory,        -1, LVNI_FOCUSED));

		// Get information
		vector<Document::VersionInfo> versions;
		document->getVersions(versions);
		bool readonly = (version < (int)versions.size() - 1);
		bool valid    = true;

		if (id >= 0)
		{
			const Document::StringInfo& str   = document->getString(id, version);
			const wchar_t*              value = document->getValue(id,  version);

			// Set texts
			SetDlgItemText(hContainer, IDC_EDIT1, str.m_name);
			SetDlgItemText(hContainer, IDC_EDIT2, value);
			SetDlgItemText(hContainer, IDC_EDIT3, str.m_comment);
			valid = (readonly || document->isValidName(id));
		}
		else
		{
			SetDlgItemText(hContainer, IDC_EDIT1, L"");
			SetDlgItemText(hContainer, IDC_EDIT2, L"");
			SetDlgItemText(hContainer, IDC_EDIT3, L"");
			readonly = (id != -2);
		}

		// Set read-only properties
		SendMessage(GetDlgItem(hContainer, IDC_EDIT1), EM_SETREADONLY, readonly, 0);
		SendMessage(GetDlgItem(hContainer, IDC_EDIT2), EM_SETREADONLY, readonly, 0);
		SendMessage(GetDlgItem(hContainer, IDC_EDIT3), EM_SETREADONLY, readonly, 0);
		ShowWindow(GetDlgItem(hContainer, IDC_STATIC1), (valid) ? SW_HIDE : SW_SHOW );
	}
}

void Application::OnStringFocused()
{
	// The user has selected a string in the main listview
	int id = (int)ListView_GetItemParam(hActiveListView, ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED));

	if (document != NULL && id != -1)
	{
		ListView_DeleteAllItems(hHistory);

		if (id >= 0)
		{
			// Fill history list
			LVITEM item;
			item.mask     = LVIF_TEXT | LVIF_PARAM;
			item.iItem    = 0;
			item.iSubItem = 0;
			
			int version = document->getActiveVersion();
			for (int v = version; v >= 0; v--)
			{
				const Document::StringInfo& str = document->getString(id, v);
				const wchar_t*              val = document->getValue(id, v);

				set<LANGID> languages;
				document->getLanguages(languages, v);
				if (languages.find(document->getActiveLanguage()) == languages.end())
				{
					break;
				}

				if (v == version || document->hasStringChanged(id, v) || document->hasValueChanged(id, v))
				{
					item.pszText = L"";
					item.lParam  = (LPARAM)v;
					if (v < version)
					{
						wchar_t tmp[16];
						wsprintf(tmp, L"%d", v + 1);
						item.pszText = tmp;
					}

					wstring modified = DateTime(str.m_modified).formatDateShort();

					ListView_InsertItem(hHistory, &item);
					ListView_SetItemText(hHistory, item.iItem, 1, (LPTSTR)str.m_name);
					ListView_SetItemText(hHistory, item.iItem, 2, (LPTSTR)val);
					ListView_SetItemText(hHistory, item.iItem, 3, (LPTSTR)str.m_comment);
					ListView_SetItemText(hHistory, item.iItem, 4, (LPTSTR)modified.c_str());
					item.iItem++;
				}

				if (str.m_flags & SF_NEW)
				{
					break;
				}			
			}

			// Select top entry (current version)
			ListView_SetItemState(hHistory, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		}
		OnHistoryFocused();
	}
	UI_OnFocusChanged();
}

void Application::OnEditChanged(UINT idCtrl)
{
	if (document != NULL)
	{
		// Make sure we update the correct item
		LVITEM item;
		item.mask     = LVIF_PARAM | LVIF_STATE;
		item.iItem    = ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED);
		item.iSubItem = 0;
		ListView_GetItem(hActiveListView, &item);

		int id = (int)item.lParam;
		if (item.iItem != -1 && id != -1)
		{
			wstring value = GetDlgItemStr(hContainer, idCtrl);

			if (id == -2 && !value.empty())
			{
				// The user has typed the first character of <New string here...>
				id = document->addString();
				item.lParam    = id;
				item.state     = LVIS_SELECTED | LVIS_FOCUSED;
				item.stateMask = item.state;

				ListView_SetItemState(hActiveListView, item.iItem, 0, LVIS_SELECTED);
				ListView_InsertItem(hActiveListView, &item);
				UI_OnFocusChanged();

				// Add first item to the history
				LVITEM hist;
				hist.mask     = LVIF_PARAM;
				hist.iItem    = 0;
				hist.iSubItem = 0;
				hist.lParam   = document->getActiveVersion();
				ListView_InsertItem(hHistory, &hist);
			}

			if (id >= 0)
			{
				// Update document
				Document::StringInfo str = document->getString(id);
				Document::String newstr;
				newstr.m_flags    = Document::String::SF_POSITION;
				newstr.m_name     = str.m_name;
				newstr.m_comment  = str.m_comment;
				newstr.m_position = item.iItem;
				newstr.m_value    = document->getValue(id);

				int iSubItem = 0;
				switch (idCtrl)
				{
					case IDC_EDIT1:	newstr.m_name    = value; newstr.m_flags |= Document::String::SF_NAME;    iSubItem = 0; break;
					case IDC_EDIT2: newstr.m_value   = value; newstr.m_flags |= Document::String::SF_VALUE;   iSubItem = 1; break;
					case IDC_EDIT3: newstr.m_comment = value; newstr.m_flags |= Document::String::SF_COMMENT; iSubItem = 2; break;
				}
				document->setString(id, newstr);

				// Update listview
				wstring changed = DateTime(document->getString(id).m_modified).formatShort();
				ListView_SetItemText(hActiveListView, item.iItem, iSubItem, (LPTSTR)value.c_str());
				ListView_SetItemText(hActiveListView, item.iItem, 3, (LPTSTR)changed.c_str());

				// Update history
				ListView_SetItemText(hHistory, 0, 1 + iSubItem, (LPTSTR)value.c_str());
			
				if (idCtrl == IDC_EDIT1)
				{
					// Check for collision
					ShowWindow(GetDlgItem(hContainer, IDC_STATIC1), document->isValidName(id) ? SW_HIDE : SW_SHOW);
				}
			}
		}
	}
}

void Application::FillVersionList(int sel)
{
	SendMessage(hVersionSelect, CB_RESETCONTENT, 0, 0);
	if (document != NULL)
	{
		if (sel < 0 && SendMessage(hVersionSelect, CB_GETCOUNT, 0, 0) > 0)
		{
			sel = (int)SendMessage(hVersionSelect, CB_GETCURSEL, 0, 0);
		}

		vector<Document::VersionInfo> versions;
		document->getVersions(versions);

        const wstring curver = L"(" + LoadString(IDS_CURRENT_VERSION) + L")";

		SendMessage(hVersionSelect, CB_ADDSTRING, 0, (LPARAM)curver.c_str());
		for (size_t i = versions.size() - 1; i > 0; i--)
		{
			wstringstream ss;
			ss << "v" << i << ": " << DateTime(versions[i-1].m_saved).formatDateShort();
			if (!versions[i-1].m_author.empty())
			{
				ss << " (" << versions[i-1].m_author << ")";
			}

			wstring value = ss.str();
			SendMessage(hVersionSelect, CB_ADDSTRING, 0, (LPARAM)value.c_str());
		}
		SendMessage(hVersionSelect, CB_SETCURSEL, max(0, sel), 0);
	}
}

void Application::FillLanguageList()
{
	SendMessage(hLanguageSelect, CB_RESETCONTENT, 0, 0);
	if (document != NULL)
	{
		set<LANGID> languages;
		document->getLanguages(languages);

		// Insert strings
		for (set<LANGID>::const_iterator p = languages.begin(); p != languages.end(); p++)
		{
			wstring name = GetLanguageName(*p);
			int item = (int)SendMessage(hLanguageSelect, CB_ADDSTRING, 0, (LPARAM)name.c_str());
			SendMessage(hLanguageSelect, CB_SETITEMDATA, item, (LPARAM)*p);
		}

		// Find and select active language
		LANGID language = document->getActiveLanguage();
		int nStrings = (int)SendMessage(hLanguageSelect, CB_GETCOUNT, 0, 0);
		for (int i = 0; i < nStrings; i++)
		{
			if (language == (LANGID)SendMessage(hLanguageSelect, CB_GETITEMDATA, i, 0))
			{
				SendMessage(hLanguageSelect, CB_SETCURSEL, i, 0);
				break;
			}
		}
	}
}

// Compares two entries in the list view
int Application::StringCompareFunc(LPARAM lParam1, LPARAM lParam2) const
{
	const vector<Document::StringInfo>& strings = document->getStrings();
	const vector<const wchar_t*>&       values  = document->getValues();

	// Make sure the negative items (i.e, the <new string here>) always end up at the end
	if (lParam1 < 0 && lParam2 < 0) return 0;
	if (lParam1 < 0) return  1;
	if (lParam2 < 0) return -1;

	const Document::StringInfo& str1 = strings[lParam1];
	const Document::StringInfo& str2 = strings[lParam2];
	if (document->getType() == Document::DT_INDEX)
	{
		// Compare positions
		return (int)str1.m_position - (int)str2.m_position;
	}

	const wchar_t* val1 = values[lParam1];
	const wchar_t* val2 = values[lParam2];
	int result;
	switch (abs(sorting))
	{
		case COLUMN_NAME:     result = _wcsicmp(str1.m_name, str2.m_name); break;
		case COLUMN_VALUE:    result = _wcsicmp(val1, val2); break;
		case COLUMN_COMMENT:  result = _wcsicmp(str1.m_comment, str2.m_comment); break;
		case COLUMN_DATE:     result = str1.m_modified < str2.m_modified; break;
		default: result = 0; break;
	}
	return (sorting > 0) ? result : -result;
}

// Completely refill the listview
void Application::FillListView()
{
	if (document != NULL)
	{

		const vector<Document::StringInfo>& strings = document->getStrings();
		const vector<const wchar_t*>&       values  = document->getValues();

		vector<Document::VersionInfo> versions;
		document->getVersions(versions);

		LVITEM item;
		item.mask     = LVIF_PARAM | LVIF_TEXT;
		item.iItem    = 0;
		item.iSubItem = 0;

		// We don't simply clear the listview to save allocation time
		// We just overwrite the existing entries, and insert or delete the difference
		int count = ListView_GetItemCount(hActiveListView);
		for (size_t i = 0; i < strings.size(); i++)
		{
			if (strings[i].m_name != NULL)
			{
				item.lParam  = (LPARAM)i;
				item.pszText = (LPWSTR)strings[i].m_name;
				if (item.iItem < count)
				{
					ListView_SetItem(hActiveListView, &item);
				}
				else
				{
					ListView_InsertItem(hActiveListView, &item);
				}

				wstring changed = DateTime(strings[i].m_modified).formatShort();
				ListView_SetItemText(hActiveListView, item.iItem, 1, (LPWSTR)values[i]);
				ListView_SetItemText(hActiveListView, item.iItem, 2, (LPWSTR)strings[i].m_comment);
				ListView_SetItemText(hActiveListView, item.iItem, 3, (LPWSTR)changed.c_str());
				item.iItem++;
			}
		}

		// Delete what's left
		while (item.iItem < count)
		{
			ListView_DeleteItem(hActiveListView, --count);
		}

		if (document->getActiveVersion() == versions.size() - 1)
		{
            const wstring newstr = L"(" + LoadString(IDS_NEW_STRING) + L")";
			// Add the "new string" item
			item.lParam  = (LPARAM)-2;
            item.pszText = (LPWSTR)newstr.c_str();
			ListView_InsertItem(hActiveListView, &item);
		}

		if (document->getType() == Document::DT_INDEX || sorting != 0)
		{
			// Resort it
			ListView_SortItems(hActiveListView, GenericStringCompareFunc, (LPARAM)this);
		}

		if (ListView_GetItemCount(hActiveListView) > 1)
		{
			// Auto-size "modified" column to fit language-specific date
			ListView_SetColumnWidth(hActiveListView, 3, LVSCW_AUTOSIZE);
		}

		ListView_EnsureVisible(hActiveListView, ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED), FALSE);

		EnableWindow(hActiveListView, TRUE);
	}
	else
	{
		ListView_DeleteAllItems(hActiveListView);
		EnableWindow(hActiveListView, FALSE);
	}
	OnStringFocused();
}

// Reset the list view's values
void Application::FillListViewValues()
{
	if (document != NULL)
	{
		const vector<const wchar_t*>& values  = document->getValues();

		LVITEM item;
		item.mask     = LVIF_PARAM;
		item.iSubItem = 0;

		int nItems = ListView_GetItemCount(hActiveListView);
		for (item.iItem = 0; item.iItem < nItems; item.iItem++)
		{
			ListView_GetItem(hActiveListView, &item);
			if (item.lParam >= 0)
			{
				ListView_SetItemText(hActiveListView, item.iItem, 1, (LPWSTR)values[item.lParam]);
			}
		}
	}
}

void Application::DoNewFile()
{
	document = NULL;

	NEW_FILE_OPTIONS options;
	if (Dialogs::NewFile(hMainWnd, options))
	{
		document = new Document(options.type, options.language);
		if (document->getType() == Document::DT_NAME)
		{
			sorting = COLUMN_NAME;
		}
		UI_OnDocumentChanged();
		FillListView();
		FillLanguageList();
		FillVersionList();
		SetFocus(hActiveListView);
		filename.clear();
	}
}

void Application::DoOpenFile()
{
	WCHAR filename[MAX_PATH];
	filename[0] = L'\0';
	
    wstring filter = LoadString(IDS_FILES_VDF) + wstring(L" (*.vdf)\0*.VDF\0", 15)
                   + LoadString(IDS_FILES_ALL) + wstring(L" (*.*)\0*.*\0", 11);

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize  = sizeof(OPENFILENAME);
	ofn.hwndOwner    = hMainWnd;
	ofn.hInstance    = hInstance;
    ofn.lpstrFilter  = filter.c_str();
	ofn.nFilterIndex = 1;
	ofn.lpstrFile    = filename;
	ofn.nMaxFile     = MAX_PATH;
	ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrDefExt  = L"VDF";

	if (GetOpenFileName(&ofn))
	{
		try
		{
			PhysicalFile file(filename);
			document = new Document(file);
			this->filename = filename;
			
			if (document->getType() == Document::DT_NAME)
			{
				// Sort by name
				sorting = COLUMN_NAME;
			}

			UI_OnDocumentChanged();
			FillListView();
			FillLanguageList();
			FillVersionList();
			SetFocus(hActiveListView);
		}
		catch (wexception&)
		{
			MessageBox(hMainWnd, LoadString(IDS_ERROR_FILE_OPEN).c_str(), NULL, MB_OK | MB_ICONERROR);
		}
	}
}

bool Application::DoSave(bool saveas)
{
	bool changed = document->isModified();
	if (saveas || changed)
	{
		// Save needed
		if (saveas || filename.empty())
		{
			// Filename needed
			WCHAR filename[MAX_PATH];
			filename[0] = L'\0';
			
            wstring filter = LoadString(IDS_FILES_VDF) + wstring(L" (*.vdf)\0*.VDF\0", 15)
                           + LoadString(IDS_FILES_ALL) + wstring(L" (*.*)\0*.*\0", 11);

			OPENFILENAME ofn;
			memset(&ofn, 0, sizeof(OPENFILENAME));
			ofn.lStructSize  = sizeof(OPENFILENAME);
			ofn.hwndOwner    = hMainWnd;
			ofn.hInstance    = hInstance;
            ofn.lpstrFilter  = filter.c_str();
			ofn.nFilterIndex = 1;
			ofn.lpstrFile    = filename;
			ofn.nMaxFile     = MAX_PATH;
			ofn.Flags        = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
			ofn.lpstrDefExt  = L"VDF";

			if (!GetSaveFileName(&ofn))
			{
				// Save aborted
				return false;
			}
			this->filename = filename;
		}

		// Ask user for version details
		VERSION_INFO versioninfo;
		vector<Document::VersionInfo> versions;
		document->getVersions(versions);
		versioninfo.version = (unsigned int)versions.size();
		for (size_t i = 0; i < versions.size(); i++)
		{
			versioninfo.authors.insert(versions[i].m_author);
		}

		if (Dialogs::GetVersionInfo(hMainWnd, versioninfo))
		{
			// Details entered, continue with save
			BusyCursor(IDC_WAIT);
			try
			{
				document->saveVersion(versioninfo.author, versioninfo.notes);
				PhysicalFile file(filename, PhysicalFile::WRITE);
				document->write(file);
				document->increaseVersion();
				document->setActiveVersion();
				FillVersionList();
			}
			catch (wexception&)
			{
				MessageBox(hMainWnd, LoadString(IDS_ERROR_FILE_SAVE).c_str(), NULL, MB_OK | MB_ICONERROR);
			}
		}
	}
	else if (!changed)
	{
		MessageBox(hMainWnd, LoadString(IDS_INFO_NO_CHANGES).c_str(), LoadString(IDS_TITLE_NO_CHANGES).c_str(), MB_OK | MB_ICONINFORMATION);
	}

	return true;
}

void Application::DoDetails()
{
	if (document != NULL)
	{
		Dialogs::FileInfo(hMainWnd, document);
	}
}

void Application::DoImportFile()
{
	if (document != NULL)
	{
		WCHAR filename[MAX_PATH];
		filename[0] = L'\0';
		
        wstring filter = LoadString(IDS_FILES_DAT) + wstring(L" (*.dat)\0*.DAT\0", 15)
                       + LoadString(IDS_FILES_ALL) + wstring(L" (*.*)\0*.*\0", 11);

		OPENFILENAME ofn;
		memset(&ofn, 0, sizeof(OPENFILENAME));
		ofn.lStructSize  = sizeof(OPENFILENAME);
		ofn.hwndOwner    = hMainWnd;
		ofn.hInstance    = hInstance;
        ofn.lpstrFilter  = filter.c_str();
		ofn.nFilterIndex = 1;
		ofn.lpstrFile    = filename;
		ofn.nMaxFile     = MAX_PATH;
		ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
		ofn.lpstrDefExt  = L"DAT";

		if (GetOpenFileName(&ofn))
		{
			// If we don't ask, just merge (without overwrite)
			Document::Method method = Document::AM_NONE;
		
			if (document->getType() == Document::DT_NAME)
			{
				method = Document::AM_UNION;
				if (ListView_GetItemCount(hActiveListView) > 1)
				{
					// We only ask for the merge method for name-indexed documents, 
					// and if the document is not empty
					method = Dialogs::ImportFile(hMainWnd, false);
				}
			}
			else if (document->getType() == Document::DT_INDEX)
			{
				method = Document::AM_APPEND;
				if (ListView_GetItemCount(hActiveListView) > 1)
				{
					method = Dialogs::ImportFile(hMainWnd, true);
				}
			}

			if (method != Document::AM_NONE)
			{
				try
				{
					BusyCursor(IDC_WAIT);

					PhysicalFile file(filename);
					StringList strings(file);
					document->addStrings(strings, method);

					FillListView();
					SetFocus(hActiveListView);
				}
				catch (wexception&)
				{
					MessageBox(hMainWnd, LoadString(IDS_ERROR_FILE_IMPORT).c_str(), NULL, MB_OK | MB_ICONERROR);
				}
			}
		}
	}
}

void Application::DoExportFile()
{
	if (document != NULL)
	{
		// First, check if all names are valid
		const vector<Document::StringInfo>& strings = document->getStrings();
		for (size_t i = 0; i < strings.size(); i++)
		{
			if (strings[i].m_name != NULL)
			{
				if (!document->isValidName((unsigned int)i))
				{
					MessageBox(hMainWnd, LoadString(IDS_ERROR_INVALID_NAMES).c_str(), NULL, MB_OK | MB_ICONERROR);
					return;
				}
			}
		}

		// Create the postfixes
		map<LANGID, wstring> postfixes = document->getPostfixes();
		map<LANGID, bool>    enabled;
		for (map<LANGID, wstring>::iterator p = postfixes.begin(); p != postfixes.end(); p++)
		{
			if (p->second.empty())
			{
				// Uppercased english language name
				p->second = L"_" + GetEnglishLanguageName(p->first);
				transform(p->second.begin(), p->second.end(), p->second.begin(), toupper);
			}
		}

		wstring basename;
		if (Dialogs::ExportFile(hMainWnd, postfixes, enabled, basename))
		{
			BusyCursor(IDC_WAIT);

			size_t slash  = basename.find_last_of(L"\\/");
			size_t period = basename.find_last_of(L".");
			wstring ext = L".DAT";
			if (period != wstring::npos && (slash == wstring::npos || period > slash))
			{
				// Filename has an extension
				ext = basename.substr(period);
				basename = basename.substr(0, period);
			}

			map<LANGID, wstring>::const_iterator p = postfixes.begin();
			map<LANGID, bool>   ::const_iterator e = enabled.begin();

			bool showmsg = false;
			for (;p != postfixes.end(); p++, e++)
			{
				if (e->second)
				{
					// Export this file
					showmsg = true;
					document->setPostfix(p->first, p->second);
					wstring filename = basename + p->second + ext;
					
					try
					{
						PhysicalFile file(filename, PhysicalFile::WRITE);
						document->exportFile(p->first, file);
					}
					catch (wexception&)
					{
						MessageBox(hMainWnd, LoadString(IDS_ERROR_FILE_EXPORT, filename.c_str()).c_str(), NULL, MB_OK | MB_ICONERROR);
						showmsg = false;
						break;
					}
				}
			}

			if (showmsg)
			{
                MessageBox(hMainWnd, LoadString(IDS_INFO_EXPORT_COMPLETE).c_str(), LoadString(IDS_INFORMATION).c_str(), MB_OK | MB_ICONINFORMATION);
			}
		}
	}
}

bool Application::DoClose()
{
	if (document != NULL)
	{
		if (document->isModified())
		{
			switch (MessageBox(hMainWnd, LoadString(IDS_QUERY_SAVE_CHANGES).c_str(), LoadString(IDS_TITLE_SAVE_CHANGES).c_str(), MB_YESNOCANCEL | MB_ICONINFORMATION))
			{
				case IDYES:		if (DoSave()) break;
				case IDCANCEL:	return false;
			}
		}
		delete document;
		document = NULL;
	}

	OnStringFocused();
	UI_OnDocumentChanged();
	return true;
}

void Application::DoAddLanguage()
{
	if (document != NULL)
	{
		set<LANGID> languages;
		GetLanguageList(languages);

		// Remove the languages that are already created
		set<LANGID> existing;
		document->getLanguages(existing);
		for (set<LANGID>::const_iterator p = existing.begin(); p != existing.end(); p++)
		{
			languages.erase(*p);
		}

		LANGID language = Dialogs::AddLanguage(hMainWnd, languages);
		if (language != MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL))
		{
			document->addLanguage(language);
			document->setActiveLanguage(language);
			FillLanguageList();
			FillListViewValues();
			OnStringFocused();
		}
	}
}

void Application::DoChangeLanguage()
{
	if (document != NULL)
	{
		set<LANGID> languages;
		GetLanguageList(languages);

		// Remove the languages that are already created
		set<LANGID> existing;
		document->getLanguages(existing);
		for (set<LANGID>::const_iterator p = existing.begin(); p != existing.end(); p++)
		{
			languages.erase(*p);
		}

		LANGID language = Dialogs::ChangeLanguage(hMainWnd, languages);
		if (language != MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL))
		{
			document->changeLanguage(document->getActiveLanguage(), language);
			document->setActiveLanguage(language);
			FillLanguageList();
			FillListViewValues();
			OnStringFocused();
		}
	}
}

void Application::DoDeleteLanguage()
{
	if (document != NULL)
	{
		set<LANGID> languages;
		document->getLanguages(languages);

		if (languages.size() > 1 && MessageBox(hMainWnd, LoadString(IDS_CONFIRM_LANGUAGE_DELETE).c_str(), LoadString(IDS_CONFIRM).c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			document->deleteLanguage(document->getActiveLanguage());
			FillListViewValues();
			OnStringFocused();
		}
	}
}

void Application::DoPreviousLanguage()
{
	if (document != NULL)
	{
		set<LANGID> languages;
		document->getLanguages(languages);
		LANGID lang = document->getActiveLanguage();

		set<LANGID>::const_iterator cur = languages.begin();
		while (cur != languages.end())
		{
			set<LANGID>::const_iterator next = cur; next++;
			if (next != languages.end() && *next == lang)
			{
				HWND hFocus = GetFocus();
				document->setActiveLanguage(*cur);
				FillListViewValues();
				FillLanguageList();
				OnStringFocused();
				SetFocus(hFocus);

				// Set cursor to end of text if it's an edit box
				wchar_t classname[32];
				GetClassName(hFocus, classname, 32);
				if (_wcsicmp(classname, L"Edit") == 0)
				{
					int count = (int)SendMessage(hFocus, EM_GETLINECOUNT, 0, 0);
					int pos   = 0;
					for (int i = 0; i < count; i++)
					{
						pos += (int)SendMessage(hFocus, EM_LINELENGTH, i, 0);
					}
					SendMessage(hFocus, EM_SETSEL, pos, pos);
				}
				break;
			}
			cur = next;
		}
	}
}

void Application::DoNextLanguage()
{
	if (document != NULL)
	{
		set<LANGID> languages;
		document->getLanguages(languages);
		LANGID lang = document->getActiveLanguage();

		set<LANGID>::const_reverse_iterator cur = languages.rbegin();
		while (cur != languages.rend())
		{
			set<LANGID>::const_reverse_iterator next = cur; next++;
			if (next != languages.rend() && *next == lang)
			{
				HWND hFocus = GetFocus();
				document->setActiveLanguage(*cur);
				FillListViewValues();
				FillLanguageList();
				OnStringFocused();
				SetFocus(hFocus);

				// Set cursor to end of text if it's an edit box
				wchar_t classname[32];
				GetClassName(hFocus, classname, 32);
				if (_wcsicmp(classname, L"Edit") == 0)
				{
					int count = (int)SendMessage(hFocus, EM_GETLINECOUNT, 0, 0);
					int pos   = 0;
					for (int i = 0; i < count; i++)
					{
						pos += (int)SendMessage(hFocus, EM_LINELENGTH, i, 0);
					}
					SendMessage(hFocus, EM_SETSEL, pos, pos);
				}
				break;
			}
			cur = next;
		}
	}
}


void Application::DoInsertString()
{
	if (document != NULL)
	{
		vector<Document::VersionInfo> versions;
		document->getVersions(versions);

		if (document->getActiveVersion() != versions.size() - 1)
		{
			return;
		}

		// Deselect everything
		int iItem = -1;
		while ((iItem = ListView_GetNextItem(hActiveListView, iItem, LVNI_SELECTED)) != -1)
		{
			ListView_SetItemState(hActiveListView, iItem, 0, LVIS_SELECTED);
		}

		if (document->getType() == Document::DT_NAME)
		{
			// Simply focus the last string
			int last = ListView_GetItemCount(hActiveListView) - 1;
			ListView_SetItemState(hActiveListView, last, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			ListView_EnsureVisible(hActiveListView, last, FALSE);
		}
		else
		{
			int id = document->addString();

			// Insert an empty string at the current position
			LVITEM item;
			item.mask      = LVIF_STATE | LVIF_PARAM;
			item.iItem     = ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED);
			item.iSubItem  = 0;
			item.lParam    = (LPARAM)id;
			item.state     = LVIS_SELECTED | LVIS_FOCUSED;
			item.stateMask = item.state;
			ListView_InsertItem(hActiveListView, &item);
			
			Document::String str;
			str.m_position = item.iItem;
			document->setString(id, str);

			// Increase the positions of all following strings by one
			item.mask = LVIF_PARAM;
			int count = ListView_GetItemCount(hActiveListView) - 1;
			for (item.iItem++; item.iItem < count; item.iItem++)
			{
				ListView_GetItem(hActiveListView, &item);
				document->setPosition((unsigned int)item.lParam, item.iItem);
			}
		}
		OnStringFocused();
		SetFocus(GetDlgItem(hContainer, IDC_EDIT1));
	}
}

static bool MatchString(LCID locale, const wchar_t* str1, const wchar_t* str2, bool matchCase, bool matchWord)
{
	size_t len1 = wcslen(str1);
	size_t len2 = wcslen(str2);
	for (const wchar_t* src = str1; len1 >= len2 && *src != L'\0'; src++, len1--)
	{
		if (CompareString(locale, matchCase ? 0 : NORM_IGNORECASE, src, (int)len2, str2, -1) == CSTR_EQUAL)
		{
			if (!matchWord ||
				// The characters in front of and behind the found string must not be characters
			    ((src       == str1  || !IsCharAlphaNumeric(*(src-1))) &&
				 (src[len2] == L'\0' || !IsCharAlphaNumeric(src[len2]))))
			{
				return true;
			}
		}
	}
	return false;
}

void Application::DoFind(bool showDialog)
{
	if (findinfo.method == FIND_INFO::FM_NONE)
	{
		showDialog = true;
		findinfo.method = FIND_INFO::FM_FROMCURSOR;
	}

	if (showDialog && ListView_GetSelectedCount(hActiveListView) > 1)
	{
		findinfo.method = FIND_INFO::FM_SELECTION;
	}

	if (!showDialog || Dialogs::Find(hMainWnd, findinfo))
	{
		const vector<Document::StringInfo>& strings = document->getStrings();
		const vector<const wchar_t*>&       values  = document->getValues();

		const wchar_t* term = findinfo.term.c_str();

		LCID locale  = MAKELCID(document->getActiveLanguage(), SORT_DEFAULT);
		LCID english = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), SORT_DEFAULT);

		LVITEM item;
		item.mask     = LVIF_PARAM;
		item.iSubItem = 0;
		item.iItem    = -1;

		int count = 0;
		switch (findinfo.method)
		{
			case FIND_INFO::FM_FROMCURSOR:
				item.iItem = ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED) + 1;
				count = ListView_GetItemCount(hActiveListView) - item.iItem + 1;
				break;

			case FIND_INFO::FM_FROMSTART:
				item.iItem = 0;
				count = ListView_GetItemCount(hActiveListView);
				break;
			
			case FIND_INFO::FM_SELECTION:
				item.iItem = -1;
				count = ListView_GetSelectedCount(hActiveListView);
				break;
		}

		bool matches = false;
		for (int i = 0; i < count; i++, item.iItem++)
		{
			if (findinfo.method == FIND_INFO::FM_SELECTION)
			{
				item.iItem = ListView_GetNextItem(hActiveListView, item.iItem, LVNI_SELECTED);
			}
			ListView_GetItem(hActiveListView, &item);
			int id = (int)item.lParam;

			if (id < 0)
			{
				// Not a valid string, deselect it
				ListView_SetItemState(hActiveListView, item.iItem, 0, LVIS_SELECTED)
			}
			else if ((findinfo.searchName    && MatchString(english, strings[id].m_name,    term, findinfo.matchCase, findinfo.matchWord)) ||
				     (findinfo.searchValue   && MatchString(locale,  values[id],            term, findinfo.matchCase, findinfo.matchWord)) ||
				     (findinfo.searchComment && MatchString(english, strings[id].m_comment, term, findinfo.matchCase, findinfo.matchWord)))
			{
				// It's a match
				matches = true;
				ListView_SetItemState(hActiveListView, item.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				if (!findinfo.selectAll)
				{
					// Deselect everything that comes before and after it and stop searching
					ListView_EnsureVisible(hActiveListView, item.iItem, FALSE);
					int iItem = -1;
					while ((iItem = ListView_GetNextItem(hActiveListView, iItem, LVNI_SELECTED)) != -1)
					{
						if (iItem != item.iItem)
						{
							ListView_SetItemState(hActiveListView, iItem, 0, LVIS_SELECTED);
						}
					}
					break;
				}
			}
			else
			{
				// Not a match, deselect it
				ListView_SetItemState(hActiveListView, item.iItem, 0, LVIS_SELECTED);
			}
		}

		if (!matches)
		{
			MessageBox(hMainWnd, LoadString(IDS_ERROR_TEXT_NOT_FOUND).c_str(), LoadString(IDS_INFORMATION).c_str(), MB_OK | MB_ICONINFORMATION);
		}
		OnStringFocused();
	}
}

void Application::DoSelectAll()
{
	HWND hFocus = GetFocus();
	if (hFocus == hActiveListView)
	{
		int count = ListView_GetItemCount(hActiveListView);
		for (int i = 0; i < count; i++)
		{
			ListView_SetItemState(hActiveListView, i, LVIS_SELECTED, LVIS_SELECTED );
		}
	}
}

static bool CopyStringFromClipboard(HWND hwndOwner, wstring& value)
{
	if (!OpenClipboard(hwndOwner))
	{
		return false;
	}

	try
	{
		// Get handle
		HANDLE hMem = GetClipboardData(CF_UNICODETEXT);
		if (hMem != NULL)
		{
			// Copy string
			size_t   size = GlobalSize(hMem);
			wchar_t* data = (wchar_t*)GlobalLock(hMem);
			value = wstring(data, size / sizeof(wchar_t));
			GlobalUnlock(hMem);
		}
		else
		{
			hMem = GetClipboardData(CF_TEXT);
			if (hMem == NULL)
			{
				CloseClipboard();
				return false;
			}
			size_t size = GlobalSize(hMem);
			char*  data = (char*)GlobalLock(hMem);
			string str = string(data, size / sizeof(char));
			GlobalUnlock(hMem);
			value = AnsiToWide(str);
		}
	}
	catch(...)
	{
		CloseClipboard();
		throw;
	}
	CloseClipboard();
	return true;
}

static bool CopyStringToClipboard(HWND hwndOwner, const wstring& valueW)
{
	string valueA = WideToAnsi(valueW);

	if (!OpenClipboard(hwndOwner))
	{
		return false;
	}

	HGLOBAL hMemW = GlobalAlloc(GMEM_MOVEABLE, (valueW.length() + 1) * sizeof(wchar_t));
	if (hMemW == NULL)
	{ 
		CloseClipboard();
        return false;
    }

	HGLOBAL hMemA = GlobalAlloc(GMEM_MOVEABLE, (valueA.length() + 1) * sizeof(char));
	if (hMemA == NULL) 
	{ 
		GlobalFree(hMemW);
		CloseClipboard();
        return false;
    }

	// Lock the handle and copy the text to the buffer. 
	wchar_t* dataW = (wchar_t*)GlobalLock(hMemW);
	memcpy(dataW, valueW.c_str(), valueW.length() * sizeof(wchar_t)); 
	dataW[valueW.length()] = L'\0';
	GlobalUnlock(dataW);

	char* dataA = (char*)GlobalLock(hMemA);
	memcpy(dataA, valueA.c_str(), valueA.length() * sizeof(char)); 
	dataW[valueA.length()] = L'\0';
	GlobalUnlock(dataA);

    // Place the handle on the clipboard. 
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, hMemW);
	SetClipboardData(CF_TEXT, hMemA);
	CloseClipboard();
	return true;
}

static std::wstring TSVEscape(const wchar_t* src)
{
    std::wstring str(src);
    std::wstring::size_type ofs = 0;
    while ((ofs = str.find_first_of(L"\"", ofs + 1)) != std::wstring::npos)
    {
        str.replace(ofs++, 1, L"\"\"");
    }
    return str;
}

bool Application::DoCopy()
{
	HWND hWnd = GetFocus();
	if (hWnd == GetDlgItem(hContainer, IDC_EDIT1) ||
		hWnd == GetDlgItem(hContainer, IDC_EDIT2) ||
		hWnd == GetDlgItem(hContainer, IDC_EDIT3))
	{
		// Copy the edit box contents as normal text
        SendMessage(hWnd, WM_COPY, 0, 0);
        return true;
	}
	else if (hWnd == hActiveListView)
	{
		// Copy the selected 'tuples' as Tab-Seperated strings so i.e., Excel will accept them
		LVITEM item;
		item.iSubItem = 0;
		item.mask     = LVIF_PARAM;
		item.iItem    = -1;

		const vector<Document::StringInfo>& strings = document->getStrings();
		const vector<const wchar_t*>&       values  = document->getValues();

		size_t   size   = 1024;
		size_t   length = 0;
		wchar_t* buffer = NULL;

		BusyCursor(IDC_WAIT);
		while ((item.iItem = ListView_GetNextItem(hActiveListView, item.iItem, LVNI_SELECTED)) != -1)
		{
			ListView_GetItem(hActiveListView, &item);
			int id = (int)item.lParam;
			if (id >= 0)
			{
				wstring line = L"\"" + TSVEscape(strings[id].m_name) +
					L"\"\t\"" +	TSVEscape(values[id]) +
					L"\"\t\"" +	TSVEscape(strings[id].m_comment) +
					L"\"\t" +	DateTime(strings[id].m_modified).formatShort() +
					L"\r\n";

				if (buffer == NULL || length + line.length() >= size)
				{
					// Expand the buffer
					size = size * 2;
					wchar_t* tmp = (wchar_t*)realloc(buffer, (size + 1) * sizeof(wchar_t));
					if (tmp == NULL)
					{
						free(buffer);
						buffer = NULL;
						break;
					}
					buffer = tmp;
				}
				memcpy(buffer + length, line.c_str(), line.length() * sizeof(wchar_t));
				length += line.length();
			}
		}

		if (buffer != NULL)
		{
			buffer[length] = L'\0';
			return CopyStringToClipboard(hWnd, buffer);
		}
	}
	return false;
}

bool Application::DoPaste()
{
	HWND hWnd = GetFocus();
	if (hWnd == GetDlgItem(hContainer, IDC_EDIT1) ||
		hWnd == GetDlgItem(hContainer, IDC_EDIT2) ||
		hWnd == GetDlgItem(hContainer, IDC_EDIT3))
	{
        SendMessage(hWnd, WM_PASTE, 0, 0);
		OnEditChanged(GetDlgCtrlID(hWnd));
		return true;
	}
	
	if (hWnd == hActiveListView)
	{
		// Paste tab-seperated clipboard text into listview
		wstring value;
		if (!CopyStringFromClipboard(hWnd, value))
		{
			return false;
		}

		wchar_t* str = (wchar_t*)value.c_str();

		LVITEM item;
		item.mask     = LVIF_PARAM;
		item.iItem    = ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED);
		item.iSubItem = 0;

		if (item.iItem == -1)
		{
			// Add at the end
			item.iItem = ListView_GetItemCount(hActiveListView) - 1;
		}

		BusyCursor(IDC_WAIT);
		while (str != NULL && *str != L'\0')
		{
			// Get the line
			wchar_t* nl = wcschr(str,L'\n');
			if (nl != NULL) {
				if (nl > str && *(nl-1) == L'\r') *(nl-1) = L'\0';
				*nl++ = L'\0';
			}

			// Seperate the line into columns
			wchar_t* columns[3] = {L"", L"", L""};
			
			for (int i = 0; i < 3 && str != NULL; i++)
			{
				wchar_t* tab = wcschr(str, L'\t');
				if (tab != NULL) *tab++ = '\0';
				columns[i] = str;
				str = tab;
			}

			// Add the string
			Document::String newstr;
			newstr.m_position = item.iItem;
			newstr.m_name     = columns[0];
			newstr.m_value    = columns[1];
			newstr.m_comment  = columns[2];

			unsigned int id = document->addString();
			document->setString(id, newstr);
			const Document::StringInfo& info = document->getString(id);
			wstring modified = DateTime(info.m_modified).formatShort();

			// Insert the item
			item.lParam = id;
			ListView_InsertItem(hActiveListView, &item);
			ListView_SetItemText(hActiveListView, item.iItem, 0, columns[0]);
			ListView_SetItemText(hActiveListView, item.iItem, 1, columns[1]);
			ListView_SetItemText(hActiveListView, item.iItem, 2, columns[2]);
			ListView_SetItemText(hActiveListView, item.iItem, 3, (LPWSTR)modified.c_str());
			item.iItem++;

			str = nl;
		}

		if (document->getType() == Document::DT_INDEX)
		{
			// Move the position of everything after up
			int count = ListView_GetItemCount(hActiveListView);
			while (item.iItem < count)
			{
				ListView_GetItem(hActiveListView, &item);
				int id = (int)item.lParam;
				if (id >= 0)
				{
					document->setPosition(id, item.iItem);
				}
				item.iItem++;
			}
		}
		return true;
	}
	return false;
}

bool Application::DoPasteSpecial(Document::Method method)
{
	HWND hWnd = GetFocus();
	if (hWnd != hActiveListView)
	{
		// Perform an ordinary paste when it's not the listview
		return DoPaste();
	}

	// Paste tab-seperated clipboard text into listview
	wstring value;
	if (!CopyStringFromClipboard(hWnd, value))
	{
		return false;
	}

	wchar_t* str = (wchar_t*)value.c_str();

	LVITEM item;
	item.mask     = LVIF_PARAM;
	item.iItem    = ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED);
	item.iSubItem = 0;

	BusyCursor(IDC_WAIT);
	
	StringList strings;

	while (str != NULL && *str != L'\0')
	{
		// Get the line
		wchar_t* nl = wcschr(str,L'\n');
		if (nl != NULL) {
			if (nl > str && *(nl-1) == L'\r') *(nl-1) = L'\0';
			*nl++ = L'\0';
		}

		// Seperate the line into columns
		wchar_t* columns[3] = {L"", L"", L""};
		
		for (int i = 0; i < 3 && str != NULL; i++)
		{
			wchar_t* tab = wcschr(str, L'\t');
			if (tab != NULL) *tab++ = '\0';
			columns[i] = str;
			str = tab;
		}

		strings.add(columns[0], columns[1], columns[2]);

		str = nl;
	}

	strings.sort();
	document->addStrings(strings, method);
	FillListView();
	SetFocus(hActiveListView);
	return true;
}

void Application::DoDelete()
{
	if (document != NULL)
	{
	    HWND hWnd = GetFocus();
	    if (hWnd == GetDlgItem(hContainer, IDC_EDIT1) ||
		    hWnd == GetDlgItem(hContainer, IDC_EDIT2) ||
		    hWnd == GetDlgItem(hContainer, IDC_EDIT3))
	    {
            SendMessage(hWnd, WM_CLEAR, 0, 0);
            OnEditChanged(GetDlgCtrlID(hWnd));
        }
        else if (hWnd == hActiveListView)
        {
		    HCURSOR hCurrentCursor = GetCursor();
		    SetCursor(LoadCursor(NULL, IDC_WAIT));
		    LVITEM item;
		    item.mask     = LVIF_PARAM;
		    item.iSubItem = 0;
		    item.iItem    = -1;

		    // Delete all selected strings
		    BusyCursor(IDC_WAIT);
		    while ((item.iItem = ListView_GetNextItem(hActiveListView, item.iItem, LVNI_SELECTED)) != -1)
		    {
			    ListView_GetItem(hActiveListView, &item);
			    if (item.lParam >= 0)
			    {
				    document->deleteString((unsigned int)item.lParam);
				    ListView_DeleteItem(hActiveListView, item.iItem-- );
			    }
		    }

		    // Select focused string
		    item.iItem = ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED);
		    ListView_SetItemState(hActiveListView, item.iItem, LVNI_SELECTED, LVNI_SELECTED);

		    OnStringFocused();
        }
	}
}

void Application::DoFindNextInvalid()
{
	if (document != NULL)
	{
		const vector<Document::StringInfo>& strings = document->getStrings();
		
		LVITEM item;
		item.mask     = LVIF_PARAM;
		int count     = ListView_GetItemCount(hActiveListView);
		item.iItem    = ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED) + 1;
		item.iSubItem = 0;

		BusyCursor(IDC_WAIT);
		while (item.iItem < count)
		{
			ListView_GetItem(hActiveListView, &item);
			int id = (int)item.lParam;
			if (id >= 0 && strings[id].m_name != NULL && !document->isValidName(id))
			{
				// We found one, deselect all currently selected items
				int iItem = -1;
				while ((iItem = ListView_GetNextItem(hActiveListView, iItem, LVNI_SELECTED)) != -1)
				{
					ListView_SetItemState(hActiveListView, iItem, 0, LVIS_SELECTED);
				}

				// And select and show the invalid one
				ListView_SetItemState(hActiveListView,  item.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				ListView_EnsureVisible(hActiveListView, item.iItem, FALSE);
				OnStringFocused();
				break;
			}
			item.iItem++;
		}

		if (item.iItem == count)
		{
			MessageBeep(MB_ICONEXCLAMATION);
		}
	}
}

void Application::SwapStrings(int id1, int pos1, int id2, int pos2)
{
	if (id1 >= 0 && id2 >= 0)
	{
		// Switch positions
		document->setPosition(id1, pos2);
		document->setPosition(id2, pos1);

		// Update listview
		const vector<Document::StringInfo>& strings = document->getStrings();
		const vector<const wchar_t*>&       values  = document->getValues();

		LVITEM item;
		item.iSubItem = 0;

		item.mask   = LVIF_PARAM;
		item.iItem  = pos2;
		item.lParam = id1;
		ListView_SetItem(hActiveListView, &item);
		wstring modified = DateTime(strings[id1].m_modified).formatShort();
		ListView_SetItemText(hActiveListView, item.iItem, 0, (LPWSTR)strings[id1].m_name);
		ListView_SetItemText(hActiveListView, item.iItem, 1, (LPWSTR)values [id1]);
		ListView_SetItemText(hActiveListView, item.iItem, 2, (LPWSTR)strings[id1].m_comment);
		ListView_SetItemText(hActiveListView, item.iItem, 3, (LPWSTR)modified.c_str());

		item.iItem  = pos1;
		item.lParam = id2;
		ListView_SetItem(hActiveListView, &item);
		modified = DateTime(strings[id2].m_modified).formatShort();
		ListView_SetItemText(hActiveListView, item.iItem, 0, (LPWSTR)strings[id2].m_name);
		ListView_SetItemText(hActiveListView, item.iItem, 1, (LPWSTR)values [id2]);
		ListView_SetItemText(hActiveListView, item.iItem, 2, (LPWSTR)strings[id2].m_comment);
		ListView_SetItemText(hActiveListView, item.iItem, 3, (LPWSTR)modified.c_str());
	}
}

void Application::SwapValues(int id1, int pos1, int id2, int pos2)
{
	if (id1 >= 0 && id2 >= 0)
	{
		const vector<const wchar_t*>& values = document->getValues();

		Document::String str1;
		str1.m_flags = Document::String::SF_VALUE;
		str1.m_value = values[id2];

		Document::String str2;
		str2.m_flags = Document::String::SF_VALUE;
		str2.m_value = values[id1];

		document->setString(id1, str1);
		document->setString(id2, str2);

		// Update listview
		const vector<Document::StringInfo>& strings = document->getStrings();

		wstring modified = DateTime(strings[id1].m_modified).formatShort();
		ListView_SetItemText(hActiveListView, pos1, 1, (LPWSTR)str1.m_value.c_str());
		ListView_SetItemText(hActiveListView, pos1, 3, (LPWSTR)modified.c_str());

		modified = DateTime(strings[id2].m_modified).formatShort();
		ListView_SetItemText(hActiveListView, pos2, 1, (LPWSTR)str2.m_value.c_str());
		ListView_SetItemText(hActiveListView, pos2, 3, (LPWSTR)modified.c_str());
	}
}

void Application::DoMoveStrings(bool up, void (Application::*callback)(int, int, int, int))
{
	if (document != NULL && document->getType() == Document::DT_INDEX && ListView_GetSelectedCount(hActiveListView) > 0)
	{
		LVITEM item1;
		item1.iItem     = -1;
		item1.iSubItem  = 0;
		item1.mask      = LVIF_PARAM | LVIF_STATE;
		item1.stateMask = LVIS_FOCUSED;

		LVITEM item2 = item1;

		set<int> selection;
		while ((item1.iItem = ListView_GetNextItem(hActiveListView, item1.iItem, LVNI_SELECTED)) != -1)
		{
			selection.insert(item1.iItem);
		}

		if (up)
		{
			// When moving up we need to start swapping at the top and move down
			for (set<int>::const_iterator p = selection.begin(); p != selection.end(); p++)
			{
				item1.iItem = *p - 0;
				item2.iItem = *p - 1;
				ListView_GetItem(hActiveListView, &item1);
				ListView_GetItem(hActiveListView, &item2);

				(this->*callback)((int)item2.lParam, item2.iItem, (int)item1.lParam, item1.iItem);

				ListView_SetItemState(hActiveListView, item1.iItem, 0, LVIS_SELECTED);
				ListView_SetItemState(hActiveListView, item2.iItem, LVIS_SELECTED, LVIS_SELECTED);
				if (item1.state & LVIS_FOCUSED)
				{
					ListView_SetItemState(hActiveListView, item2.iItem, LVIS_FOCUSED, LVIS_FOCUSED);
				}
			}
		}
		else
		{
			// When moving down we need to start swapping at the bottom and move up
			for (set<int>::const_reverse_iterator p = selection.rbegin(); p != selection.rend(); p++)
			{
				item1.iItem = *p + 0;
				item2.iItem = *p + 1;
				ListView_GetItem(hActiveListView, &item1);
				ListView_GetItem(hActiveListView, &item2);

				(this->*callback)((int)item1.lParam, item1.iItem, (int)item2.lParam, item2.iItem);

				ListView_SetItemState(hActiveListView, item1.iItem, 0, LVIS_SELECTED);
				ListView_SetItemState(hActiveListView, item2.iItem, LVIS_SELECTED, LVIS_SELECTED);
				if (item1.state & LVIS_FOCUSED)
				{
					ListView_SetItemState(hActiveListView, item2.iItem, LVIS_FOCUSED, LVIS_FOCUSED);
				}
			}
		}

		// Focus could have changed, update
		OnStringFocused();
	}
}

bool Application::DoMenuItem(WORD id)
{
	switch (id)
	{
		case ID_FILE_NEW:		if (!DoClose()) return false; DoNewFile();  break;
		case ID_FILE_OPEN:		if (!DoClose()) return false; DoOpenFile(); break;
		case ID_FILE_CLOSE:		if (!DoClose()) return false; break;
		case ID_FILE_EXIT:		if (!DoClose()) return false; DestroyWindow(hMainWnd); break;
		case ID_FILE_SAVE:		DoSave(); break;
		case ID_FILE_SAVEAS:	DoSave(true);	break;
		case ID_FILE_DETAILS:	DoDetails();	break;
		case ID_FILE_IMPORT:	DoImportFile(); break;
		case ID_FILE_EXPORT:	DoExportFile(); break;

		case ID_EDIT_COPY:				DoCopy();   break;
		case ID_EDIT_PASTE:				DoPaste();  break;
		case ID_EDIT_DELETE:			DoDelete(); break;
		case ID_EDIT_CUT:				if (DoCopy()) DoDelete();   break;
		case ID_EDIT_ADDSTRING:			DoInsertString(); break;
		case ID_EDIT_FIND:				DoFind(true); break;
		case ID_EDIT_FINDNEXT:			DoFind(false); break;
		case ID_EDIT_SELECTALL:			DoSelectAll(); break;
		case ID_EDIT_FINDNEXTINVALID:	DoFindNextInvalid(); break;
		case ID_EDIT_MOVESTRINGSUP:		DoMoveStrings(true,  &Application::SwapStrings);  break;
		case ID_EDIT_MOVESTRINGSDOWN:	DoMoveStrings(false, &Application::SwapStrings); break;
		case ID_EDIT_MOVEVALUESUP:		DoMoveStrings(true,  &Application::SwapValues);  break;
		case ID_EDIT_MOVEVALUESDOWN:	DoMoveStrings(false, &Application::SwapValues); break;

		case ID_PASTESPECIAL_UNION:			DoPasteSpecial(Document::AM_UNION_OVERWRITE); break;
		case ID_PASTESPECIAL_INTERSECTION:	DoPasteSpecial(Document::AM_INTERSECT);  break;
		case ID_PASTESPECIAL_DIFFERENCE:	DoPasteSpecial(Document::AM_DIFFERENCE); break;

		case ID_LANGUAGE_ADD:	 DoAddLanguage(); break;
		case ID_LANGUAGE_CHANGE: DoChangeLanguage(); break;
		case ID_LANGUAGE_DELETE: DoDeleteLanguage(); break;
		case ID_LANGUAGE_PREVIOUS: DoPreviousLanguage(); break;
		case ID_LANGUAGE_NEXT:     DoNextLanguage(); break;

		case ID_HELP_ABOUT:
			MessageBox(hMainWnd,
				L"String Editor v1.4.\n"
				L"Copyright (C) 2008 Mike Lankamp.\n\n"
				L"Distribute freely.\n\n",
				L"About", MB_OK | MB_ICONINFORMATION);
			break;
	}
	return true;
}

BOOL Application::DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			     hHistory   = GetDlgItem(hWnd, IDC_LIST1);
			HWND hListView1 = GetDlgItem(hWnd, IDC_LIST2);
			HWND hListView2 = GetDlgItem(hWnd, IDC_LIST3);

			LVCOLUMN col;
			col.mask     = LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
			col.iSubItem = 0;

            wstring title;

			// Populate history list view columns
			                                        col.pszText = L"#";	         		 col.cx = 20;	ListView_InsertColumn(hHistory, 0, &col);
            title = LoadString(IDS_COLUMN_NAME);    col.pszText = (LPWSTR)title.c_str(); col.cx = 100;	ListView_InsertColumn(hHistory, 1, &col);
			title = LoadString(IDS_COLUMN_VALUE);   col.pszText = (LPWSTR)title.c_str(); col.cx = 100;	ListView_InsertColumn(hHistory, 2, &col);
			title = LoadString(IDS_COLUMN_COMMENT); col.pszText = (LPWSTR)title.c_str(); col.cx = 75;	ListView_InsertColumn(hHistory, 3, &col);
			title = LoadString(IDS_COLUMN_DATE);    col.pszText = (LPWSTR)title.c_str(); col.cx = 75;	ListView_InsertColumn(hHistory, 4, &col);

			// Initialize list views
			col.mask     = LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
			col.iSubItem = 0;

            title        = LoadString(IDS_COLUMN_LAST_MODIFIED);
            col.pszText	 = (LPWSTR)title.c_str();
			col.cx		 = 100;
			ListView_InsertColumn(hListView1, 0, &col);
			ListView_InsertColumn(hListView2, 0, &col);

            title        = LoadString(IDS_COLUMN_COMMENT);
            col.pszText	 = (LPWSTR)title.c_str();
			col.cx		 = 150;
			ListView_InsertColumn(hListView1, 0, &col);
			ListView_InsertColumn(hListView2, 0, &col);

            title        = LoadString(IDS_COLUMN_VALUE);
            col.pszText	 = (LPWSTR)title.c_str();
			col.cx		 = 300;
			ListView_InsertColumn(hListView1, 0, &col);
			ListView_InsertColumn(hListView2, 0, &col);

            title        = LoadString(IDS_COLUMN_NAME);
            col.pszText	 = (LPWSTR)title.c_str();
			col.cx		 = 200;
			ListView_InsertColumn(hListView1, 0, &col);
			ListView_InsertColumn(hListView2, 0, &col);

			ListView_SetExtendedListViewStyle(hHistory,   LVS_EX_FULLROWSELECT);
			ListView_SetExtendedListViewStyle(hListView1, LVS_EX_FULLROWSELECT);
			ListView_SetExtendedListViewStyle(hListView2, LVS_EX_FULLROWSELECT);
			break;
		}

		case WM_COMMAND:
			if (!locked && lParam != 0)
			{
				locked = true;
				if (HIWORD(wParam) == EN_CHANGE)
				{
					OnEditChanged(LOWORD(wParam));
				}
				locked = false;
			}
			break;

		case WM_NOTIFY:
			if (!locked)
			{
				locked = true;
				NMHDR* nmhdr = (NMHDR*)lParam;
				switch (nmhdr->code)
				{
					case NM_DBLCLK:
					{
						// Select name field on a double click
						if (ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED) != -1)
						{
							SetFocus(GetDlgItem(hContainer, IDC_EDIT1));
						}
						break;
					}

					case LVN_ITEMCHANGED:
					{
						const NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
						if (pnmv->uChanged & LVIF_STATE && pnmv->uNewState & LVIS_FOCUSED)
						{
							if (nmhdr->idFrom == IDC_LIST1)
							{
								// User has selected a different history string
								OnHistoryFocused();
							}
							else
							{
								// User has selected a different string
								OnStringFocused();
							}
						}
						break;
					}

					case LVN_COLUMNCLICK:
						if (document != NULL)
						{
							const NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
							vector<Document::VersionInfo> versions;
							document->getVersions(versions);

							int column = COLUMN_NAME + pnmv->iSubItem;
							if (versions.size() > 0)
							{
								sorting = (sorting == column) ? -column : column;
								ListView_SortItems(hActiveListView, GenericStringCompareFunc, (LPARAM)this);
								ListView_EnsureVisible(hActiveListView, ListView_GetNextItem(hActiveListView, -1, LVNI_FOCUSED), FALSE);
							}
						}
						break;

					case LVN_KEYDOWN:
					{
						NMLVKEYDOWN* pnkd = (NMLVKEYDOWN*)lParam;
						switch (pnkd->wVKey)
						{
							case VK_RETURN:
							{
								HWND hEdit = GetDlgItem(hWnd, IDC_EDIT1);
								if (IsWindowEnabled(hEdit))
								{
									SetFocus(hEdit);
								}
								break;
							}

							case VK_DELETE:
								DoMenuItem(ID_EDIT_DELETE );
								break;

							case VK_INSERT:
								DoMenuItem(ID_EDIT_ADDSTRING );
						}
						break;
					}
				}
				locked = false;
			}
			break;

		case WM_SIZE:
		{
			HWND hPropsGroup   = GetDlgItem(hWnd, IDC_GROUP1);
			HWND hHistoryGroup = GetDlgItem(hWnd, IDC_GROUP2);
			HWND hListview1    = GetDlgItem(hWnd, IDC_LIST2);
			HWND hListview2    = GetDlgItem(hWnd, IDC_LIST3);
			HWND hWarning      = GetDlgItem(hWnd, IDC_STATIC1);
			HWND hStatic1	   = GetDlgItem(hWnd, IDC_STATIC2);
			HWND hStatic2	   = GetDlgItem(hWnd, IDC_STATIC3);
			HWND hStatic3	   = GetDlgItem(hWnd, IDC_STATIC4);
			HWND hName         = GetDlgItem(hWnd, IDC_EDIT1);
			HWND hValue        = GetDlgItem(hWnd, IDC_EDIT2);
			HWND hComment      = GetDlgItem(hWnd, IDC_EDIT3);

			// Location of vertical division
			int div = LOWORD(lParam) * 3 / 5;

			// Move history group
			RECT rect1, rect2, rect3;
			GetWindowRect(hHistoryGroup, &rect1);
			int height = rect1.bottom - rect1.top;
			GetWindowRect(hHistory, &rect2);
			MoveWindow(hHistoryGroup, div + 2, HIWORD(lParam) - height - 4, LOWORD(lParam) - 6 - div, height, TRUE);
			GetWindowRect(hHistoryGroup, &rect3);

			// Move history list
			POINT point1, point2;
			point1.x = rect2.left - rect1.left + rect3.left;
			point1.y = rect2.top  - rect1.top  + rect3.top;
			point2.x = rect2.right  - rect1.right  + rect3.right;
			point2.y = rect2.bottom - rect1.bottom + rect3.bottom;
			ScreenToClient(hWnd, &point1);
			ScreenToClient(hWnd, &point2);
			MoveWindow(hHistory, point1.x, point1.y, point2.x - point1.x, point2.y - point1.y, TRUE);

			RECT rects[9];
			GetWindowRect(hStatic1,    &rects[0]);
			GetWindowRect(hStatic2,    &rects[1]);
			GetWindowRect(hStatic3,    &rects[2]);
			GetWindowRect(hName,       &rects[3]);
			GetWindowRect(hValue,      &rects[4]);
			GetWindowRect(hComment,    &rects[5]);
			GetWindowRect(hWarning,    &rects[6]);
			GetWindowRect(hPropsGroup, &rects[7]);

			// Move properties group
			height = rects[7].bottom - rects[7].top;
			MoveWindow(hPropsGroup, 4, HIWORD(lParam) - height - 4, div - 6, height, TRUE);
			GetWindowRect(hPropsGroup, &rects[8]);

			// Move edit boxes
			POINT points[7];
			for (int i = 0; i < 7; i++)
			{
				points[i].x = rects[i].left;
				points[i].y = rects[i].top;
				rects[i].right  = rects[i].right  - rects[i].left;
				rects[i].bottom = rects[i].bottom - rects[i].top;
				ScreenToClient(hWnd, &points[i]);
				points[i].x = points[i].x - rects[7].left + rects[8].left;
				points[i].y = points[i].y - rects[7].top  + rects[8].top;
			}

			int width = (rects[8].right - rects[8].left) - points[3].x - 4;

			MoveWindow(hStatic1, points[0].x, points[0].y, rects[0].right, rects[0].bottom, TRUE);
			MoveWindow(hStatic2, points[1].x, points[1].y, rects[1].right, rects[1].bottom, TRUE);
			MoveWindow(hStatic3, points[2].x, points[2].y, rects[2].right, rects[2].bottom, TRUE);
			MoveWindow(hName,    points[3].x, points[3].y, width - 20, rects[3].bottom, TRUE);
			MoveWindow(hValue,   points[4].x, points[4].y, width,      rects[4].bottom, TRUE);
			MoveWindow(hComment, points[5].x, points[5].y, width,      rects[5].bottom, TRUE);
			MoveWindow(hWarning, points[3].x + width - 16, points[3].y + (rects[3].bottom - 16) / 2, 16, 16, TRUE);

			// Move listviews
			MoveWindow(hListview1, 0, 0, LOWORD(lParam), HIWORD(lParam) - height - 4, TRUE);
			MoveWindow(hListview2, 0, 0, LOWORD(lParam), HIWORD(lParam) - height - 4, TRUE);
			break;
		}
	}
	return FALSE;
}

LRESULT Application::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_CREATE:
		{
			// Create child windows
			if ((hToolbar = CreateWindow(TOOLBARCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | CCS_NORESIZE | CCS_NODIVIDER | TBSTYLE_TOOLTIPS, 0, 0, 0, 0, hWnd, NULL, hInstance, NULL)) == NULL)
			{
				return -1;
			}

			HIMAGELIST hImgList = ImageList_LoadImage(hInstance, MAKEINTRESOURCE(IDB_BITMAP1), 16, 0, RGB(0,128,128), IMAGE_BITMAP, 0);
			SendMessage(hToolbar, TB_SETIMAGELIST, 0, (LPARAM)hImgList);

			TBBUTTON buttons[] = {
				{0, 0, 0, BTNS_SEP},
				{0, ID_FILE_NEW,  TBSTATE_ENABLED, BTNS_BUTTON},
				{1, ID_FILE_OPEN, TBSTATE_ENABLED, BTNS_BUTTON},
				{2, ID_FILE_SAVE, 0, BTNS_BUTTON},
				{0, 0, 0, BTNS_SEP},
				{3, ID_EDIT_FIND, 0, BTNS_BUTTON},
				{0, 0, 0, BTNS_SEP},
				{4, ID_EDIT_CUT,   0, BTNS_BUTTON},
				{5, ID_EDIT_COPY,  0, BTNS_BUTTON},
				{6, ID_EDIT_PASTE, 0, BTNS_BUTTON},
				{0, 0, 0, BTNS_SEP},
				{7, ID_EDIT_MOVEUP,   0, BTNS_BUTTON},
				{8, ID_EDIT_MOVEDOWN, 0, BTNS_BUTTON},
				{0, 0, 0, BTNS_SEP},
				{9, ID_EDIT_FINDNEXTINVALID, 0, BTNS_BUTTON},
			};
			SendMessage(hToolbar, TB_ADDBUTTONS, 15, (LPARAM)&buttons);
			
			// Set tooltips
			HWND hTooltip = (HWND)SendMessage( (HWND) hToolbar, (UINT) TB_GETTOOLTIPS, 0, 0);
			

			if ((hLanguageSelect = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL | WS_DISABLED | CBS_DROPDOWNLIST | CBS_SORT,
				0, 0, 175, 150, hWnd, NULL, hInstance, NULL)) == NULL)
			{
				return -1;
			}
			SendMessage(hLanguageSelect, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

			if ((hVersionSelect = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL | WS_DISABLED | CBS_DROPDOWNLIST,
				0, 0, 175, 150, hWnd, NULL, hInstance, NULL)) == NULL)
			{
				return -1;
			}
			SendMessage(hVersionSelect, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);		

			if ((hRebar = CreateWindow(REBARCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
				0, 0, 10, 10, hWnd, NULL, hInstance, NULL)) == NULL)
			{
				return -1;
			}

			RECT rect;
			SIZE size;
			SendMessage(hToolbar, TB_GETMAXSIZE, 0, (LPARAM)&size);

			REBARBANDINFO rbbi;
			rbbi.cbSize     = sizeof(REBARBANDINFO);
			rbbi.fMask      = RBBIM_STYLE | RBBIM_CHILD | RBBIM_SIZE | RBBIM_CHILDSIZE;
			rbbi.fStyle     = RBBS_NOGRIPPER;
			rbbi.hwndChild  = hToolbar;
			rbbi.cxMinChild = size.cx;
			rbbi.cyMinChild = size.cy + 2;
			rbbi.cx         = rbbi.cxMinChild;
			SendMessage(hRebar, RB_INSERTBAND, -1, (LPARAM)&rbbi);

			rbbi.fMask      = RBBIM_STYLE | RBBIM_SIZE | RBBIM_CHILDSIZE;
			rbbi.cxMinChild = 0;
			rbbi.cyMinChild = 0;
			rbbi.cx         = rbbi.cxMinChild;
			SendMessage(hRebar, RB_INSERTBAND, -1, (LPARAM)&rbbi);

            wstring label;

			GetWindowRect(hVersionSelect, &rect);
            label           = LoadString(IDS_LABEL_VERSION) + L":";
			rbbi.fMask      = RBBIM_STYLE | RBBIM_CHILD | RBBIM_SIZE | RBBIM_CHILDSIZE | RBBIM_TEXT;
			rbbi.lpText     = (LPWSTR)label.c_str();
			rbbi.hwndChild  = hVersionSelect;
			rbbi.cxMinChild = rect.right  - rect.left;
			rbbi.cyMinChild = rect.bottom - rect.top;
			rbbi.cx         = rbbi.cxMinChild;
			SendMessage(hRebar, RB_INSERTBAND, -1, (LPARAM)&rbbi);

			GetWindowRect(hLanguageSelect, &rect);
            label           = LoadString(IDS_LABEL_LANGUAGE) + L":";
			rbbi.fMask      = RBBIM_STYLE | RBBIM_CHILD | RBBIM_SIZE | RBBIM_CHILDSIZE | RBBIM_TEXT;
			rbbi.lpText     = (LPWSTR)label.c_str();
			rbbi.hwndChild  = hLanguageSelect;
			rbbi.cxMinChild = rect.right  - rect.left;
			rbbi.cyMinChild = rect.bottom - rect.top;
			rbbi.cx         = rbbi.cxMinChild;
			SendMessage(hRebar, RB_INSERTBAND, -1, (LPARAM)&rbbi);

			if ((hContainer = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_MAINDIALOG), hWnd, GenericDialogProc, (LPARAM)(IDialog*)this)) == NULL)
			{
				DestroyWindow(hRebar);
				DestroyWindow(hToolbar);
				return -1;
			}
			break;
		}

		case WM_CLOSE:
			if (DoMenuItem(ID_FILE_EXIT))
			{
				DestroyWindow(hWnd);
			}
			return 0;

		case WM_DESTROY:
			DestroyWindow(hLanguageSelect);
			DestroyWindow(hVersionSelect);
			DestroyWindow(hToolbar);
			DestroyWindow(hRebar);
			DestroyWindow(hContainer);
			PostQuitMessage(0);
			break;

		case WM_SIZE:
			if (wParam != SIZE_MINIMIZED)
			{
				MoveWindow(hRebar, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);

				REBARBANDINFO band1, band2, band3;
				band1.cbSize = band2.cbSize = band3.cbSize = sizeof(REBARBANDINFO);
				band1.fMask  = band2.fMask  = band3.fMask  = RBBIM_SIZE;
				SendMessage(hRebar, RB_GETBANDINFO, 0, (LPARAM)&band1);
				SendMessage(hRebar, RB_GETBANDINFO, 2, (LPARAM)&band3);
				band2.cx = LOWORD(lParam) - band1.cx - band3.cx;
				SendMessage(hRebar, RB_SETBANDINFO, 1, (LPARAM)&band2);

				RECT rect;
				GetWindowRect(hRebar, &rect);
				int top = rect.bottom - rect.top + 6;
				MoveWindow(hContainer, 0, top, LOWORD(lParam), HIWORD(lParam) - top, TRUE);
			}
			break;

		case WM_SIZING:
		{
			RECT* size = (RECT*)lParam;
			if (size->right - size->left < MIN_WINDOW_WIDTH)
			{
				if (wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT)
					size->left = size->right - MIN_WINDOW_WIDTH;
				else
					size->right = size->left + MIN_WINDOW_WIDTH;
			}
			if (size->bottom - size->top < MIN_WINDOW_HEIGHT)
			{
				if (wParam == WMSZ_BOTTOM || wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_BOTTOMRIGHT)
					size->bottom = size->top + MIN_WINDOW_HEIGHT;
				else
					size->top = size->bottom - MIN_WINDOW_HEIGHT;
			}
			break;
		}

		case WM_INITMENU:
			OnInitMenu( (HMENU)wParam );
			break;

		case WM_ACTIVATE:
			if (wParam == WA_ACTIVE)
			{
				SetFocus(hwndFocus);
			}
			else if (wParam == WA_INACTIVE)
			{
				hwndFocus = GetFocus();
			}
			return 0;

		case WM_NOTIFY:
		{
			NMHDR* hdr = (NMHDR*)lParam;
			switch (hdr->code)
			{
				case TTN_GETDISPINFO:
				{
					// Toolbar wants tooltips
                    static const struct {
                        int idFrom;
                        int idStr;
                    } Tooltips[] = {
                        {ID_FILE_NEW,             IDS_TOOLTIP_NEW},
                        {ID_FILE_OPEN,            IDS_TOOLTIP_OPEN},
                        {ID_FILE_SAVE,            IDS_TOOLTIP_SAVE},
                        {ID_EDIT_FIND,            IDS_TOOLTIP_FIND},
                        {ID_EDIT_CUT,             IDS_TOOLTIP_CUT},
                        {ID_EDIT_COPY,            IDS_TOOLTIP_COPY},
                        {ID_EDIT_PASTE,           IDS_TOOLTIP_PASTE},
                        {ID_EDIT_MOVEUP,          IDS_TOOLTIP_MOVEUP},
                        {ID_EDIT_MOVEDOWN,        IDS_TOOLTIP_MOVEDOWN},
                        {ID_EDIT_FINDNEXTINVALID, IDS_TOOLTIP_FINDNEXTINVALID},
                        {-1}
                    };

                    for (int i = 0; Tooltips[i].idFrom != -1; i++)
                    {
                        if (Tooltips[i].idFrom == hdr->idFrom)
                        {
					        NMTTDISPINFO* nmdi = (NMTTDISPINFO*)hdr;
                            nmdi->hinst    = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
                            nmdi->lpszText = MAKEINTRESOURCE(Tooltips[i].idStr);
                            break;
                        }
                    }
    				break;
				}
			}
			break;
		}

		case WM_COMMAND:
			if (!locked)
			{
				locked = true;
				HWND hCtrl = (HWND)lParam;
				if (hCtrl == NULL || hCtrl == hToolbar)
				{
					// Menu or accelerator
					DoMenuItem(LOWORD(wParam));
				}
				else switch (HIWORD(wParam))
				{
					case CBN_SELCHANGE:
					{
						if (hCtrl == hLanguageSelect)
						{
							// User selected a different language
							int item = (int)SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
							LANGID language = (LANGID)SendMessage(hCtrl, CB_GETITEMDATA, item, 0);
							if (language != document->getActiveLanguage())
							{
								document->setActiveLanguage(language);
								FillListViewValues();
								OnStringFocused();
							}
						}
						else if (hCtrl == hVersionSelect)
						{
							// User selected a different version
							int item = (int)SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
							vector<Document::VersionInfo> versions;
							document->getVersions(versions);
							document->setActiveVersion( (int)versions.size() - 1 - item);
							FillListView();
							FillLanguageList();
							OnStringFocused();
						}
						break;
					}
				}
				locked = false;
			}
			break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int Application::run()
{
	HACCEL hAccTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(hMainWnd, hAccTable, &msg))
		{
			if (IsDialogMessage(hContainer, &msg))
			{
				continue;
			}
			TranslateMessage(&msg);
		}
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}

bool Application::initialize()
{
	INITCOMMONCONTROLSEX icce = {
		sizeof icce,
		ICC_STANDARD_CLASSES
	};
	InitCommonControlsEx(&icce);

	WNDCLASSEX wcx;
	wcx.cbSize			= sizeof(WNDCLASSEX);
    wcx.style			= CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc		= GenericWindowProc;
    wcx.cbClsExtra		= 0;
    wcx.cbWndExtra		= 0;
	wcx.hInstance		= hInstance;
    wcx.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    wcx.hCursor			= LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground	= (HBRUSH)(COLOR_BTNFACE + 1);
    wcx.lpszMenuName	= MAKEINTRESOURCE(IDR_MENU1);
    wcx.lpszClassName	= L"StringEditor";
    wcx.hIconSm			= NULL;

	if (RegisterClassEx(&wcx) == 0)
	{
		return false;
	}

	if (!EditList_Initialize(hInstance))
	{
		UnregisterClass(L"StringEditor", hInstance);
		return false;
	}

	hMainWnd = CreateWindowEx(WS_EX_CONTROLPARENT, L"StringEditor", L"String Editor", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_GROUP, 
		CW_USEDEFAULT, CW_USEDEFAULT, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT,
		NULL, NULL, hInstance, (IWindow*)this);
	if (hMainWnd == NULL)
	{
		UnregisterClass(L"StringEditor", hInstance);
		return false;
	}

	return true;
}

Application::Application(HINSTANCE _hInstance)
{
	hInstance       = _hInstance;
	document        = NULL;
	hActiveListView = NULL;
	sorting         = 0;			// Not sorted
	locked          = false;

	if (!initialize())
	{
		throw wruntime_error(LoadString(IDS_ERROR_UI_INITIALIZATION));
	}
}

Application::~Application()
{
	delete document;
	DestroyWindow(hMainWnd);
	UnregisterClass(L"VDFEditor", hInstance);
}
