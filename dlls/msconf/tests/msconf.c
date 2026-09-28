/*
 * MSCONF.DLL export tests
 */

#include "windows.h"
#include "wine/test.h"

START_TEST(msconf)
{
    static const char * const exports[] =
    {
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
