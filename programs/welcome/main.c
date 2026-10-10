/*
 * Windows 98 Welcome compatibility frontend for Water.
 *
 * Clean-room UI recreation based on documented Windows 98 behavior.
 * Original WELCOME.DAT and .WBM content is neither parsed nor reproduced.
 *
 * Copyright 2026 Water project contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "resource.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static const char run_key[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char run_name[] = "WaterWelcome";

struct welcome_topic
{
    const char *name;
    const char *description;
    const char *program;
    const char *parameters;
};

/* The helper programs are separate from Welcome. In particular, Discover.exe
 * is stored on Windows 98 CD media, and Water's icwconn1 is currently an
 * Internet Properties frontend rather than a full ISP-provisioning wizard. */
static const struct welcome_topic topics[] =
{
    {"Register Now",
     "Start the Windows 98 Registration Wizard if installed. Registration is "
     "optional; Welcome itself does not submit any information.",
     "regwiz.exe", "/r"},
    {"Connect to the Internet",
     "Start Internet connection setup. Water's Internet Connection Wizard "
     "opens Internet Properties but does not provision an ISP.",
     "icwconn1.exe", NULL},
    {"Discover Windows 98",
     "Explore the Windows 98 tour when its separate CD-ROM components are "
     "available. The tour is not part of this Welcome frontend.",
     "discover.exe", NULL},
    {"Maintain Your Computer",
     "Start the Windows 98 Maintenance Wizard when installed. Welcome itself "
     "does not schedule repairs or modify any disks.",
     "tuneup.exe", NULL},
};

/* Never rewrite Windows 98's original HKLM Run entry ("Welcome"). The
 * Water-owned per-user entry is explicit and independently reversible. */
static BOOL build_startup_command(char *command, DWORD capacity)
{
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, path, ARRAY_SIZE(path));

    if (!length || length >= ARRAY_SIZE(path) || length + 6 > capacity)
        return FALSE;
    lstrcpyA(command, "\"");
    lstrcatA(command, path);
    lstrcatA(command, "\" /R");
    return TRUE;
}

static BOOL startup_enabled(void)
{
    char command[MAX_PATH + 8], saved[MAX_PATH + 8];
    HKEY key;
    DWORD type = 0, size = sizeof(saved);
    LONG status;
    BOOL enabled = FALSE;

    if (!build_startup_command(command, sizeof(command))) return FALSE;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, run_key, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return FALSE;

    status = RegQueryValueExA(key, run_name, NULL, &type, (BYTE *)saved, &size);
    if (status == ERROR_SUCCESS && type == REG_SZ && size && size <= sizeof(saved) &&
        saved[size - 1] == 0)
        enabled = !lstrcmpiA(saved, command);
    RegCloseKey(key);
    return enabled;
}

static BOOL set_startup(BOOL enabled)
{
    char command[MAX_PATH + 8];
    HKEY key;
    LONG status;

    if (!build_startup_command(command, sizeof(command))) return FALSE;

    if (enabled)
    {
        status = RegCreateKeyExA(HKEY_CURRENT_USER, run_key, 0, NULL, 0,
                                KEY_SET_VALUE, NULL, &key, NULL);
        if (status != ERROR_SUCCESS) return FALSE;
        status = RegSetValueExA(key, run_name, 0, REG_SZ, (const BYTE *)command,
                                lstrlenA(command) + 1);
    }
    else
    {
        /* An unrelated value under this name must not be removed. */
        if (!startup_enabled()) return TRUE;
        status = RegOpenKeyExA(HKEY_CURRENT_USER, run_key, 0, KEY_SET_VALUE, &key);
        if (status != ERROR_SUCCESS) return FALSE;
        status = RegDeleteValueA(key, run_name);
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

static int selected_topic(HWND dialog)
{
    LRESULT index = SendDlgItemMessageA(dialog, IDC_TOPICS, LB_GETCURSEL, 0, 0);
    return (index >= 0 && index < (LRESULT)ARRAY_SIZE(topics)) ? (int)index : -1;
}

static void show_topic(HWND dialog)
{
    int index = selected_topic(dialog);

    if (index < 0)
    {
        SetDlgItemTextA(dialog, IDC_TITLE, "Welcome");
        SetDlgItemTextA(dialog, IDC_DESCRIPTION,
                        "Welcome to Windows 98. Choose a topic from Contents "
                        "to learn more or begin an activity.");
    }
    else
    {
        SetDlgItemTextA(dialog, IDC_TITLE, topics[index].name);
        SetDlgItemTextA(dialog, IDC_DESCRIPTION, topics[index].description);
    }
}

static void begin_topic(HWND dialog)
{
    int index = selected_topic(dialog);
    HINSTANCE result;
    char message[240];

    if (index < 0)
    {
        SendDlgItemMessageA(dialog, IDC_TOPICS, LB_SETCURSEL, 0, 0);
        show_topic(dialog);
        index = 0;
    }

    result = ShellExecuteA(dialog, "open", topics[index].program,
                           topics[index].parameters, NULL, SW_SHOWNORMAL);
    if ((UINT_PTR)result > 32) return;

    wsprintfA(message, "The component '%s' is not available.\n\n"
                       "This Welcome frontend does not replace its "
                       "installer or wizard.", topics[index].program);
    MessageBoxA(dialog, message, "Welcome to Windows 98", MB_OK | MB_ICONINFORMATION);
}

static INT_PTR CALLBACK welcome_dialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    unsigned int i;
    (void)lparam;

    switch (message)
    {
    case WM_INITDIALOG:
        for (i = 0; i < ARRAY_SIZE(topics); ++i)
            SendDlgItemMessageA(dialog, IDC_TOPICS, LB_ADDSTRING, 0, (LPARAM)topics[i].name);
        CheckDlgButton(dialog, IDC_STARTUP, startup_enabled() ? BST_CHECKED : BST_UNCHECKED);
        show_topic(dialog);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_TOPICS:
            if (HIWORD(wparam) == LBN_SELCHANGE) show_topic(dialog);
            if (HIWORD(wparam) == LBN_DBLCLK) begin_topic(dialog);
            return TRUE;
        case IDC_STARTUP:
            if (HIWORD(wparam) == BN_CLICKED)
            {
                BOOL enabled = IsDlgButtonChecked(dialog, IDC_STARTUP) == BST_CHECKED;
                if (!set_startup(enabled))
                {
                    CheckDlgButton(dialog, IDC_STARTUP, enabled ? BST_UNCHECKED : BST_CHECKED);
                    MessageBoxA(dialog, "Unable to update the current user's startup setting.",
                                "Welcome to Windows 98", MB_OK | MB_ICONWARNING);
                }
            }
            return TRUE;
        case IDC_BEGIN:
            begin_topic(dialog);
            return TRUE;
        case IDCANCEL:
            EndDialog(dialog, 0);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(dialog, 0);
        return TRUE;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    const char *arg = command_line;

    (void)previous;
    (void)show;
    while (*arg == ' ' || *arg == '\t') ++arg;

    /* Historical /R startup mode: display only if the user opted into the
     * Water-owned HKCU startup entry, never based on the original HKLM one. */
    if (!lstrcmpiA(arg, "/R") && !startup_enabled()) return 0;

    return DialogBoxParamA(instance, MAKEINTRESOURCEA(IDD_WELCOME), NULL,
                           welcome_dialog, 0) == -1 ? 1 : 0;
}
