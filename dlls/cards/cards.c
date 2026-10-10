/*
 * Cards dll implementation
 *
 * Copyright (C) 2004 Sami Nopanen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"

#include "cards.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(cards);


void WINAPI cdtTerm(void);


static HINSTANCE hInst;
static int cardWidth;
static int cardHeight;
static HBITMAP cardBitmaps[CARD_MAX + 1];


/***********************************************************************
 * Initializes the cards.dll library. Loads the card bitmaps from the
 * resources, and initializes the card size variables.
 */
BOOL WINAPI cdtInit(int *width, int *height)
{
	BITMAP bm;
	int i;

	TRACE("(%p, %p)\n", width, height);

	for(i = 0; i <= CARD_MAX; i++)
		cardBitmaps[i] = 0;

	for(i = 0; i <= CARD_MAX; i++)
	{
		cardBitmaps[i] = LoadBitmapA(hInst, MAKEINTRESOURCEA(i));
		if(cardBitmaps[i] == 0)
		{
			cdtTerm();
			return FALSE;
		}
	}

	GetObjectA(cardBitmaps[0], sizeof(BITMAP), &bm);
	*width = cardWidth = bm.bmWidth;
	*height = cardHeight = bm.bmHeight;
	return TRUE;
}

static DWORD do_blt(HDC hdc, int x, int y, int dx, int dy, HDC hMemoryDC, DWORD rasterOp )
{
	if((cardWidth == dx) && (cardHeight == dy))
		return BitBlt(hdc, x, y, cardWidth, cardHeight, hMemoryDC, 0, 0, rasterOp);
	return StretchBlt(hdc, x, y, dx, dy, hMemoryDC, 0, 0, cardWidth, cardHeight, rasterOp);
}

/***********************************************************************
 * Draw a card. Unlike cdtDrawCard, this version allows you to stretch
 * card bitmaps to the size you specify (dx, dy). See cdtDraw for info
 * on card, mode and color parameters.
 */
BOOL WINAPI cdtDrawExt(HDC hdc, int x, int y, int dx, int dy, int card, int mode, DWORD color)
{
	HDC hMemoryDC;
	HBITMAP hCardBitmap;
	HGDIOBJ result;
	DWORD rasterOp = SRCCOPY;
	BOOL roundCornersFlag;
	BOOL eraseFlag = FALSE;
	BOOL drawFlag = TRUE;

	TRACE("(%p, %d, %d, %d, %d, %d, %d, %ld)\n", hdc, x, y, dx, dy, card, mode, color);

	roundCornersFlag = !(mode & MODEFLAG_DONT_ROUND_CORNERS) &&
			   (dx == cardWidth) && (dy == cardHeight);
	mode &= ~MODEFLAG_DONT_ROUND_CORNERS;

	if((card < 0) || (card > CARD_MAX))
	{
		FIXME("Unexpected card: %d\n", card);
		return FALSE;
	}

	if((mode < MODE_FACEUP) || (mode > MODE_DECKO))
	{
		FIXME("Unexpected mode: %d\n", mode);
		return FALSE;
	}

	switch(mode)
	{
	case MODE_FACEUP:
		break;
	case MODE_FACEDOWN:
		break;
	case MODE_HILITE:
		rasterOp = NOTSRCCOPY;
		break;
	case MODE_GHOST:
		card = CARD_FREE_MASK;
		eraseFlag = TRUE;
		rasterOp = SRCAND;
		break;
	case MODE_REMOVE:
		eraseFlag = TRUE;
		drawFlag = FALSE;
		break;
	case MODE_INVISIBLEGHOST:
		card = CARD_FREE_MASK;
		rasterOp = SRCAND;
		break;
	case MODE_DECKX:
		card = CARD_BACK_THE_X;
		break;
	case MODE_DECKO:
		card = CARD_BACK_THE_O;
		break;
	}

	hMemoryDC = CreateCompatibleDC(hdc);
	if(hMemoryDC == 0)
		return FALSE;

	if(eraseFlag)
	{
		HBRUSH hBrush;
		RECT rect;
		hBrush = CreateSolidBrush(color);
                SetRect(&rect, x, y, x + cardWidth - 1, y + cardHeight - 1);
		FillRect(hdc, &rect, hBrush);
	}

	if(drawFlag)
	{
		hCardBitmap = cardBitmaps[card];
		if(hCardBitmap == 0)
			return FALSE;

		result = SelectObject(hMemoryDC, hCardBitmap);
		if((result == 0) || (result == HGDI_ERROR))
		{
			DeleteDC(hMemoryDC);
			return FALSE;
		}

		SetBkColor(hdc, color);

		if(roundCornersFlag)
		{
                    /* NOTE: native uses Get/SetPixel for corners, but that really
                     * hurts on X11 since it needs a server round-trip for each pixel.
                     * So we use a clip region instead. */
                    HRGN saved = CreateRectRgn( 0, 0, 0, 0 );
                    HRGN line = CreateRectRgn( x + 2, y, x + dx - 2, y + 1 );
                    HRGN clip = CreateRectRgn( x, y + 2, x + dx, y + dy - 2 );

                    CombineRgn( clip, clip, line, RGN_OR );
                    SetRectRgn( line, x + 1, y + 1, x + dx - 1, y + 2 );
                    CombineRgn( clip, clip, line, RGN_OR );
                    SetRectRgn( line, x + 1, y + dy - 2, x + dx - 1, y + dy - 1 );
                    CombineRgn( clip, clip, line, RGN_OR );
                    SetRectRgn( line, x + 2, y + dy - 1, x + dx - 2, y + dy );
                    CombineRgn( clip, clip, line, RGN_OR );
                    DeleteObject( line );

                    if (!GetClipRgn( hdc, saved ))
                    {
                        DeleteObject( saved );
                        saved = 0;
                    }
                    ExtSelectClipRgn( hdc, clip, RGN_AND );
                    DeleteObject( clip );

                    do_blt(hdc, x, y, dx, dy, hMemoryDC, rasterOp);

                    SelectClipRgn( hdc, saved );
                    if (saved) DeleteObject( saved );
		}
		else
			do_blt(hdc, x, y, dx, dy, hMemoryDC, rasterOp);
	}

	DeleteDC(hMemoryDC);

	return TRUE;
}


/***********************************************************************
 * Draws a card at position x, y in its default size (as returned by
 * cdtInit.
 *
 * Mode controls how the card gets drawn:
 *   MODE_FACEUP                ; draw card facing up
 *   MODE_FACEDOWN              ; draw card facing down
 *   MODE_HILITE                ; draw face up, with NOTSRCCOPY
 *   MODE_GHOST                 ; draw 'ghost' card
 *   MODE_REMOVE                ; draw with background color
 *   MODE_INVISIBLEGHOST        ; draw 'ghost' card, without clearing background
 *   MODE_DECKX                 ; draw X
 *   MODE_DECKO                 ; draw O
 *
 * The card parameter defines the card graphic to be drawn. If we are
 * drawing fronts of cards, card should have a value from 0 through 51
 * to represent the card face. If we are drawing card backs, 53 through
 * 68 represent different card backs.
 *
 * When drawing card faces, two lowest bits represent the card suit
 * (clubs, diamonds, hearts, spades), and the bits above that define the
 * card value (ace, 2, ..., king). That is,
 *   card = face * 4 + suit.
 *
 * Color parameter defines the background color, used when drawing some
 * card backs.
 */
BOOL WINAPI cdtDraw(HDC hdc, int x, int y, int card, int mode, DWORD color)
{
	TRACE("(%p, %d, %d, %d, %d, %ld)\n", hdc, x, y, card, mode, color);

	return cdtDrawExt(hdc, x, y, cardWidth, cardHeight, card, mode, color);
}


/***********************************************************************
 * Animates the card backs, e.g. blinking lights on the robot, the sun
 * donning sunglasses, bats flying across the caste, etc.. Works only
 * for cards of normal size (as drawn with cdtDraw). To draw frames of
 * the card back animation, start with frame = 0, and increment the
 * frame by one, until cdtAnimate returns FALSE (to indicate that we
 * have gone through all frames of animation).
 */
/*
 * The historical function paints a small frame onto an already drawn card
 * back.  The original animated bitmap resources are not present in Water;
 * these compact GDI drawings are deliberate substitute artwork, not a
 * pixel-accurate reproduction of the Windows 98 sprites.
 *
 * Frame timing belongs to the caller.  Four finite, independently
 * renderable frames keep the API useful without a background timer.
 */
static BOOL fill_animation_rect(HDC hdc, int left, int top, int right, int bottom, COLORREF color)
{
    RECT rect = {left, top, right, bottom};
    HBRUSH brush = CreateSolidBrush(color);
    BOOL ret;

    if (!brush) return FALSE;
    ret = FillRect(hdc, &rect, brush) != 0;
    DeleteObject(brush);
    return ret;
}

BOOL WINAPI cdtAnimate(HDC hdc, int cardback, int x, int y, int frame)
{
    int left, top, right, bottom, width, height, saved, offset;
    BOOL ret = TRUE;

    TRACE("(%p, %d, %d, %d, %d)\n", hdc, cardback, x, y, frame);

    if (!hdc || frame < 0 || frame >= 4 || cardWidth <= 0 || cardHeight <= 0)
        return FALSE;

    /* Only the four historically animated card backs have replacement
     * frames.  A nonanimated back must not be modified by this export. */
    if (cardback != CARD_BACK_ROBOT && cardback != CARD_BACK_CASTLE &&
        cardback != CARD_BACK_ISLAND && cardback != CARD_BACK_CARDHAND)
        return FALSE;
    if (!cardBitmaps[cardback]) return FALSE;  /* cdtInit() is required. */

    left = x + cardWidth / 4;
    right = x + 3 * cardWidth / 4;
    top = y + cardHeight / 4;
    bottom = y + 3 * cardHeight / 4;
    width = right - left;
    height = bottom - top;
    if (width < 12 || height < 12) return FALSE;

    saved = SaveDC(hdc);
    if (!saved) return FALSE;

    switch (cardback)
    {
    case CARD_BACK_ROBOT:
        /* Alternating indicator lights and a four-position gauge. */
        ret = fill_animation_rect(hdc, left, top, right, bottom, RGB(42, 48, 78));
        ret = fill_animation_rect(hdc, left + width / 6, top + height / 4,
                                  left + width / 2 - 2, top + height / 2,
                                  frame & 1 ? RGB(250, 57, 57) : RGB(105, 35, 35)) && ret;
        ret = fill_animation_rect(hdc, left + width / 2 + 2, top + height / 4,
                                  right - width / 6, top + height / 2,
                                  frame & 1 ? RGB(105, 35, 35) : RGB(57, 246, 112)) && ret;
        ret = fill_animation_rect(hdc, left + width / 6 + frame * width / 6,
                                  top + 3 * height / 4, left + width / 6 + frame * width / 6 + 3,
                                  bottom - height / 8, RGB(255, 215, 0)) && ret;
        break;

    case CARD_BACK_CASTLE:
        /* Stylized bat travelling across a night sky. */
        ret = fill_animation_rect(hdc, left, top, right, bottom, RGB(81, 87, 150));
        offset = (width - 10) * frame / 3;
        ret = fill_animation_rect(hdc, left + offset, top + height / 3,
                                  left + offset + 10, top + height / 3 + 2, RGB(22, 21, 43)) && ret;
        ret = fill_animation_rect(hdc, left + offset + 4, top + height / 3 + 2,
                                  left + offset + 6, top + height / 3 + 5, RGB(22, 21, 43)) && ret;
        break;

    case CARD_BACK_ISLAND:
        /* Sun and alternating sunglasses / tongue on the beach back. */
        ret = fill_animation_rect(hdc, left, top, right, bottom, RGB(110, 205, 240));
        {
            HBRUSH sun = CreateSolidBrush(RGB(255, 221, 66));
            if (!sun) ret = FALSE;
            else
            {
                HGDIOBJ previous = SelectObject(hdc, sun);
                if (!previous || previous == HGDI_ERROR) ret = FALSE;
                else
                {
                    ret = Ellipse(hdc, left + width / 5, top + height / 6,
                                  right - width / 5, top + height / 6 + width / 2) && ret;
                    SelectObject(hdc, previous);
                }
                DeleteObject(sun);
            }
        }
        if (frame == 1)
            ret = fill_animation_rect(hdc, left + width / 3, top + height / 2,
                                      right - width / 3, top + height / 2 + 2,
                                      RGB(40, 40, 40)) && ret;
        if (frame >= 2)
        {
            ret = fill_animation_rect(hdc, left + width / 3, top + height / 3,
                                      left + width / 2, top + height / 3 + 3, RGB(12, 32, 49)) && ret;
            ret = fill_animation_rect(hdc, left + width / 2 + 1, top + height / 3,
                                      right - width / 3, top + height / 3 + 3, RGB(12, 32, 49)) && ret;
        }
        if (frame == 3)
            ret = fill_animation_rect(hdc, left + width / 2 - 2, top + height / 2,
                                      left + width / 2 + 2, top + height / 2 + 5,
                                      RGB(235, 58, 95)) && ret;
        break;

    case CARD_BACK_CARDHAND:
        /* A card is progressively drawn upward from a sleeve. */
        ret = fill_animation_rect(hdc, left, top, right, bottom, RGB(45, 99, 86));
        offset = (3 - frame) * height / 8;
        ret = fill_animation_rect(hdc, left + width / 3, top + height / 8 + offset,
                                  right - width / 3, bottom - height / 6,
                                  RGB(244, 237, 217)) && ret;
        ret = fill_animation_rect(hdc, left + width / 4, bottom - height / 3,
                                  right - width / 4, bottom - height / 7,
                                  RGB(108, 60, 47)) && ret;
        break;
    }

    RestoreDC(hdc, saved);
    return ret;
}


/***********************************************************************
 * Frees resources reserved by cdtInit.
 */
void WINAPI cdtTerm(void)
{
	int i;

	TRACE("()\n");

	for(i = 0; i <= CARD_MAX; i++)
	{
		if(cardBitmaps[i] != 0)
			DeleteObject(cardBitmaps[i]);
		cardBitmaps[i] = 0;
	}
}


/***********************************************************************
 * DllMain.
 */
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        hInst = inst;
        DisableThreadLibraryCalls( inst );
        break;
    }
    return TRUE;
}
