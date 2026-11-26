/*
 * NSC metadata reader tests.
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 or any later version.
 */
#include "wine/test.h"
#include "../nsc.h"

#define FORMAT "02fG0000S0001GC2QoTOvcpn6csG2g06BER00000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
#define ADDRESS "[Address]\r\nIP Address=239.1.2.3\r\nIP Port=0x00001234\r\n"

static void test_encoding(void)
{
    unsigned char *buf;
    uint32_t key;
    size_t size;
    char text[16];
    enum nsc_result ret;

    ret = nsc_text("029G0000000008Cm0k0300000", text, sizeof(text));
    ok(ret == NSC_OK && !strcmp(text, "3.0"), "Version decoding failed: %d\n", ret);
    ret = nsc_decode(FORMAT, &buf, &key, &size);
    ok(ret == NSC_OK && key == 7 && size == 80, "Format envelope failed: %d\n", ret);
    free(buf);
    ret = nsc_decode("029G0000000008Cm0k0300001", &buf, &key, &size);
    ok(ret == NSC_INVALID && !buf, "Invalid padding accepted: %d\n", ret);
    ret = nsc_decode("029G0000000008Cm0k030000", &buf, &key, &size);
    ok(ret == NSC_INVALID && !buf, "Truncated block accepted: %d\n", ret);
    ret = nsc_decode("028G0000000008Cm0k0300000", &buf, &key, &size);
    ok(ret == NSC_INVALID && !buf, "Incorrect checksum accepted: %d\n", ret);
}

static void test_station(void)
{
    /* The synthetic block tests metadata only, not a playable ASF file. */
    static const struct {
        const char *data;
        enum nsc_result expected;
    } cases[] = {
        { ADDRESS "[Formats]\r\nFormat1=" FORMAT "\r\n", NSC_OK },
        { "[Address]\r\nNSC Format Version=029G0000000008Cm0k0300000\r\n"
          "IP Address=239.1.2.3\r\nIP Port=0x00001234\r\n[Formats]\r\nFormat1=" FORMAT "\r\n", NSC_OK },
        { ADDRESS "[Formats]\r\nFormat1=" FORMAT "\r\nFormat2=" FORMAT "\r\n", NSC_INVALID },
        { ADDRESS "[Formats]\r\n", NSC_INVALID },
        { ADDRESS "[Formats]\r\nFormat1=bad\r\n", NSC_INVALID },
        { "[Address]\r\nIP Address=127.0.0.1\r\nIP Port=0x00001234\r\n[Formats]\r\nFormat1=" FORMAT "\r\n", NSC_INVALID },
        { "[Address]\r\nIP Address=239.1.2.3\r\nIP Port=0x00010000\r\n[Formats]\r\nFormat1=" FORMAT "\r\n", NSC_INVALID },
        { "[Address]\r\nNSC Format Version=2.0\r\n", NSC_UNSUPPORTED },
        { "[Address]\nIP Address=239.1.2.3\n", NSC_INVALID }
    };
    struct nsc_station station;
    enum nsc_result ret;
    unsigned int i;
    char *data;

    for(i = 0; i < ARRAY_SIZE(cases); ++i) {
        data = malloc(strlen(cases[i].data) + 1);
        if(!data) { ok(0, "Allocation failed\n"); return; }
        strcpy(data, cases[i].data);
        ret = nsc_parse(data, strlen(data), &station);
        ok(ret == cases[i].expected, "Case %u returned %d, expected %d\n", i, ret, cases[i].expected);
        if(ret == NSC_OK)
            ok(!strcmp(station.address, "239.1.2.3") && station.port == 0x1234 && station.formats == 1,
                    "Case %u station metadata mismatch\n", i);
        free(data);
    }
}

START_TEST(nsc)
{
    test_encoding();
    test_station();
}
