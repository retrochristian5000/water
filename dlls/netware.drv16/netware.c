/*
 * Win16 NetWare network driver compatibility layer
 *
 * This initial implementation provides the documented Windows WinNet
 * network-driver ABI and NetWare identity without pretending that an
 * IPX/SPX or NCP transport is present.
 */

#include "windef.h"
#include "wine/winnet16.h"

/* WNNC_NET_TYPE values occupy the high byte in the WinNet driver ABI. */
#define NETWARE_WINNET_SPEC_VERSION 0x0300
#define NETWARE_WINNET_TYPE         0x0300

static WORD netware_not_supported(void)
{
    return WN16_NOT_SUPPORTED;
}

WORD WINAPI NETWARE_WNetOpenJob(LPSTR queue, LPSTR title, WORD copies, LPINT16 handle)
{
    (void)queue;
    (void)title;
    (void)copies;
    (void)handle;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetCloseJob(WORD handle, LPINT16 job, LPSTR queue)
{
    (void)handle;
    (void)job;
    (void)queue;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetAbortJob(LPSTR queue, WORD job)
{
    (void)queue;
    (void)job;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetHoldJob(LPSTR queue, WORD job)
{
    (void)queue;
    (void)job;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetReleaseJob(LPSTR queue, WORD job)
{
    (void)queue;
    (void)job;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetCancelJob(LPSTR queue, WORD job)
{
    (void)queue;
    (void)job;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetSetJobCopies(LPSTR queue, WORD job, WORD copies)
{
    (void)queue;
    (void)job;
    (void)copies;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetWatchQueue(HWND16 hwnd, LPSTR local, LPSTR user, WORD queue)
{
    (void)hwnd;
    (void)local;
    (void)user;
    (void)queue;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetUnwatchQueue(LPSTR queue)
{
    (void)queue;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetLockQueueData(LPSTR queue, LPSTR user, LPQUEUESTRUCT16 *data)
{
    (void)queue;
    (void)user;
    (void)data;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetUnlockQueueData(LPSTR queue)
{
    (void)queue;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetGetConnection(LPSTR local, LPSTR remote, UINT16 *size)
{
    (void)local;
    (void)remote;
    (void)size;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetGetCaps(WORD capability)
{
    switch (capability)
    {
    case WNNC16_SPEC_VERSION:
        return NETWARE_WINNET_SPEC_VERSION;
    case WNNC16_NET_TYPE:
        return NETWARE_WINNET_TYPE;
    default:
        return 0;
    }
}

WORD WINAPI NETWARE_WNetDeviceMode(HWND16 hwnd)
{
    (void)hwnd;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetBrowseDialog(HWND16 hwnd, WORD type, LPSTR path)
{
    (void)hwnd;
    (void)type;
    (void)path;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetGetUser(LPSTR user, LPINT16 size)
{
    (void)user;
    (void)size;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetAddConnection(LPCSTR remote, LPCSTR password, LPCSTR local)
{
    (void)remote;
    (void)password;
    (void)local;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetCancelConnection(LPSTR name, BOOL16 force)
{
    (void)name;
    (void)force;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetGetError(LPINT16 error)
{
    (void)error;
    return netware_not_supported();
}

WORD WINAPI NETWARE_WNetGetErrorText(WORD error, LPSTR buffer, LPINT16 size)
{
    (void)error;
    (void)buffer;
    (void)size;
    return netware_not_supported();
}

void WINAPI NETWARE_Enable(void)
{
}

void WINAPI NETWARE_Disable(void)
{
}

INT16 WINAPI NETWARE_WEP(INT16 system_exit)
{
    (void)system_exit;
    return 1;
}
