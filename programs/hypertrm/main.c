/* Water's experimental direct-serial terminal for Win32.
 * Copyright (C) 2026 Water contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Not Hilgraeve's proprietary HyperTerminal: no HT sessions,
 * ANSI/VT100 emulation, dialing, or file transfer is implemented yet.
 */
#include "windows.h"
#include "settings.h"
#include <stdio.h>

#define ID_PORT       100
#define ID_BAUD       101
#define ID_CONNECT    102
#define ID_DISCONNECT 103
#define ID_DISPLAY    104
#define ID_STATUS     105
#define READ_TIMER    1

static struct {
    HWND port_edit, baud_edit, connect_button, disconnect_button, display, status;
    HANDLE serial;
    WNDPROC edit_proc;
    BOOL last_cr;
} terminal;

static void resize_controls(HWND window)
{
    RECT rect;
    int width, height;
    GetClientRect(window, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    MoveWindow(terminal.port_edit, 8, 8, 75, 23, TRUE);
    MoveWindow(terminal.baud_edit, 88, 8, 80, 23, TRUE);
    MoveWindow(terminal.connect_button, 174, 8, 78, 23, TRUE);
    MoveWindow(terminal.disconnect_button, 258, 8, 90, 23, TRUE);
    MoveWindow(terminal.display, 8, 39, width > 16 ? width - 16 : 0,
               height > 70 ? height - 70 : 0, TRUE);
    MoveWindow(terminal.status, 8, height > 26 ? height - 26 : 0,
               width > 16 ? width - 16 : 0, 20, TRUE);
}

static void disconnect_serial(HWND window)
{
    KillTimer(window, READ_TIMER);
    if (terminal.serial != INVALID_HANDLE_VALUE) {
        CloseHandle(terminal.serial);
        terminal.serial = INVALID_HANDLE_VALUE;
    }
    EnableWindow(terminal.port_edit, TRUE);
    EnableWindow(terminal.baud_edit, TRUE);
    EnableWindow(terminal.connect_button, TRUE);
    EnableWindow(terminal.disconnect_button, FALSE);
    SetWindowTextA(terminal.status, "Disconnected");
}

static void show_serial_error(HWND window, const char *operation)
{
    char message[160];
    DWORD error = GetLastError();
    wsprintfA(message, "%s failed (Win32 error %lu).", operation, error);
    MessageBoxA(window, message, "Serial connection", MB_OK | MB_ICONERROR);
}

static void append_received(const char *input, DWORD length)
{
    char text[1024];
    DWORD i, used = 0;
    for (i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)input[i];
        if (ch == '\r') {
            text[used++] = '\r';
            text[used++] = '\n';
            terminal.last_cr = TRUE;
        } else if (ch == '\n') {
            if (!terminal.last_cr) {
                text[used++] = '\r';
                text[used++] = '\n';
            }
            terminal.last_cr = FALSE;
        } else {
            terminal.last_cr = FALSE;
            if (ch == '\t' || ch >= 32) text[used++] = ch;
        }
    }
    text[used] = 0;
    if (!used) return;
    /* Keep history below the Win9x EDIT-control size limit. */
    if (GetWindowTextLengthA(terminal.display) > 56000)
        SetWindowTextA(terminal.display, "");
    SendMessageA(terminal.display, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageA(terminal.display, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageA(terminal.display, EM_SCROLLCARET, 0, 0);
}

static void connect_serial(HWND window)
{
    char port[32], baud[32], status[128];
    struct serial_settings settings;
    DCB dcb;
    COMMTIMEOUTS timeouts;
    HANDLE handle;

    GetWindowTextA(terminal.port_edit, port, sizeof(port));
    GetWindowTextA(terminal.baud_edit, baud, sizeof(baud));
    if (!serial_parse_settings(port, baud, &settings)) {
        MessageBoxA(window, "Enter COM1..COM256 and a positive baud rate (up to 4000000).",
                    "Invalid serial connection", MB_OK | MB_ICONWARNING);
        return;
    }
    handle = CreateFileA(settings.device, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                         OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        show_serial_error(window, "Opening COM port");
        return;
    }
    ZeroMemory(&dcb, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb)) {
        show_serial_error(window, "Reading COM settings");
        CloseHandle(handle);
        return;
    }
    dcb.BaudRate = settings.baud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;
    if (!SetCommState(handle, &dcb)) {
        show_serial_error(window, "Applying COM settings");
        CloseHandle(handle);
        return;
    }
    /* Read immediately, even when no bytes are available. */
    ZeroMemory(&timeouts, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.WriteTotalTimeoutConstant = 1000;
    if (!SetCommTimeouts(handle, &timeouts)) {
        show_serial_error(window, "Applying COM timeouts");
        CloseHandle(handle);
        return;
    }
    terminal.serial = handle;
    terminal.last_cr = FALSE;
    if (!SetTimer(window, READ_TIMER, 40, NULL)) {
        show_serial_error(window, "Starting receive timer");
        disconnect_serial(window);
        return;
    }
    EnableWindow(terminal.port_edit, FALSE);
    EnableWindow(terminal.baud_edit, FALSE);
    EnableWindow(terminal.connect_button, FALSE);
    EnableWindow(terminal.disconnect_button, TRUE);
    wsprintfA(status, "Connected: %s at %lu baud, 8-N-1, no flow control",
              port, settings.baud);
    SetWindowTextA(terminal.status, status);
    SetFocus(terminal.display);
}

static LRESULT CALLBACK terminal_edit_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CHAR) {
        char ch = (char)wp;
        DWORD written;
        if (ch == '\n' || ch == '\r') ch = '\r';
        else if (ch != '\b' && ch != '\t' && ((unsigned char)ch < 32))
            return 0;
        if (terminal.serial != INVALID_HANDLE_VALUE) {
            if (!WriteFile(terminal.serial, &ch, 1, &written, NULL) || written != 1) {
                HWND parent = GetParent(window);
                show_serial_error(parent, "Sending serial data");
                disconnect_serial(parent);
            }
        }
        return 0;
    }
    return CallWindowProcA(terminal.edit_proc, window, msg, wp, lp);
}

static LRESULT CALLBACK terminal_window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        terminal.serial = INVALID_HANDLE_VALUE;
        terminal.port_edit = CreateWindowExA(0, "EDIT", "COM1",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0,
            window, (HMENU)ID_PORT, NULL, NULL);
        terminal.baud_edit = CreateWindowExA(0, "EDIT", "9600",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0,
            window, (HMENU)ID_BAUD, NULL, NULL);
        terminal.connect_button = CreateWindowExA(0, "BUTTON", "Connect",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
            window, (HMENU)ID_CONNECT, NULL, NULL);
        terminal.disconnect_button = CreateWindowExA(0, "BUTTON", "Disconnect",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
            window, (HMENU)ID_DISCONNECT, NULL, NULL);
        terminal.display = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_READONLY, 0, 0, 0, 0,
            window, (HMENU)ID_DISPLAY, NULL, NULL);
        terminal.status = CreateWindowExA(0, "STATIC", "Disconnected",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
            window, (HMENU)ID_STATUS, NULL, NULL);
        if (!terminal.port_edit || !terminal.baud_edit || !terminal.connect_button ||
            !terminal.disconnect_button || !terminal.display || !terminal.status)
            return -1;
        SendMessageA(terminal.display, WM_SETFONT,
                     (WPARAM)GetStockObject(ANSI_FIXED_FONT), TRUE);
        SendMessageA(terminal.display, EM_LIMITTEXT, 61440, 0);
        terminal.edit_proc = (WNDPROC)SetWindowLongPtrA(terminal.display, GWLP_WNDPROC,
                                                       (LONG_PTR)terminal_edit_proc);
        if (!terminal.edit_proc) return -1;
        EnableWindow(terminal.disconnect_button, FALSE);
        return 0;
    case WM_SIZE:
        resize_controls(window);
        return 0;
    case WM_SETFOCUS:
        SetFocus(terminal.display);
        return 0;
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) {
            if (LOWORD(wp) == ID_CONNECT) connect_serial(window);
            if (LOWORD(wp) == ID_DISCONNECT) disconnect_serial(window);
        }
        return 0;
    case WM_TIMER:
        if (wp == READ_TIMER && terminal.serial != INVALID_HANDLE_VALUE) {
            DWORD i;
            for (i = 0; i < 8; ++i) {
                char buf[256];
                DWORD received = 0;
                if (!ReadFile(terminal.serial, buf, sizeof(buf), &received, NULL)) {
                    show_serial_error(window, "Receiving serial data");
                    disconnect_serial(window);
                    break;
                }
                if (!received) break;
                append_received(buf, received);
            }
        }
        return 0;
    case WM_DESTROY:
        disconnect_serial(window);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    WNDCLASSA cls;
    HWND window;
    MSG msg;
    (void)previous;
    (void)command;

    ZeroMemory(&cls, sizeof(cls));
    cls.lpfnWndProc = terminal_window_proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorA(NULL, IDC_IBEAM);
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    cls.lpszClassName = "WaterSerialTerminal";
    if (!RegisterClassA(&cls)) return 1;
    window = CreateWindowExA(0, cls.lpszClassName,
                            "Serial Terminal (HyperTerminal compatibility preview)",
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 720, 500,
                            NULL, NULL, instance, NULL);
    if (!window) return 1;
    ShowWindow(window, show);
    UpdateWindow(window);
    ZeroMemory(&msg, sizeof(msg));
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
