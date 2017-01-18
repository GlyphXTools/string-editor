#include <map>
#include <algorithm>
#include "exceptions.h"
#include "ui.h"
#include "utils.h"
#include "resource.h"
using namespace std;

struct EntryInfo
{
	int  yPos;
	int  nameWidth;
	HWND hWnd;
};

struct EDITLIST_INFO
{
	SCROLLINFO             si;
	int		               entryHeight;
	HINSTANCE		       hInstance;
	int                    maxNameWidth;
	map<LANGID, EntryInfo> languages;
};

static void MoveEntries(EDITLIST_INFO* info)
{
	int y = 0;
	for (map<LANGID, EntryInfo>::iterator p = info->languages.begin(); p != info->languages.end(); p++)
	{
		HWND hEntry  = p->second.hWnd;
		SetWindowPos(hEntry, 0, 0, y - info->si.nPos, 0, 0, SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
		p->second.yPos = y;
		y += info->entryHeight;
	}
}

static void EnsureVisible(EDITLIST_INFO* info, HWND hEdit)
{
	LANGID language = (LANGID)(LONG_PTR)GetWindowLongPtr(hEdit, GWLP_USERDATA);
	int y = info->languages[language].yPos;
	if (y + info->entryHeight > info->si.nPos + (int)info->si.nPage)
	{
		info->si.nPos = y + info->entryHeight - info->si.nPage;
	}
	else if (y < info->si.nPos)
	{
		info->si.nPos = y;
	}
}

LRESULT CALLBACK EditListProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EDITLIST_INFO* info = (EDITLIST_INFO*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_CREATE:
		{
			CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
			info = new EDITLIST_INFO;
			if (info == NULL) return false;
			info->maxNameWidth = 0;
			info->hInstance    = pcs->hInstance;
			info->si.cbSize    = sizeof(SCROLLINFO);
			info->si.fMask     = SIF_ALL;
			info->entryHeight  = 0;
			GetScrollInfo(hWnd, SB_VERT, &info->si);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)(LONG_PTR)info);
			break;
		}

		case WM_DESTROY:
			delete info;
			break;

		case WM_VSCROLL:
			GetScrollInfo(hWnd, SB_VERT, &info->si);
			switch (LOWORD(wParam))
			{
				case SB_BOTTOM:		info->si.nPos += info->entryHeight; break;
				case SB_LINEDOWN:	info->si.nPos += info->entryHeight; break;
				case SB_PAGEDOWN:	info->si.nPos += info->si.nPage; break;
				case SB_TOP:		info->si.nPos -= info->entryHeight; break;
				case SB_LINEUP:		info->si.nPos -= info->entryHeight; break;
				case SB_PAGEUP:		info->si.nPos -= info->si.nPage; break;
				case SB_THUMBTRACK:	info->si.nPos = HIWORD(wParam); break;
			}
			SetScrollInfo(hWnd, SB_VERT, &info->si, TRUE);
			GetScrollInfo(hWnd, SB_VERT, &info->si);
			MoveEntries(info);
			InvalidateRect(hWnd, NULL, TRUE);
			UpdateWindow(hWnd);
			return 0;

		case WM_COMMAND:
			if (lParam != 0)
			{
				HWND hEdit = (HWND)lParam;
				switch (HIWORD(wParam))
				{
					case EN_SETFOCUS:
					{
						// Make sure the edit box is completely visible
						int oldpos = info->si.nPos;
						EnsureVisible(info, hEdit);
						SetScrollInfo(hWnd, SB_VERT, &info->si, TRUE);
						GetScrollInfo(hWnd, SB_VERT, &info->si);
						if (info->si.nPos != oldpos)
						{
							MoveEntries(info);
						}
						break;
					}
				}
			}
			break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

INT_PTR CALLBACK DlgEntryProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			CheckDlgButton(hWnd, IDC_CHECK1, BST_CHECKED);
			break;

		case WM_COMMAND:
			// Pass WM_COMMAND's on to the parent
			SendMessage(GetParent(hWnd), uMsg, wParam, lParam);
			break;
	}
	return FALSE;
}

void EditList_AddPair(HWND hWnd, LANGID language, const wstring& postfix)
{
	EDITLIST_INFO* info = (EDITLIST_INFO*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (info != NULL)
	{
		EntryInfo& ei = info->languages[language];
		if ((ei.hWnd = CreateDialogParam(info->hInstance, MAKEINTRESOURCE(IDD_EDITLISTENTRY), hWnd, DlgEntryProc, NULL)) == NULL)
		{
			info->languages.erase(language);
			throw wruntime_error(LoadString(IDS_ERROR_WINDOW_CREATE));
		}
		wstring name = GetLanguageName(language) + L":";

		SetDlgItemText(ei.hWnd, IDC_STATIC1, name.c_str());
		SetDlgItemText(ei.hWnd, IDC_EDIT1,   postfix.c_str());
		ShowWindow(ei.hWnd, SW_SHOW);

		SetWindowLongPtr(GetDlgItem(ei.hWnd, IDC_EDIT1), GWLP_USERDATA, (LONG)(LONG_PTR)language);

		// Recompute maximum name width
		SIZE size;
		HDC  hDC = GetDC(GetDlgItem(ei.hWnd, IDC_STATIC1));
		SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
		GetTextExtentPoint32(hDC, name.c_str(), (int)name.length() + 1, &size);
		DeleteDC(hDC);
		info->maxNameWidth = max(info->maxNameWidth, size.cx + 10);

		// Rearrange windows
		RECT area, rect;
		GetWindowRect(ei.hWnd, &area);
		GetClientRect(hWnd,    &rect);
		int height = area.bottom - area.top;
		int width  = rect.right;
		info->entryHeight = max(info->entryHeight, height + 4);

		// Adjust scrollbar range
		info->si.nMin   = 0;
		info->si.nMax   = (int)info->languages.size() * (height + 4);
		info->si.nPage  = rect.bottom;
		SetScrollInfo(hWnd, SB_VERT, &info->si, TRUE);
		GetScrollInfo(hWnd, SB_VERT, &info->si);

		int y = 0;
		for (map<LANGID, EntryInfo>::iterator p = info->languages.begin(); p != info->languages.end(); p++)
		{
			HWND hEntry  = p->second.hWnd;
			HWND hStatic = GetDlgItem(hEntry, IDC_STATIC1);
			HWND hEdit   = GetDlgItem(hEntry, IDC_EDIT1);
			MoveWindow(hEntry, 0, y - info->si.nPos, width, height, TRUE);
			p->second.yPos = y;
			y += info->entryHeight;

			// Move child windows
			RECT size1, size2;
			GetWindowRect(hStatic, &size1);
			GetWindowRect(hEdit,   &size2);
			POINT p1 = {size1.left, size1.top};
			POINT p2 = {size2.left, size2.top};
			ScreenToClient(hEntry, &p1);
			ScreenToClient(hEntry, &p2);

			MoveWindow(hStatic, p1.x, p1.y, info->maxNameWidth, size1.bottom - size1.top, TRUE);
			int left = p1.x + info->maxNameWidth + 4;
			MoveWindow(hEdit, left, p2.y, width - left - 4, size2.bottom - size2.top, TRUE);
		}
	}
}

void EditList_GetPairs(HWND hWnd, map<LANGID,wstring>& pairs, map<LANGID,bool>& enabled)
{
	EDITLIST_INFO* info = (EDITLIST_INFO*)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (info != NULL)
	{
		pairs.clear();
		enabled.clear();
		for (map<LANGID, EntryInfo>::const_iterator p = info->languages.begin(); p != info->languages.end(); p++)
		{
			pairs  [p->first] = GetDlgItemStr(p->second.hWnd, IDC_EDIT1);
			enabled[p->first] = (IsDlgButtonChecked(p->second.hWnd, IDC_CHECK1) == BST_CHECKED);
		}
	}
}

bool EditList_Initialize(HINSTANCE hInstance)
{
	WNDCLASSEX wcx;
	wcx.cbSize			= sizeof(WNDCLASSEX);
    wcx.style			= CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc		= EditListProc;
    wcx.cbClsExtra		= 0;
    wcx.cbWndExtra		= 0;
	wcx.hInstance		= hInstance;
    wcx.hIcon			= NULL;
    wcx.hCursor			= LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground	= (HBRUSH)(COLOR_BTNFACE + 1);
    wcx.lpszMenuName	= NULL;
    wcx.lpszClassName	= L"EditList";
    wcx.hIconSm			= NULL;

	return (RegisterClassEx(&wcx) != 0);
}