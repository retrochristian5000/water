/*
 * WATER Outlook Express local message reader
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <stdarg.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <ole2.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include "mimeole.h"

#define MAX_MESSAGE_SIZE (8 * 1024 * 1024)
#define IDM_OPEN 100
#define IDM_EXIT 101

static HWND body_view;

/* Use MIME metadata, not the current system ANSI code page. */
static HRESULT get_body_codepage(IMimeMessage *message, HBODY handle, UINT *codepage)
{
    IMimeAllocator *allocator;
    IMimeBody *body;
    MIMEPARAMINFO *params = NULL;
    INETCSETINFO info;
    HCHARSET charset;
    ULONG count = 0, i;
    HRESULT hr;

    *codepage = 20127; /* MIME default: US-ASCII. */
    hr = MimeOleGetAllocator(&allocator);
    if(hr != S_OK)
        return hr;

    hr = IMimeMessage_BindToObject(message, handle, &IID_IMimeBody, (void **)&body);
    if(hr == S_OK) {
        hr = IMimeBody_GetParameters(body, "Content-Type", &count, &params);
        IMimeBody_Release(body);
        if(hr == S_OK) {
            for(i = 0; i < count; ++i) {
                if(lstrcmpiA(params[i].pszName, "charset"))
                    continue;
                hr = MimeOleFindCharset(params[i].pszData, &charset);
                if(hr == S_OK) {
                    hr = MimeOleGetCharsetInfo(charset, &info);
                    if(hr == S_OK)
                        *codepage = info.cpiInternet;
                }
                break;
            }
        }else if(hr == MIME_E_NOT_FOUND) {
            hr = S_OK;
        }
        if(params)
            IMimeAllocator_FreeParamInfoArray(allocator, count, params, TRUE);
    }
    IMimeAllocator_Release(allocator);
    return hr;
}

static HRESULT read_body(IStream *stream, UINT codepage, WCHAR **text)
{
    STATSTG stat;
    char *bytes = NULL;
    WCHAR *result = NULL;
    ULONG size, read;
    int chars, i;
    ULONG j;
    HRESULT hr;

    *text = NULL;
    hr = IStream_Stat(stream, &stat, STATFLAG_NONAME);
    if(FAILED(hr))
        return hr;
    if(stat.cbSize.QuadPart > MAX_MESSAGE_SIZE)
        return STG_E_MEDIUMFULL;

    size = stat.cbSize.LowPart;
    if(!(bytes = malloc(size ? size : 1)))
        return E_OUTOFMEMORY;

    hr = IStream_Read(stream, bytes, size, &read);
    if(FAILED(hr))
        goto done;
    if(read != size) {
        hr = STG_E_READFAULT;
        goto done;
    }

    if(codepage == 20127) {
        for(j = 0; j < size; ++j) {
            if((unsigned char)bytes[j] > 127) {
                hr = HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION);
                goto done;
            }
        }
    }

    chars = size ? MultiByteToWideChar(codepage, codepage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
                                     bytes, size, NULL, 0) : 0;
    if(size && !chars) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto done;
    }

    if(!(result = malloc((chars + 1) * sizeof(*result)))) {
        hr = E_OUTOFMEMORY;
        goto done;
    }

    if(chars && !MultiByteToWideChar(codepage, codepage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
                                    bytes, size, result, chars)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto done;
    }
    /* EDIT controls use NUL-terminated strings. Do not silently truncate. */
    for(i = 0; i < chars; ++i)
        if(!result[i])
            result[i] = 0xfffd;
    result[chars] = 0;
    *text = result;
    result = NULL;
    hr = S_OK;

done:
    free(result);
    free(bytes);
    return hr;
}

static HRESULT load_message(const WCHAR *path, WCHAR **text, BOOL *html_source)
{
    IStream *source = NULL, *body = NULL;
    IMimeMessage *message = NULL;
    STATSTG stat;
    HBODY handle;
    UINT codepage;
    HRESULT hr;

    *text = NULL;
    *html_source = FALSE;

    hr = SHCreateStreamOnFileEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, NULL, &source);
    if(FAILED(hr))
        goto done;
    hr = IStream_Stat(source, &stat, STATFLAG_NONAME);
    if(FAILED(hr))
        goto done;
    if(stat.cbSize.QuadPart > MAX_MESSAGE_SIZE) {
        hr = STG_E_MEDIUMFULL;
        goto done;
    }
    hr = MimeOleCreateMessage(NULL, &message);
    if(hr != S_OK)
        goto done;
    hr = IMimeMessage_Load(message, source);
    if(hr != S_OK)
        goto done;

    hr = IMimeMessage_GetTextBody(message, TXT_PLAIN, IET_BINARY, &body, &handle);
    if(hr == MIME_E_NOT_FOUND) {
        hr = IMimeMessage_GetTextBody(message, TXT_HTML, IET_BINARY, &body, &handle);
        *html_source = TRUE;
    }
    if(hr != S_OK)
        goto done;

    hr = get_body_codepage(message, handle, &codepage);
    if(hr == S_OK)
        hr = read_body(body, codepage, text);

done:
    if(body) IStream_Release(body);
    if(message) IMimeMessage_Release(message);
    if(source) IStream_Release(source);
    return hr;
}

static void open_message(HWND window, const WCHAR *path)
{
    WCHAR *text;
    WCHAR error[80];
    BOOL html_source;
    HRESULT hr;

    hr = load_message(path, &text, &html_source);
    if(hr != S_OK) {
        swprintf(error, ARRAY_SIZE(error), L"Unable to read message (0x%08lx).", hr);
        MessageBoxW(window, error, L"WATER Outlook Express", MB_OK | MB_ICONERROR);
        return;
    }

    if(!SetWindowTextW(body_view, text))
        MessageBoxW(window, L"Unable to display message.", L"WATER Outlook Express", MB_OK | MB_ICONERROR);
    else
        SetWindowTextW(window, html_source ? L"WATER Outlook Express - HTML source (inactive)"
                                          : L"WATER Outlook Express - Plain text");
    free(text);
}

static void choose_message(HWND window)
{
    WCHAR path[32768] = {0};
    OPENFILENAMEW ofn = {0};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = window;
    ofn.lpstrFilter = L"Email messages (*.eml)\0*.eml\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = ARRAY_SIZE(path);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if(GetOpenFileNameW(&ofn))
        open_message(window, path);
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch(msg) {
    case WM_CREATE:
        body_view = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                0, 0, 0, 0, window, NULL, GetModuleHandleW(NULL), NULL);
        if(!body_view)
            return -1;
        SendMessageW(body_view, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        SendMessageW(body_view, EM_SETLIMITTEXT, MAX_MESSAGE_SIZE, 0);
        return 0;
    case WM_SIZE:
        MoveWindow(body_view, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE);
        return 0;
    case WM_COMMAND:
        switch(LOWORD(wparam)) {
        case IDM_OPEN:
            choose_message(window);
            return 0;
        case IDM_EXIT:
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, msg, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, WCHAR *cmdline, int show)
{
    WNDCLASSW cls = {0};
    HMENU menu, file;
    HWND window;
    WCHAR **argv;
    int argc, ret;
    MSG msg;
    HRESULT hr;

    hr = OleInitialize(NULL);
    if(FAILED(hr))
        return 1;

    cls.lpfnWndProc = window_proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    cls.lpszClassName = L"WATER_MSIMN";
    if(!RegisterClassW(&cls)) {
        OleUninitialize();
        return 1;
    }

    menu = CreateMenu();
    file = CreatePopupMenu();
    if(!menu || !file || !AppendMenuW(file, MF_STRING, IDM_OPEN, L"&Open message...") ||
       !AppendMenuW(file, MF_STRING, IDM_EXIT, L"E&xit") ||
       !AppendMenuW(menu, MF_POPUP, (UINT_PTR)file, L"&File")) {
        if(file) DestroyMenu(file);
        if(menu) DestroyMenu(menu);
        OleUninitialize();
        return 1;
    }

    window = CreateWindowExW(0, cls.lpszClassName, L"WATER Outlook Express",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 720, 480,
            NULL, menu, instance, NULL);
    if(!window) {
        DestroyMenu(menu);
        OleUninitialize();
        return 1;
    }
    ShowWindow(window, show);
    UpdateWindow(window);

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if(argv) {
        if(argc == 2 && argv[1][0] != '/' && argv[1][0] != '-')
            open_message(window, argv[1]);
        else if(argc > 1)
            MessageBoxW(window, L"Usage: msimn.exe [message.eml]", L"WATER Outlook Express", MB_OK);
        LocalFree(argv);
    }

    while((ret = GetMessageW(&msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if(ret == -1)
        DestroyWindow(window);
    OleUninitialize();
    return ret == -1 ? 1 : (int)msg.wParam;
}
