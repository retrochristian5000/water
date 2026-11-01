/*
 * MSCONF.DLL export tests
 */

#include "windows.h"
#include "msconf.h"
#include "wine/test.h"

C_ASSERT(CONF_VERSION == 0x0002);
C_ASSERT(CONFERR_SUCCESS == 0);
C_ASSERT(offsetof(CONFADDRA, dwIp) == offsetof(CONFADDRA, psz));
C_ASSERT(offsetof(CONFADDRW, dwIp) == offsetof(CONFADDRW, psz));
#ifdef UNICODE
C_ASSERT(sizeof(CONFADDR) == sizeof(CONFADDRW));
C_ASSERT(sizeof(CONFINFO) == sizeof(CONFINFOW));
#else
C_ASSERT(sizeof(CONFADDR) == sizeof(CONFADDRA));
C_ASSERT(sizeof(CONFINFO) == sizeof(CONFINFOA));
#endif


/*
 * Invalid-argument probes must never start a conference or open network
 * listeners, including on native Windows systems with NetMeeting installed.
 * The existence of an exported symbol does not establish H.323 support.
 */
static void test_reject_invalid_calls(HMODULE module)
{
    DWORD (WINAPI *connect_a)(HCONF *, LPCONFADDRA, LPCONFINFOA, LPCONFNOTIFY);
    DWORD (WINAPI *connect_w)(HCONF *, LPCONFADDRW, LPCONFINFOW, LPCONFNOTIFY);
    DWORD (WINAPI *disconnect)(HCONF);
    DWORD (WINAPI *share)(HCONF, HWND, DWORD);
    HCONF conf;
    DWORD err;

    connect_a = (void *)GetProcAddress(module, "ConferenceConnectA");
    connect_w = (void *)GetProcAddress(module, "ConferenceConnectW");
    disconnect = (void *)GetProcAddress(module, "ConferenceDisconnect");
    share = (void *)GetProcAddress(module, "ConferenceShareWindow");

    if (connect_a)
    {
        conf = NULL;
        err = connect_a(&conf, NULL, NULL, NULL);
        ok(err != CONFERR_SUCCESS, "ConferenceConnectA accepted a null address.\n");
    }
    if (connect_w)
    {
        conf = NULL;
        err = connect_w(&conf, NULL, NULL, NULL);
        ok(err != CONFERR_SUCCESS, "ConferenceConnectW accepted a null address.\n");
    }
    if (disconnect)
    {
        err = disconnect(NULL);
        ok(err != CONFERR_SUCCESS, "ConferenceDisconnect accepted a null handle.\n");
    }
    if (share)
    {
        err = share(NULL, NULL, CONF_SW_SHARE);
        ok(err != CONFERR_SUCCESS, "ConferenceShareWindow accepted a null handle.\n");
    }
}

START_TEST(msconf)
{
    static const char * const exports[] =
    {
        "CallToProtocolHandler",
        "ConferenceCancelTransfer",
        "ConferenceConnectA",
        "ConferenceConnectW",
        "ConferenceDisconnect",
        "ConferenceGetInfoA",
        "ConferenceGetInfoW",
        "ConferenceLaunchRemote",
        "ConferenceListen",
        "ConferenceRemoveNotify",
        "ConferenceSendData",
        "ConferenceSendFileA",
        "ConferenceSendFileW",
        "ConferenceSetInfoA",
        "ConferenceSetInfoW",
        "ConferenceSetNotify",
        "ConferenceShareWindow",
        "NewMediaPhone",
        "OpenConfLink",
    };
    HMODULE module;
    unsigned int i;

    module = LoadLibraryA("msconf.dll");
    if (!module)
    {
        win_skip("MSCONF.DLL is not available\n");
        return;
    }

    for (i = 0; i < ARRAY_SIZE(exports); ++i)
        ok(GetProcAddress(module, exports[i]) != NULL, "missing export %s\n", exports[i]);

    test_reject_invalid_calls(module);

    FreeLibrary(module);
}
