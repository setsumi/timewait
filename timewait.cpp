// timewait.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <iostream>
#include <format>
//#include <locale>
#include <codecvt>

#include <shobjidl.h>
#pragma comment(lib, "shell32.lib")

//---------------------------------------------------------------------------
#define MSG_WNDCLASS L"timewaitMessageWindow"
#define IDT_TIMER1 1001

HINSTANCE hInstance = GetModuleHandle(NULL);
HWND hMsgWnd = NULL;
int Countdown = 10; // default
int CountdownMax = 10;

//---------------------------------------------------------------------------
void UpdateTaskbarProgress(HWND hwnd, ULONGLONG completed, ULONGLONG total)
{
	std::ignore = CoInitialize(NULL);
	ITaskbarList3* pTaskbarList;
	HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, (void**)&pTaskbarList);
	if (SUCCEEDED(hr))
	{
		hr = pTaskbarList->HrInit();
		if (SUCCEEDED(hr))
		{
			if (!completed && !total) {
				pTaskbarList->SetProgressState(hwnd, TBPF_NOPROGRESS);
			}
			else {
				hr = pTaskbarList->SetProgressState(hwnd, TBPF_NORMAL);
				if (SUCCEEDED(hr))
				{
					hr = pTaskbarList->SetProgressValue(hwnd, completed, total);
				}
			}
		}
		pTaskbarList->Release();
	}
	CoUninitialize();
}

//---------------------------------------------------------------------------
std::string ws2s(const std::wstring wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

//---------------------------------------------------------------------------
void GetModuleName(wchar_t* buffer, DWORD size)
{
	if (!GetModuleFileName(NULL, buffer, size)) {
		std::cout << std::format("GetModuleName() fail: {}\n", GetLastError());
		buffer[0] = L'\0';
	}
}

//---------------------------------------------------------------------------
std::string GetProgramVer()
{
	wchar_t mod[MAX_PATH];
	GetModuleName(mod, sizeof(mod));

	std::string ret;
	DWORD hnd = 0, vis;
	if (0 != (vis = GetFileVersionInfoSize(mod, &hnd)))
	{
		BYTE* data = new BYTE[vis];
		if (GetFileVersionInfo(mod, 0, vis, data))
		{
			UINT ilen = 0;

			struct LANGANDCODEPAGE
			{
				WORD wLanguage;
				WORD wCodePage;
			}*lpTranslate;

			// Read first language and code page
			if (VerQueryValue(data, L"\\VarFileInfo\\Translation",
				(LPVOID*)&lpTranslate, &ilen))
			{
				std::wstring str = std::format(L"\\StringFileInfo\\{:04x}{:04x}\\FileVersion",
					lpTranslate->wLanguage, lpTranslate->wCodePage);
				// Retrieve file version for language and code page
				wchar_t* pver = NULL;
				if (VerQueryValue(data, str.c_str(), (LPVOID*)&pver, &ilen))
				{
					ret = ws2s(pver);
				}
			}
		}
		delete[]data;
	}
	return ret;
}

//---------------------------------------------------------------------------
COORD GetConsoleCursorPosition()
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO cbsi;
	if (GetConsoleScreenBufferInfo(h, &cbsi))
	{
		return cbsi.dwCursorPosition;
	}
	else
	{
		// The function failed. Call GetLastError() for details.
		COORD invalid = { 0, 0 };
		return invalid;
	}
}

//---------------------------------------------------------------------------
void SetConsoleCursorPosition(COORD c)
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleCursorPosition(h, c);
}

//---------------------------------------------------------------------------
bool GetConsoleChar(KEY_EVENT_RECORD& krec)
{
	DWORD cc;
	INPUT_RECORD irec;
	DWORD evnum = 0;
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	if (h == NULL)
	{
		return false; // console not found
	}

	GetNumberOfConsoleInputEvents(h, &evnum);
	for (size_t i = 0; i < evnum; i++)
	{
		ReadConsoleInput(h, &irec, 1, &cc);
		if (irec.EventType == KEY_EVENT && ((KEY_EVENT_RECORD&)irec.Event).bKeyDown)
		{
			krec = (KEY_EVENT_RECORD&)irec.Event;
			return true;
		}
	}
	return false;
}

//---------------------------------------------------------------------------
void print_countdown()
{
	static const char* tail = " seconds...  ";
	static const size_t tail_len = strlen(tail);
	char buffer[32];
	size_t len;
	COORD pos;
	_itoa_s(Countdown, buffer, sizeof(buffer), 10);
	len = strlen(buffer);
	std::cout << "\rTimeout " << std::string(len, ' ') << tail;
	pos = GetConsoleCursorPosition();
	pos.X -= (SHORT)(tail_len + len);
	SetConsoleCursorPosition(pos);
	std::cout << buffer;

	SetConsoleTitleA(buffer);
	UpdateTaskbarProgress(GetConsoleWindow(), CountdownMax - Countdown, CountdownMax);
}

//---------------------------------------------------------------------------
LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_TIMER:
		switch (wParam)
		{
		case IDT_TIMER1:
			// process the 1-second timer
			Countdown--;
			print_countdown();
			return 0;
		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

//---------------------------------------------------------------------------
HWND CreateMessageWindow()
{
	// Register the window class
	WNDCLASSEX wc =
	{
		sizeof(WNDCLASSEX), CS_DBLCLKS, MessageWindowProc, 0L, 0L,
		hInstance, NULL, NULL, NULL, NULL,
		MSG_WNDCLASS, NULL
	};
	if (0 == RegisterClassEx(&wc)) {
		std::cout << "CreateMessageWindow(): RegisterClassEx() fail\n";
		return NULL;
	}

	// Create the application's window
	HWND hWnd = CreateWindow(MSG_WNDCLASS, L"timewait Message Window",
		WS_OVERLAPPEDWINDOW, 0, 0, 640, 480,
		NULL, NULL, hInstance, NULL);
	if (hWnd == NULL) {
		std::cout << "CreateMessageWindow(): CreateWindow() fail\n";
	}
	return hWnd;
}

//---------------------------------------------------------------------------
#define STR_KEYHELP "<Enter>, <Space> proceed; <Esc> abort\n"

void print_help()
{
	std::cout << std::format("timewait v{}\nsimple console countdown\n\n", GetProgramVer());
	std::cout << "Usage:\n";
	std::cout << "    timewait.exe t\n";
	std::cout << "Parameters:\n";
	std::cout << "    t\ttimeout interval in seconds\n";
	std::cout << "Example:\n";
	std::cout << "    timewait.exe 10\twait for 10 seconds\n";
	std::cout << "Remarks:\n";
	std::cout << "    " << STR_KEYHELP;
	std::cout << "    proceed - set errorlevel 0\n";
	std::cout << "    abort   - set errorlevel 1"; // extra linebreak printed on quit
}

//---------------------------------------------------------------------------
int main(int argc, char** argv)
{
	//std::cout << argc << std::endl;
	//for (int i = 0; i < argc; i++)
	//	std::cout << argv[i] << std::endl;

	// store console title
	TCHAR s_title[1024];
	DWORD c_title = GetConsoleTitle(s_title, 1024);

	if (argc > 1 && (Countdown = strtol(argv[1], NULL, 10)) > 0) {
		// argument is good, proceed
		CountdownMax = Countdown;
	}
	else { // no/bad arguments, print help
		print_help();
		goto quit;
	}

	// create window
	if (NULL == (hMsgWnd = CreateMessageWindow())) {
		std::cout << "CreateMessageWindow() fail";
		goto quit;
	}

	// print header
	std::cout << STR_KEYHELP;
	print_countdown();

	// start timeout timer
	SetTimer(hMsgWnd, IDT_TIMER1, 1000, (TIMERPROC)NULL);

	// run message loop
	KEY_EVENT_RECORD key;
	MSG messages;
	while (true) {
		if (PeekMessage(&messages, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&messages);
			DispatchMessage(&messages);
		}

		if (GetConsoleChar(key)) {
			switch (key.wVirtualKeyCode) {
			case 13: // Enter
			case 32: // Space
				goto quit; // exit-proceed
			case 27: // Esc
				std::cout << "\nAbort\n";
				return 1; // exit-abort
			}
			//std::cout << "key: " << key.uChar.AsciiChar << " code:  " << key.wVirtualKeyCode << std::endl;
		}
		if (Countdown <= 0) goto quit; // exit-proceed on timeout elapsed
		Sleep(1);
	}
quit:
	std::cout << std::endl;

	if (c_title) {
		SetConsoleTitle(s_title);
	}
	UpdateTaskbarProgress(GetConsoleWindow(), 0, 0);

	return 0;
}

