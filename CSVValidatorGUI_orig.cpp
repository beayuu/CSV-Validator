// CSV Validator - Modern native Win32 GUI (C++ only)
// No HTML, CSS, JavaScript, Qt, or WebView2 required.
//
// Build with MinGW-w64 / MSYS2 UCRT64:
// g++ CSVValidatorGUI.cpp -o CSVValidator.exe -mwindows -static -static-libgcc -static-libstdc++ -lgdi32 -luser32
//
// Input format: header,fields|data,fields
// Example: name,age|alice,30
//
// A CSV entry is valid only when:
// 1. It contains exactly one '|'
// 2. Every field is non-empty and contains only lowercase letters or digits
// 3. Header and data rows contain the same number of fields

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

// --------------------------- Validation logic --------------------------- //

vector<string> splitByComma(const string& rowText) {
    vector<string> fields;
    string currentField;
    for (char ch : rowText) {
        if (ch == ',') {
            fields.push_back(currentField);
            currentField.clear();
        } else {
            currentField += ch;
        }
    }
    fields.push_back(currentField);
    return fields;
}

bool isValidField(const string& field) {
    if (field.empty()) return false;
    for (char ch : field) {
        const bool lowercase = ch >= 'a' && ch <= 'z';
        const bool digit = ch >= '0' && ch <= '9';
        if (!lowercase && !digit) return false;
    }
    return true;
}

bool areAllFieldsValid(const vector<string>& fields) {
    for (const string& field : fields) {
        if (!isValidField(field)) return false;
    }
    return true;
}

int countBars(const string& entry) {
    return (int)count(entry.begin(), entry.end(), '|');
}

bool isValidCsv(const string& entry) {
    if (countBars(entry) != 1) return false;
    const size_t bar = entry.find('|');
    vector<string> header = splitByComma(entry.substr(0, bar));
    vector<string> data = splitByComma(entry.substr(bar + 1));
    if (!areAllFieldsValid(header) || !areAllFieldsValid(data)) return false;
    return header.size() == data.size();
}

string padRight(const string& text, size_t width) {
    string result = text;
    while (result.size() < width) result += ' ';
    return result;
}

string getCell(const vector<string>& fields, size_t position) {
    return position < fields.size() ? fields[position] : "";
}

string rowText(const vector<string>& fields, const vector<size_t>& widths) {
    string result;
    for (size_t i = 0; i < widths.size(); ++i) {
        if (i) result += "|";
        result += " " + padRight(getCell(fields, i), widths[i]) + " ";
    }
    return result + "\r\n";
}

string dividerText(const vector<size_t>& widths) {
    string result;
    for (size_t i = 0; i < widths.size(); ++i) {
        if (i) result += "+";
        result += string(widths[i] + 2, '-');
    }
    return result + "\r\n";
}

string entryAsTableText(const string& entry, int number, bool valid) {
    size_t bar = entry.find('|');
    bool hasData = bar != string::npos;
    vector<string> header;
    vector<string> data;

    if (hasData) {
        header = splitByComma(entry.substr(0, bar));
        data = splitByComma(entry.substr(bar + 1));
    } else {
        header = splitByComma(entry);
    }

    size_t columns = max(header.size(), data.size());
    vector<size_t> widths;
    for (size_t i = 0; i < columns; ++i) {
        size_t width = max<size_t>(1, getCell(header, i).size());
        width = max(width, getCell(data, i).size());
        widths.push_back(width);
    }

    string result = "Entry #" + to_string(number) + " - " +
                    (valid ? "VALID" : "INVALID") + "\r\n";
    result += rowText(header, widths);
    result += dividerText(widths);
    result += hasData ? rowText(data, widths) : " (no data row)\r\n";
    return result + "\r\n";
}

string dataTableText(const vector<string>& entries) {
    if (entries.empty()) return "No data entered yet.\r\n";

    string result;
    int validCount = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        bool valid = isValidCsv(entries[i]);
        if (valid) ++validCount;
        result += entryAsTableText(entries[i], (int)i + 1, valid);
    }
    result += "Total: " + to_string(entries.size()) + " entries, " +
              to_string(validCount) + " valid, " +
              to_string(entries.size() - validCount) + " invalid\r\n";
    return result;
}

// ------------------------------- Theme ---------------------------------- 

const COLORREF C_BG       = RGB(242, 245, 249);
const COLORREF C_NAVY     = RGB(25, 39, 62);
const COLORREF C_NAVY2    = RGB(35, 52, 79);
const COLORREF C_WHITE    = RGB(255, 255, 255);
const COLORREF C_TEXT     = RGB(36, 48, 66);
const COLORREF C_MUTED    = RGB(105, 119, 139);
const COLORREF C_BORDER   = RGB(220, 227, 236);
const COLORREF C_BLUE     = RGB(57, 103, 214);
const COLORREF C_BLUEPALE = RGB(235, 241, 255);
const COLORREF C_GREEN    = RGB(27, 133, 86);
const COLORREF C_GREENPALE= RGB(230, 247, 238);
const COLORREF C_RED      = RGB(190, 55, 64);
const COLORREF C_REDBLE   = RGB(255, 237, 238);
const COLORREF C_INPUT    = RGB(250, 251, 253);

enum {
    ID_MODE1 = 101, ID_MODE2, ID_INPUT, ID_ADD, ID_SHOW, ID_QUIT,
    ID_LOG, ID_TABLE, ID_RULE1, ID_RULE2, ID_RULE3, ID_COUNT
};

HWND hMain = NULL, hMode1 = NULL, hMode2 = NULL, hInput = NULL;
HWND hAdd = NULL, hShow = NULL, hQuit = NULL, hLog = NULL, hTable = NULL;
HWND hRule[3] = { NULL, NULL, NULL }, hCount = NULL;
HFONT fontBody = NULL, fontSmall = NULL, fontTitle = NULL, fontHeading = NULL, fontMono = NULL;
WNDPROC oldEditProc = NULL;
int outputMode = 0; // 0 = not chosen, 1 = on request, 2 = instant
vector<string> savedEntries;

string getInputText() {
    int length = GetWindowTextLengthA(hInput);
    string value(length + 1, '\0');
    if (length > 0) GetWindowTextA(hInput, &value[0], length + 1);
    value.resize(length);
    return value;
}

void setFont(HWND control, HFONT font) {
    if (control && font) SendMessageA(control, WM_SETFONT, (WPARAM)font, TRUE);
}

void drawRoundRect(HDC dc, RECT r, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawText(HDC dc, const char* text, RECT r, COLORREF color, HFONT font,
              UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HGDIOBJ old = font ? SelectObject(dc, font) : NULL;
    DrawTextA(dc, text, -1, &r, flags);
    if (old) SelectObject(dc, old);
}

void drawCard(HDC dc, RECT r) {
    drawRoundRect(dc, r, 14, C_WHITE, C_BORDER);
}

void paintButton(LPDRAWITEMSTRUCT dis) {
    if (!dis || !dis->hwndItem) return;
    int id = GetDlgCtrlID(dis->hwndItem);
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool selectedMode = (id == ID_MODE1 && outputMode == 1) ||
                        (id == ID_MODE2 && outputMode == 2);
    COLORREF fill = C_WHITE, border = C_BORDER, text = C_TEXT;

    if (id == ID_ADD) {
        fill = pressed ? RGB(40, 79, 176) : C_BLUE;
        border = fill; text = C_WHITE;
    } else if (id == ID_SHOW) {
        fill = pressed ? C_BLUEPALE : C_WHITE;
        border = C_BORDER; text = C_TEXT;
    } else if (id == ID_QUIT) {
        fill = pressed ? C_REDBLE : C_WHITE;
        border = C_BORDER; text = C_MUTED;
    } else if (id == ID_MODE1 || id == ID_MODE2) {
        fill = selectedMode ? C_BLUEPALE : C_WHITE;
        border = selectedMode ? C_BLUE : C_BORDER;
        text = selectedMode ? C_BLUE : C_TEXT;
    }
    if (disabled) {
        fill = RGB(239, 242, 246);
        text = RGB(155, 164, 177);
    }

    RECT r = dis->rcItem;
    drawRoundRect(dis->hDC, r, 9, fill, border);
    RECT tr = r;
    tr.left += 12; tr.right -= 12;
    char label[256] = {};
    GetWindowTextA(dis->hwndItem, label, sizeof(label));
    drawText(dis->hDC, label, tr, text, fontBody, DT_LEFT | DT_VCENTER | DT_WORDBREAK);
    if (dis->itemState & ODS_FOCUS) {
        RECT focus = r;
        InflateRect(&focus, -5, -5);
        DrawFocusRect(dis->hDC, &focus);
    }
}

void ruleStates(const string& raw, int states[3]) {
    states[0] = states[1] = states[2] = -1;
    if (raw.empty() || raw == "show" || raw == "quit") return;
    if (countBars(raw) != 1) { states[0] = 0; return; }
    states[0] = 1;
    size_t bar = raw.find('|');
    vector<string> header = splitByComma(raw.substr(0, bar));
    vector<string> data = splitByComma(raw.substr(bar + 1));
    bool fieldsOk = areAllFieldsValid(header) && areAllFieldsValid(data);
    states[1] = fieldsOk ? 1 : 0;
    if (!fieldsOk) return;
    states[2] = header.size() == data.size() ? 1 : 0;
}

void updateRules() {
    const char* labels[3] = {
        "Exactly one | (two rows)",
        "Fields are non-empty: a-z and 0-9",
        "Header and data have equal field counts"
    };
    int states[3];
    ruleStates(getInputText(), states);
    for (int i = 0; i < 3; ++i) {
        string text = states[i] < 0 ? "[  -  ]   " : (states[i] ? "[PASS]   " : "[FAIL]   ");
        text += labels[i];
        SetWindowTextA(hRule[i], text.c_str());
        InvalidateRect(hRule[i], NULL, TRUE);
    }
}

void updateCount() {
    int valid = 0;
    for (const string& entry : savedEntries) if (isValidCsv(entry)) ++valid;
    string text = to_string(savedEntries.size()) + " entered   |   " +
                  to_string(valid) + " valid   |   " +
                  to_string(savedEntries.size() - valid) + " invalid";
    SetWindowTextA(hCount, text.c_str());
}

void setControlsEnabled(bool enabled) {
    EnableWindow(hInput, enabled);
    EnableWindow(hAdd, enabled);
    EnableWindow(hShow, enabled);
    InvalidateRect(hAdd, NULL, TRUE);
    InvalidateRect(hShow, NULL, TRUE);
}

void setMode(int mode) {
    outputMode = mode;
    savedEntries.clear();
    SendMessageA(hLog, LB_RESETCONTENT, 0, 0);
    SetWindowTextA(hTable, "");
    SetWindowTextA(hInput, "");
    setControlsEnabled(true);
    updateCount();
    updateRules();
    InvalidateRect(hMain, NULL, TRUE);
    InvalidateRect(hMode1, NULL, TRUE);
    InvalidateRect(hMode2, NULL, TRUE);
    SetFocus(hInput);
}

void resetToMenu() {
    outputMode = 0;
    savedEntries.clear();
    SendMessageA(hLog, LB_RESETCONTENT, 0, 0);
    SetWindowTextA(hTable, "");
    SetWindowTextA(hInput, "");
    setControlsEnabled(false);
    updateCount();
    updateRules();
    InvalidateRect(hMain, NULL, TRUE);
    InvalidateRect(hMode1, NULL, TRUE);
    InvalidateRect(hMode2, NULL, TRUE);
}

void showTable() {
    SetWindowTextA(hTable, dataTableText(savedEntries).c_str());
}

void doQuit() {
    int answer = MessageBoxA(
        hMain,
        "Yes: choose another output option and clear saved entries\n"
        "No: close the application\n"
        "Cancel: keep working",
        "CSV Validator", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (answer == IDYES) resetToMenu();
    else if (answer == IDNO) DestroyWindow(hMain);
}

void submitEntry() {
    if (outputMode == 0) return;
    string value = getInputText();
    if (value.empty()) return;
    if (value == "quit") { SetWindowTextA(hInput, ""); doQuit(); return; }
    if (value == "show") { SetWindowTextA(hInput, ""); showTable(); return; }

    savedEntries.push_back(value);
    string line;
    if (outputMode == 2) {
        line = string(isValidCsv(value) ? "ACCEPT" : "REJECT") + "   #" +
               to_string(savedEntries.size()) + "   " + value;
    } else {
        line = "Saved as entry #" + to_string(savedEntries.size()) + "   " + value;
    }
    int index = (int)SendMessageA(hLog, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    SendMessageA(hLog, LB_SETTOPINDEX, index, 0);
    SetWindowTextA(hInput, "");
    updateCount();
    SetFocus(hInput);
}

LRESULT CALLBACK InputProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CHAR && wParam == VK_RETURN) {
        submitEntry();
        return 0;
    }
    return CallWindowProcA(oldEditProc, hwnd, msg, wParam, lParam);
}

HWND makeControl(const char* cls, const char* text, DWORD style,
                 int x, int y, int width, int height, int id, DWORD exStyle = 0) {
    return CreateWindowExA(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, hMain, (HMENU)(INT_PTR)id,
        GetModuleHandleA(NULL), NULL);
}

void createFonts() {
    fontBody = CreateFontA(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, "Segoe UI");
    fontSmall = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, "Segoe UI");
    fontTitle = CreateFontA(-30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, "Segoe UI");
    fontHeading = CreateFontA(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, "Segoe UI");
    fontMono = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH, "Consolas");
}

void createControls() {
    createFonts();

    // Header title and subtitle are custom-painted in WM_PAINT.

    // Output choice cards
    hMode1 = makeControl("BUTTON",
        "SHOW ON REQUEST\r\nResults appear when you click Show data table.",
        BS_OWNERDRAW | WS_TABSTOP, 32, 164, 292, 86, ID_MODE1);
    hMode2 = makeControl("BUTTON",
        "INSTANT RESULT\r\nACCEPT or REJECT appears after each entry.",
        BS_OWNERDRAW | WS_TABSTOP, 340, 164, 292, 86, ID_MODE2);

    // Data entry
    hInput = makeControl("EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP,
        48, 350, 576, 38, ID_INPUT, WS_EX_CLIENTEDGE);
    hAdd = makeControl("BUTTON", "Add entry", BS_OWNERDRAW | WS_TABSTOP,
        48, 402, 166, 40, ID_ADD);
    hShow = makeControl("BUTTON", "Show data table", BS_OWNERDRAW | WS_TABSTOP,
        226, 402, 190, 40, ID_SHOW);
    hQuit = makeControl("BUTTON", "Quit / change mode", BS_OWNERDRAW | WS_TABSTOP,
        428, 402, 196, 40, ID_QUIT);

    // Live rules
    hRule[0] = makeControl("STATIC", "", SS_LEFT | SS_CENTERIMAGE,
        48, 504, 570, 24, ID_RULE1);
    hRule[1] = makeControl("STATIC", "", SS_LEFT | SS_CENTERIMAGE,
        48, 536, 570, 24, ID_RULE2);
    hRule[2] = makeControl("STATIC", "", SS_LEFT | SS_CENTERIMAGE,
        48, 568, 570, 24, ID_RULE3);

    // Output panel and data table
    hLog = makeControl("LISTBOX", "", LBS_NOSEL | WS_VSCROLL |
        WS_HSCROLL | LBS_NOINTEGRALHEIGHT, 678, 174, 396, 190, ID_LOG,
        WS_EX_CLIENTEDGE);
    hTable = makeControl("EDIT", "", ES_MULTILINE | ES_READONLY |
        WS_VSCROLL | WS_HSCROLL | ES_AUTOHSCROLL,
        678, 414, 396, 180, ID_TABLE, WS_EX_CLIENTEDGE);
    SendMessageA(hTable, EM_SETLIMITTEXT, 1000000, 0);
    hCount = makeControl("STATIC", "", SS_LEFT | SS_CENTERIMAGE,
        680, 606, 392, 24, ID_COUNT);

    setFont(hInput, fontMono);
    setFont(hLog, fontMono);
    setFont(hTable, fontMono);
    setFont(hAdd, fontBody);
    setFont(hShow, fontBody);
    setFont(hQuit, fontBody);
    for (int i = 0; i < 3; ++i) setFont(hRule[i], fontBody);
    setFont(hCount, fontSmall);

    oldEditProc = (WNDPROC)SetWindowLongPtrA(hInput, GWLP_WNDPROC, (LONG_PTR)InputProc);
    resetToMenu();
}

void paintMain(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client;
    GetClientRect(hwnd, &client);

    HBRUSH bg = CreateSolidBrush(C_BG);
    FillRect(dc, &client, bg);
    DeleteObject(bg);

    // Header
    RECT header = { 0, 0, client.right, 124 };
    HBRUSH navy = CreateSolidBrush(C_NAVY);
    FillRect(dc, &header, navy);
    DeleteObject(navy);

    RECT eyebrow = { 36, 12, 650, 30 };
    drawText(dc, "DESKTOP TOOL  /  CSV QUALITY CHECK", eyebrow,
             RGB(178, 199, 232), fontSmall);
    // Static controls render over this area, but repaint title styling here too.
    RECT title = { 34, 30, 720, 66 };
    drawText(dc, "CSV File Validator", title, C_WHITE, fontTitle);
    RECT subtitle = { 36, 70, 900, 100 };
    drawText(dc, "Validate two-row CSV entries with clear, immediate feedback.",
             subtitle, RGB(218, 226, 239), fontBody);

    // Cards
    RECT modeCard1 = { 24, 142, 640, 264 };
    RECT modeCard2 = { 666, 142, 1088, 390 };
    RECT entryCard = { 24, 278, 640, 454 };
    RECT rulesCard = { 24, 468, 640, 638 };
    RECT outputCard = { 666, 142, 1088, 390 };
    RECT tableCard = { 666, 402, 1088, 642 };

    // left output-choice card background; mode controls sit inside
    drawCard(dc, modeCard1);
    // data card and rules card
    drawCard(dc, entryCard);
    drawCard(dc, rulesCard);
    // right output and table cards
    drawCard(dc, outputCard);
    drawCard(dc, tableCard);

    RECT section = { 46, 286, 620, 316 };
    drawText(dc, "2. Enter your data", section, C_TEXT, fontHeading);
    RECT hint = { 48, 316, 620, 340 };
    drawText(dc, "Use a vertical bar | to separate header and data rows.",
             hint, C_MUTED, fontSmall);

    RECT ruleTitle = { 46, 480, 620, 510 };
    drawText(dc, "Validation rules", ruleTitle, C_TEXT, fontHeading);
    RECT ruleHint = { 48, 510, 620, 532 };
    drawText(dc, "Checked live as you type", ruleHint, C_MUTED, fontSmall);

    RECT outputTitle = { 688, 150, 1000, 178 };
    drawText(dc, "Output", outputTitle, C_TEXT, fontHeading);
    RECT tableTitle = { 688, 410, 1000, 438 };
    drawText(dc, "Data table", tableTitle, C_TEXT, fontHeading);

    // Small right-aligned entry count in output heading
    RECT count = { 900, 150, 1066, 178 };
    int valid = 0;
    for (const string& e : savedEntries) if (isValidCsv(e)) ++valid;
    string countText = to_string(savedEntries.size()) + " entered";
    drawText(dc, countText.c_str(), count, C_MUTED, fontSmall,
             DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hMain = hwnd;
        createControls();
        return 0;

    case WM_PAINT:
        paintMain(hwnd);
        return 0;

    case WM_DRAWITEM:
        paintButton((LPDRAWITEMSTRUCT)lParam);
        return TRUE;

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_INPUT);
        static HBRUSH editBrush = CreateSolidBrush(C_INPUT);
        return (LRESULT)editBrush;
    }
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wParam;
        HWND control = (HWND)lParam;
        SetTextColor(dc, C_TEXT);
        if (control == hCount) {
            SetBkColor(dc, C_WHITE);
            static HBRUSH whiteBrush = CreateSolidBrush(C_WHITE);
            return (LRESULT)whiteBrush;
        }
        for (int i = 0; i < 3; ++i) {
            if (control == hRule[i]) {
                SetBkColor(dc, C_WHITE);
                static HBRUSH ruleBrush = CreateSolidBrush(C_WHITE);
                return (LRESULT)ruleBrush;
            }
        }
        SetBkColor(dc, C_WHITE);
        static HBRUSH whiteBrush2 = CreateSolidBrush(C_WHITE);
        return (LRESULT)whiteBrush2;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_MODE1:
            if (HIWORD(wParam) == BN_CLICKED) setMode(1);
            break;
        case ID_MODE2:
            if (HIWORD(wParam) == BN_CLICKED) setMode(2);
            break;
        case ID_INPUT:
            if (HIWORD(wParam) == EN_CHANGE) updateRules();
            break;
        case ID_ADD:
            submitEntry();
            break;
        case ID_SHOW:
            showTable();
            SetFocus(hInput);
            break;
        case ID_QUIT:
            doQuit();
            break;
        }
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = (MINMAXINFO*)lParam;
        info->ptMinTrackSize.x = 1120;
        info->ptMinTrackSize.y = 720;
        return 0;
    }

    case WM_DESTROY:
        if (oldEditProc && hInput) {
            SetWindowLongPtrA(hInput, GWLP_WNDPROC, (LONG_PTR)oldEditProc);
        }
        if (fontBody) DeleteObject(fontBody);
        if (fontSmall) DeleteObject(fontSmall);
        if (fontTitle) DeleteObject(fontTitle);
        if (fontHeading) DeleteObject(fontHeading);
        if (fontMono) DeleteObject(fontMono);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCmd) {
    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(C_BG);
    wc.lpszClassName = "ModernCsvValidatorWindow";
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                  WS_MINIMIZEBOX | WS_VISIBLE;
    RECT rect = { 0, 0, 1120, 720 };
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowA(
        "ModernCsvValidatorWindow", "CSV File Validator",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, instance, NULL);

    if (!hwnd) {
        MessageBoxA(NULL, "The application window could not be created.",
                    "CSV Validator", MB_ICONERROR | MB_OK);
        return 1;
    }

    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
