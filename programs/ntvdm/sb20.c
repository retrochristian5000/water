/*
 * Windows XP NTVDM Sound Blaster 2.0 compatibility profile.
 */

#include <ctype.h>
#include <stdlib.h>

#include "sb20.h"

static BOOL parse_number( const char **cursor, int base, unsigned long max, unsigned long *value )
{
    char *end;
    unsigned long parsed;

    if (!**cursor) return FALSE;

    parsed = strtoul( *cursor, &end, base );
    if (end == *cursor || parsed > max || (*end && !isspace( (unsigned char)*end ))) return FALSE;

    *cursor = end;
    *value = parsed;
    return TRUE;
}

void ntvdm_sb20_config_init( struct ntvdm_sb20_config *config )
{
    config->base = NTVDM_SB20_DEFAULT_BASE;
    config->irq = NTVDM_SB20_DEFAULT_IRQ;
    config->dma = NTVDM_SB20_DEFAULT_DMA;
    config->mpu_base = NTVDM_SB20_DEFAULT_MPU;
    config->card_type = NTVDM_SB20_CARD_TYPE;
    config->enabled = TRUE;
}

BOOL ntvdm_sb20_configure( struct ntvdm_sb20_config *config, const char *blaster )
{
    const char *cursor = blaster;
    unsigned long value;

    ntvdm_sb20_config_init( config );
    if (!cursor || !*cursor) return TRUE;

    while (*cursor)
    {
        int field;

        while (isspace( (unsigned char)*cursor )) cursor++;
        if (!*cursor) break;

        field = toupper( (unsigned char)*cursor++ );
        switch (field)
        {
        case 'A':
            if (!parse_number( &cursor, 16, 0xffff, &value )) goto invalid;
            config->base = value;
            if (!config->base) config->enabled = FALSE;
            break;
        case 'I':
            if (!parse_number( &cursor, 10, 15, &value )) goto invalid;
            config->irq = value;
            break;
        case 'D':
            if (!parse_number( &cursor, 10, 7, &value )) goto invalid;
            config->dma = value;
            break;
        case 'P':
            if (!parse_number( &cursor, 16, 0xffff, &value )) goto invalid;
            config->mpu_base = value;
            break;
        case 'T':
            if (!parse_number( &cursor, 10, 0xff, &value )) goto invalid;
            config->card_type = value;
            if (config->card_type != NTVDM_SB20_CARD_TYPE) config->enabled = FALSE;
            break;
        default:
            while (*cursor && !isspace( (unsigned char)*cursor )) cursor++;
            break;
        }
    }

    return TRUE;

invalid:
    config->enabled = FALSE;
    return FALSE;
}
