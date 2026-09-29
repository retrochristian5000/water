/*
 * Windows 9x IO.SYS compatibility ownership.
 *
 * These interfaces model boot state that must eventually be produced by the
 * real-mode IO.SYS image before WIN.COM starts.
 */
#ifndef __WATER_IO_SYS_H
#define __WATER_IO_SYS_H

#include "windef.h"

struct iosys_config_sys
{
    WORD buffers_count;
    WORD buffers_lookahead;
    BYTE last_drive;
    INT umb_linked;       /* -1 when CONFIG.SYS did not specify UMB/NOUMB */
    BOOL break_on;
};

void IOSYS_InitConfig(void);
BYTE IOSYS_GetBootDrive(void);
void IOSYS_ReadConfigSys(struct iosys_config_sys *config);

#endif /* __WATER_IO_SYS_H */
