#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "csdecomp/decompile_service.hpp"

#include <cstdio>
#include <exception>
#include <memory>
#include <new>
#include <string>

namespace csdecomp {

namespace {

constexpr UINT WM_DECOMPILE_DONE = WM_APP + 1;
constexpr UINT WM_DECOMPILE_ERROR = WM_APP + 2;

constexpr int kInputEditId = 1001;
constexpr int kInputBrowseId = 1002;
constexpr int kOutputEditId = 1003;
constexpr int kOutputBrowseId = 1004;
constexpr int kIlCommentsId = 1005;
constexpr int kDecompileId = 1006;
constexpr int kStatusEditId = 1007;

constexpr int kMargin = 16;
constexpr int kButtonWidth = 110;
constexpr int kRowHeight = 24;
constexpr int kClientWidth = 520;
constexpr int kClientHeight = 360;

constexpr SIZE_T kWorkerStackSize = 16 * 1024 * 1024;

HWND g_window = nullptr;
HWND g_input_edit = nullptr;
HWND g_output_edit = nullptr;
HWND g_il_comments = nullptr;
HWND g_status_edit = nullptr;
HFONT g_ui_font = nullptr;
HANDLE g_worker_thread = nullptr;
DWORD g_worker_thread_id = 0;
volatile bool g_decompile_worker_active = false;

struct DecompileWorkerPayload {
    DecompileRequest request;
    std::wstring output_path;
};

std::wstring utf8_to_wide(const std::string& text);
void set_status(const std::wstring& text);
void set_ui_busy(bool busy);
void on_decompile_done(std::wstring* message);
void on_decompile_error(std::wstring* message);
void post_decompile_error(const std::wstring& text);
LONG WINAPI gui_unhandled_exception_filter(EXCEPTION_POINTERS* info);
PVOID g_vectored_handler = nullptr;

LONG CALLBACK gui_vectored_exception_handler(EXCEPTION_POINTERS* info) {
    if (!g_decompile_worker_active || info == nullptr || info->ExceptionRecord == nullptr) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    wchar_t buffer[512]{};
    std::swprintf(buffer, 512,
                  L"Critical error while decompiling.\n\n"
                  L"Exception code: 0x%08lX\n\n"
                  L"The assembly may be corrupted, obfuscated, or too complex.",
                  static_cast<unsigned long>(code));
    post_decompile_error(buffer);
    g_decompile_worker_active = false;
    ExitThread(1);
}

void post_decompile_error(const std::wstring& text) {
    auto* message = new std::wstring(text);
    if (g_window != nullptr) {
        PostMessageW(g_window, WM_DECOMPILE_ERROR, 0, reinterpret_cast<LPARAM>(message));
    } else {
        MessageBoxW(nullptr, message->c_str(), L"csdecomp", MB_ICONERROR | MB_OK);
        delete message;
    }
}

LONG WINAPI gui_unhandled_exception_filter(EXCEPTION_POINTERS* info) {
    wchar_t buffer[512]{};
    const unsigned long code =
        info != nullptr && info->ExceptionRecord != nullptr
            ? static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode)
            : 0UL;
    std::swprintf(buffer, 512,
                  L"csdecomp crashed.\n\nException code: 0x%08lX\n\n"
                  L"The assembly may be corrupted, obfuscated, or too large.",
                  code);
    MessageBoxW(g_window, buffer, L"csdecomp", MB_ICONERROR | MB_OK);
    return EXCEPTION_EXECUTE_HANDLER;
}

[[noreturn]] void gui_terminate_handler() {
    MessageBoxW(g_window,
                L"Unexpected termination during decompilation.\n\n"
                L"This often happens when the process runs out of memory.",
                L"csdecomp", MB_ICONERROR | MB_OK);
    std::abort();
}

void run_decompile_job(DecompileWorkerPayload* payload) {
    g_decompile_worker_active = true;
    try {
        decompile_to_file(payload->request);
        auto* message = new std::wstring(L"Saved to:\n" + payload->output_path);
        PostMessageW(g_window, WM_DECOMPILE_DONE, 0, reinterpret_cast<LPARAM>(message));
    } catch (const std::bad_alloc&) {
        post_decompile_error(
            L"Out of memory while decompiling.\n\n"
            L"Try a smaller assembly or close other programs.");
    } catch (const std::exception& ex) {
        post_decompile_error(utf8_to_wide(ex.what()));
    } catch (...) {
        post_decompile_error(L"Unknown error during decompilation.");
    }
    g_decompile_worker_active = false;
}

DWORD WINAPI decompile_worker_thread(LPVOID param) {
    auto* payload = static_cast<DecompileWorkerPayload*>(param);
    g_worker_thread_id = GetCurrentThreadId();
    run_decompile_job(payload);
    delete payload;
    return 0;
}

void set_ui_busy(bool busy) {
    const BOOL enable = busy ? FALSE : TRUE;
    EnableWindow(GetDlgItem(g_window, kDecompileId), enable);
    EnableWindow(GetDlgItem(g_window, kInputBrowseId), enable);
    EnableWindow(GetDlgItem(g_window, kOutputBrowseId), enable);
    EnableWindow(g_input_edit, enable);
    EnableWindow(g_output_edit, enable);
    EnableWindow(g_il_comments, enable);
}

void on_decompile_done(std::wstring* message) {
    set_ui_busy(false);
    if (g_worker_thread != nullptr) {
        CloseHandle(g_worker_thread);
        g_worker_thread = nullptr;
    }
    set_status(L"Done.");
    MessageBoxW(g_window, message->c_str(), L"csdecomp", MB_ICONINFORMATION | MB_OK);
    delete message;
}

void on_decompile_error(std::wstring* message) {
    set_ui_busy(false);
    if (g_worker_thread != nullptr) {
        CloseHandle(g_worker_thread);
        g_worker_thread = nullptr;
    }
    set_status(L"Error.");
    MessageBoxW(g_window, message->c_str(), L"csdecomp", MB_ICONERROR | MB_OK);
    delete message;
}

std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length =
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &wide[0], length);
    return wide;
}

std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &utf8[0], length,
                        nullptr, nullptr);
    return utf8;
}

std::wstring get_window_text(HWND control) {
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, &text[0], length + 1);
    text.resize(wcslen(text.c_str()));
    return text;
}

void set_window_text(HWND control, const std::wstring& text) { SetWindowTextW(control, text.c_str()); }

void set_status(const std::wstring& text) { set_window_text(g_status_edit, text); }

std::wstring suggested_output_path(const std::wstring& input_path) {
    if (input_path.empty()) {
        return L"output.decompiled.cs";
    }
    const size_t slash = input_path.find_last_of(L"\\/");
    const size_t dot = input_path.find_last_of(L'.');
    if (dot != std::wstring::npos &&
        (slash == std::wstring::npos || dot > slash)) {
        return input_path.substr(0, dot) + L".decompiled.cs";
    }
    return input_path + L".decompiled.cs";
}

void apply_font(HWND control) {
    if (g_ui_font != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui_font), TRUE);
    }
}

HWND create_label(HWND parent, const wchar_t* text, int x, int y, int width) {
    HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, width, 20, parent,
                                 nullptr, GetModuleHandleW(nullptr), nullptr);
    apply_font(label);
    return label;
}

HWND create_button(HWND parent, const wchar_t* text, int x, int y, int width, int height, int id) {
    HWND button = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y,
                                  width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                  GetModuleHandleW(nullptr), nullptr);
    apply_font(button);
    return button;
}

HWND create_edit(HWND parent, int x, int y, int width, int height, int id, bool multiline) {
    DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    if (multiline) {
        style = WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL;
    }
    HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", style, x, y, width, height, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                GetModuleHandleW(nullptr), nullptr);
    apply_font(edit);
    return edit;
}

bool browse_for_assembly(HWND owner, std::wstring& selected) {
    wchar_t buffer[MAX_PATH]{};
    if (!selected.empty() && selected.size() < MAX_PATH) {
        wcscpy_s(buffer, selected.c_str());
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L".NET assemblies (*.dll;*.exe)\0*.dll;*.exe\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = L"Select .NET assembly";

    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }
    selected = buffer;
    return true;
}

bool browse_for_output(HWND owner, std::wstring& selected) {
    wchar_t buffer[MAX_PATH]{};
    if (!selected.empty() && selected.size() < MAX_PATH) {
        wcscpy_s(buffer, selected.c_str());
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"C# source (*.cs)\0*.cs\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"cs";
    ofn.lpstrTitle = L"Save decompiled C# file";

    if (!GetSaveFileNameW(&ofn)) {
        return false;
    }
    selected = buffer;
    return true;
}

void on_browse_input() {
    std::wstring path = get_window_text(g_input_edit);
    if (browse_for_assembly(g_window, path)) {
        set_window_text(g_input_edit, path);
        if (get_window_text(g_output_edit).empty()) {
            set_window_text(g_output_edit, suggested_output_path(path));
        }
        set_status(L"Assembly selected.");
    }
}

void on_browse_output() {
    std::wstring path = get_window_text(g_output_edit);
    if (path.empty()) {
        path = suggested_output_path(get_window_text(g_input_edit));
    }
    if (browse_for_output(g_window, path)) {
        set_window_text(g_output_edit, path);
        set_status(L"Output file selected.");
    }
}

void on_decompile() {
    if (g_worker_thread != nullptr) {
        MessageBoxW(g_window, L"Decompilation is already in progress.", L"csdecomp", MB_ICONINFORMATION | MB_OK);
        return;
    }

    const std::wstring input_wide = get_window_text(g_input_edit);
    if (input_wide.empty()) {
        MessageBoxW(g_window, L"Select a .NET assembly (.dll or .exe).", L"csdecomp",
                    MB_ICONWARNING | MB_OK);
        return;
    }

    std::wstring output_wide = get_window_text(g_output_edit);
    if (output_wide.empty()) {
        output_wide = suggested_output_path(input_wide);
        set_window_text(g_output_edit, output_wide);
    }

    auto* payload = new DecompileWorkerPayload{};
    payload->request.input_path = wide_to_utf8(input_wide);
    payload->request.output_path = wide_to_utf8(output_wide);
    payload->request.options.include_il_comments =
        SendMessageW(g_il_comments, BM_GETCHECK, 0, 0) == BST_CHECKED;
    payload->output_path = output_wide;

    set_ui_busy(true);
    set_status(L"Decompiling... (this may take a while for large assemblies)");

    g_worker_thread =
        CreateThread(nullptr, kWorkerStackSize, decompile_worker_thread, payload, 0, nullptr);
    if (g_worker_thread == nullptr) {
        set_ui_busy(false);
        delete payload;
        MessageBoxW(g_window, L"Failed to start decompilation thread.", L"csdecomp", MB_ICONERROR | MB_OK);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            g_window = hwnd;
            const int edit_width = kClientWidth - kMargin * 2 - kButtonWidth - 8;

            g_ui_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            int y = kMargin;
            create_label(hwnd, L".NET assembly:", kMargin, y, edit_width);
            y += 22;
            g_input_edit = create_edit(hwnd, kMargin, y, edit_width, kRowHeight, kInputEditId, false);
            create_button(hwnd, L"Browse...", kMargin + edit_width + 8, y - 2, kButtonWidth, 28,
                          kInputBrowseId);

            y += kRowHeight + 18;
            create_label(hwnd, L"Output .cs file:", kMargin, y, edit_width);
            y += 22;
            g_output_edit =
                create_edit(hwnd, kMargin, y, edit_width, kRowHeight, kOutputEditId, false);
            create_button(hwnd, L"Browse...", kMargin + edit_width + 8, y - 2, kButtonWidth, 28,
                          kOutputBrowseId);

            y += kRowHeight + 18;
            g_il_comments = CreateWindowExW(
                0, L"BUTTON", L"Include IL comments", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                kMargin, y, 260, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIlCommentsId)),
                GetModuleHandleW(nullptr), nullptr);
            apply_font(g_il_comments);

            y += 34;
            create_button(hwnd, L"Decompile", kMargin, y, 160, 34, kDecompileId);

            y += 48;
            g_status_edit = create_edit(hwnd, kMargin, y, kClientWidth - kMargin * 2,
                                        kClientHeight - y - kMargin, kStatusEditId, true);
            set_status(L"Select a .NET assembly to decompile.");
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case kInputBrowseId:
                    on_browse_input();
                    return 0;
                case kOutputBrowseId:
                    on_browse_output();
                    return 0;
                case kDecompileId:
                    on_decompile();
                    return 0;
                default:
                    break;
            }
            break;
        case WM_DECOMPILE_DONE:
            on_decompile_done(reinterpret_cast<std::wstring*>(lparam));
            return 0;
        case WM_DECOMPILE_ERROR:
            on_decompile_error(reinterpret_cast<std::wstring*>(lparam));
            return 0;
        case WM_DESTROY:
            if (g_vectored_handler != nullptr) {
                RemoveVectoredExceptionHandler(g_vectored_handler);
                g_vectored_handler = nullptr;
            }
            if (g_worker_thread != nullptr) {
                WaitForSingleObject(g_worker_thread, INFINITE);
                CloseHandle(g_worker_thread);
                g_worker_thread = nullptr;
            }
            if (g_ui_font != nullptr) {
                DeleteObject(g_ui_font);
                g_ui_font = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

int run_win32_gui() {
    FreeConsole();

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    std::set_terminate(gui_terminate_handler);
    SetUnhandledExceptionFilter(gui_unhandled_exception_filter);
    g_vectored_handler = AddVectoredExceptionHandler(1, gui_vectored_exception_handler);

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* class_name = L"CsDecompMainWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = class_name;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    RECT window_rect{0, 0, kClientWidth, kClientHeight};
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(0, class_name, L"csdecomp - C# Decompiler", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, window_rect.right - window_rect.left,
                                window_rect.bottom - window_rect.top, nullptr, nullptr, instance,
                                nullptr);
    if (hwnd == nullptr) {
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

}  // namespace csdecomp

#else

namespace csdecomp {

int run_win32_gui() { return 1; }

}  // namespace csdecomp

#endif
