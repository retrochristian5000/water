/*
 * Compile-time Win32 Winsock function-pointer ABI checks.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define UNICODE
#define INCL_WINSOCK_API_TYPEDEFS 1
#include <winsock2.h>

#include "wine/test.h"

/* Test the precise typedef/prototype contract, including pointer-sized
 * SOCKET returns and Unicode pointer arguments. Both GCC and Clang support
 * __builtin_types_compatible_p, including for WINAPI function pointers. */
#if defined(__GNUC__) || defined(__clang__)
C_ASSERT(__builtin_types_compatible_p(__typeof__(&WSAJoinLeaf), LPFN_WSAJOINLEAF));
C_ASSERT(__builtin_types_compatible_p(__typeof__(&WSAStringToAddressA), LPFN_WSASTRINGTOADDRESSA));
C_ASSERT(__builtin_types_compatible_p(__typeof__(&WSAStringToAddressW), LPFN_WSASTRINGTOADDRESSW));
C_ASSERT(__builtin_types_compatible_p(__typeof__(&WSAGetServiceClassInfoW), LPFN_WSAGETSERVICECLASSINFOW));
C_ASSERT(__builtin_types_compatible_p(LPFN_WSASTRINGTOADDRESS, LPFN_WSASTRINGTOADDRESSW));
C_ASSERT(__builtin_types_compatible_p(LPFN_WSAGETSERVICECLASSINFO, LPFN_WSAGETSERVICECLASSINFOW));
#endif

START_TEST(header_abi)
{
    ok(sizeof(SOCKET) == sizeof(UINT_PTR), "SOCKET must be pointer-sized\\n");
}
