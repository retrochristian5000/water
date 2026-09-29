/*
 * Shared Water DOS keyboard compatibility state.
 *
 * KEYB.COM records the DOS-facing layout here; winevdm projects the safe
 * subset into the external DOS backend.  The remaining fields are retained
 * for a future internal DOS/NTVDM keyboard implementation.
 */
#ifndef __WINE_DOSKEYB_H
#define __WINE_DOSKEYB_H

#define WINE_DOS_KEYB_REGKEY          L"Software\\Wine\\DOS\\Keyboard"
#define WINE_DOS_KEYB_LAYOUT_VALUE    L"Layout"
#define WINE_DOS_KEYB_CODEPAGE_VALUE  L"CodePage"
#define WINE_DOS_KEYB_FILE_VALUE      L"DefinitionFile"
#define WINE_DOS_KEYB_ID_VALUE        L"KeyboardId"
#define WINE_DOS_KEYB_ENHANCED_VALUE  L"Enhanced"

#define WINE_DOS_KEYB_MAX_LAYOUT      5
#define WINE_DOS_KEYB_MAX_ID          3

#endif /* __WINE_DOSKEYB_H */
