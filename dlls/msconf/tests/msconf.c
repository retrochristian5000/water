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

    FreeLibrary(module);
}
