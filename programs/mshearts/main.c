/*
 * Water's clean-room, offline Microsoft Hearts-compatible frontend.
 * Rules and card indexing are shared with the portable hearts.c engine.
 * Uses CARDS.DLL at runtime, with a basic Win32 GDI fallback.
 *
 * Copyright 2026 Water project contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "hearts.h"

#define ID_NEW   101
#define ID_PASS  102
#define ID_NEXT  103
#define ID_RULES 104
#define CARD_W  74
#define CARD_H  102

static struct hearts_game game;
static int selected[52], selected_count;
static HWND main_window, pass_button, next_button;
static HMODULE cards_module;
static BOOL (WINAPI *cards_init)(int *, int *);
static BOOL (WINAPI *cards_draw_ext)(HDC, int, int, int, int, int, int, DWORD);
static void (WINAPI *cards_term)(void);
static int cards_ready;
static const char *names[4] = {"You", "West", "North", "East"};
static const char *passes[4] = {"left", "right", "across", "none"};

static void card_title(int card, char *out)
{
    static const char *ranks[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    static const char *suits[] = {"C", "D", "H", "S"};
    wsprintfA(out, "%s%s", ranks[card / 4], suits[card % 4]);
}

static void draw_card(HDC dc, int x, int y, int card, int width, int height, int highlighted)
{
    RECT rect = {x, y, x + width, y + height};
    HBRUSH white;
    HGDIOBJ oldbrush;
    char title[8];
    int oldmode;

    if (cards_ready && cards_draw_ext(dc, x, y, width, height, card, 0, 0))
    {
        if (highlighted) FrameRect(dc, &rect, GetSysColorBrush(COLOR_HIGHLIGHT));
        return;
    }
    white = CreateSolidBrush(card == 53 ? RGB(65, 90, 165) : RGB(253, 253, 247));
    if (!white) return;
    oldbrush = SelectObject(dc, white);
    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, oldbrush);
    DeleteObject(white);
    if (card == 53)
    {
        SetTextColor(dc, RGB(245, 245, 250));
        TextOutA(dc, x + 13, y + height / 2 - 8, "WATER", 5);
    }
    else
    {
        card_title(card, title);
        SetTextColor(dc, (card % 4 == 1 || card % 4 == 2) ?
                          RGB(160, 25, 25) : RGB(25, 25, 25));
        oldmode = SetBkMode(dc, TRANSPARENT);
        TextOutA(dc, x + 7, y + 7, title, lstrlenA(title));
        SetBkMode(dc, oldmode);
    }
    if (highlighted) FrameRect(dc, &rect, GetSysColorBrush(COLOR_HIGHLIGHT));
}

static int hand_step(int width)
{
    int step = (width - 50 - CARD_W) / 12;
    if (step < 19) step = 19;
    if (step > 60) step = 60;
    return step;
}

static int hand_origin(int width)
{
    int span = CARD_W + 12 * hand_step(width);
    return width > span ? (width - span) / 2 : 10;
}

static void text_at(HDC dc, int x, int y, const char *str)
{
    TextOutA(dc, x, y, str, lstrlenA(str));
}

static void paint_board(HWND hwnd, HDC dc)
{
    RECT rect;
    HBRUSH felt;
    HFONT font, oldfont;
    char line[160];
    int cx, cy, w, h, i, p, x, y, step, origin;
    GetClientRect(hwnd, &rect);
    w = rect.right; h = rect.bottom;
    felt = CreateSolidBrush(RGB(10, 104, 69));
    if (felt) { FillRect(dc, &rect, felt); DeleteObject(felt); }
    font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    oldfont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(247, 247, 226));

    wsprintfA(line, "Hearts  |  Hand %u  |  Pass %s", game.round + 1, passes[game.round % 4]);
    text_at(dc, 18, 54, line);
    for (p = 0; p < 4; ++p)
    {
        wsprintfA(line, "%s: %u  (this hand: %u)", names[p], game.score[p], game.points[p]);
        text_at(dc, 18 + (p % 2) * 235, 78 + (p / 2) * 20, line);
    }
    if (game.phase == HEARTS_PASS)
    {
        wsprintfA(line, "Select three cards, then pass them %s. Selected: %d/3.",
                  passes[game.round % 4], selected_count);
    }
    else if (game.phase == HEARTS_PLAY)
        wsprintfA(line, "%s's turn. Trick %u/13. Hearts %s.",
                  names[game.turn], game.completed + 1, game.broken ? "broken" : "unbroken");
    else if (game.phase == HEARTS_HAND_DONE)
        wsprintfA(line, "Hand complete. Choose Next Hand to continue.");
    else
        wsprintfA(line, "Game over! Winner: %s (lowest score). Choose New Game.",
                  names[hearts_winner(&game)]);
    text_at(dc, 18, 127, line);
    if (game.last_winner >= 0)
    {
        wsprintfA(line, "Last trick: %s took %d penalty point(s).",
                  names[game.last_winner], game.last_points);
        text_at(dc, 18, 149, line);
    }

    cx = w / 2;
    cy = h / 2 - 16;
    for (p = 1; p < 4; ++p)
    {
        static const int dx[] = {0, -175, -37, 100};
        static const int dy[] = {0, -30, -140, -30};
        wsprintfA(line, "%s (%u cards)", names[p], game.count[p]);
        text_at(dc, cx + dx[p], cy + dy[p] - 20, line);
    }
    if (game.phase == HEARTS_PLAY || game.phase == HEARTS_HAND_DONE || game.phase == HEARTS_GAME_DONE)
    {
        static const int dx[] = {-37, -175, -37, 100};
        static const int dy[] = {66, -16, -120, -16};
        for (p = 0; p < 4; ++p)
            if (game.trick[p] != 255)
                draw_card(dc, cx + dx[p], cy + dy[p], game.trick[p], CARD_W, CARD_H, 0);
    }

    step = hand_step(w);
    origin = hand_origin(w);
    y = h - CARD_H - 24;
    for (i = 0; i < (int)game.count[0]; ++i)
    {
        int card = game.hand[0][i];
        x = origin + i * step;
        draw_card(dc, x, y - (selected[card] ? 18 : 0), card, CARD_W, CARD_H, selected[card]);
    }
    text_at(dc, 18, h - CARD_H - 43, "Your cards (click a card to select or play):");
    SelectObject(dc, oldfont);
}

static void sync_controls(void)
{
    ShowWindow(pass_button, game.phase == HEARTS_PASS ? SW_SHOW : SW_HIDE);
    EnableWindow(pass_button, selected_count == 3);
    ShowWindow(next_button, game.phase == HEARTS_HAND_DONE ? SW_SHOW : SW_HIDE);
    InvalidateRect(main_window, NULL, TRUE);
}

static void run_opponents(void)
{
    unsigned guard = 0;
    while (game.phase == HEARTS_PLAY && game.turn != 0 && ++guard <= 52)
    {
        int card = hearts_ai_choose(&game, game.turn);
        if (card < 0 || !hearts_play(&game, card)) break;
    }
}

static void new_game(void)
{
    memset(selected, 0, sizeof(selected));
    selected_count = 0;
    hearts_new(&game, GetTickCount());
    run_opponents();
    sync_controls();
}

static void pass_cards(void)
{
    uint8_t to_pass[3];
    int card, count = 0;
    if (game.phase != HEARTS_PASS || selected_count != 3) return;
    for (card = 0; card < 52; ++card)
        if (selected[card] && count < 3) to_pass[count++] = card;
    if (count != 3 || !hearts_pass(&game, to_pass)) return;
    memset(selected, 0, sizeof(selected));
    selected_count = 0;
    run_opponents();
    sync_controls();
}

static void choose_card(HWND hwnd, int mouse_x, int mouse_y)
{
    RECT rect;
    int width, y, start, step, i;
    GetClientRect(hwnd, &rect);
    width = rect.right;
    y = rect.bottom - CARD_H - 24;
    step = hand_step(width);
    start = hand_origin(width);
    if (game.phase != HEARTS_PASS &&
        (game.phase != HEARTS_PLAY || game.turn != 0)) return;
    for (i = (int)game.count[0] - 1; i >= 0; --i)
    {
        int card = game.hand[0][i];
        int x = start + i * step, top = y - (selected[card] ? 18 : 0);
        if (mouse_x < x || mouse_x >= x + CARD_W || mouse_y < top || mouse_y >= top + CARD_H)
            continue;
        if (game.phase == HEARTS_PASS)
        {
            if (selected[card]) { selected[card] = 0; --selected_count; }
            else if (selected_count < 3) { selected[card] = 1; ++selected_count; }
        }
        else if (hearts_play(&game, card)) run_opponents();
        sync_controls();
        return;
    }
}

static void init_cards(void)
{
    int width, height;
    cards_module = LoadLibraryA("cards.dll");
    if (!cards_module) return;
    cards_init = (void *)GetProcAddress(cards_module, "cdtInit");
    cards_draw_ext = (void *)GetProcAddress(cards_module, "cdtDrawExt");
    cards_term = (void *)GetProcAddress(cards_module, "cdtTerm");
    if (cards_init && cards_draw_ext && cards_term)
        cards_ready = cards_init(&width, &height) && width > 0 && height > 0;
}

static LRESULT CALLBACK hearts_window(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
        main_window = hwnd;
        CreateWindowA("BUTTON", "New Game", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      10, 10, 95, 30, hwnd, (HMENU)(UINT_PTR)ID_NEW, NULL, NULL);
        pass_button = CreateWindowA("BUTTON", "Pass 3 Cards",
                                    WS_CHILD | BS_PUSHBUTTON, 118, 10, 110, 30,
                                    hwnd, (HMENU)(UINT_PTR)ID_PASS, NULL, NULL);
        next_button = CreateWindowA("BUTTON", "Next Hand",
                                    WS_CHILD | BS_PUSHBUTTON, 118, 10, 110, 30,
                                    hwnd, (HMENU)(UINT_PTR)ID_NEXT, NULL, NULL);
        CreateWindowA("BUTTON", "Rules", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      240, 10, 90, 30, hwnd, (HMENU)(UINT_PTR)ID_RULES, NULL, NULL);
        init_cards();
        new_game();
        return 0;
    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_LBUTTONUP:
        choose_card(hwnd, (short)LOWORD(lparam), (short)HIWORD(lparam));
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case ID_NEW: new_game(); return 0;
        case ID_PASS: pass_cards(); return 0;
        case ID_NEXT:
            if (game.phase == HEARTS_HAND_DONE)
            {
                hearts_next(&game);
                run_opponents();
                sync_controls();
            }
            return 0;
        case ID_RULES:
            MessageBoxA(hwnd,
                        "Four players receive 13 cards each. Pass three cards left, "
                        "right, across, then keep your cards on every fourth hand.\n\n"
                        "The two of clubs begins the first trick. Follow the led suit "
                        "when possible. Hearts cannot be led until broken, unless "
                        "your hand contains only hearts. Avoid winning hearts "
                        "(1 point each) and the queen of spades (13 points).\n\n"
                        "Taking all 26 penalty points is 'shooting the moon': "
                        "your opponents each receive 26 instead. When a score "
                        "reaches 100, the lowest total wins.\n\n"
                        "This Water clone supports computer opponents; "
                        "historical LAN multiplayer is not yet implemented.",
                        "Hearts Rules", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            paint_board(hwnd, dc);
            EndPaint(hwnd, &paint);
            return 0;
        }
    case WM_DESTROY:
        if (cards_ready && cards_term) cards_term();
        if (cards_module) FreeLibrary(cards_module);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE previous, LPSTR cmdline, int show)
{
    WNDCLASSEXA cls = {0};
    MSG msg;
    HWND hwnd;
    (void)previous;
    (void)cmdline;
    cls.cbSize = sizeof(cls);
    cls.style = CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = hearts_window;
    cls.hInstance = inst;
    cls.hCursor = LoadCursorA(NULL, (const char *)IDC_ARROW);
    cls.hIcon = LoadIconA(NULL, (const char *)IDI_APPLICATION);
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    cls.lpszClassName = "WaterMSHearts";
    if (!RegisterClassExA(&cls)) return 1;
    hwnd = CreateWindowExA(0, cls.lpszClassName, "Hearts - Water",
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                           CW_USEDEFAULT, CW_USEDEFAULT, 940, 690,
                           NULL, NULL, inst, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    while (GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
