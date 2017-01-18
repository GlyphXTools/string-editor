#ifndef APPLICATION_H
#define APPLICATION_H

#include <string>
#include "dialogs.h"

class IWindow
{
public:
	virtual LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
};

class IDialog
{
public:
	virtual BOOL DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
};

class Application : public IWindow, public IDialog
{
	friend static int     CALLBACK GenericStringCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	friend static LRESULT CALLBACK GenericWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	friend static INT_PTR CALLBACK GenericDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
	int run();

	Application(HINSTANCE hInstance);
	~Application();

private:
	//
	// Functions
	//
	LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL    DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	int     StringCompareFunc(LPARAM lParam1, LPARAM lParam2) const;

	void FillVersionList(int sel = -1);
	void FillLanguageList();
	void FillListView();
	void FillListViewValues();

	// UI functions
	void UI_OnDocumentChanged();
	void UI_OnFocusChanged();

	// Event handlers
	void OnHistoryFocused();
	void OnStringFocused();
	void OnEditChanged(UINT idCtrl);
	void OnInitMenu(HMENU hMenu);

	//
	// Menu item handlers
	//
	bool DoMenuItem(WORD id);

	// File
	void DoNewFile();
	void DoOpenFile();
	bool DoSave(bool saveas = false);
	void DoImportFile();
	void DoExportFile();
	bool DoClose();
	void DoDetails();

	// Edit
	void DoInsertString();
	void DoSelectAll();
	bool DoCopy();
	bool DoPaste();
	void DoDelete();
	bool DoPasteSpecial(Document::Method method);
	void DoMoveStrings(bool up, void (Application::*callback)(int, int, int,int));
	void DoFind(bool showDialog);
	void DoFindNextInvalid();

	// Language
	void DoAddLanguage();
	void DoChangeLanguage();
	void DoDeleteLanguage();
	void DoPreviousLanguage();
	void DoNextLanguage();

	//
	// Misc functions
	//
	void SwapStrings(int id1, int pos1, int id2, int pos2);
	void SwapValues(int id1, int pos1, int id2, int pos2);
	bool initialize();

	//
	// Members
	//
	HINSTANCE hInstance;
	HWND      hMainWnd;
	HWND      hRebar;
	HWND      hToolbar;
	HWND      hLanguageSelect;
	HWND	  hVersionSelect;
	HWND      hActiveListView;
	HWND	  hHistory;
	HWND      hContainer;

	std::wstring filename;		// The document's filename. Empty for unnamed
	Document*	 document;		// The document!
	int			 sorting;		// How and on what we're sorting
	bool		 locked;		// When true, don't act upon notifications
	HWND		 hwndFocus;		// Last window to have focus
	FIND_INFO    findinfo;		// Latest find options
};

#endif