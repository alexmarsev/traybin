#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdio.h>

#pragma comment(lib, "comctl32")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WM_APP_NOTIFYCALLBACK (WM_APP + 1)
#define WINDOW_CLASS _T("traybin_class")
#define WINDOW_TITLE _T("traybin")
#define EXIT_STRING _T("Exit")

HMENU hsubmenu; //popup menu
BOOL hidden = TRUE; //calendar's state
HWND hcal; //calendar control
UINT WM_TASKBARCREATED;

struct TrayThreadInfo {
	HWND hwnd, hcal;
	HANDLE hnotify;
	BOOL exit;
} tti; //TrayThreadProc() param

HICON CreateTimeIcon(SYSTEMTIME* time) {
	WORD matrix[][2] = { {10, 10}, {10, 5}, {10, 0}, {5, 10}, {5, 5}, {5, 0}, {0, 10}, {0, 5}, {0, 0} };
	HDC hdc0, hdc1;
	HGDIOBJ undo;
	HBITMAP bmp, mask;
	HICON ret;
	ICONINFO ii = {TRUE};
	HBRUSH brush0, brush1;
	RECT rect;
	WORD x;

	brush0 = CreateSolidBrush(RGB(0, 0, 0));
	brush1 = CreateSolidBrush(RGB(255, 255, 255));

	hdc0 = GetDC(NULL);
	bmp = CreateCompatibleBitmap(hdc0, 16, 16);
	mask = CreateCompatibleBitmap(hdc0, 16, 16);
	hdc1 = CreateCompatibleDC(hdc0);

	rect.left = 0; rect.top = 0; rect.right = 16; rect.bottom = 16;
	undo = SelectObject(hdc1, mask); FillRect(hdc1, &rect, brush1);

	x = time->wMinute + (time->wHour % 6 << 6);
	for (int i = 0; i < 9; i++, x /= 2) {
		if (x % 2) {
			rect.left = matrix[i][0]; rect.top = matrix[i][1]; rect.right = rect.left + 6; rect.bottom = rect.top + 6;
			SelectObject(hdc1, mask); FillRect(hdc1, &rect, brush0);
			rect.left++; rect.top++; rect.right--; rect.bottom--;
			SelectObject(hdc1, bmp); FillRect(hdc1, &rect, brush1);
		}
	}

	ReleaseDC(NULL, hdc0);
	SelectObject(hdc1, undo); DeleteDC(hdc1);
	DeleteObject(brush0);
	DeleteObject(brush1);

	ii.hbmColor = bmp;
	ii.hbmMask = mask;
	ret = CreateIconIndirect(&ii);

	DeleteObject(bmp);
	DeleteObject(mask);
	return ret;
}

void ChangeTip(NOTIFYICONDATA* nid, SYSTEMTIME* time) {
	TCHAR *buf, *datef = _T("dddd yyyy.MM.dd");
	HANDLE heap = GetProcessHeap();
	size_t n;

	n = GetDateFormat(LOCALE_USER_DEFAULT, 0, time, datef, NULL, 0);
	buf = (TCHAR*)HeapAlloc(heap, HEAP_ZERO_MEMORY, n * sizeof(TCHAR));
	GetDateFormat(LOCALE_USER_DEFAULT, 0, time, datef, buf, n);
	_stprintf_s(nid->szTip, 64, _T("%s %02d:%02d"), buf, time->wHour, time->wMinute);
	HeapFree(heap, 0, buf);
}

DWORD WINAPI TrayThreadProc(LPVOID lp) {
	TrayThreadInfo *tti = (TrayThreadInfo*)lp;
	SYSTEMTIME time, lasttime;
	HICON oldicon = NULL;
	NOTIFYICONDATA nid = {sizeof(nid)};
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
	nid.hWnd = tti->hwnd;
	nid.uID = 0;
	nid.uCallbackMessage = WM_APP_NOTIFYCALLBACK;
	nid.uVersion = NOTIFYICON_VERSION_4;

	MonthCal_GetToday(tti->hcal, &lasttime);
	do {
		if (WaitForSingleObject(tti->hnotify, 0) != WAIT_TIMEOUT) ResetEvent(tti->hnotify);
		
		GetLocalTime(&time);
		
		if ((time.wDay != lasttime.wDay) || (time.wMonth != lasttime.wMonth) || (time.wYear != lasttime.wYear)) {
			MonthCal_SetToday(tti->hcal, &time);
		}
		
		oldicon = nid.hIcon;
		nid.hIcon = CreateTimeIcon(&time);
		ChangeTip(&nid, &time);	
		if (!Shell_NotifyIcon(NIM_MODIFY, &nid)) {
			Shell_NotifyIcon(NIM_ADD, &nid);
			Shell_NotifyIcon(NIM_SETVERSION, &nid);
		}
		
		lasttime = time;
		if (oldicon) { DestroyIcon(oldicon); oldicon = NULL; }
	} while (WaitForSingleObject(tti->hnotify, (60 - time.wSecond) * 1000 - time.wMilliseconds) && !tti->exit);

	Shell_NotifyIcon(NIM_DELETE, &nid);
	if (nid.hIcon) DestroyIcon(nid.hIcon);
	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	RECT rect0, rect1;
	SYSTEMTIME time;

	switch (msg) {
	case WM_COMMAND:
		if (!HIWORD(wp) && LOWORD(wp)) DestroyWindow(hwnd);
		break;
	case WM_APP_NOTIFYCALLBACK:
		switch (LOWORD(lp)) {
		case WM_CONTEXTMENU:
			if (!hsubmenu) break;
			SetForegroundWindow(hwnd);
			TrackPopupMenuEx(hsubmenu, 0, LOWORD(wp), HIWORD(wp), hwnd, NULL);
			break;
		case NIN_KEYSELECT:
		case NIN_SELECT:
			if (hidden) {
				SystemParametersInfo(SPI_GETWORKAREA, 0, &rect0, 0);
				GetClientRect(hwnd, &rect1);
				SetWindowPos(hwnd, NULL, rect0.right - rect1.right, rect0.bottom - rect1.bottom, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);
				hidden = FALSE;
				MonthCal_GetToday(hcal, &time);
				MonthCal_SetCurSel(hcal, &time);
				MonthCal_SetCurrentView(hcal, MCMV_MONTH);
				SetForegroundWindow(hcal);
			} else {
				ShowWindow(hwnd, SW_HIDE);
				hidden = TRUE;
			}
			break;
		}
		break;
	case WM_DESTROY:
		tti.exit = TRUE;
		SetEvent(tti.hnotify);
		PostQuitMessage(0);
		break;
	default:
		if (msg == WM_TASKBARCREATED) SetEvent(tti.hnotify);
		else return DefWindowProc(hwnd, msg, wp, lp);
	}
	return 0;
}

int APIENTRY _tWinMain(HINSTANCE hinst,	HINSTANCE foo1, LPTSTR foo2, int foo3) {
	MSG msg;
	WNDCLASSEX wcex = {sizeof(wcex)};
	HANDLE htray;
	HMENU hmenu;
	MENUITEMINFO mi = {sizeof(mi)};
	INITCOMMONCONTROLSEX icex;
	RECT rect;
	int style;
	HWND hwnd;

	wcex.lpfnWndProc = WndProc;
	wcex.lpszClassName = WINDOW_CLASS;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	RegisterClassEx(&wcex);

	icex.dwSize = sizeof(icex);
	icex.dwICC = ICC_DATE_CLASSES;
	InitCommonControlsEx(&icex);

	hwnd = CreateWindowEx(WS_EX_NOACTIVATE | WS_EX_TOPMOST, WINDOW_CLASS, WINDOW_TITLE, 0,
		0, 0, 0, 0, NULL, NULL, hinst, NULL);
	if (!hwnd) return 1;
	style = GetWindowLong(hwnd, GWL_STYLE);
	if (style & WS_CAPTION)  { 
		style ^= WS_CAPTION;
		SetWindowLong(hwnd, GWL_STYLE, style);
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	hcal = CreateWindowEx(0, MONTHCAL_CLASS, _T(""),
		WS_CHILD | WS_VISIBLE | MCS_NOTODAY | MCS_NOTRAILINGDATES | MCS_SHORTDAYSOFWEEK | MCS_NOSELCHANGEONNAV,
		0, 0, 0, 0, hwnd, NULL, hinst, NULL);
	MonthCal_GetMinReqRect(hcal, &rect);
	SetWindowPos(hcal, NULL, 0, 0, rect.right, rect.bottom, SWP_NOZORDER | SWP_NOMOVE);
	SetWindowPos(hwnd, NULL, 0, 0, rect.right, rect.bottom, SWP_NOZORDER | SWP_NOMOVE);

	tti.hwnd = hwnd;
	tti.hcal = hcal;
	tti.hnotify = CreateEvent(NULL, TRUE, FALSE, NULL);
	tti.exit = FALSE;
	htray = CreateThread(NULL, 0, &TrayThreadProc, &tti, 0, NULL);
	if (!htray) return 1;

	hsubmenu = CreateMenu();
	mi.fMask = MIIM_STRING | MIIM_ID;
	mi.wID = 1;
	mi.dwTypeData = EXIT_STRING;
	InsertMenuItem(hsubmenu, 0, TRUE, &mi);
	hmenu = CreateMenu();
	mi.fMask = MIIM_SUBMENU;
	mi.hSubMenu = hsubmenu;
	InsertMenuItem(hmenu, 0, TRUE, &mi);

	WM_TASKBARCREATED = RegisterWindowMessageA(_T("TaskbarCreated"));
	
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	DestroyMenu(hmenu);
	DestroyMenu(hsubmenu);

	WaitForSingleObject(htray, 1000);
	CloseHandle(htray);

	return (int)msg.wParam;
}
