/*
 * Water: 3D Flower Box screensaver approximation (SSFLWBOX.SCR).
 *
 * Copyright 2026 the Water contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This is an independently implemented, GDI-rendered approximation of the
 * Windows 98-era Flower Box visual. The historical program rendered with
 * OpenGL. No Microsoft SDK screensaver source code or assets are used.
 * It deliberately supports only three Water-defined cube deformation
 * modes, not the full original shape list or settings storage format.
 */

#include <math.h>
#include <stdlib.h>

#include "windows.h"
#include "resource.h"

#define FLOWER_TIMER 1
#define FLOWER_INTERVAL 40
#define FLOWER_COLORS 8
#define FLOWER_SHADES 4

static const char settings_key[] = "Software\\Water\\Screen Savers\\3D Flower Box";

struct flower_settings
{
    int shape;
    int speed;
    int detail;
};

struct flower_vec
{
    double x, y, z;
};

static struct flower_settings settings = { 0, 1, 1 };
static BOOL preview_mode;
static POINT initial_mouse;
static BOOL have_initial_mouse;
static HBRUSH flower_brushes[FLOWER_COLORS * FLOWER_SHADES];

static void load_setting(const char *name, int *field)
{
    DWORD value = 0, size = sizeof(value), type = 0;
    HKEY key;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, settings_key, 0, KEY_QUERY_VALUE, &key))
        return;
    if (!RegQueryValueExA(key, name, NULL, &type, (BYTE *)&value, &size) &&
        type == REG_DWORD && size == sizeof(value) && value <= 2)
        *field = (int)value;
    RegCloseKey(key);
}

static void load_settings(void)
{
    load_setting("Shape", &settings.shape);
    load_setting("Speed", &settings.speed);
    load_setting("Detail", &settings.detail);
}

static void save_settings(void)
{
    HKEY key;
    DWORD value;

    if (RegCreateKeyExA(HKEY_CURRENT_USER, settings_key, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &key, NULL))
        return;
    value = settings.shape;
    RegSetValueExA(key, "Shape", 0, REG_DWORD, (BYTE *)&value, sizeof(value));
    value = settings.speed;
    RegSetValueExA(key, "Speed", 0, REG_DWORD, (BYTE *)&value, sizeof(value));
    value = settings.detail;
    RegSetValueExA(key, "Detail", 0, REG_DWORD, (BYTE *)&value, sizeof(value));
    RegCloseKey(key);
}

static void init_brushes(void)
{
    static const BYTE colors[FLOWER_COLORS][3] =
    {
        { 245, 65, 120 }, { 250, 105, 50 }, { 250, 215, 60 },
        { 80, 225, 90 }, { 40, 215, 205 }, { 90, 145, 245 },
        { 180, 90, 240 }, { 245, 90, 220 }
    };
    int shade, hue;

    for (shade = 0; shade < FLOWER_SHADES; ++shade)
        for (hue = 0; hue < FLOWER_COLORS; ++hue)
        {
            int c = shade * FLOWER_COLORS + hue;
            int r = colors[hue][0] * (5 + shade) / 8;
            int g = colors[hue][1] * (5 + shade) / 8;
            int b = colors[hue][2] * (5 + shade) / 8;
            flower_brushes[c] = CreateSolidBrush(RGB(r, g, b));
        }
}

static void free_brushes(void)
{
    int i;

    for (i = 0; i < FLOWER_COLORS * FLOWER_SHADES; ++i)
        if (flower_brushes[i]) DeleteObject(flower_brushes[i]);
}

static struct flower_vec face_point(int face, double u, double v, double t)
{
    struct flower_vec p;
    double envelope = (1.0 - u * u) * (1.0 - v * v);
    double pulse = 0.5 + 0.5 * sin(t * 1.4);
    double offset;

    /* A convex cube changes into a six-petal, bulging geometric form. */
    if (settings.shape == 0)
        offset = 0.78 * envelope * pulse;
    else if (settings.shape == 1)
        offset = 0.96 * envelope * (0.5 + 0.5 * sin(t * 1.4 + face * 0.7));
    else
        offset = 0.42 * envelope * sin(t * 1.3 + u * 4.0 + v * 3.0);

    switch (face)
    {
    default:
    case 0: p.x = u;           p.y = v;           p.z = 1.0 + offset; break;
    case 1: p.x = u;           p.y = -v;          p.z = -1.0 - offset; break;
    case 2: p.x = 1.0 + offset;p.y = u;           p.z = v;            break;
    case 3: p.x = -1.0-offset;p.y = u;            p.z = -v;           break;
    case 4: p.x = u;           p.y = 1.0 + offset;p.z = v;            break;
    case 5: p.x = -u;          p.y = -1.0-offset; p.z = v;            break;
    }
    return p;
}

static struct flower_vec rotate_point(struct flower_vec p, double t)
{
    struct flower_vec r;
    double a = t * 0.43, b = t * 0.26, c = t * 0.17;
    double x, y, z;

    x = cos(a) * p.x - sin(a) * p.y;
    y = sin(a) * p.x + cos(a) * p.y;
    z = p.z;

    r.x = cos(c) * x + sin(c) * (sin(b) * y + cos(b) * z);
    r.y = cos(b) * y - sin(b) * z;
    r.z = -sin(c) * x + cos(c) * (sin(b) * y + cos(b) * z);
    return r;
}

static POINT project_point(struct flower_vec p, int width, int height, double t)
{
    POINT point;
    double distance = 4.4 - p.z;
    double size = (width < height ? width : height) * 0.83 / distance;
    double center_x = width * (0.5 + 0.12 * sin(t * 0.20));
    double center_y = height * (0.5 + 0.10 * cos(t * 0.17));

    point.x = (LONG)(center_x + p.x * size);
    point.y = (LONG)(center_y - p.y * size);
    return point;
}

static void render_flower(HWND hwnd, HDC dest)
{
    RECT rect;
    HDC mem;
    HBITMAP bitmap, old_bitmap;
    HGDIOBJ old_pen, old_brush;
    double t, z, depth[6];
    int width, height, n, order[6], face, row, col, i, j;

    GetClientRect(hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    mem = CreateCompatibleDC(dest);
    if (!mem) return;
    bitmap = CreateCompatibleBitmap(dest, width, height);
    if (!bitmap)
    {
        DeleteDC(mem);
        return;
    }
    old_bitmap = SelectObject(mem, bitmap);
    FillRect(mem, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    old_pen = SelectObject(mem, GetStockObject(NULL_PEN));
    old_brush = SelectObject(mem, GetStockObject(WHITE_BRUSH));

    t = GetTickCount() * (0.00055 + settings.speed * 0.00030);
    n = 5 + settings.detail * 2;

    for (face = 0; face < 6; ++face)
    {
        struct flower_vec p = rotate_point(face_point(face, 0.0, 0.0, t), t);
        depth[face] = p.z;
        order[face] = face;
    }
    /* Paint the farthest faces first. Each tessellated face remains intact. */
    for (i = 1; i < 6; ++i)
        for (j = i; j > 0 && depth[order[j - 1]] > depth[order[j]]; --j)
        {
            int tmp = order[j];
            order[j] = order[j - 1];
            order[j - 1] = tmp;
        }

    for (i = 0; i < 6; ++i)
    {
        int f = order[i];
        int shade = 1 + (int)((depth[f] + 2.0) * 0.5);
        if (shade < 0) shade = 0;
        if (shade >= FLOWER_SHADES) shade = FLOWER_SHADES - 1;

        for (row = 0; row < n; ++row)
            for (col = 0; col < n; ++col)
            {
                POINT polygon[4];
                double u0 = -1.0 + 2.0 * col / n;
                double u1 = -1.0 + 2.0 * (col + 1) / n;
                double v0 = -1.0 + 2.0 * row / n;
                double v1 = -1.0 + 2.0 * (row + 1) / n;
                int hue = (f * 2 + row / 2 + col / 2 + (int)t) % FLOWER_COLORS;

                polygon[0] = project_point(rotate_point(face_point(f, u0, v0, t), t), width, height, t);
                polygon[1] = project_point(rotate_point(face_point(f, u1, v0, t), t), width, height, t);
                polygon[2] = project_point(rotate_point(face_point(f, u1, v1, t), t), width, height, t);
                polygon[3] = project_point(rotate_point(face_point(f, u0, v1, t), t), width, height, t);
                SelectObject(mem, flower_brushes[shade * FLOWER_COLORS + hue]);
                Polygon(mem, polygon, 4);
            }
    }

    BitBlt(dest, 0, 0, width, height, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old_brush);
    SelectObject(mem, old_pen);
    SelectObject(mem, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(mem);
}

static INT_PTR CALLBACK config_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    static const char * const shapes[] = { "Flowering cube", "Pulsing facets", "Wave box" };
    static const char * const speeds[] = { "Slow", "Normal", "Fast" };
    static const char * const details[] = { "Low", "Normal", "High" };
    const char * const *lists[3] = { shapes, speeds, details };
    const int ids[3] = { IDC_FLOWERBOX_SHAPE, IDC_FLOWERBOX_SPEED, IDC_FLOWERBOX_DETAIL };
    int i, j;

    switch (message)
    {
    case WM_INITDIALOG:
        for (i = 0; i < 3; ++i)
        {
            HWND combo = GetDlgItem(dialog, ids[i]);
            for (j = 0; j < 3; ++j)
                SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)lists[i][j]);
        }
        SendDlgItemMessageA(dialog, ids[0], CB_SETCURSEL, settings.shape, 0);
        SendDlgItemMessageA(dialog, ids[1], CB_SETCURSEL, settings.speed, 0);
        SendDlgItemMessageA(dialog, ids[2], CB_SETCURSEL, settings.detail, 0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDOK:
            i = SendDlgItemMessageA(dialog, ids[0], CB_GETCURSEL, 0, 0);
            if (i >= 0 && i < 3) settings.shape = i;
            i = SendDlgItemMessageA(dialog, ids[1], CB_GETCURSEL, 0, 0);
            if (i >= 0 && i < 3) settings.speed = i;
            i = SendDlgItemMessageA(dialog, ids[2], CB_GETCURSEL, 0, 0);
            if (i >= 0 && i < 3) settings.detail = i;
            save_settings();
            EndDialog(dialog, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static LRESULT CALLBACK flower_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_CREATE:
        have_initial_mouse = GetCursorPos(&initial_mouse);
        if (!SetTimer(hwnd, FLOWER_TIMER, FLOWER_INTERVAL, NULL))
            return -1;
        return 0;

    case WM_TIMER:
        if (wparam == FLOWER_TIMER)
        {
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        render_flower(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (!preview_mode && have_initial_mouse)
        {
            POINT mouse;
            if (GetCursorPos(&mouse) &&
                (abs(mouse.x - initial_mouse.x) > 5 ||
                 abs(mouse.y - initial_mouse.y) > 5))
                DestroyWindow(hwnd);
        }
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        if (!preview_mode) DestroyWindow(hwnd);
        return 0;

    case WM_SETCURSOR:
        if (!preview_mode) { SetCursor(NULL); return TRUE; }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, FLOWER_TIMER);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    WNDCLASSA cls;
    HWND hwnd, parent = NULL;
    MSG msg;
    int width, height;
    char mode = 'c';
    DWORD style, exstyle;
    const char *args = command;

    (void)previous;
    (void)show;
    load_settings();

    while (*args == ' ' || *args == '\t') ++args;
    if (*args == '/' || *args == '-') ++args;
    if (*args)
    {
        mode = *args++;
        if (mode >= 'A' && mode <= 'Z') mode += 'a' - 'A';
    }

    if (mode == 'c')
    {
        DialogBoxParamA(instance, MAKEINTRESOURCEA(IDD_FLOWERBOX_CONFIG),
                        NULL, config_proc, 0);
        return 0;
    }
    if (mode == 'p')
    {
        unsigned long long id;
        while (*args == ':' || *args == ' ' || *args == '\t') ++args;
        id = strtoull(args, NULL, 0);
        parent = (HWND)(ULONG_PTR)id;
        if (!parent || !IsWindow(parent)) return 1;
        preview_mode = TRUE;
    }
    else if (mode != 's')
    {
        /* Win9x /a password handling cannot be imitated by a GUI preview. */
        return 1;
    }

    cls.style = CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = flower_proc;
    cls.cbClsExtra = 0;
    cls.cbWndExtra = 0;
    cls.hInstance = instance;
    cls.hIcon = NULL;
    cls.hCursor = preview_mode ? LoadCursorA(NULL, (LPCSTR)IDC_ARROW) : NULL;
    cls.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    cls.lpszMenuName = NULL;
    cls.lpszClassName = "WaterFlowerBox";
    if (!RegisterClassA(&cls)) return 1;

    if (preview_mode)
    {
        RECT rect;
        GetClientRect(parent, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
        style = WS_CHILD | WS_VISIBLE;
        exstyle = 0;
    }
    else
    {
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
        style = WS_POPUP | WS_VISIBLE;
        exstyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    }

    init_brushes();
    hwnd = CreateWindowExA(exstyle, "WaterFlowerBox", "3D Flower Box (Water)",
                           style, 0, 0, width, height, parent, NULL, instance, NULL);
    if (!hwnd)
    {
        free_brushes();
        return 1;
    }

    if (!preview_mode)
    {
        ShowCursor(FALSE);
        SetForegroundWindow(hwnd);
    }

    while (GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (!preview_mode) ShowCursor(TRUE);
    free_brushes();
    return (int)msg.wParam;
}
