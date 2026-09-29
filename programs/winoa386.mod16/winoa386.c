/*
 * WINOA386 enhanced-mode WinOldAp frontend.
 *
 * The historical file has WINOLDAP as its internal module name. Share the
 * launcher implementation with WINOLDAP.MOD while preserving the distinct
 * enhanced-mode filename selected by KRNL386.
 */

#define WINE_WINOLDAP_GRABBER_KEY "386grabber"
#include "../winoldap.mod16/winoldap.c"
