#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <winreg.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <string>
#include <new>
#include <stdarg.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ===========================================================================
//  资源 IDs
// ===========================================================================
#define IDR_OEMFONT   101  // FONT 资源 ID (.rc)
#define IDI_APPICON   100  // ICON 资源 ID (.rc)

// ===========================================================================
//  注册表路径 — OEM 信息存储位置
// ===========================================================================
static const wchar_t* REG_KEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OEMInformation";

// ===========================================================================
//  配置文件名常量
// ===========================================================================
static const wchar_t* EXPORT_FILENAME = L"oeminfo.oeminfo";  // 导出文件
static const wchar_t* BACKUP_FILENAME = L"oeminfo.oembak";
static wchar_t g_szExeName[MAX_PATH] = L"oemedit.exe";  // cached exe filename for display   // 启动检测备份

// ===========================================================================
//  INI 格式常量
// ===========================================================================
static const wchar_t* INI_SECTION   = L"OEMInformation";
static const wchar_t* INI_KEY_MFR    = L"Manufacturer";
static const wchar_t* INI_KEY_MODEL = L"Model";
static const wchar_t* INI_KEY_PHONE  = L"SupportPhone";
static const wchar_t* INI_KEY_URL    = L"SupportURL";
static const wchar_t* INI_KEY_HOURS  = L"SupportHours";

// ===========================================================================
//  字体 & 画刷
// ===========================================================================
static HFONT g_hFontTitle    = nullptr;
static HFONT g_hFontNormal   = nullptr;
static HFONT g_hFontFooter   = nullptr;
static bool  g_bCustomFont   = false;
static wchar_t g_szFontName[64] = L"Microsoft YaHei";

// Initialize g_szExeName from the actual module path
static void InitExeName()
{
    wchar_t fullPath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, fullPath, MAX_PATH);
    const wchar_t* fname = wcsrchr(fullPath, L'\\');
    if (!fname) fname = wcsrchr(fullPath, L'/');
    if (fname) wcscpy_s(g_szExeName, fname + 1);
    else wcscpy_s(g_szExeName, fullPath);
}

static HBRUSH g_hBrushBg         = nullptr;
static HBRUSH g_hBrushBtnApply   = nullptr;
static HBRUSH g_hBrushBtnOK      = nullptr;
static HBRUSH g_hBrushBtnCancel  = nullptr;
static HBRUSH g_hBrushBtnExport  = nullptr;
static HBRUSH g_hBrushBtnImport  = nullptr;
static HBRUSH g_hBrushBtnHelp    = nullptr;

static const COLORREF CLR_BTN_HELP = RGB(120, 120, 130);   // 帮助 - 深灰

// ---- 颜色 ----
static const COLORREF CLR_BG           = RGB(245, 245, 248);
static const COLORREF CLR_TITLE        = RGB(30,  30,  60);
static const COLORREF CLR_FOOTER       = RGB(150, 150, 150);
static const COLORREF CLR_BTN_OK       = RGB(76,  175, 80);    // 确定 - 绿色
static const COLORREF CLR_BTN_APPLY    = RGB(66,  133, 244);   // 应用 - 蓝色
static const COLORREF CLR_BTN_CANCEL  = RGB(200, 200, 200);   // 取消 - 浅灰
static const COLORREF CLR_BTN_EXPORT  = RGB(255, 152, 0);     // 导出 - 橙色
static const COLORREF CLR_BTN_IMPORT  = RGB(156, 39, 176);     // 导入 - 紫色

// ===========================================================================
//  主窗口控件 IDs
// ===========================================================================
#define IDC_LABEL_TITLE   101
#define IDC_LABEL_MFR     102
#define IDC_EDIT_MFR      103
#define IDC_LABEL_MODEL   104
#define IDC_EDIT_MODEL    105
#define IDC_LABEL_PHONE   106
#define IDC_EDIT_PHONE   107
#define IDC_LABEL_URL     108
#define IDC_EDIT_URL      109
#define IDC_LABEL_HOURS   110
#define IDC_EDIT_HOURS    111
#define IDC_LABEL_FOOTER  112

#define IDC_BTN_IMPORT    201   // 导入按钮
#define IDC_BTN_EXPORT    202   // 导出按钮
#define IDC_BTN_CANCEL    203   // 取消按钮
#define IDC_BTN_APPLY     204   // 应用按钮（保存）
#define IDC_BTN_OK        205   // 确定按钮（保存并退出）
#define IDC_BTN_HELP       206   // 帮助按钮（命令行用法）

// ===========================================================================
//  备份恢复对话框控件 IDs
// ===========================================================================
#define IDC_RESTORE_DLG_TITLE    301
#define IDC_RESTORE_DLG_YES      302
#define IDC_RESTORE_DLG_NO       303
#define IDC_RESTORE_DLG_PROMPT   304
#define IDC_RESTORE_PHASE2_DEL   305
#define IDC_RESTORE_PHASE2_KEEP  306

// ===========================================================================
//  全局变量
// ===========================================================================
static HWND g_hEditMfr   = nullptr;
static HWND g_hEditModel = nullptr;
static HWND g_hEditPhone = nullptr;
static HWND g_hEditUrl   = nullptr;
static HWND g_hEditHours = nullptr;

// 修改追踪：记录注册表原始值，用于判断是否有未保存的修改
static std::wstring g_origMfr;
static std::wstring g_origModel;
static std::wstring g_origPhone;
static std::wstring g_origUrl;
static std::wstring g_origHours;

// 备份恢复标记
static bool g_bRestoredAndExit = false;

// ===========================================================================
//  OemInfo 结构体
// ===========================================================================
struct OemInfo {
    std::wstring manufacturer;
    std::wstring model;
    std::wstring phone;
    std::wstring url;
    std::wstring hours;
};

// ===========================================================================
//  辅助函数
// ===========================================================================

// 从注册表读取字符串值
static std::wstring RegReadString(HKEY hKey, const wchar_t* name)
{
    wchar_t buf[512] = {};
    DWORD sz = sizeof(buf), type = REG_SZ;
    if (RegQueryValueExW(hKey, name, nullptr, &type, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
        return buf;
    return L"";
}

// 从注册表加载 OEM 信息
static OemInfo LoadOemInfo()
{
    OemInfo info;
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        info.manufacturer = RegReadString(hKey, L"Manufacturer");
        info.model        = RegReadString(hKey, L"Model");
        info.phone        = RegReadString(hKey, L"SupportPhone");
        info.url          = RegReadString(hKey, L"SupportURL");
        info.hours        = RegReadString(hKey, L"SupportHours");
        RegCloseKey(hKey);
    }
    return info;
}

// 将 OEM 信息保存到注册表
static bool SaveOemInfo(const OemInfo& info)
{
    HKEY hKey = nullptr;
    LONG res = RegCreateKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, nullptr,
                               REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (res != ERROR_SUCCESS) return false;
    auto writeVal = [&](const wchar_t* name, const std::wstring& val) {
        RegSetValueExW(hKey, name, 0, REG_SZ,
                       (const BYTE*)val.c_str(),
                       (DWORD)((val.size() + 1) * sizeof(wchar_t)));
    };
    writeVal(L"Manufacturer", info.manufacturer);
    writeVal(L"Model",        info.model);
    writeVal(L"SupportPhone", info.phone);
    writeVal(L"SupportURL",   info.url);
    writeVal(L"SupportHours", info.hours);
    RegCloseKey(hKey);
    return true;
}

// 获取 Edit 控件文本
static std::wstring GetEditText(HWND hEdit)
{
    int len = GetWindowTextLengthW(hEdit);
    if (len <= 0) return L"";
    std::wstring s(len + 1, L'\0');
    GetWindowTextW(hEdit, &s[0], len + 1);
    s.resize(len);
    return s;
}

// 获取程序所在目录
static std::wstring GetAppDir(HINSTANCE hInst)
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(hInst, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return path;
}

// 从输入框收集当前 OEM 信息
static OemInfo GetCurrentInfo()
{
    OemInfo info;
    info.manufacturer = GetEditText(g_hEditMfr);
    info.model        = GetEditText(g_hEditModel);
    info.phone        = GetEditText(g_hEditPhone);
    info.url          = GetEditText(g_hEditUrl);
    info.hours        = GetEditText(g_hEditHours);
    return info;
}

// 判断当前输入框内容与原始值是否有差异（未保存的修改）
static bool IsModified()
{
    return (GetEditText(g_hEditMfr)   != g_origMfr   ||
            GetEditText(g_hEditModel) != g_origModel ||
            GetEditText(g_hEditPhone) != g_origPhone ||
            GetEditText(g_hEditUrl)   != g_origUrl   ||
            GetEditText(g_hEditHours) != g_origHours);
}

// 更新原始值快照（保存成功后调用，重置修改状态）
static void UpdateOrigSnapshot()
{
    g_origMfr   = GetEditText(g_hEditMfr);
    g_origModel = GetEditText(g_hEditModel);
    g_origPhone = GetEditText(g_hEditPhone);
    g_origUrl   = GetEditText(g_hEditUrl);
    g_origHours = GetEditText(g_hEditHours);
}

// 执行保存操作，返回是否成功
static bool DoSave(HWND hWnd)
{
    OemInfo info = GetCurrentInfo();
    if (SaveOemInfo(info)) {
        UpdateOrigSnapshot();
        return true;
    }
    MessageBoxW(hWnd,
        L"保存 OEM 信息失败，请确认程序以管理员身份运行。",
        L"错误", MB_OK | MB_ICONERROR);
    return false;
}

// ===========================================================================
//  INI 导出/导入
// ===========================================================================

// 导出 OEM 信息到 .oeminfo 文件
static bool ExportOemInfoToFile(const OemInfo& info, const std::wstring& filepath)
{
    WritePrivateProfileStringW(INI_SECTION, INI_KEY_MFR,   info.manufacturer.c_str(), filepath.c_str());
    WritePrivateProfileStringW(INI_SECTION, INI_KEY_MODEL,  info.model.c_str(),        filepath.c_str());
    WritePrivateProfileStringW(INI_SECTION, INI_KEY_PHONE,  info.phone.c_str(),        filepath.c_str());
    WritePrivateProfileStringW(INI_SECTION, INI_KEY_URL,    info.url.c_str(),          filepath.c_str());
    WritePrivateProfileStringW(INI_SECTION, INI_KEY_HOURS,  info.hours.c_str(),        filepath.c_str());
    DWORD attr = GetFileAttributesW(filepath.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
}

// ExportOemInfo: convenience wrapper - export to app directory with given filename
static bool ExportOemInfo(const OemInfo& info, const std::wstring& filename)
{
    wchar_t dir[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, dir, MAX_PATH);
    PathRemoveFileSpecW(dir);
    std::wstring filepath = std::wstring(dir) + L"\\" + filename;
    return ExportOemInfoToFile(info, filepath);
}

// 从 INI 文件导入 OEM 信息
static bool ImportOemInfo(const std::wstring& filepath, OemInfo& info)
{
    wchar_t buf[1024];
    GetPrivateProfileStringW(INI_SECTION, INI_KEY_MFR,   L"", buf, _countof(buf), filepath.c_str());
    info.manufacturer = buf;
    GetPrivateProfileStringW(INI_SECTION, INI_KEY_MODEL,  L"", buf, _countof(buf), filepath.c_str());
    info.model = buf;
    GetPrivateProfileStringW(INI_SECTION, INI_KEY_PHONE,  L"", buf, _countof(buf), filepath.c_str());
    info.phone = buf;
    GetPrivateProfileStringW(INI_SECTION, INI_KEY_URL,    L"", buf, _countof(buf), filepath.c_str());
    info.url = buf;
    GetPrivateProfileStringW(INI_SECTION, INI_KEY_HOURS,  L"", buf, _countof(buf), filepath.c_str());
    info.hours = buf;
    return !info.manufacturer.empty();
}

// 检测程序目录下是否存在备份文件
static std::wstring CheckBackupFile(const std::wstring& dir)
{
    std::wstring bakPath = dir + L"\\" + BACKUP_FILENAME;
    DWORD attr = GetFileAttributesW(bakPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return bakPath;
    return L"";
}

// ===========================================================================
//  备份恢复对话框 — 独立窗口
// ===========================================================================
// Phase constants for restore dialog
#define RESTORE_PHASE_ASK    0
#define RESTORE_PHASE_OK     1
#define RESTORE_PHASE_FAIL   2

static bool ShowRestoreDialog(HINSTANCE hInstance, const std::wstring& backupPath)
{
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        switch (msg) {
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(255, 255, 255));
            SetTextColor(hdc, RGB(40, 40, 60));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, CreateSolidBrush(RGB(255, 255, 255)));
            return 1;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            LONG_PTR phase = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
            std::wstring* pBakPath = (std::wstring*)GetPropW(hWnd, L"OEMBakPath");

            if (phase == RESTORE_PHASE_ASK) {
                if (id == IDC_RESTORE_DLG_YES) {
                    OemInfo info;
                    if (ImportOemInfo(*pBakPath, info) && SaveOemInfo(info)) {
                        g_bRestoredAndExit = true;
                        SetWindowTextW(hWnd, L"恢复成功");
                        SetWindowTextW(GetDlgItem(hWnd, IDC_RESTORE_DLG_TITLE), L"\u2705 OEM 备份已恢复");
                        SetWindowTextW(GetDlgItem(hWnd, IDC_RESTORE_DLG_PROMPT), L"是否删除备份文件？\n\n删除后无法恢复此备份。");
                        SetWindowTextW(GetDlgItem(hWnd, IDC_RESTORE_DLG_YES), L"删除备份");
                        SetWindowTextW(GetDlgItem(hWnd, IDC_RESTORE_DLG_NO),  L"保留备份");
                        SetWindowLongPtrW(hWnd, GWLP_USERDATA, RESTORE_PHASE_OK);
                    } else {
                        SetWindowTextW(hWnd, L"恢复失败");
                        SetWindowTextW(GetDlgItem(hWnd, IDC_RESTORE_DLG_TITLE), L"\u274C 恢复失败");
                        SetWindowTextW(GetDlgItem(hWnd, IDC_RESTORE_DLG_PROMPT), L"请确认备份文件有效且程序具有管理员权限。");
                        SetWindowTextW(GetDlgItem(hWnd, IDC_RESTORE_DLG_YES), L"关闭");
                        ShowWindow(GetDlgItem(hWnd, IDC_RESTORE_DLG_NO), SW_HIDE);
                        SetWindowLongPtrW(hWnd, GWLP_USERDATA, RESTORE_PHASE_FAIL);
                    }
                } else if (id == IDC_RESTORE_DLG_NO) {
                    DestroyWindow(hWnd);
                }
            } else if (phase == RESTORE_PHASE_OK) {
                if (id == IDC_RESTORE_DLG_YES) {
                    DeleteFileW(pBakPath->c_str());
                }
                DestroyWindow(hWnd);
            } else if (phase == RESTORE_PHASE_FAIL) {
                if (id == IDC_RESTORE_DLG_YES) {
                    DestroyWindow(hWnd);
                }
            }
            return 0;
        }
        case WM_DESTROY: {
            std::wstring* pBakPath = (std::wstring*)RemovePropW(hWnd, L"OEMBakPath");
            if (pBakPath) delete pBakPath;
            PostQuitMessage(0);
            return 0;
        }
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    };
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"OEMRestoreDlgClass";

    HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hIcon   = hIcon ? hIcon : LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassExW(&wc)) return false;

    // 居中
    RECT rc = { 0, 0, 360, 160 };
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
    int ww = rc.right - rc.left;
    int wh = rc.bottom - rc.top;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;

    HWND hDlg = CreateWindowExW(0,
        L"OEMRestoreDlgClass",
        L"检测到 OEM 备份",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        sx, sy, ww, wh,
        nullptr, nullptr, hInstance, nullptr);
    if (!hDlg) return false;

    if (hIcon) {
        SendMessageW(hDlg, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    // 存储备份路径
    std::wstring* pBakPath = new std::wstring(backupPath);
    SetPropW(hDlg, L"OEMBakPath", pBakPath);
    SetWindowLongPtrW(hDlg, GWLP_USERDATA, RESTORE_PHASE_ASK);

    // 标题
    HWND hTitle = CreateWindowExW(0, L"STATIC", L"\u26A0 检测到 OEM 配置备份",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 12, 360, 28, hDlg, (HMENU)IDC_RESTORE_DLG_TITLE, hInstance, nullptr);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

    // 提示
    HWND hPrompt = CreateWindowExW(0, L"STATIC",
        L"发现 oeminfo.oembak 备份文件，\n是否立即导入恢复 OEM 信息？",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 50, 360, 44, hDlg, (HMENU)IDC_RESTORE_DLG_PROMPT, hInstance, nullptr);
    SendMessageW(hPrompt, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    // 按钮
    HWND hBtnYes = CreateWindowExW(0, L"BUTTON", L"立即导入",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        50, 108, 120, 34, hDlg, (HMENU)IDC_RESTORE_DLG_YES, hInstance, nullptr);
    SendMessageW(hBtnYes, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hBtnNo = CreateWindowExW(0, L"BUTTON", L"稍后再说",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        190, 108, 120, 34, hDlg, (HMENU)IDC_RESTORE_DLG_NO, hInstance, nullptr);
    SendMessageW(hBtnNo, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return g_bRestoredAndExit;
}

// ===========================================================================
//  加载自定义字体
// ===========================================================================
static void LoadCustomFont(HINSTANCE hInst)
{
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCE(IDR_OEMFONT), RT_FONT);
    if (!hRes) return;

    HGLOBAL hMem = LoadResource(hInst, hRes);
    if (!hMem) return;

    const void* pData = LockResource(hMem);
    DWORD dwSize = SizeofResource(hInst, hRes);
    if (!pData || dwSize == 0) return;

    wchar_t tmpPath[MAX_PATH] = {};
    if (!GetTempPathW(MAX_PATH, tmpPath)) {
        GetModuleFileNameW(nullptr, tmpPath, MAX_PATH);
        PathRemoveFileSpecW(tmpPath);
    }
    wcscat_s(tmpPath, L"\\~oemfont.tmp");

    HANDLE hFile = CreateFileW(tmpPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD written = 0;
    WriteFile(hFile, pData, dwSize, &written, nullptr);
    CloseHandle(hFile);

    if (written < dwSize) return;

    if (AddFontResourceExW(tmpPath, FR_PRIVATE, nullptr)) {
        wcscpy_s(g_szFontName, L"DingTalk JinBuTi");
        g_bCustomFont = true;
    }
}

// 创建字体
static void CreateFonts()
{
    g_hFontTitle = CreateFontW(
        20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        g_szFontName);

    g_hFontNormal = CreateFontW(
        15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        g_szFontName);

    g_hFontFooter = CreateFontW(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        g_szFontName);
}

// ===========================================================================
//  UAC
// ===========================================================================
static bool IsRunAsAdmin()
{
    BOOL bAdmin = FALSE;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION te = {};
        DWORD dwSize = sizeof(te);
        if (GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &dwSize))
            bAdmin = te.TokenIsElevated;
        CloseHandle(hToken);
    }
    return bAdmin != FALSE;
}

static void RelaunchAsAdmin()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = path;
    sei.nShow  = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

// ===========================================================================
//  主窗口过程
//
//  布局（类似系统属性对话框）：
//  ┌──────────────────────────────────────────────────────────┐
//  │                    OEM 信息编辑器                         │
//  ├──────────────────────────────────────────────────────────┤
//  │                                                          │
//  │  制造商 (M):    [__________________________________]     │
//  │  型号 (O):      [__________________________________]     │
//  │  支持电话 (P):  [__________________________________]     │
//  │  支持网址 (U):  [__________________________________]     │
//  │  服务时间 (H):  [__________________________________]     │
//  │                                                          │
//  │  [导入...]  [导出...]                                    │
//  │                              [取消]  [应用]  [确定]       │
//  │                                                          │
//  │  HYCX Studio. 版权所有                                   │
//  └──────────────────────────────────────────────────────────┘
// ===========================================================================
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        // 创建画刷
        g_hBrushBg        = CreateSolidBrush(CLR_BG);
        g_hBrushBtnApply   = CreateSolidBrush(CLR_BTN_APPLY);
        g_hBrushBtnOK      = CreateSolidBrush(CLR_BTN_OK);
        g_hBrushBtnCancel  = CreateSolidBrush(CLR_BTN_CANCEL);
        g_hBrushBtnExport  = CreateSolidBrush(CLR_BTN_EXPORT);
        g_hBrushBtnImport  = CreateSolidBrush(CLR_BTN_IMPORT);
        g_hBrushBtnHelp    = CreateSolidBrush(CLR_BTN_HELP);

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

        // ---- 窗口尺寸 ----
        // 客户区 480 x 310
        int W = 480, H = 310;

        // ---- 标题 ----
        HWND hTitle = CreateWindowExW(0, L"STATIC", L"OEM 信息编辑器",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            12, 8, W - 24, 28, hWnd, (HMENU)IDC_LABEL_TITLE, hInst, nullptr);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

        // ---- 分隔线（用 STATIC 模拟一条灰线）----
        HWND hSep = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            12, 38, W - 24, 1, hWnd, nullptr, hInst, nullptr);

        // ---- 五行表单（类似系统属性对话框布局）----
        // 标签左对齐 + 快捷键提示，输入框紧跟其后
        struct Row {
            int id_lbl; int id_edit;
            const wchar_t* text;   // 带快捷键的标签，如 "制造商 (&M):"
            HWND* pEdit;
        };
        Row rows[] = {
            { IDC_LABEL_MFR,   IDC_EDIT_MFR,   L"制造商 (&M):",   &g_hEditMfr   },
            { IDC_LABEL_MODEL, IDC_EDIT_MODEL, L"型号 (&O):",      &g_hEditModel },
            { IDC_LABEL_PHONE, IDC_EDIT_PHONE, L"支持电话 (&P):",  &g_hEditPhone },
            { IDC_LABEL_URL,   IDC_EDIT_URL,   L"支持网址 (&U):",  &g_hEditUrl   },
            { IDC_LABEL_HOURS, IDC_EDIT_HOURS, L"服务时间 (&H):",  &g_hEditHours },
        };

        int startX = 16;
        int labelW = 82;
        int editX  = startX + labelW + 4;
        int editW  = W - editX - 16;
        int startY = 50;
        int rowH   = 30;

        for (int i = 0; i < 5; i++) {
            int y = startY + i * rowH;

            // 标签：左对齐，下方对齐输入框
            HWND hLbl = CreateWindowExW(0, L"STATIC", rows[i].text,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                startX, y + 4, labelW, 20,
                hWnd, (HMENU)(UINT_PTR)rows[i].id_lbl, hInst, nullptr);
            SendMessageW(hLbl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            // 输入框
            HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                editX, y, editW, 24,
                hWnd, (HMENU)(UINT_PTR)rows[i].id_edit, hInst, nullptr);
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            *rows[i].pEdit = hEdit;
        }

        // ---- 按钮区域 ----
        int btnH = 26;
        int bottomAreaY = startY + 5 * rowH + 12;  // 212

        // 第一行：导入、导出（左对齐）
        HWND hBtnImport = CreateWindowExW(0, L"BUTTON", L"导入...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            16, bottomAreaY, 75, btnH,
            hWnd, (HMENU)IDC_BTN_IMPORT, hInst, nullptr);
        SendMessageW(hBtnImport, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        HWND hBtnExport = CreateWindowExW(0, L"BUTTON", L"导出...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            97, bottomAreaY, 75, btnH,
            hWnd, (HMENU)IDC_BTN_EXPORT, hInst, nullptr);
        SendMessageW(hBtnExport, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // 第二行：取消、应用、确定（右对齐）
        int btnRow2Y = bottomAreaY + 34;
        int btnW2 = 75;
        int btnGap2 = 6;
        int rightX = W - 16;

        HWND hBtnOK = CreateWindowExW(0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            rightX - btnW2, btnRow2Y, btnW2, btnH,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_OK, hInst, nullptr);
        SendMessageW(hBtnOK, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        HWND hBtnApply = CreateWindowExW(0, L"BUTTON", L"应用",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            rightX - btnW2 * 2 - btnGap2, btnRow2Y, btnW2, btnH,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_APPLY, hInst, nullptr);
        SendMessageW(hBtnApply, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            rightX - btnW2 * 3 - btnGap2 * 2, btnRow2Y, btnW2, btnH,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_CANCEL, hInst, nullptr);
        SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // ---- 帮助按钮（右上角）----
        HWND hBtnHelp = CreateWindowExW(0, L"BUTTON", L"?",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            W - 36, 4, 24, 24,
            hWnd, (HMENU)(INT_PTR)IDC_BTN_HELP, hInst, nullptr);
        SendMessageW(hBtnHelp, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // ---- 页脚 ----
        HWND hFooter = CreateWindowExW(0, L"STATIC",
            L"HYCX Studio. ALL RIGHTS WASTED \u00A9 HYCX Studio",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            16, H - 22, W - 32, 18, hWnd, (HMENU)IDC_LABEL_FOOTER, hInst, nullptr);
        SendMessageW(hFooter, WM_SETFONT, (WPARAM)g_hFontFooter, TRUE);

        // ---- 加载注册表 OEM 信息 ----
        OemInfo info = LoadOemInfo();
        SetWindowTextW(g_hEditMfr,   info.manufacturer.c_str());
        SetWindowTextW(g_hEditModel, info.model.c_str());
        SetWindowTextW(g_hEditPhone, info.phone.c_str());
        SetWindowTextW(g_hEditUrl,   info.url.c_str());
        SetWindowTextW(g_hEditHours, info.hours.c_str());

        // ---- 保存原始值快照（用于修改检测）----
        UpdateOrigSnapshot();
        return 0;
    }

    // ---- 静态文本颜色 ----
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, CLR_BG);
        int id = GetDlgCtrlID((HWND)lParam);
        if (id == IDC_LABEL_TITLE)
            SetTextColor(hdc, CLR_TITLE);
        else if (id == IDC_LABEL_FOOTER)
            SetTextColor(hdc, CLR_FOOTER);
        else
            SetTextColor(hdc, RGB(50, 50, 50));
        return (LRESULT)g_hBrushBg;
    }

    // ---- 编辑框 ----
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(255, 255, 255));
        SetTextColor(hdc, RGB(30, 30, 30));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }

    // ---- 按钮颜色 ----
    case WM_CTLCOLORBTN:
    {
        int id = GetDlgCtrlID((HWND)lParam);
        if (id == IDC_BTN_OK)        return (LRESULT)g_hBrushBtnOK;
        if (id == IDC_BTN_APPLY)     return (LRESULT)g_hBrushBtnApply;
        if (id == IDC_BTN_CANCEL)    return (LRESULT)g_hBrushBtnCancel;
        if (id == IDC_BTN_EXPORT)    return (LRESULT)g_hBrushBtnExport;
        if (id == IDC_BTN_IMPORT)    return (LRESULT)g_hBrushBtnImport;
        if (id == IDC_BTN_HELP)     return (LRESULT)g_hBrushBtnHelp;
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    // ---- 窗口背景 ----
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_hBrushBg);
        return 1;
    }

    // ---- 关闭窗口前检测未保存修改 ----
    case WM_CLOSE:
    {
        if (IsModified()) {
            int ret = MessageBoxW(hWnd,
                L"OEM 信息已修改但尚未保存。\n\n是否在关闭前保存更改？",
                L"OEM 信息编辑器",
                MB_YESNOCANCEL | MB_ICONWARNING);
            if (ret == IDYES) {
                // 保存 → 关闭
                if (DoSave(hWnd)) {
                    DestroyWindow(hWnd);
                }
                // 保存失败则不关闭
                return 0;
            } else if (ret == IDCANCEL) {
                // 取消关闭
                return 0;
            }
            // IDNO → 不保存，直接关闭（继续 WM_DESTROY）
        }
        DestroyWindow(hWnd);
        return 0;
    }

    // ---- 按钮事件 ----
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

        if (id == IDC_BTN_OK) {
            // ---- 确定：保存并退出 ----
            if (DoSave(hWnd)) {
                DestroyWindow(hWnd);
            }
        }
        else if (id == IDC_BTN_APPLY) {
            // ---- 应用：保存但不退出 ----
            if (DoSave(hWnd)) {
                MessageBoxW(hWnd,
                    L"OEM 信息已保存！\n请重启设置应用查看更改。",
                    L"成功", MB_OK | MB_ICONINFORMATION);
            }
        }
        else if (id == IDC_BTN_HELP) {
            // ---- 帮助：弹窗显示命令行调用方法 ----
            {
                wchar_t helpText[1024] = {};
                swprintf(helpText,
                    L"%ls 命令行用法:\n\n"
                    L"%ls restore [配置文件]\n"
                    L"  从指定文件恢复 OEM 信息\n"
                    L"  省略路径则使用当前目录 oeminfo.oembak\n\n"
                    L"%ls backup <输出文件>\n"
                    L"  导出当前 OEM 信息到指定文件\n\n"
                    L"%ls edit Key=Value [Key=Value ...]\n"
                    L"  直接修改注册表指定键\n"
                    L"  可用键: Manufacturer, Model,\n"
                    L"  SupportPhone, SupportURL, SupportHours\n\n"
                    L"  示例: %ls edit Manufacturer=ACME Model=X1\n\n"
                    L"%ls help\n"
                    L"  显示本帮助信息",
                    g_szExeName, g_szExeName, g_szExeName, g_szExeName, g_szExeName, g_szExeName);
                MessageBoxW(hWnd, helpText, L"命令行用法",
                    MB_OK | MB_ICONINFORMATION);
            }
        }
        else if (id == IDC_BTN_CANCEL) {
            // ---- 取消：若有未保存修改则询问，否则直接退出 ----
            if (IsModified()) {
                int ret = MessageBoxW(hWnd,
                    L"OEM 信息已修改但尚未保存。\n\n是否放弃修改并退出？",
                    L"OEM 信息编辑器",
                    MB_YESNO | MB_ICONWARNING);
                if (ret == IDYES) {
                    DestroyWindow(hWnd);
                }
            } else {
                DestroyWindow(hWnd);
            }
        }
        else if (id == IDC_BTN_EXPORT) {
            // ---- 导出配置（另存为对话框，两个过滤器）----
            OemInfo info = GetCurrentInfo();
            std::wstring dir = GetAppDir(hInst);

            // 构造默认文件名（不含扩展名）
            std::wstring defName = dir + L"\\oeminfo";

            wchar_t szPath[MAX_PATH] = {};
            wcscpy_s(szPath, defName.c_str());

            OPENFILENAMEW ofn = {};
            ofn.lStructSize   = sizeof(ofn);
            ofn.hwndOwner     = hWnd;
            // 两个过滤器：
            //   1. OEM 配置信息 (*.oeminfo)
            //   2. 自动化配置信息 (*.oembak)
            ofn.lpstrFilter   = L"OEM 配置信息 (*.oeminfo)\0*.oeminfo\0自动化配置信息 (*.oembak)\0*.oembak\0";
            ofn.nFilterIndex  = 1;  // 默认选中第一个过滤器
            ofn.lpstrFile     = szPath;
            ofn.nMaxFile      = MAX_PATH;
            ofn.lpstrInitialDir = dir.c_str();
            ofn.lpstrDefExt   = L"oeminfo";  // 用户未输入扩展名时的默认后缀
            ofn.Flags         = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            ofn.lpstrTitle    = L"导出 OEM 配置";

            if (GetSaveFileNameW(&ofn)) {
                if (ExportOemInfoToFile(info, szPath)) {
                    MessageBoxW(hWnd,
                        L"OEM 配置已导出。",
                        L"导出成功", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hWnd,
                        L"导出失败，请确认目标路径可写。",
                        L"导出失败", MB_OK | MB_ICONERROR);
                }
            }
        }
        else if (id == IDC_BTN_IMPORT) {
            // ---- 导入配置（文件选择对话框）----
            wchar_t szPath[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hWnd;
            ofn.lpstrFilter = L"OEM 配置文件\0*.oeminfo;*.oembak\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFile    = szPath;
            ofn.nMaxFile     = MAX_PATH;
            ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            ofn.lpstrTitle   = L"选择 OEM 配置文件";
            std::wstring dir = GetAppDir(hInst);
            ofn.lpstrInitialDir = dir.c_str();

            if (GetOpenFileNameW(&ofn)) {
                OemInfo info;
                if (ImportOemInfo(szPath, info)) {
                    SetWindowTextW(g_hEditMfr,   info.manufacturer.c_str());
                    SetWindowTextW(g_hEditModel, info.model.c_str());
                    SetWindowTextW(g_hEditPhone, info.phone.c_str());
                    SetWindowTextW(g_hEditUrl,   info.url.c_str());
                    SetWindowTextW(g_hEditHours, info.hours.c_str());
                    MessageBoxW(hWnd,
                        L"配置已导入到编辑框，请检查后点击应用或确定写入注册表。",
                        L"导入成功", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hWnd,
                        L"导入失败：文件格式无效或缺少必要字段。",
                        L"导入失败", MB_OK | MB_ICONERROR);
                }
            }
        }
        return 0;
    }

    // ---- 窗口销毁 ----
    case WM_DESTROY:
    {
        if (g_bCustomFont) {
            wchar_t tmpPath[MAX_PATH] = {};
            GetTempPathW(MAX_PATH, tmpPath);
            wcscat_s(tmpPath, L"\\~oemfont.tmp");
            RemoveFontResourceExW(tmpPath, FR_PRIVATE, nullptr);
            DeleteFileW(tmpPath);
        }
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ===========================================================================
//  WinMain — 程序入口
//
//  启动流程：
//  1. UAC 提权检测
//  2. 初始化公共控件
//  3. 加载自定义字体（钉钉进步体）
//  4. 检测 oeminfo.oembak → 弹恢复对话框
//  5. 创建主窗口（类似系统属性对话框布局）
//  6. 消息循环
// ===========================================================================
// Ensure console is attached for command line output
static HANDLE g_hConOut = INVALID_HANDLE_VALUE;

static void EnsureConsole()
{
    // Try to attach to parent console first (if launched from cmd)
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        // If no parent console, allocate a new one
        AllocConsole();
    }
    g_hConOut = GetStdHandle(STD_OUTPUT_HANDLE);
}

// Sync console cursor to end of written content
static void CmdSyncCursor() {
    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (GetConsoleScreenBufferInfo(g_hConOut, &csbi)) {
        COORD pos = { 0, (SHORT)(csbi.dwCursorPosition.Y + 1) };
        SetConsoleCursorPosition(g_hConOut, pos);
    }
}

static void CmdPrint(const wchar_t* text) {
    if (!text || !text[0]) return;
    DWORD written = 0;
    WriteConsoleW(g_hConOut, text, (DWORD)wcslen(text), &written, nullptr);
}

// Variadic helper: format and print
static void CmdPrintf(const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    wchar_t buf[2048];
    vswprintf(buf, 2048, fmt, args);
    va_end(args);
    CmdPrint(buf);
}

// Command line: edit specific registry keys
// Usage: <exe> edit Manufacturer=xxx Model=xxx ...
static int CmdEdit(int argc, wchar_t* argv[])
{
    if (argc < 1) {
        CmdPrintf(L"Usage: %ls edit Key=Value [Key=Value ...]\n", g_szExeName);
        CmdPrint(L"  Valid keys: Manufacturer, Model, SupportPhone, SupportURL, SupportHours\n");
        CmdSyncCursor();

        return 1;
    }

    HKEY hKey = nullptr;
    LONG res = RegCreateKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, nullptr,
                               REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (res != ERROR_SUCCESS) {
        CmdPrint(L"Error: Failed to open registry. Run as administrator.\n");
        CmdSyncCursor();

        return 2;
    }

    int changed = 0;
    for (int i = 0; i < argc; i++) {
        std::wstring arg = argv[i];
        size_t eq = arg.find(L'=');
        if (eq == std::wstring::npos || eq == 0) {
            CmdPrintf(L"Warning: Invalid format: %ls (expected Key=Value)\n", argv[i]);
            continue;
        }
        std::wstring key = arg.substr(0, eq);
        std::wstring val = arg.substr(eq + 1);

        LONG lr = RegSetValueExW(hKey, key.c_str(), 0, REG_SZ,
                                  (const BYTE*)val.c_str(),
                                  (DWORD)((val.size() + 1) * sizeof(wchar_t)));
        if (lr == ERROR_SUCCESS) {
            CmdPrintf(L"Set %ls = %ls\n", key.c_str(), val.c_str());
            changed++;
        } else {
            CmdPrintf(L"Error: Failed to set %ls (code %d)\n", key.c_str(), lr);
        }
    }

    RegCloseKey(hKey);
    if (changed > 0) {
        CmdPrintf(L"Done. %d key(s) updated. Restart Settings app to see changes.\n", changed);
    }
    CmdSyncCursor();

    return (changed > 0) ? 0 : 1;
}

// Resolve a file path: if relative (no drive/root), prepend exe directory
static std::wstring ResolvePath(const wchar_t* path)
{
    if (!path || !path[0]) return std::wstring();
    // If it has a drive letter or starts with \, it's absolute
    if (wcslen(path) >= 2 && path[1] == L':') return std::wstring(path);
    if (wcslen(path) >= 2 && path[0] == L'\\' && path[1] == L'\\') return std::wstring(path);
    if (path[0] == L'\\') return std::wstring(path);
    // Relative path - prepend exe directory
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    std::wstring result = exePath;
    result += path;
    return result;
}

// Command line: restore from backup file
static int CmdRestore(const wchar_t* filepath)
{
    if (!filepath || !filepath[0]) {
        CmdPrintf(L"Usage: %ls restore [config_file]\n", g_szExeName);
        CmdPrint(L"  Omit path to use default: oeminfo.oembak\n");
        CmdSyncCursor();

        return 1;
    }

    std::wstring fullPath = ResolvePath(filepath);
    DWORD attr = GetFileAttributesW(fullPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        CmdPrintf(L"Error: File not found: %ls\n", filepath);
        CmdSyncCursor();

        return 2;
    }

    OemInfo info;
    if (!ImportOemInfo(fullPath.c_str(), info)) {
        CmdPrintf(L"Error: Failed to import from: %ls\n", filepath);
        CmdPrint(L"Missing required Manufacturer field.\n");
        CmdSyncCursor();

        return 3;
    }

    if (!SaveOemInfo(info)) {
        CmdPrint(L"Error: Failed to write registry. Run as administrator.\n");
        CmdSyncCursor();

        return 4;
    }

    CmdPrintf(L"OEM info restored from: %ls\n", filepath);
    CmdPrintf(L"  Manufacturer: %ls\n", info.manufacturer.c_str());
    CmdPrintf(L"  Model:        %ls\n", info.model.c_str());
    CmdPrintf(L"  SupportPhone:  %ls\n", info.phone.c_str());
    CmdPrintf(L"  SupportURL:    %ls\n", info.url.c_str());
    CmdPrintf(L"  SupportHours:  %ls\n", info.hours.c_str());
    CmdPrint(L"Restart Settings app to see changes.\n");
    CmdSyncCursor();

    return 0;
}

// Command line: backup (export) to specified path
static int CmdBackup(const wchar_t* filepath)
{
    if (!filepath || !filepath[0]) {
        CmdPrintf(L"Usage: %ls backup <output_file>\n", g_szExeName);
        CmdSyncCursor();

        return 1;
    }


    OemInfo info = LoadOemInfo();

    if (!ExportOemInfoToFile(info, filepath)) {
        CmdPrintf(L"Error: Failed to export to: %ls\n", filepath);

        CmdSyncCursor();

        return 2;
    }


    CmdPrintf(L"OEM info exported to: %ls\n", filepath);
    CmdPrintf(L"  Manufacturer: %ls\n", info.manufacturer.c_str());
    CmdPrintf(L"  Model:        %ls\n", info.model.c_str());
    CmdPrintf(L"  SupportPhone:  %ls\n", info.phone.c_str());
    CmdPrintf(L"  SupportURL:    %ls\n", info.url.c_str());
    CmdPrintf(L"  SupportHours:  %ls\n", info.hours.c_str());
    CmdSyncCursor();

    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow)
{
    // ---- Command line argument parsing ----
    // Supports:
    //   restore <file>  - silently restore OEM info from config file
    //   backup <file>   - silently export current OEM info to file
    //   edit <K>=<V>    - silently set specific registry keys
    //   help            - show usage info
    // No arguments -> launch GUI
    
    // Initialize exe name first (needed by both CLI and GUI)
    InitExeName();

    // First check if we have command line arguments
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc >= 2) {
        EnsureConsole();
        std::wstring cmd = argv[1];
        if (_wcsicmp(cmd.c_str(), L"restore") == 0) {
            // restore with or without path; default to current dir oeminfo.oembak
            const wchar_t* restorePath = (argc >= 3) ? argv[2] : BACKUP_FILENAME;
            int ret = CmdRestore(restorePath);
            LocalFree(argv);
            CmdSyncCursor();

            return ret;
        }
        else if (_wcsicmp(cmd.c_str(), L"backup") == 0 && argc >= 3) {
            int ret = CmdBackup(argv[2]);
            LocalFree(argv);
            CmdSyncCursor();

            return ret;
        }
        else if (_wcsicmp(cmd.c_str(), L"edit") == 0 && argc >= 3) {
            int ret = CmdEdit(argc - 2, argv + 2);
            LocalFree(argv);
            CmdSyncCursor();

            return ret;
        }
        else if (_wcsicmp(cmd.c_str(), L"help") == 0 || _wcsicmp(cmd.c_str(), L"--help") == 0 || _wcsicmp(cmd.c_str(), L"-?") == 0) {
            LANGID langId = GetUserDefaultUILanguage();
            if (PRIMARYLANGID(langId) == LANG_CHINESE) {
                CmdPrintf(L"%ls \u547d\u4ee4\u884c\u7528\u6cd5 - HYCX Studio\n\n", g_szExeName);
                CmdPrintf(L"%ls restore [\u914d\u7f6e\u6587\u4ef6]\n", g_szExeName);
                CmdPrint(L"  \u4ece\u6307\u5b9a\u6587\u4ef6\u6062\u590d OEM \u4fe1\u606f\uff08\u7701\u7565\u8def\u5f84\u5219\u4f7f\u7528\u5f53\u524d\u76ee\u5f55 oeminfo.oembak\uff09\n\n");
                CmdPrintf(L"%ls backup <\u8f93\u51fa\u6587\u4ef6>\n", g_szExeName);
                CmdPrint(L"  \u5bfc\u51fa\u5f53\u524d OEM \u4fe1\u606f\u5230\u6307\u5b9a\u6587\u4ef6\n\n");
                CmdPrintf(L"%ls edit Key=Value [Key=Value ...]\n", g_szExeName);
                CmdPrint(L"  \u4fee\u6539\u6ce8\u518c\u8868\u6307\u5b9a\u952e\u503c\n");
                CmdPrint(L"  \u53ef\u7528\u952e: Manufacturer, Model,\n");
                CmdPrint(L"         SupportPhone, SupportURL, SupportHours\n\n");
                CmdPrint(L"  \u793a\u4f8b:\n");
                CmdPrintf(L"    %ls edit Manufacturer=ACME Model=X1\n", g_szExeName);
                CmdPrintf(L"    %ls edit Manufacturer=HP Model=EliteBook SupportPhone=800-123-4567\n\n", g_szExeName);
                CmdPrintf(L"%ls\n", g_szExeName);
                CmdPrint(L"  \u542f\u52a8 GUI \u7f16\u8f91\u5668\n\n");
                CmdPrint(L"\u9700\u8981\u7ba1\u7406\u5458\u6743\u9650\u3002\n");
            } else {
                CmdPrintf(L"%ls Command Line Usage - HYCX Studio\n\n", g_szExeName);
                CmdPrintf(L"%ls restore [config_file]\n", g_szExeName);
                CmdPrint(L"  Restore OEM info (default: oeminfo.oembak in current dir)\n\n");
                CmdPrintf(L"%ls backup <output_file>\n", g_szExeName);
                CmdPrint(L"  Export current OEM info to file\n\n");
                CmdPrintf(L"%ls edit Key=Value [Key=Value ...]\n", g_szExeName);
                CmdPrint(L"  Modify registry keys directly\n");
                CmdPrint(L"  Valid keys: Manufacturer, Model,\n");
                CmdPrint(L"             SupportPhone, SupportURL, SupportHours\n\n");
                CmdPrint(L"  Examples:\n");
                CmdPrintf(L"    %ls edit Manufacturer=ACME Model=X1\n", g_szExeName);
                CmdPrintf(L"    %ls edit Manufacturer=HP Model=EliteBook SupportPhone=800-123-4567\n\n", g_szExeName);
                CmdPrintf(L"%ls\n", g_szExeName);
                CmdPrint(L"  Launch GUI editor\n\n");
                CmdPrint(L"Requires administrator privileges.\n");
            }
            LocalFree(argv);
            CmdSyncCursor();

            return 0;
        }
        else {
            LANGID langId = GetUserDefaultUILanguage();
            if (PRIMARYLANGID(langId) == LANG_CHINESE) {
                CmdPrintf(L"\u672a\u8bc6\u522b\u7684\u547d\u4ee4: %ls\n", cmd.c_str());
                CmdPrintf(L"\u4f7f\u7528 %ls help \u67e5\u770b\u5e2e\u52a9\n", g_szExeName);
            } else {
                CmdPrintf(L"Unknown command: %ls\n", cmd.c_str());
                CmdPrintf(L"Use %ls help for usage information\n", g_szExeName);
            }
            LocalFree(argv);
            CmdSyncCursor();

            return 1;
        }
    }
    if (argv) LocalFree(argv);

    if (!IsRunAsAdmin()) {
        RelaunchAsAdmin();
        CmdSyncCursor();

        return 0;
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    LoadCustomFont(hInstance);
    CreateFonts();

    // 检测备份
    std::wstring appDir = GetAppDir(hInstance);
    std::wstring backupPath = CheckBackupFile(appDir);
    if (!backupPath.empty()) {
        bool restored = ShowRestoreDialog(hInstance, backupPath);
        if (restored) return 0;
    }

    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"OEMEditorClass";

    HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hIcon   = hIcon ? hIcon : LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;

    RegisterClassExW(&wc);

    // 创建窗口（居中）
    RECT rc = { 0, 0, 480, 310 };
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int ww = rc.right - rc.left;
    int wh = rc.bottom - rc.top;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;

    HWND hWnd = CreateWindowExW(0,
        L"OEMEditorClass",
        L"OEM 信息编辑器 - HYCX Studio",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        sx, sy, ww, wh,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return 1;

    if (hIcon) {
        SendMessageW(hWnd, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
        SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 清理
    if (g_hFontTitle)  DeleteObject(g_hFontTitle);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontFooter) DeleteObject(g_hFontFooter);
    if (g_hBrushBg)        DeleteObject(g_hBrushBg);
    if (g_hBrushBtnOK)      DeleteObject(g_hBrushBtnOK);
    if (g_hBrushBtnApply)   DeleteObject(g_hBrushBtnApply);
    if (g_hBrushBtnCancel)  DeleteObject(g_hBrushBtnCancel);
    if (g_hBrushBtnExport)  DeleteObject(g_hBrushBtnExport);
    if (g_hBrushBtnImport)  DeleteObject(g_hBrushBtnImport);
    if (g_hBrushBtnHelp)    DeleteObject(g_hBrushBtnHelp);

    return (int)msg.wParam;
}
