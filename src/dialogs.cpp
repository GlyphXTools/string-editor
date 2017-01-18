#include <sstream>
#include "dialogs.h"
#include "utils.h"
#include "resource.h"
#include "ui.h"
#include <commdlg.h>
#include <commctrl.h>

namespace Dialogs
{
using namespace std;

//
// New File
//
INT_PTR CALLBACK DlgNewFileProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	NEW_FILE_OPTIONS* info = (NEW_FILE_OPTIONS*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			info = (NEW_FILE_OPTIONS*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)info);

			HWND hCombo1 = GetDlgItem(hWnd, IDC_COMBO1);

			set<LANGID> languages;
			GetLanguageList(languages);
			for (set<LANGID>::const_iterator p = languages.begin(); p != languages.end(); p++)
			{
				wstring name = GetLanguageName(*p);
				int i = (int)SendMessage(hCombo1, CB_ADDSTRING, 0, (LPARAM)name.c_str());
				SendMessage(hCombo1, CB_SETITEMDATA, i, (LPARAM)*p);
			}
			SendMessage(hCombo1, CB_SETCURSEL, 0, 0);
			CheckDlgButton(hWnd, IDC_RADIO1, BST_CHECKED);
			return TRUE;
		}

		case WM_COMMAND:
			if (lParam != 0 && HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
					case IDOK:
					{
						HWND hCombo1 = GetDlgItem(hWnd, IDC_COMBO1);
						int sel = (int)SendMessage(hCombo1, CB_GETCURSEL, 0, 0);
						info->language = (LANGID)SendMessage(hCombo1, CB_GETITEMDATA, sel, 0);
						info->type     = IsDlgButtonChecked(hWnd, IDC_RADIO1) ? Document::DT_NAME : Document::DT_INDEX;
						EndDialog(hWnd, 1);
						break;
					}
				
					case IDCANCEL:
						EndDialog(hWnd, 0);
						break;
				}
			}
			break;
	}
	return FALSE;
}

bool NewFile(HWND hParent, NEW_FILE_OPTIONS& options)
{
	return (DialogBoxParam((HINSTANCE)(LONG_PTR)GetWindowLongPtr(hParent, GWLP_HINSTANCE), MAKEINTRESOURCE(IDD_NEWFILE), hParent, DlgNewFileProc, (LPARAM)&options) != 0);
}

//
// Import File
//
INT_PTR CALLBACK DlgImportProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			CheckDlgButton(hWnd, IDC_CHECK1, BST_CHECKED);
			CheckDlgButton(hWnd, IDC_RADIO2, BST_CHECKED);
			CheckDlgButton(hWnd, IDC_RADIO5, BST_CHECKED);
			EnableWindow(GetDlgItem(hWnd, IDC_CHECK1), TRUE);
			return TRUE;
		}

		case WM_COMMAND:
			if (lParam != 0 && HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
					case IDC_RADIO2:
						EnableWindow(GetDlgItem(hWnd, IDC_CHECK1), TRUE);
						break;

					case IDC_RADIO3:
					case IDC_RADIO4:
						EnableWindow(GetDlgItem(hWnd, IDC_CHECK1), FALSE);
						break;

					case IDOK:
						     if (IsDlgButtonChecked(hWnd, IDC_RADIO2) == BST_CHECKED) EndDialog(hWnd, IsDlgButtonChecked(hWnd, IDC_CHECK1) ? Document::AM_UNION_OVERWRITE : Document::AM_UNION);
						else if (IsDlgButtonChecked(hWnd, IDC_RADIO3) == BST_CHECKED) EndDialog(hWnd, Document::AM_INTERSECT);
						else if (IsDlgButtonChecked(hWnd, IDC_RADIO4) == BST_CHECKED) EndDialog(hWnd, Document::AM_DIFFERENCE);
						else if (IsDlgButtonChecked(hWnd, IDC_RADIO5) == BST_CHECKED) EndDialog(hWnd, Document::AM_OVERWRITE);
						else if (IsDlgButtonChecked(hWnd, IDC_RADIO6) == BST_CHECKED) EndDialog(hWnd, Document::AM_APPEND);
						else EndDialog(hWnd, (int)Document::AM_NONE);
						break;
				
					case IDCANCEL:
						EndDialog(hWnd, (int)Document::AM_NONE);
						break;
				}
			}
			break;
	}
	return FALSE;
}

Document::Method ImportFile(HWND hParent, bool byIndex)
{
	UINT dialog = byIndex ? IDD_IMPORT_INDEX : IDD_IMPORT_NAME;
	return (Document::Method)DialogBox((HINSTANCE)(LONG_PTR)GetWindowLongPtr(hParent, GWLP_HINSTANCE), MAKEINTRESOURCE(dialog), hParent, DlgImportProc);
}

INT_PTR CALLBACK DlgVersionInfoProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	VERSION_INFO* info = (VERSION_INFO*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			info = (VERSION_INFO*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)info);
			
			// Show document version
			wstringstream ss;
			ss << (unsigned int)info->version;
			SetDlgItemText(hWnd, IDC_VERSION, ss.str().c_str() );

			// Populate authors listbox
			HWND hCombo1 = GetDlgItem(hWnd, IDC_COMBO1);
			for (set<wstring>::const_iterator p = info->authors.begin(); p != info->authors.end(); p++)
			{
				if (!p->empty())
				{
					SendMessage(hCombo1, CB_ADDSTRING, 0, (LPARAM)p->c_str());
				}
			}

			return TRUE;
		}

		case WM_COMMAND:
			if (lParam != 0)
			{
				switch (HIWORD(wParam))
				{
					case BN_CLICKED:
						// OK or Cancel has been clicked
						if (LOWORD(wParam) == IDOK)
						{
							info->author = GetDlgItemStr(hWnd, IDC_COMBO1);
							info->notes  = GetDlgItemStr(hWnd, IDC_EDIT2);
							EndDialog(hWnd, 1);
						}
						else
						{
							EndDialog(hWnd, 0);
						}
						break;
				}
			}
			break;
	}
	return FALSE;
}

bool GetVersionInfo(HWND hParent, VERSION_INFO& info)
{
	return (DialogBoxParam((HINSTANCE)(LONG_PTR)GetWindowLongPtr(hParent, GWLP_HINSTANCE), MAKEINTRESOURCE(IDD_VERSION_INFO), hParent, DlgVersionInfoProc, (LPARAM)&info) != 0);
}

struct LANGUAGE_SELECT
{
	wstring            text;
	const set<LANGID>* languages;
};

INT_PTR CALLBACK DlgLangSelectProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LANGUAGE_SELECT* info = (LANGUAGE_SELECT*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			info = (LANGUAGE_SELECT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)info);

			SetDlgItemText(hWnd, IDC_STATIC1, info->text.c_str());
			
			RECT rect;
			HWND hCombo = GetDlgItem(hWnd, IDC_COMBO1);
			GetClientRect(hCombo, &rect);
			SetWindowPos(hCombo, NULL, 0, 0, rect.right, 400, SWP_NOREPOSITION | SWP_NOMOVE);

			for (set<LANGID>::const_iterator p = info->languages->begin(); p != info->languages->end(); p++)
			{
				wstring name = GetLanguageName(*p);
				int item = (int)SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)name.c_str());
				SendMessage(hCombo, CB_SETITEMDATA, item, (LPARAM)*p);
			}
			SendMessage(hCombo, CB_SETCURSEL, 0, 0);

			return TRUE;
		}

		case WM_COMMAND:
			if (lParam != 0 && HIWORD(wParam) == BN_CLICKED)
			{
				if (LOWORD(wParam) == IDOK)
				{
					HWND hCombo = GetDlgItem(hWnd, IDC_COMBO1);
					int item = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
					EndDialog(hWnd, (LANGID)SendMessage(hCombo, CB_GETITEMDATA, item, 0));
				}
				else
				{
					EndDialog(hWnd, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL) );
				}
			}
			break;
	}
	return FALSE;
}

LANGID AddLanguage(HWND hParent, const std::set<LANGID>& languages)
{
	LANGUAGE_SELECT langselect;
	langselect.text      = LoadString(IDS_LANGUAGE_ADD);
	langselect.languages = &languages;

	return (LANGID)DialogBoxParam((HINSTANCE)(LONG_PTR)GetWindowLongPtr(hParent, GWLP_HINSTANCE), MAKEINTRESOURCE(IDD_LANG_SELECT), hParent, DlgLangSelectProc, (LPARAM)&langselect);
}

LANGID ChangeLanguage(HWND hParent, const std::set<LANGID>& languages)
{
	LANGUAGE_SELECT langselect;
	langselect.text      = LoadString(IDS_LANGUAGE_CHANGE);
	langselect.languages = &languages;

	return (LANGID)DialogBoxParam((HINSTANCE)(LONG_PTR)GetWindowLongPtr(hParent, GWLP_HINSTANCE), MAKEINTRESOURCE(IDD_LANG_SELECT), hParent, DlgLangSelectProc, (LPARAM)&langselect);
}

struct EXPORT_INFO
{
	wstring*             basename;
	map<LANGID,wstring>* postfixes;
	map<LANGID,bool>*    enabled;
};

INT_PTR CALLBACK DlgExportProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EXPORT_INFO* info = (EXPORT_INFO*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			info = (EXPORT_INFO*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)info);
			HWND hList = GetDlgItem(hWnd, IDC_EDITLIST1);
			for (map<LANGID,wstring>::const_iterator p = info->postfixes->begin(); p != info->postfixes->end(); p++)
			{
				EditList_AddPair(hList, p->first, p->second);
			}
			return TRUE;
		}

		case WM_COMMAND:
			if (lParam != 0 && HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
					case IDOK:
					{
						// Validate values
						set<wstring> postfixes;
						EditList_GetPairs(GetDlgItem(hWnd, IDC_EDITLIST1), *info->postfixes, *info->enabled);
						
						bool used   = false;
						bool unique = true;

						map<LANGID, wstring>::const_iterator p = info->postfixes->begin();
						map<LANGID, bool>   ::const_iterator e = info->enabled->begin();
						for (; p != info->postfixes->end(); p++)
						{
							if (e->second)
							{
								used = true;
								if (postfixes.find(p->second) != postfixes.end())
								{
									MessageBox(hWnd, LoadString(IDS_ERROR_EXPORT_POSTFIX).c_str(), NULL, MB_OK | MB_ICONERROR);
									unique = false;
									break;
								}
								postfixes.insert(p->second);
							}
						}

						if (!used)
						{
							MessageBox(hWnd, LoadString(IDS_ERROR_EXPORT_SELECT).c_str(), NULL, MB_OK | MB_ICONERROR);
						}
						else if (unique)
						{
							*info->basename = GetDlgItemStr(hWnd, IDC_EDIT1);
							EndDialog(hWnd, 1);
						}
						break;
					}

					case IDCANCEL:
						EndDialog(hWnd, 0);
						break;

					case IDC_BUTTON1:
					{
						// Filename needed
						WCHAR filename[MAX_PATH];
						filename[0] = L'\0';
						
                        wstring filter = LoadString(IDS_FILES_DAT) + wstring(L" (*.dat)\0*.DAT\0", 15)
                                       + LoadString(IDS_FILES_ALL) + wstring(L" (*.*)\0*.*\0", 11);

						OPENFILENAME ofn;
						memset(&ofn, 0, sizeof(OPENFILENAME));
						ofn.lStructSize  = sizeof(OPENFILENAME);
						ofn.hwndOwner    = hWnd;
						ofn.hInstance    = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
                        ofn.lpstrFilter  = filter.c_str();
						ofn.nFilterIndex = 1;
						ofn.lpstrFile    = filename;
						ofn.nMaxFile     = MAX_PATH;
						ofn.Flags        = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
						ofn.lpstrDefExt  = L"DAT";

						if (GetSaveFileName(&ofn))
						{
							SetDlgItemText(hWnd, IDC_EDIT1, filename);
						}
						break;
					}
				}
				
			}
			break;
	}
	return FALSE;
}

bool ExportFile(HWND hParent, map<LANGID,wstring>& postfixes, map<LANGID,bool>& enabled, wstring& basename)
{
	EXPORT_INFO info;
	info.basename  = &basename;
	info.postfixes = &postfixes;
	info.enabled   = &enabled;
	return (DialogBoxParam((HINSTANCE)(LONG_PTR)GetWindowLongPtr(hParent, GWLP_HINSTANCE), MAKEINTRESOURCE(IDD_EXPORT), hParent, DlgExportProc, (LPARAM)&info) != 0);
}

INT_PTR CALLBACK DlgFindProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	FIND_INFO* info = (FIND_INFO*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
			info = (FIND_INFO*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)info);
			SetDlgItemText(hWnd, IDC_EDIT1, info->term.c_str());
			CheckDlgButton(hWnd, IDC_RADIO1, info->method == FIND_INFO::FM_FROMSTART  ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_RADIO2, info->method == FIND_INFO::FM_SELECTION  ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_RADIO3, info->method == FIND_INFO::FM_FROMCURSOR ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_CHECK1, info->searchName    ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_CHECK2, info->searchValue   ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_CHECK3, info->searchComment ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_CHECK4, info->matchCase ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_CHECK5, info->matchWord ? BST_CHECKED : BST_UNCHECKED);
			break;

		case WM_COMMAND:
			if (lParam != 0 && HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
					case IDC_BUTTON1:
					case IDC_BUTTON2:
						info->term          = GetDlgItemStr(hWnd, IDC_EDIT1);
						info->searchName	= IsDlgButtonChecked(hWnd, IDC_CHECK1) == BST_CHECKED;
						info->searchValue	= IsDlgButtonChecked(hWnd, IDC_CHECK2) == BST_CHECKED;
						info->searchComment = IsDlgButtonChecked(hWnd, IDC_CHECK3) == BST_CHECKED;
						info->matchCase		= IsDlgButtonChecked(hWnd, IDC_CHECK4) == BST_CHECKED;
						info->matchWord		= IsDlgButtonChecked(hWnd, IDC_CHECK5) == BST_CHECKED;
						info->selectAll     = (LOWORD(wParam) == IDC_BUTTON2);
						info->method =
							(IsDlgButtonChecked(hWnd, IDC_RADIO1) == BST_CHECKED) ? FIND_INFO::FM_FROMSTART  :
							(IsDlgButtonChecked(hWnd, IDC_RADIO2) == BST_CHECKED) ? FIND_INFO::FM_SELECTION  :
							(IsDlgButtonChecked(hWnd, IDC_RADIO3) == BST_CHECKED) ? FIND_INFO::FM_FROMCURSOR : FIND_INFO::FM_NONE;

						EndDialog(hWnd, 1);
						break;

					case IDCANCEL:
						EndDialog(hWnd, 0);
						break;
				}
				
			}
			break;
	}
	return FALSE;
}

bool Find(HWND hParent, FIND_INFO& info)
{
	return (DialogBoxParam((HINSTANCE)(LONG_PTR)GetWindowLongPtr(hParent, GWLP_HINSTANCE), MAKEINTRESOURCE(IDD_FIND), hParent, DlgFindProc, (LPARAM)&info) != 0);
}

//
// File details
//
INT_PTR CALLBACK DlgFileInfoProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	wchar_t tmp[32];

	const Document* document = (const Document*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			document = (const Document*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)document);

			SetDlgItemText(hWnd, IDC_STATIC8, LoadString((document->getType() == Document::DT_NAME) ? IDS_FILEINFO_NAME_INDEXED : IDS_FILEINFO_INDEX_INDEXED).c_str());

			// Create versions set (we ignore the last, current, version)
			vector<Document::VersionInfo> versions;
			document->getVersions(versions);
			versions.erase(versions.end()-1);

			wsprintf(tmp, L"%u", versions.size());
			SetDlgItemText(hWnd, IDC_STATIC2, tmp);

			// Create author set
			set<wstring> authors;
			for (size_t i = 0; i < versions.size(); i++)
			{
				authors.insert(versions[i].m_author);
			}
			wsprintf(tmp, L"%u", authors.size());
			SetDlgItemText(hWnd, IDC_STATIC1, tmp);

			// Fill versions list
			HWND hList = GetDlgItem(hWnd, IDC_LIST1);
			ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT);

            wstring title;

			LVCOLUMN column;
			column.mask = LVCF_TEXT | LVCF_WIDTH;
			                                                column.cx   = 20;  column.pszText = (LPWSTR)L"#";		   ListView_InsertColumn(hList, 0, &column);
            title = LoadString(IDS_FILEINFO_COLUMN_SAVED);  column.cx   = 100; column.pszText = (LPWSTR)title.c_str(); ListView_InsertColumn(hList, 1, &column);
			title = LoadString(IDS_FILEINFO_COLUMN_AUTHOR); column.cx   = 100; column.pszText = (LPWSTR)title.c_str(); ListView_InsertColumn(hList, 2, &column);
			title = LoadString(IDS_FILEINFO_COLUMN_NOTES);  column.cx   = 150; column.pszText = (LPWSTR)title.c_str(); ListView_InsertColumn(hList, 3, &column);

			LVITEM item;
			item.mask     = LVIF_TEXT;
			item.iSubItem = 0;
			for (size_t i = 0; i < versions.size(); i++)
			{
				wsprintf(tmp, L"%u", i + 1);
				item.iItem    = (int)i;
				item.pszText  = tmp;
				ListView_InsertItem(hList, &item);

				wstring modified = DateTime(versions[i].m_saved).formatShort();
				wstring notes = versions[i].m_notes;
				// Replace linefeeds with spaces
				size_t ofs;
				ofs = 0; while ((ofs = notes.find(L"\r\n", ofs)) != wstring::npos) notes.replace(ofs, 2, L" ");
				ofs = 0; while ((ofs = notes.find(L"\n", ofs))   != wstring::npos) notes.replace(ofs, 1, L" ");

				ListView_SetItemText(hList, i, 1, (LPWSTR)modified.c_str());
				ListView_SetItemText(hList, i, 2, (LPWSTR)versions[i].m_author.c_str());
				ListView_SetItemText(hList, i, 3, (LPWSTR)notes.c_str());
			}
			ListView_SetItemState(hList, versions.size() - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			ListView_EnsureVisible(hList, versions.size() - 1, FALSE);
			break;
		}

		case WM_NOTIFY:
		{
			const NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
			if (pnmv->hdr.code == LVN_ITEMCHANGED && pnmv->uChanged & LVIF_STATE && pnmv->uNewState & LVIS_FOCUSED)
			{
				// User has selected a different version			
				vector<Document::VersionInfo> versions;
				document->getVersions(versions);
				const Document::VersionInfo& version = versions[pnmv->iItem];

				wstring modified = DateTime(version.m_saved).formatShort();
				SetDlgItemText(hWnd, IDC_STATIC3, modified.c_str());
				SetDlgItemText(hWnd, IDC_STATIC4, version.m_author.c_str());
				SetDlgItemText(hWnd, IDC_EDIT1, version.m_notes.c_str());

				wsprintf(tmp, L"%u", version.m_numDifferences);	SetDlgItemText(hWnd, IDC_STATIC5, tmp);
				wsprintf(tmp, L"%u", version.m_numStrings);		SetDlgItemText(hWnd, IDC_STATIC6, tmp);
				wsprintf(tmp, L"%u", version.m_numLanguages);	SetDlgItemText(hWnd, IDC_STATIC7, tmp);
			}
			break;
		}

		case WM_COMMAND:
			if (lParam != 0 && HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
					case IDOK:
					case IDCANCEL:
						EndDialog(hWnd, 0);
						break;
				}
				
			}
			break;
	}
	return FALSE;
}

void FileInfo(HWND hParent, const Document* document)
{
	DialogBoxParam((HINSTANCE)(LONG_PTR)GetWindowLongPtr(hParent, GWLP_HINSTANCE), MAKEINTRESOURCE(IDD_FILE_INFO), hParent, DlgFileInfoProc, (LPARAM)document);
}
}
