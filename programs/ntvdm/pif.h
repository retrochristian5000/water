/*
 * DOS Program Information File (PIF) settings for the NTVDM launcher.
 * This describes an on-disk compatibility format, not a host ABI.
 */
#ifndef __WATER_NTVDM_PIF_H
#define __WATER_NTVDM_PIF_H

#include "windef.h"

struct ntvdm_pif
{
    char program[64];
    char title[31];
    char parameters[65];
    char directory[65];
    BOOL text_mode;
    BOOL close_on_exit;
};

BOOL ntvdm_read_pif( const char *path, struct ntvdm_pif *settings );

#endif
