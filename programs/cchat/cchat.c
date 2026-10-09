/*
 * Comic Chat (Water): independent Win32 IRC / comic-panel approximation.
 *
 * Copyright 2026 the Water contributors.
 *
 * This program is free software; you may redistribute it and/or
 * modify it under the GNU Lesser General Public License, version 2.1
 * or (at your option) any later version.
 *
 * The historical target is Microsoft Comic Chat 2.1 (1998). This is
 * NOT an AVB decoder or a reproduction of the original panel engine.
 */

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOCKET_MESSAGE (WM_APP + 10)
#define ID_SERVER  101
#define ID_PORT    102
#define ID_NICK    103
#define ID_CHANNEL 104
#define ID_CONNECT 105
#define ID_INPUT   106
#define ID_SEND    107
#define TOP_MARGIN 53
#define BOTTOM_MARGIN 45
#define PANEL_HEIGHT 126
#define MAX_PANELS 96
#define MAX_INPUT 380
#define MAX_IRC_LINE 510

struct irc_line
{
    char prefix[128], command[32], target[128], trailing[512];
};

struct panel
{
    char who[48], body[MAX_INPUT + 1];
    BOOL mine, event, action;
};

static HWND hserver, hport, hnick, hchannel, hconnect, hinput, hsend, hstatus;
static HWND labels[4];
static SOCKET sock = INVALID_SOCKET;
static BOOL connecting, registered;
static char mynick[40], mychannel[100];
static char input_buffer[1024], output_buffer[8192];
static unsigned int input_len, output_len;
static BOOL discard_input;
static struct panel panels[MAX_PANELS];
static unsigned int panel_count;
static int offset_y, viewport_height;
static HFONT bodyfont, headingfont;

static void copy_n(char *dst, size_t size, const char *src, size_t n)
{
    if (!size) return;
    if (n >= size) n = size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void copy_text(char *dst, size_t size, const char *src)
{
    copy_n(dst, size, src, strlen(src));
}

static BOOL valid_nickname(const char *nick)
{
    const unsigned char *p = (const unsigned char *)nick;
    if (!*p || strlen(nick) >= sizeof(mynick) || (*p >= '0' && *p <= '9'))
        return FALSE;
    while (*p)
    {
        if (*p <= ' ' || *p >= 127 || strchr(",!@*?:#", *p)) return FALSE;
        p++;
    }
    return TRUE;
}

static BOOL valid_channel(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;
    if ((*p != '#' && *p != '&') || !p[1] || strlen(name) >= sizeof(mychannel))
        return FALSE;
    for (; *p; p++)
        if (*p <= ' ' || *p == ',' || *p == ':' || *p == 127) return FALSE;
    return TRUE;
}

/* Kept independent from sockets so --self-test exercises the IRC parser. */
static BOOL parse_irc(const char *line, struct irc_line *out)
{
    const char *p = line, *begin;
    size_t len;

    memset(out, 0, sizeof(*out));
    while (*p == ' ') p++;
    if (*p == ':')
    {
        begin = ++p;
        while (*p && *p != ' ') p++;
        len = p - begin;
        if (!len || len >= sizeof(out->prefix) || !*p) return FALSE;
        copy_n(out->prefix, sizeof(out->prefix), begin, len);
        while (*p == ' ') p++;
    }
    begin = p;
    while (*p && *p != ' ') p++;
    len = p - begin;
    if (!len || len >= sizeof(out->command)) return FALSE;
    copy_n(out->command, sizeof(out->command), begin, len);
    while (*p == ' ') p++;
    if (*p && *p != ':')
    {
        begin = p;
        while (*p && *p != ' ') p++;
        len = p - begin;
        if (len >= sizeof(out->target)) return FALSE;
        copy_n(out->target, sizeof(out->target), begin, len);
        while (*p == ' ') p++;
    }
    if (*p == ':') p++;
    copy_text(out->trailing, sizeof(out->trailing), p);
    return TRUE;
}

static void layout_scroll(HWND hwnd, BOOL to_bottom)
{
    RECT rect;
    SCROLLINFO si;
    int end;

    GetClientRect(hwnd, &rect);
    viewport_height = rect.bottom - TOP_MARGIN - BOTTOM_MARGIN;
    if (viewport_height < 1) viewport_height = 1;
    end = panel_count * PANEL_HEIGHT - viewport_height;
    if (end < 0) end = 0;
    if (to_bottom || offset_y > end) offset_y = end;
    if (offset_y < 0) offset_y = 0;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = panel_count * PANEL_HEIGHT ? panel_count * PANEL_HEIGHT - 1 : 0;
    si.nPage = viewport_height;
    si.nPos = offset_y;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void append_panel(HWND hwnd, const char *sender, const char *message,
                         BOOL mine, BOOL is_event, BOOL action)
{
    struct panel *p;
    BOOL follow = offset_y + viewport_height >= (int)panel_count * PANEL_HEIGHT - 16;

    if (panel_count >= MAX_PANELS)
    {
        memmove(panels, panels + 1, sizeof(panels) - sizeof(panels[0]));
        panel_count--;
        if (offset_y >= PANEL_HEIGHT) offset_y -= PANEL_HEIGHT;
    }
    p = &panels[panel_count++];
    copy_text(p->who, sizeof(p->who), sender);
    copy_text(p->body, sizeof(p->body), message);
    p->mine = mine;
    p->event = is_event;
    p->action = action;
    layout_scroll(hwnd, follow || panel_count == 1);
}

static void note(HWND hwnd, const char *message)
{
    append_panel(hwnd, "System", message, FALSE, TRUE, FALSE);
}

static unsigned int hash_name(const char *name)
{
    unsigned int n = 5381;
    while (*name) n = (n * 33) ^ (unsigned char)*name++;
    return n;
}

static void draw_character(HDC dc, const struct panel *p, int x, int y)
{
    unsigned int h = hash_name(p->who);
    HBRUSH oldbrush, brush;
    HPEN oldpen, pen;

    pen = CreatePen(PS_SOLID, 2, RGB(40, 42, 47));
    oldpen = SelectObject(dc, pen);
    brush = CreateSolidBrush(RGB(50 + (h & 90), 80 + ((h >> 9) & 80),
                                110 + ((h >> 17) & 90)));
    oldbrush = SelectObject(dc, brush);
    RoundRect(dc, x + 1, y + 63, x + 72, y + 105, 25, 25);
    SelectObject(dc, oldbrush);
    DeleteObject(brush);

    brush = CreateSolidBrush(RGB(37 + (h & 56), 25 + ((h >> 6) & 57),
                                26 + ((h >> 12) & 50)));
    SelectObject(dc, brush);
    Ellipse(dc, x + 10, y + 4, x + 64, y + 76);
    SelectObject(dc, oldbrush);
    DeleteObject(brush);

    brush = CreateSolidBrush(RGB(251, 206 + (h & 16), 172 + ((h >> 7) & 25)));
    SelectObject(dc, brush);
    Ellipse(dc, x + 17, y + 16, x + 59, y + 73);
    SelectObject(dc, oldbrush);
    DeleteObject(brush);

    Ellipse(dc, x + 28, y + 40, x + 32, y + 44);
    Ellipse(dc, x + 46, y + 40, x + 50, y + 44);
    if (p->action) Ellipse(dc, x + 35, y + 54, x + 43, y + 64);
    else
    {
        MoveToEx(dc, x + 32, y + 58, NULL);
        LineTo(dc, x + 47, y + 58);
    }
    SelectObject(dc, oldpen);
    DeleteObject(pen);
}

static void draw_panel(HDC dc, const struct panel *p, int y, int width)
{
    RECT frame = { 8, y + 4, width - 10, y + PANEL_HEIGHT - 5 };
    RECT box, who, speech;
    HPEN border, oldpen;
    HBRUSH paper, fill, oldbrush;
    POINT tail[3];
    COLORREF bg = p->event ? RGB(238, 241, 247) :
                  p->mine ? RGB(227, 245, 229) : RGB(254, 250, 235);

    if (frame.right <= frame.left) return;
    border = CreatePen(PS_SOLID, 2, RGB(42, 43, 48));
    oldpen = SelectObject(dc, border);
    paper = CreateSolidBrush(RGB(255, 255, 255));
    oldbrush = SelectObject(dc, paper);
    Rectangle(dc, frame.left, frame.top, frame.right, frame.bottom);
    SelectObject(dc, oldbrush);
    DeleteObject(paper);

    box = frame;
    box.left += p->event ? 14 : 96;
    box.right -= 16;
    box.top += 12;
    box.bottom -= 12;
    fill = CreateSolidBrush(bg);
    SelectObject(dc, fill);
    RoundRect(dc, box.left, box.top, box.right, box.bottom, 25, 25);
    if (!p->event)
    {
        tail[0].x = box.left + 1; tail[0].y = box.top + 41;
        tail[1].x = box.left - 18; tail[1].y = box.top + 58;
        tail[2].x = box.left + 1; tail[2].y = box.top + 62;
        Polygon(dc, tail, 3);
    }
    SelectObject(dc, oldbrush);
    DeleteObject(fill);
    SelectObject(dc, oldpen);
    DeleteObject(border);

    if (!p->event) draw_character(dc, p, frame.left + 12, frame.top);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(35, 36, 55));
    SelectObject(dc, headingfont);
    who = box;
    who.left += 14;
    who.top += 7;
    who.bottom = who.top + 20;
    DrawTextA(dc, p->who, -1, &who, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dc, bodyfont);
    SetTextColor(dc, RGB(20, 23, 25));
    speech = box;
    speech.left += 13; speech.right -= 14;
    speech.top += 31; speech.bottom -= 8;
    DrawTextA(dc, p->body, -1, &speech, DT_WORDBREAK | DT_NOPREFIX | DT_TOP);
}

static void draw_comic(HWND hwnd)
{
    PAINTSTRUCT paint;
    HDC dc;
    RECT client, clip;
    HBRUSH paper;
    unsigned int i;

    dc = BeginPaint(hwnd, &paint);
    GetClientRect(hwnd, &client);
    clip = client;
    clip.top = TOP_MARGIN;
    clip.bottom -= BOTTOM_MARGIN;
    if (clip.bottom < clip.top) clip.bottom = clip.top;
    IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom);
    paper = CreateSolidBrush(RGB(229, 225, 216));
    FillRect(dc, &clip, paper);
    DeleteObject(paper);
    for (i = 0; i < panel_count; i++)
    {
        int y = TOP_MARGIN + i * PANEL_HEIGHT - offset_y;
        if (y + PANEL_HEIGHT < clip.top || y > clip.bottom) continue;
        draw_panel(dc, &panels[i], y, client.right);
    }
    EndPaint(hwnd, &paint);
}

static void set_status(HWND hwnd, BOOL is_connecting, BOOL is_registered, const char *message)
{
    connecting = is_connecting;
    registered = is_registered;
    if (hconnect) SetWindowTextA(hconnect, connecting ? "Disconnect" : "Connect");
    if (hsend) EnableWindow(hsend, registered);
    if (hstatus)
        SetWindowTextA(hstatus, registered ? "Connected: unencrypted IRC" :
                       connecting ? "Connecting..." : "Offline: plaintext IRC only");
    if (message) note(hwnd, message);
}

static void disconnect_chat(HWND hwnd, const char *message)
{
    if (sock != INVALID_SOCKET)
    {
        WSAAsyncSelect(sock, hwnd, 0, 0);
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    input_len = output_len = 0;
    discard_input = FALSE;
    set_status(hwnd, FALSE, FALSE, message);
}

/* A bounded queue handles asynchronous connect and partial/nonblocking sends. */
static BOOL flush_output(void)
{
    while (sock != INVALID_SOCKET && output_len)
    {
        int sent = send(sock, output_buffer, output_len, 0);
        if (sent == SOCKET_ERROR)
            return WSAGetLastError() == WSAEWOULDBLOCK;
        if (!sent) return FALSE;
        output_len -= sent;
        if (output_len) memmove(output_buffer, output_buffer + sent, output_len);
    }
    return TRUE;
}

static BOOL queue_wire(const char *command)
{
    size_t len = strlen(command);
    if (sock == INVALID_SOCKET || len > MAX_IRC_LINE ||
        output_len + len + 2 > sizeof(output_buffer) ||
        strchr(command, '\r') || strchr(command, '\n'))
        return FALSE;
    memcpy(output_buffer + output_len, command, len);
    output_len += len;
    output_buffer[output_len++] = '\r';
    output_buffer[output_len++] = '\n';
    return flush_output();
}

static BOOL queue_one_arg(const char *verb, const char *arg)
{
    char line[600];
    int len = snprintf(line, sizeof(line), "%s %s", verb, arg);
    return len > 0 && len <= MAX_IRC_LINE && queue_wire(line);
}

static BOOL queue_message(const char *target, const char *body, BOOL action)
{
    char line[600];
    int n;
    if (action)
        n = snprintf(line, sizeof(line), "PRIVMSG %s :\001ACTION %s\001", target, body);
    else
        n = snprintf(line, sizeof(line), "PRIVMSG %s :%s", target, body);
    return n >= 0 && n <= MAX_IRC_LINE && queue_wire(line);
}

static void nick_from_prefix(char *dst, size_t size, const char *prefix)
{
    const char *end = strchr(prefix, '!');
    if (!end) end = strchr(prefix, '@');
    if (!end) end = prefix + strlen(prefix);
    copy_n(dst, size, prefix, end - prefix);
}

static void receive_line(HWND hwnd, const char *text)
{
    struct irc_line msg;
    char who[48], body[512], *end;
    BOOL action = FALSE;

    if (!parse_irc(text, &msg)) return;
    nick_from_prefix(who, sizeof(who), msg.prefix);
    if (!lstrcmpiA(msg.command, "PING"))
    {
        queue_one_arg("PONG", msg.trailing[0] ? msg.trailing : msg.target);
        return;
    }
    if (!lstrcmpiA(msg.command, "001"))
    {
        set_status(hwnd, TRUE, TRUE, "Registered with IRC. Joining the channel...");
        queue_one_arg("JOIN", mychannel);
        return;
    }
    if (!lstrcmpiA(msg.command, "433"))
    {
        disconnect_chat(hwnd, "Nickname is already in use.");
        return;
    }
    if (!lstrcmpiA(msg.command, "PRIVMSG") || !lstrcmpiA(msg.command, "NOTICE"))
    {
        if (!*who) copy_text(who, sizeof(who), "Server");
        copy_text(body, sizeof(body), msg.trailing);
        if (body[0] == '\001' && !strncmp(body + 1, "ACTION ", 7))
        {
            memmove(body, body + 8, strlen(body + 8) + 1);
            end = strrchr(body, '\001');
            if (end) *end = '\0';
            action = TRUE;
        }
        if (lstrcmpiA(msg.target, mychannel))
        {
            char private_nick[65];
            snprintf(private_nick, sizeof(private_nick), "%s (private)", who);
            append_panel(hwnd, private_nick, body, FALSE, FALSE, action);
        }
        else
            append_panel(hwnd, who, body, !lstrcmpiA(who, mynick), FALSE, action);
    }
    else if (!lstrcmpiA(msg.command, "JOIN"))
    {
        snprintf(body, sizeof(body), "%s joined %s", who,
                 *msg.trailing ? msg.trailing : msg.target);
        note(hwnd, body);
    }
    else if (!lstrcmpiA(msg.command, "PART") || !lstrcmpiA(msg.command, "QUIT"))
    {
        snprintf(body, sizeof(body), "%s left the chat", who);
        note(hwnd, body);
    }
    else if (!lstrcmpiA(msg.command, "NICK"))
    {
        const char *newnick = *msg.trailing ? msg.trailing : msg.target;
        snprintf(body, sizeof(body), "%s is now %s", who, newnick);
        if (!lstrcmpiA(who, mynick)) copy_text(mynick, sizeof(mynick), newnick);
        note(hwnd, body);
    }
    else if ((msg.command[0] == '4' || msg.command[0] == '5') &&
             msg.command[1] >= '0' && msg.command[1] <= '9' &&
             msg.command[2] >= '0' && msg.command[2] <= '9' && !msg.command[3])
    {
        if (*msg.trailing) note(hwnd, msg.trailing);
    }
}

static void receive_socket(HWND hwnd)
{
    char block[1024];
    int n, i;

    for (;;)
    {
        n = recv(sock, block, sizeof(block), 0);
        if (n == SOCKET_ERROR)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                disconnect_chat(hwnd, "IRC receive error.");
            return;
        }
        if (!n)
        {
            disconnect_chat(hwnd, "Server closed the connection.");
            return;
        }
        for (i = 0; i < n; i++)
        {
            unsigned char ch = (unsigned char)block[i];
            if (ch == '\n')
            {
                if (!discard_input)
                {
                    if (input_len && input_buffer[input_len - 1] == '\r')
                        input_len--;
                    input_buffer[input_len] = '\0';
                    receive_line(hwnd, input_buffer);
                }
                input_len = 0;
                discard_input = FALSE;
            }
            else if (!discard_input)
            {
                if (input_len + 1 < sizeof(input_buffer))
                    input_buffer[input_len++] = ch;
                else
                {
                    input_len = 0;
                    discard_input = TRUE;
                    note(hwnd, "Oversized IRC line discarded.");
                }
            }
            if (sock == INVALID_SOCKET) return;
        }
    }
}

static void connect_chat(HWND hwnd)
{
    char server[160], port_text[20], wanted_nick[40], wanted_channel[100], text[240];
    struct hostent *host;
    struct sockaddr_in address;
    unsigned long port;
    char *end;
    int rc;

    if (connecting)
    {
        disconnect_chat(hwnd, "Disconnected.");
        return;
    }
    GetWindowTextA(hserver, server, sizeof(server));
    GetWindowTextA(hport, port_text, sizeof(port_text));
    GetWindowTextA(hnick, wanted_nick, sizeof(wanted_nick));
    GetWindowTextA(hchannel, wanted_channel, sizeof(wanted_channel));
    if (!server[0] || strpbrk(server, " \t\r\n") ||
        !valid_nickname(wanted_nick) || !valid_channel(wanted_channel))
    {
        note(hwnd, "Please enter a valid server, nickname and #channel.");
        return;
    }
    port = strtoul(port_text, &end, 10);
    if (!port || port > 65535 || *end)
    {
        note(hwnd, "TCP port must be between 1 and 65535.");
        return;
    }
    host = gethostbyname(server);
    if (!host || host->h_addrtype != AF_INET || !host->h_addr_list ||
        !host->h_addr_list[0])
    {
        note(hwnd, "Could not resolve IRC server (IPv4 only).");
        return;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    memcpy(&address.sin_addr, host->h_addr_list[0], sizeof(address.sin_addr));
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        note(hwnd, "Could not create a TCP socket.");
        return;
    }
    copy_text(mynick, sizeof(mynick), wanted_nick);
    copy_text(mychannel, sizeof(mychannel), wanted_channel);
    if (WSAAsyncSelect(sock, hwnd, SOCKET_MESSAGE,
                       FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
    {
        disconnect_chat(hwnd, "Could not enable asynchronous socket events.");
        return;
    }
    rc = connect(sock, (struct sockaddr *)&address, sizeof(address));
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        disconnect_chat(hwnd, "Could not connect to IRC server.");
        return;
    }
    snprintf(text, sizeof(text), "Connecting to %s:%lu (plaintext IRC).", server, port);
    set_status(hwnd, TRUE, FALSE, text);
}

static void handle_socket(HWND hwnd, SOCKET source, LPARAM data)
{
    int event = WSAGETSELECTEVENT(data);
    int error = WSAGETSELECTERROR(data);
    char command[160];

    if (source != sock || source == INVALID_SOCKET) return;
    if (error)
    {
        disconnect_chat(hwnd, "IRC network error.");
        return;
    }
    switch (event)
    {
    case FD_CONNECT:
        snprintf(command, sizeof(command), "NICK %s", mynick);
        if (!queue_wire(command))
        {
            disconnect_chat(hwnd, "Could not send IRC nickname.");
            return;
        }
        snprintf(command, sizeof(command), "USER %s 0 * :Water Comic Chat", mynick);
        if (!queue_wire(command))
            disconnect_chat(hwnd, "Could not send IRC registration.");
        break;
    case FD_READ:
        receive_socket(hwnd);
        break;
    case FD_WRITE:
        if (!flush_output()) disconnect_chat(hwnd, "IRC send error.");
        break;
    case FD_CLOSE:
        disconnect_chat(hwnd, "IRC socket closed.");
        break;
    }
}

static void clean_user_text(char *p)
{
    for (; *p; p++)
        if ((unsigned char)*p < 32 || (unsigned char)*p == 127) *p = ' ';
}

static void send_input(HWND hwnd)
{
    char input[MAX_INPUT + 1], arg[MAX_INPUT + 1];
    if (!registered)
    {
        note(hwnd, "Connect and register before sending IRC messages.");
        return;
    }
    GetWindowTextA(hinput, input, sizeof(input));
    clean_user_text(input);
    if (!*input) return;

    if (!strncmp(input, "/join ", 6))
    {
        copy_text(arg, sizeof(arg), input + 6);
        if (!valid_channel(arg)) note(hwnd, "Invalid IRC channel.");
        else if (queue_one_arg("JOIN", arg))
        {
            copy_text(mychannel, sizeof(mychannel), arg);
            SetWindowTextA(hchannel, arg);
        }
        else note(hwnd, "Could not queue JOIN.");
    }
    else if (!strncmp(input, "/nick ", 6))
    {
        copy_text(arg, sizeof(arg), input + 6);
        if (!valid_nickname(arg)) note(hwnd, "Invalid IRC nickname.");
        else if (!queue_one_arg("NICK", arg))
            note(hwnd, "Could not queue NICK.");
    }
    else if (!strncmp(input, "/me ", 4))
    {
        if (queue_message(mychannel, input + 4, TRUE))
            append_panel(hwnd, mynick, input + 4, TRUE, FALSE, TRUE);
        else note(hwnd, "Could not queue action.");
    }
    else if (!lstrcmpiA(input, "/quit"))
    {
        queue_wire("QUIT :Leaving");
        disconnect_chat(hwnd, "Disconnected.");
    }
    else if (input[0] == '/')
        note(hwnd, "Commands: /join #channel, /nick name, /me action, /quit.");
    else if (queue_message(mychannel, input, FALSE))
        append_panel(hwnd, mynick, input, TRUE, FALSE, FALSE);
    else note(hwnd, "Could not queue message.");

    SetWindowTextA(hinput, "");
    SetFocus(hinput);
}

static HWND child(HWND parent, const char *cls, const char *name, DWORD styles, int id)
{
    HWND h = CreateWindowExA(0, cls, name, WS_CHILD | WS_VISIBLE | styles,
                             0, 0, 20, 20, parent, (HMENU)(ULONG_PTR)id,
                             (HINSTANCE)GetWindowLongPtrA(parent, GWLP_HINSTANCE), NULL);
    if (h && bodyfont) SendMessageA(h, WM_SETFONT, (WPARAM)bodyfont, TRUE);
    return h;
}

static void arrange(HWND hwnd)
{
    RECT r;
    int w, h;

    GetClientRect(hwnd, &r);
    w = r.right - r.left; h = r.bottom - r.top;
    MoveWindow(labels[0], 7, 10, 43, 18, TRUE);
    MoveWindow(hserver, 51, 6, 143, 23, TRUE);
    MoveWindow(labels[1], 198, 10, 30, 18, TRUE);
    MoveWindow(hport, 230, 6, 50, 23, TRUE);
    MoveWindow(labels[2], 284, 10, 32, 18, TRUE);
    MoveWindow(hnick, 316, 6, 94, 23, TRUE);
    MoveWindow(labels[3], 416, 10, 51, 18, TRUE);
    MoveWindow(hchannel, 467, 6, 112, 23, TRUE);
    MoveWindow(hconnect, w - 105, 6, 94, 24, TRUE);
    MoveWindow(hstatus, 12, 32, w - 24, 18, TRUE);
    MoveWindow(hinput, 12, h - 37, w - 113, 26, TRUE);
    MoveWindow(hsend, w - 90, h - 37, 79, 26, TRUE);
    layout_scroll(hwnd, FALSE);
}

/* Enter sends the message without requiring any dialog manager. */
static LRESULT CALLBACK input_window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    WNDPROC oldproc = (WNDPROC)(ULONG_PTR)GetPropA(hwnd, "WaterCChatOldProc");
    if (msg == WM_KEYDOWN && wp == VK_RETURN)
    {
        SendMessageA(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(ID_SEND, BN_CLICKED), 0);
        return 0;
    }
    return CallWindowProcA(oldproc, hwnd, msg, wp, lp);
}

static LRESULT CALLBACK main_window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        WNDPROC oldproc;
        bodyfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        headingfont = CreateFontA(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, "Arial");
        if (!headingfont) headingfont = bodyfont;
        labels[0] = child(hwnd, "STATIC", "Server", 0, 0);
        labels[1] = child(hwnd, "STATIC", "Port", 0, 0);
        labels[2] = child(hwnd, "STATIC", "Nick", 0, 0);
        labels[3] = child(hwnd, "STATIC", "Channel", 0, 0);
        hserver = child(hwnd, "EDIT", "irc.libera.chat", WS_BORDER | ES_AUTOHSCROLL, ID_SERVER);
        hport = child(hwnd, "EDIT", "6667", WS_BORDER | ES_AUTOHSCROLL, ID_PORT);
        hnick = child(hwnd, "EDIT", "WaterGuest", WS_BORDER | ES_AUTOHSCROLL, ID_NICK);
        hchannel = child(hwnd, "EDIT", "#comicchat", WS_BORDER | ES_AUTOHSCROLL, ID_CHANNEL);
        hconnect = child(hwnd, "BUTTON", "Connect", BS_PUSHBUTTON, ID_CONNECT);
        hstatus = child(hwnd, "STATIC", "Offline: plaintext IRC only", 0, 0);
        hinput = child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, ID_INPUT);
        hsend = child(hwnd, "BUTTON", "Send", BS_PUSHBUTTON, ID_SEND);
        EnableWindow(hsend, FALSE);
        oldproc = (WNDPROC)(ULONG_PTR)SetWindowLongPtrA(hinput, GWLP_WNDPROC,
                                                       (LONG_PTR)input_window_proc);
        SetPropA(hinput, "WaterCChatOldProc", (HANDLE)oldproc);
        note(hwnd, "Water Comic Chat: choose a server and channel to start.");
        note(hwnd, "Original .AVB character art is not decoded in this first version.");
        return 0;
    }
    case WM_SIZE:
        if (hinput) arrange(hwnd);
        return 0;
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED && LOWORD(wp) == ID_CONNECT)
            connect_chat(hwnd);
        if (HIWORD(wp) == BN_CLICKED && LOWORD(wp) == ID_SEND)
            send_input(hwnd);
        return 0;
    case WM_VSCROLL:
    {
        SCROLLINFO si;
        int end = (int)panel_count * PANEL_HEIGHT - viewport_height;
        if (end < 0) end = 0;
        memset(&si, 0, sizeof(si));
        si.cbSize = sizeof(si);
        si.fMask = SIF_TRACKPOS;
        GetScrollInfo(hwnd, SB_VERT, &si);
        switch (LOWORD(wp))
        {
        case SB_LINEUP: offset_y -= 36; break;
        case SB_LINEDOWN: offset_y += 36; break;
        case SB_PAGEUP: offset_y -= viewport_height; break;
        case SB_PAGEDOWN: offset_y += viewport_height; break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK: offset_y = si.nTrackPos; break;
        case SB_TOP: offset_y = 0; break;
        case SB_BOTTOM: offset_y = end; break;
        }
        layout_scroll(hwnd, FALSE);
        return 0;
    }
    case WM_MOUSEWHEEL:
        offset_y -= (short)HIWORD(wp) / WHEEL_DELTA * 48;
        layout_scroll(hwnd, FALSE);
        return 0;
    case SOCKET_MESSAGE:
        handle_socket(hwnd, (SOCKET)wp, lp);
        return 0;
    case WM_PAINT:
        draw_comic(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        if (sock != INVALID_SOCKET) disconnect_chat(hwnd, NULL);
        if (hinput)
        {
            WNDPROC oldproc = (WNDPROC)(ULONG_PTR)RemovePropA(hinput, "WaterCChatOldProc");
            if (oldproc) SetWindowLongPtrA(hinput, GWLP_WNDPROC, (LONG_PTR)oldproc);
        }
        if (headingfont != bodyfont) DeleteObject(headingfont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static int parser_self_test(void)
{
    struct irc_line m;
    if (!parse_irc(":dan!u@server PRIVMSG #water :Hello there", &m) ||
        strcmp(m.prefix, "dan!u@server") || strcmp(m.command, "PRIVMSG") ||
        strcmp(m.target, "#water") || strcmp(m.trailing, "Hello there"))
        return 1;
    if (!parse_irc("PING :example.org", &m) || strcmp(m.command, "PING") ||
        strcmp(m.trailing, "example.org")) return 2;
    if (parse_irc(":badprefix", &m)) return 3;
    if (!valid_channel("#channel") || valid_channel("channel") ||
        !valid_nickname("Dan98") || valid_nickname("invalid nick"))
        return 4;
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR command, int show)
{
    WNDCLASSA cls;
    WSADATA wsa;
    HWND hwnd;
    MSG msg;
    int result;
    (void)prev;

    if (command && !strcmp(command, "--self-test"))
        return parser_self_test();
    if (WSAStartup(MAKEWORD(2, 0), &wsa))
    {
        MessageBoxA(NULL, "Winsock 2 is unavailable.", "Comic Chat", MB_ICONERROR);
        return 1;
    }
    memset(&cls, 0, sizeof(cls));
    cls.hInstance = instance;
    cls.lpszClassName = "WaterComicChat";
    cls.lpfnWndProc = main_window_proc;
    cls.hCursor = LoadCursorA(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    cls.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassA(&cls))
    {
        WSACleanup();
        return 1;
    }
    hwnd = CreateWindowExA(0, "WaterComicChat", "Comic Chat (Water)",
                           WS_OVERLAPPEDWINDOW | WS_VSCROLL, CW_USEDEFAULT,
                           CW_USEDEFAULT, 860, 640, NULL, NULL, instance, NULL);
    if (!hwnd)
    {
        WSACleanup();
        return 1;
    }
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    while ((result = GetMessageA(&msg, NULL, 0, 0)) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    WSACleanup();
    return result < 0 ? 1 : msg.wParam;
}
