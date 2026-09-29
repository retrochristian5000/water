/*
 * Windows XP NTVDM Sound Blaster 2.0 compatibility profile.
 *
 * The BLASTER settings are guest-visible virtual hardware coordinates. Keep
 * them independent from the host audio implementation so the DOSBox fallback
 * and the future in-process VDM device model share one personality.
 */
#ifndef __WATER_NTVDM_SB20_H
#define __WATER_NTVDM_SB20_H

#include "windef.h"

#define NTVDM_SB20_DEFAULT_BASE 0x220
#define NTVDM_SB20_DEFAULT_IRQ  5
#define NTVDM_SB20_DEFAULT_DMA  1
#define NTVDM_SB20_DEFAULT_MPU  0x330
#define NTVDM_SB20_CARD_TYPE    3

struct ntvdm_sb20_config
{
    WORD base;
    BYTE irq;
    BYTE dma;
    WORD mpu_base;
    BYTE card_type;
    BOOL enabled;
};

void ntvdm_sb20_config_init( struct ntvdm_sb20_config *config );
BOOL ntvdm_sb20_configure( struct ntvdm_sb20_config *config, const char *blaster );

#endif /* __WATER_NTVDM_SB20_H */
