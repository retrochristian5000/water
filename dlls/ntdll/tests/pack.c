/*
 * Win32 packing header ABI regression tests.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdarg.h>
#include <stddef.h>
#include <windef.h>
#include "wine/test.h"

/* The split push/pop headers intentionally change packing across #include
 * boundaries. Limit diagnostic suppression to this test, not the project. */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif
#ifdef _MSC_VER
#pragma warning(push)
#endif

#pragma pack(push,8)
struct header_pack8
{
    char flag;
    long long value;
};
C_ASSERT(offsetof(struct header_pack8, value) == 8);
C_ASSERT(sizeof(struct header_pack8) == 16);

#include <pshpack4.h>
struct header_pack4
{
    char flag;
    long long value;
    char tail;
};
C_ASSERT(offsetof(struct header_pack4, value) == 4);
C_ASSERT(sizeof(struct header_pack4) == 16);

#include <pshpack1.h>
struct header_pack1
{
    char flag;
    long long value;
};
C_ASSERT(offsetof(struct header_pack1, value) == 1);
C_ASSERT(sizeof(struct header_pack1) == 9);
#include <poppack.h>

struct header_restored4
{
    char flag;
    long long value;
};
C_ASSERT(offsetof(struct header_restored4, value) == 4);
C_ASSERT(sizeof(struct header_restored4) == 12);

#include <pshpack2.h>
struct header_pack2
{
    char flag;
    long long value;
};
C_ASSERT(offsetof(struct header_pack2, value) == 2);
C_ASSERT(sizeof(struct header_pack2) == 10);
#include <poppack.h>

struct header_restored4_again
{
    char flag;
    long long value;
};
C_ASSERT(offsetof(struct header_restored4_again, value) == 4);
#include <poppack.h>

struct header_restored8
{
    char flag;
    long long value;
};
C_ASSERT(offsetof(struct header_restored8, value) == 8);
C_ASSERT(sizeof(struct header_restored8) == 16);
#pragma pack(pop)

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif

START_TEST(pack)
{
    ok(sizeof(struct header_pack4) == 16, "pack(4) size changed: %u\n",
       (unsigned int)sizeof(struct header_pack4));
}
