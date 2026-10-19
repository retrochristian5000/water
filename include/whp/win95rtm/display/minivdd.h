/* WHP Windows 95 display mini-VDD C reference boundary.
 *
 * Evidence:
 *   Windows 95 DDK, "About Refresh Rate Support" (MINIVDD.INC DISPLAYINFO)
 *     https://techshelps.github.io/MSDN/WIN95DDK/HTML/S281C.HTM
 *   VMDisp9x ddk/minivdd.h (Windows 95/98/Me cross-version C witness)
 *     https://github.com/JHRobotics/vmdisp9x/blob/main/ddk/minivdd.h
 *
 * Independent WHP declarations of the observed numeric values/field layout,
 * not a copy of the DDK or a certified Windows 95 RTM (4.00.950) header.
 * VDD service calling convention, dispatch table, and Win95 RTM guest
 * support are not established here. DO NOT call VDD or access hardware.
 */
#ifndef WHP_WIN95RTM_DISPLAY_MINIVDD_H
#define WHP_WIN95RTM_DISPLAY_MINIVDD_H

#include <stddef.h>

#define WHP_W95_VDD_API_BASE             0x80u
#define WHP_W95_VDD_DRIVER_REGISTER      (WHP_W95_VDD_API_BASE + 0u)
#define WHP_W95_VDD_DRIVER_UNREGISTER    (WHP_W95_VDD_API_BASE + 1u)
#define WHP_W95_VDD_SAVE_DRIVER_STATE    (WHP_W95_VDD_API_BASE + 2u)
#define WHP_W95_VDD_REGISTER_DRIVER_INFO (WHP_W95_VDD_API_BASE + 3u)
#define WHP_W95_VDD_REGISTER_SSB_FLAGS   (WHP_W95_VDD_API_BASE + 4u)
#define WHP_W95_VDD_GET_DISPLAY_CONFIG   (WHP_W95_VDD_API_BASE + 5u)
#define WHP_W95_VDD_PRE_MODE_CHANGE      (WHP_W95_VDD_API_BASE + 6u)
#define WHP_W95_VDD_POST_MODE_CHANGE     (WHP_W95_VDD_API_BASE + 7u)
#define WHP_W95_VDD_SET_USER_FLAGS       (WHP_W95_VDD_API_BASE + 8u)
#define WHP_W95_VDD_SET_BUSY_FLAG_ADDR   (WHP_W95_VDD_API_BASE + 9u)

/* Selected Main VDD mini-dispatch indices attested in the 9x C witness.
 * Their presence here is not evidence that a physical GPU supports them.
 */
#define WHP_W95_MINI_REGISTER_DISPLAY_DRIVER  0u
#define WHP_W95_MINI_PRE_HIRES_TO_VGA         4u
#define WHP_W95_MINI_PRE_VGA_TO_HIRES         6u
#define WHP_W95_MINI_ENABLE_TRAPS             13u
#define WHP_W95_MINI_DISABLE_TRAPS            14u
#define WHP_W95_MINI_GET_CHIP_ID             42u

#define WHP_W95_DISPLAYINFO_STALE            0x0001u
#define WHP_W95_DISPLAYINFO_MINIVDD_LOAD_FAILED 0x0002u
#define WHP_W95_DISPLAYINFO_CHIP_MISMATCH    0x0004u
#define WHP_W95_DISPLAYINFO_MONITOR_INVALID  0x0080u

/* Observed 50-byte field image from the 9x C header. Use as an ABI
 * comparison candidate ONLY: version-specific fields and native VDD
 * register/pointer calling contract still require RTM validation.
 * No pointer-sized fields; Win16/Win32 layouts have the same byte offsets.
 */
#pragma pack(push, 1)
typedef struct whp_w95_displayinfo {
    unsigned short diHdrSize;
    unsigned short diInfoFlags;
    unsigned long diDevNodeHandle;
    char diDriverName[16];
    unsigned short diXRes;
    unsigned short diYRes;
    unsigned short diDPI;
    unsigned char diPlanes;
    unsigned char diBpp;
    unsigned short diRefreshRateMax;
    unsigned short diRefreshRateMin;
    unsigned short diLowHorz;
    unsigned short diHighHorz;
    unsigned short diLowVert;
    unsigned short diHighVert;
    unsigned long diMonitorDevNodeHandle;
    unsigned char diHorzSyncPolarity;
    unsigned char diVertSyncPolarity;
} WHP_W95_DISPLAYINFO;
#pragma pack(pop)

typedef char whp_w95_displayinfo_short_size[(sizeof(unsigned short) == 2) ? 1 : -1];
typedef char whp_w95_displayinfo_long_size[(sizeof(unsigned long) == 4) ? 1 : -1];
typedef char whp_w95_displayinfo_size_check[(sizeof(WHP_W95_DISPLAYINFO) == 50) ? 1 : -1];
typedef char whp_w95_displayinfo_devnode_off[(offsetof(WHP_W95_DISPLAYINFO, diDevNodeHandle) == 4) ? 1 : -1];
typedef char whp_w95_displayinfo_xres_off[(offsetof(WHP_W95_DISPLAYINFO, diXRes) == 24) ? 1 : -1];
typedef char whp_w95_displayinfo_refresh_off[(offsetof(WHP_W95_DISPLAYINFO, diRefreshRateMax) == 32) ? 1 : -1];
typedef char whp_w95_displayinfo_monitor_off[(offsetof(WHP_W95_DISPLAYINFO, diMonitorDevNodeHandle) == 44) ? 1 : -1];

#endif /* WHP_WIN95RTM_DISPLAY_MINIVDD_H */
