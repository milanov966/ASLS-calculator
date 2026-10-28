// Windows (Win32) графичен интерфейс за калкулатора за анализ и синтез на логически схеми
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "core.h"

enum { ID_COMBO = 101, ID_INPUT, ID_SOLVE, ID_OUT, ID_STYLE, ID_HINT };
static HWND hCombo, hInput, hSolve, hOut, hStyle, hHint, hLbl1, hLbl2;
static HFONT hUi, hMono;

static string toUtf8(const wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), 0, 0, 0, 0);
    string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, 0, 0); return s;
}
static wstring toWide(const string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), 0, 0);
    wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n); return w;
}
static string crlf(const string& s) {
    string r; for (char c : s) { if (c == '\n') r += '\r'; r += c; } return r;
}
static const wchar_t* kHints[4] = {
    L"Пример:  !a*b+c",
    L"Пример:  !(a+b)*c+a*b+a*!b",
    L"Пример:  a+!a*b=a+b      (допускат се и !!!a)",
    L"Пример:  f(x,y,z)=(x+y)(!z+y)"};

static void solve() {
    wchar_t buf[2048]; GetWindowTextW(hInput, buf, 2048);
    string in = trim(toUtf8(buf));
    if (in.empty()) { SetWindowTextW(hOut, L"Въведете израз."); return; }
    g_over = SendMessageW(hStyle, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_reader = [&](const string&, string& out) { out = in; return true; };
    ostringstream oss; auto* old = cout.rdbuf(oss.rdbuf());
    try {
        switch ((int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0)) {
        case 0: taskCircuit(); break;
        case 1: taskAlgebra(); break;
        case 2: taskIdentity(); break;
        default: taskTable(); break;
        }
    } catch (const exception& e) { cout << "Грешка: " << e.what() << "\n"; }
    cout.rdbuf(old);
    SetWindowTextW(hOut, toWide(crlf(oss.str())).c_str());
}

static HWND mk(const wchar_t* cls, const wchar_t* txt, DWORD st, HWND p, int id, DWORD ex = 0) {
    HWND h = CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | st, 0, 0, 10, 10, p, (HMENU)(INT_PTR)id, GetModuleHandleW(0), 0);
    SendMessageW(h, WM_SETFONT, (WPARAM)hUi, TRUE); return h;
}
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE:
        hUi = CreateFontW(-17, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        hMono = CreateFontW(-17, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        hLbl1 = mk(L"STATIC", L"Тип задача:", 0, h, 0);
        hCombo = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, h, ID_COMBO);
        for (auto t : {L"1. Свързване на логическа схема (И, ИЛИ, НЕ)", L"2. Алгебрични преобразувания със закони (пълно решение)",
                       L"3. Доказване на тъждество", L"4. Функция: таблица на истината, минтерми, макстерми + алгебрично решение"})
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)t);
        SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
        hLbl2 = mk(L"STATIC", L"Израз:", 0, h, 0);
        hInput = mk(L"EDIT", L"!a*b+c", ES_AUTOHSCROLL | WS_TABSTOP, h, ID_INPUT, WS_EX_CLIENTEDGE);
        hSolve = mk(L"BUTTON", L"Реши", BS_DEFPUSHBUTTON | WS_TABSTOP, h, ID_SOLVE);
        hStyle = mk(L"BUTTON", L"Инверсия с надчерта (a̅)", BS_AUTOCHECKBOX | WS_TABSTOP, h, ID_STYLE);
        hHint = mk(L"STATIC", kHints[0], 0, h, ID_HINT);
        hOut = mk(L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL, h, ID_OUT, WS_EX_CLIENTEDGE);
        SendMessageW(hOut, WM_SETFONT, (WPARAM)hMono, TRUE);
        SendMessageW(hOut, EM_SETLIMITTEXT, 0x7FFFFFFF, 0);
        SendMessageW(hHint, WM_SETFONT, (WPARAM)hUi, TRUE);
        return 0;
    case WM_SIZE: {
        int W = LOWORD(l), H = HIWORD(l), M = 12;
        MoveWindow(hLbl1, M, 14, 100, 24, TRUE);
        MoveWindow(hCombo, 120, 10, W - 120 - M, 300, TRUE);
        MoveWindow(hLbl2, M, 52, 100, 24, TRUE);
        MoveWindow(hInput, 120, 48, W - 120 - 110 - 2 * M, 28, TRUE);
        MoveWindow(hSolve, W - 110 - M, 48, 110, 28, TRUE);
        MoveWindow(hHint, 120, 82, W - 120 - 260, 22, TRUE);
        MoveWindow(hStyle, W - 250, 82, 240, 22, TRUE);
        MoveWindow(hOut, M, 112, W - 2 * M, H - 112 - M, TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO: ((MINMAXINFO*)l)->ptMinTrackSize = {640, 420}; return 0;
    case WM_COMMAND:
        if (LOWORD(w) == ID_SOLVE || LOWORD(w) == IDOK) solve();
        else if (LOWORD(w) == ID_COMBO && HIWORD(w) == CBN_SELCHANGE) {
            int i = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
            const wchar_t* ex[4] = {L"!a*b+c", L"!(a+b)*c+a*b+a*!b", L"a+!a*b=a+b", L"f(x,y,z)=(x+y)(!z+y)"};
            SetWindowTextW(hHint, kHints[i]); SetWindowTextW(hInput, ex[i]);
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int show) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = L"LogicCalc";
    wc.hCursor = LoadCursorW(0, (LPCWSTR)IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);
    HWND h = CreateWindowExW(0, L"LogicCalc", L"Анализ и синтез на логически схеми", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 980, 720, 0, 0, hi, 0);
    ShowWindow(h, show); UpdateWindow(h);
    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0)) { if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    return 0;
}
