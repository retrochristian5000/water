/*
 * Water FrontPage-era Form Wizard approximation (VTIFORM.EXE).
 * Copyright 2026 the Water contributors.
 * LGPL-2.1-or-later. Independent code, not the Microsoft binary.
 * Original FrontPage host integration and templates are unsupported.
 */
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>

#define ID_SAVE 100
#define MAX_FIELDS 20
#define MAX_LABEL 128
struct fields { char label[MAX_FIELDS][MAX_LABEL]; unsigned int count; };
static HWND title_edit, fields_edit, button, help_text, title_text, fields_text;
static HFONT ui_font;

static HWND control(HWND parent, const char *class_name, const char *text,
                    DWORD style, int id)
{
    HWND result = CreateWindowExA(0, class_name, text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 10, 10, parent, (HMENU)(ULONG_PTR)id,
        (HINSTANCE)GetWindowLongPtrA(parent, GWLP_HINSTANCE), NULL);
    if (result) SendMessageA(result, WM_SETFONT, (WPARAM)ui_font, TRUE);
    return result;
}
static BOOL parse_fields(const char *text, struct fields *out)
{
    const char *begin, *end;
    size_t len;
    memset(out, 0, sizeof(*out));
    while (*text)
    {
        begin = text;
        while (*text && *text != '\r' && *text != '\n') text++;
        end = text;
        while (begin < end && (unsigned char)*begin <= ' ') begin++;
        while (end > begin && (unsigned char)end[-1] <= ' ') end--;
        len = end - begin;
        if (len)
        {
            if (out->count >= MAX_FIELDS || len >= MAX_LABEL) return FALSE;
            memcpy(out->label[out->count], begin, len);
            out->label[out->count][len] = 0;
            out->count++;
        }
        if (*text == '\r') text++;
        if (*text == '\n') text++;
    }
    return out->count != 0;
}
static void html_escape(FILE *file, const char *p)
{
    while (*p)
    {
        switch (*p)
        {
        case '&': fputs("&amp;", file); break;
        case '<': fputs("&lt;", file); break;
        case '>': fputs("&gt;", file); break;
        case '"': fputs("&quot;", file); break;
        default: fputc((unsigned char)*p, file); break;
        }
        p++;
    }
}
static BOOL generate_form(FILE *file, const char *title, const struct fields *fields)
{
    unsigned int i;
    fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n"
          "<html><head><meta http-equiv=\"Content-Type\" "
          "content=\"text/html; charset=windows-1252\"><title>", file);
    html_escape(file, title);
    fputs("</title></head><body><h1>", file);
    html_escape(file, title);
    fputs("</h1>\n<!-- Template only: no response handler is configured. -->\n"
          "<form action=\"#\" method=\"post\" onsubmit=\"return false;\">\n", file);
    for (i = 0; i < fields->count; i++)
    {
        fprintf(file, "<p><label for=\"field%u\">", i + 1);
        html_escape(file, fields->label[i]);
        fprintf(file, "</label><br><input type=\"text\" id=\"field%u\" "
                      "name=\"field%u\" size=\"40\"></p>\n", i + 1, i + 1);
    }
    fputs("<p><input type=\"submit\" value=\"Submit\"> "
          "<input type=\"reset\" value=\"Reset\"></p></form></body></html>\n", file);
    return !ferror(file);
}
static void save_form(HWND hwnd)
{
    static const char filter[] = "HTML pages (*.htm;*.html)\0*.htm;*.html\0All files\0*.*\0";
    char title[160], raw[4096], filename[MAX_PATH] = "";
    struct fields parsed;
    OPENFILENAMEA save;
    FILE *file;
    BOOL ok;

    if (!GetWindowTextLengthA(title_edit) ||
        GetWindowTextLengthA(title_edit) >= sizeof(title) ||
        !GetWindowTextLengthA(fields_edit) ||
        GetWindowTextLengthA(fields_edit) >= sizeof(raw))
    {
        MessageBoxA(hwnd, "Enter a short title and field labels.",
                    "Form Wizard", MB_OK | MB_ICONWARNING);
        return;
    }
    GetWindowTextA(title_edit, title, sizeof(title));
    GetWindowTextA(fields_edit, raw, sizeof(raw));
    if (!parse_fields(raw, &parsed))
    {
        MessageBoxA(hwnd, "Enter 1-20 field labels, one per line "
                    "(under 128 characters each).", "Form Wizard",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    memset(&save, 0, sizeof(save));
    save.lStructSize = sizeof(save);
    save.hwndOwner = hwnd;
    save.lpstrFilter = filter;
    save.lpstrFile = filename;
    save.nMaxFile = sizeof(filename);
    save.lpstrDefExt = "htm";
    save.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameA(&save)) return;

    file = fopen(filename, "wb");
    if (!file)
    {
        MessageBoxA(hwnd, "Cannot open output file.", "Form Wizard",
                    MB_OK | MB_ICONERROR);
        return;
    }
    ok = generate_form(file, title, &parsed);
    if (fclose(file)) ok = FALSE;
    MessageBoxA(hwnd, ok ? "HTML template saved. Submission is disabled until "
                        "you add a server-side handler." :
                        "Could not write a complete HTML form.",
                "Form Wizard", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
}
static void resize_window(HWND hwnd)
{
    RECT r;
    int w, h;
    GetClientRect(hwnd, &r);
    w = r.right;
    h = r.bottom;
    MoveWindow(help_text, 14, 10, w - 28, 38, TRUE);
    MoveWindow(title_text, 14, 58, w - 28, 19, TRUE);
    MoveWindow(title_edit, 14, 79, w - 28, 26, TRUE);
    MoveWindow(fields_text, 14, 117, w - 28, 19, TRUE);
    MoveWindow(fields_edit, 14, 139, w - 28, h - 202, TRUE);
    MoveWindow(button, w - 171, h - 46, 157, 28, TRUE);
}
static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_CREATE:
        ui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        help_text = control(hwnd, "STATIC", "Build a standalone HTML form page. "
            "This is a Water approximation, not a FrontPage Express plugin.", 0, 0);
        title_text = control(hwnd, "STATIC", "Page title:", 0, 0);
        title_edit = control(hwnd, "EDIT", "Contact Form",
            WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 101);
        fields_text = control(hwnd, "STATIC", "Form fields (one label per line):", 0, 0);
        fields_edit = control(hwnd, "EDIT", "Name\r\nEmail\r\nMessage",
            WS_BORDER | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_WANTRETURN, 102);
        button = control(hwnd, "BUTTON", "Create HTML Page...",
                         WS_TABSTOP | BS_DEFPUSHBUTTON, ID_SAVE);
        return 0;
    case WM_SIZE:
        if (button) resize_window(hwnd);
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == ID_SAVE && HIWORD(w) == BN_CLICKED)
            save_form(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, w, l);
}
static int self_test(void)
{
    struct fields data;
    FILE *file;
    char output[4096];
    size_t n;
    if (!parse_fields("Name\r\nEmail\n", &data) || data.count != 2) return 1;
    if (parse_fields("\r\n \n", &data)) return 2;
    if (!parse_fields("Name\r\nEmail", &data)) return 3;
    file = tmpfile();
    if (!file) return 4;
    if (!generate_form(file, "<Test & Form>", &data))
    {
        fclose(file);
        return 5;
    }
    rewind(file);
    n = fread(output, 1, sizeof(output) - 1, file);
    fclose(file);
    output[n] = 0;
    if (!strstr(output, "&lt;Test &amp; Form&gt;") ||
        !strstr(output, "name=\"field2\"") ||
        !strstr(output, "onsubmit=\"return false;\"")) return 6;
    return 0;
}
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR args, int show)
{
    WNDCLASSA cls;
    HWND hwnd;
    MSG msg;
    int result;
    (void)prev;
    if (args && !strcmp(args, "--self-test")) return self_test();
    memset(&cls, 0, sizeof(cls));
    cls.lpfnWndProc = window_proc;
    cls.hInstance = inst;
    cls.lpszClassName = "WaterFormWizard";
    cls.hCursor = LoadCursorA(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    if (!RegisterClassA(&cls)) return 1;
    hwnd = CreateWindowExA(0, cls.lpszClassName,
        "FrontPage Form Wizard (Water)", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 680, 440, NULL, NULL, inst, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    while ((result = GetMessageA(&msg, NULL, 0, 0)) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return result < 0 ? 1 : (int)msg.wParam;
}
