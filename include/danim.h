/*
 * Microsoft DirectAnimation (DirectX Media) class identifiers.
 *
 * This is a deliberately limited compatibility header.  The 1998 SDK
 * documented these ActiveX viewer controls; their complete COM interface
 * layouts, IIDs, and type-library DISPIDs are not yet recovered.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef __WINE_DANIM_H
#define __WINE_DANIM_H

#include <objbase.h>

/* Integrated-media controls in danim.dll, not DirectX Transform objects.
 * CLSID_DAViewerControl appears in Microsoft's March 1998 SDK examples.
 * The windowed variant is documented in the March 2001 DirectX Transform
 * SDK article and historical DirectMedia COM object inventories. */
DEFINE_GUID(CLSID_DAViewerControl,
            0xb6ffc24c, 0x7e13, 0x11d0, 0x9b, 0x47, 0x00, 0xc0, 0x4f, 0xc2, 0xf5, 0x1d);
DEFINE_GUID(CLSID_DAViewerControlWindowed,
            0x69ad90ef, 0x1c20, 0x11d1, 0x88, 0x01, 0x00, 0xc0, 0x4f, 0xc2, 0x9d, 0x46);

#endif /* __WINE_DANIM_H */
