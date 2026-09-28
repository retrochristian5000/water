/*
 * Microsoft NetMeeting conferencing API
 *
 * Water currently exposes the MSCONF.DLL version-2 ABI without pretending
 * that a conferencing transport exists. This is important for old clients
 * which delay-load MSCONF and otherwise mistake a missing DLL for success.
 */

#include "windef.h"
#include "winbase.h"
#include "msconf.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msconf);

static DWORD unsupported(const char *name)
{
    FIXME("%s: conferencing backend is not implemented\n", name);
    return CONFERR_NOT_IMPLEMENTED;
}

static DWORD check_conf(HCONF hconf, const char *name)
{
    if (!hconf)
    {
        WARN("%s: null conference handle\n", name);
        return CONFERR_INVALID_HCONF;
    }
    return unsupported(name);
}

DWORD WINAPI ConferenceConnectA(HCONF *phconf, LPCONFADDRA addr, LPCONFINFOA info,
        LPCONFNOTIFY notify)
{
    TRACE("phconf %p, addr %p, info %p, notify %p\n", phconf, addr, info, notify);

    if (!phconf) return CONFERR_INVALID_PARAMETER;
    *phconf = NULL;
    if (!addr) return CONFERR_INVALID_ADDRESS;
    if (addr->dwSize < sizeof(*addr)) return CONFERR_INVALID_BUFFER;
    if (info && info->dwSize < sizeof(*info)) return CONFERR_INVALID_BUFFER;
    if (notify && notify->dwSize < sizeof(*notify)) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceConnectA");
}

DWORD WINAPI ConferenceConnectW(HCONF *phconf, LPCONFADDRW addr, LPCONFINFOW info,
        LPCONFNOTIFY notify)
{
    TRACE("phconf %p, addr %p, info %p, notify %p\n", phconf, addr, info, notify);

    if (!phconf) return CONFERR_INVALID_PARAMETER;
    *phconf = NULL;
    if (!addr) return CONFERR_INVALID_ADDRESS;
    if (addr->dwSize < sizeof(*addr)) return CONFERR_INVALID_BUFFER;
    if (info && info->dwSize < sizeof(*info)) return CONFERR_INVALID_BUFFER;
    if (notify && notify->dwSize < sizeof(*notify)) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceConnectW");
}

DWORD WINAPI ConferenceListen(DWORD reserved)
{
    TRACE("reserved %#lx\n", reserved);

    if (reserved) return CONFERR_INVALID_PARAMETER;
    return unsupported("ConferenceListen");
}

DWORD WINAPI ConferenceDisconnect(HCONF hconf)
{
    TRACE("hconf %p\n", hconf);
    return check_conf(hconf, "ConferenceDisconnect");
}

DWORD WINAPI ConferenceGetInfoA(HCONF hconf, DWORD code, LPVOID buffer)
{
    TRACE("hconf %p, code %#lx, buffer %p\n", hconf, code, buffer);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!buffer) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceGetInfoA");
}

DWORD WINAPI ConferenceGetInfoW(HCONF hconf, DWORD code, LPVOID buffer)
{
    TRACE("hconf %p, code %#lx, buffer %p\n", hconf, code, buffer);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!buffer) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceGetInfoW");
}

DWORD WINAPI ConferenceSetInfoA(HCONF hconf, DWORD code, LPVOID buffer)
{
    TRACE("hconf %p, code %#lx, buffer %p\n", hconf, code, buffer);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!buffer) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceSetInfoA");
}

DWORD WINAPI ConferenceSetInfoW(HCONF hconf, DWORD code, LPVOID buffer)
{
    TRACE("hconf %p, code %#lx, buffer %p\n", hconf, code, buffer);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!buffer) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceSetInfoW");
}

DWORD WINAPI ConferenceSetNotify(HCONF hconf, LPCONFNOTIFY notify, HCONFNOTIFY *handle)
{
    TRACE("hconf %p, notify %p, handle %p\n", hconf, notify, handle);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!notify || !handle) return CONFERR_INVALID_PARAMETER;
    *handle = NULL;
    if (notify->dwSize < sizeof(*notify)) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceSetNotify");
}

DWORD WINAPI ConferenceRemoveNotify(HCONF hconf, HCONFNOTIFY notify)
{
    TRACE("hconf %p, notify %p\n", hconf, notify);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!notify) return CONFERR_INVALID_PARAMETER;
    return unsupported("ConferenceRemoveNotify");
}

DWORD WINAPI ConferenceSendData(HCONF hconf, LPCONFDEST dest, LPVOID data, DWORD size,
        DWORD flags)
{
    TRACE("hconf %p, dest %p, data %p, size %lu, flags %#lx\n",
            hconf, dest, data, size, flags);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!data && size) return CONFERR_INVALID_BUFFER;
    if (dest && dest->dwSize < sizeof(*dest)) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceSendData");
}

DWORD WINAPI ConferenceSendFileA(HCONF hconf, LPCONFDEST dest, LPCSTR filename, DWORD flags)
{
    TRACE("hconf %p, dest %p, filename %s, flags %#lx\n",
            hconf, dest, debugstr_a(filename), flags);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!filename || !*filename) return CONFERR_INVALID_PARAMETER;
    if (dest && dest->dwSize < sizeof(*dest)) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceSendFileA");
}

DWORD WINAPI ConferenceSendFileW(HCONF hconf, LPCONFDEST dest, LPCWSTR filename, DWORD flags)
{
    TRACE("hconf %p, dest %p, filename %s, flags %#lx\n",
            hconf, dest, debugstr_w(filename), flags);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!filename || !*filename) return CONFERR_INVALID_PARAMETER;
    if (dest && dest->dwSize < sizeof(*dest)) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceSendFileW");
}

DWORD WINAPI ConferenceCancelTransfer(HCONF hconf, DWORD file_id)
{
    TRACE("hconf %p, file_id %lu\n", hconf, file_id);

    if (!hconf) return CONFERR_INVALID_HCONF;
    return unsupported("ConferenceCancelTransfer");
}

DWORD WINAPI ConferenceLaunchRemote(HCONF hconf, LPCONFDEST dest, DWORD reserved)
{
    TRACE("hconf %p, dest %p, reserved %#lx\n", hconf, dest, reserved);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (reserved) return CONFERR_INVALID_PARAMETER;
    if (dest && dest->dwSize < sizeof(*dest)) return CONFERR_INVALID_BUFFER;
    return unsupported("ConferenceLaunchRemote");
}

DWORD WINAPI ConferenceShareWindow(HCONF hconf, HWND hwnd, DWORD code)
{
    TRACE("hconf %p, hwnd %p, code %#lx\n", hconf, hwnd, code);

    if (!hconf) return CONFERR_INVALID_HCONF;
    if (!hwnd) return CONFERR_INVALID_HWND;
    switch (code)
    {
    case CONF_SW_SHARE:
    case CONF_SW_UNSHARE:
    case CONF_SW_SHAREABLE:
    case CONF_SW_IS_SHARED:
        break;
    default:
        return CONFERR_INVALID_PARAMETER;
    }
    return unsupported("ConferenceShareWindow");
}
