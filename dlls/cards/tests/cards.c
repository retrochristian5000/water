/* Tests for Water's documented cdtAnimate replacement frames.
 * These visual frames are intentionally not compared pixel-for-pixel with
 * Microsoft's original cards.dll resources. */
#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/test.h"

BOOL WINAPI cdtInit(int *width, int *height);
BOOL WINAPI cdtAnimate(HDC hdc, int cardback, int x, int y, int frame);
void WINAPI cdtTerm(void);

static void test_animation(void)
{
    static const int animated[] = {56, 63, 64, 65};
    HDC screen = GetDC(NULL), dc;
    HBITMAP bitmap, previous;
    int i, j, width = 0, height = 0, mode;

    if (!screen)
    {
        win_skip("No screen DC available\n");
        return;
    }
    dc = CreateCompatibleDC(screen);
    bitmap = CreateCompatibleBitmap(screen, 300, 300);
    ReleaseDC(NULL, screen);
    if (!dc || !bitmap)
    {
        win_skip("Cannot create an animation test bitmap\n");
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        return;
    }
    previous = SelectObject(dc, bitmap);

    ok(!cdtAnimate(dc, 56, 0, 0, 0), "Animation before initialization must fail\n");
    ok(cdtInit(&width, &height), "cdtInit failed\n");
    ok(width > 0 && height > 0, "Invalid card size %d x %d\n", width, height);
    mode = SetBkMode(dc, TRANSPARENT);
    for (i = 0; i < ARRAY_SIZE(animated); i++)
    {
        for (j = 0; j < 4; j++)
            ok(cdtAnimate(dc, animated[i], 10, 10, j), "card %d frame %d failed\n", animated[i], j);
        ok(!cdtAnimate(dc, animated[i], 10, 10, 4), "card %d accepted terminal frame\n", animated[i]);
    }
    ok(GetBkMode(dc) == TRANSPARENT, "cdtAnimate modified the caller's DC\n");
    SetBkMode(dc, mode);
    ok(!cdtAnimate(dc, 53, 10, 10, 0), "Nonanimated card accepted a frame\n");
    ok(!cdtAnimate(dc, 56, 10, 10, -1), "Negative frame was accepted\n");
    ok(!cdtAnimate(NULL, 56, 10, 10, 0), "NULL DC was accepted\n");
    cdtTerm();
    ok(!cdtAnimate(dc, 56, 10, 10, 0), "Animation after cdtTerm must fail\n");

    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

START_TEST(cards)
{
    test_animation();
}
