/*
 * COOL.DLL - documented Windows 95 Plus! presence check.
 *
 * The Windows 98 FE version of COOL.DLL also supplies desktop icons.
 * Those resources are not implemented here.  In particular, this small
 * compatibility entry point must not be mistaken for a complete clone
 * of the Windows 98 icon library.
 *
 * A Windows 95 Plus! compatibility workaround describes an ordinal-2
 * function returning 0x41524245 ("BEAR") as a DWORD (DX:AX on Win16).
 * Source: https://www.winfaq.de/faq_html/Content/tip0000/onlinefaq.php?h=tip0278.htm
 *
 * This verifies the workaround's ABI expectation, not the original
 * Microsoft binary's exact export names or implementation.
 */

#include "windef.h"

DWORD WINAPI CoolPlusCheck16(void)
{
    return 0x41524245UL;
}
