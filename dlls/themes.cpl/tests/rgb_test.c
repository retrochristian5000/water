#include "../rgb.h"
#include <assert.h>
#include <stdio.h>
static void good(const char *text, unsigned r, unsigned g, unsigned b)
{
    unsigned char rgb[3] = {0xaa, 0xaa, 0xaa};
    assert(theme_parse_rgb(text, rgb));
    assert(rgb[0] == r && rgb[1] == g && rgb[2] == b);
}
int main(void)
{
    unsigned char rgb[3] = {42, 42, 42};
    const char *invalid[] = {
        "", "0", "0 0", "256 0 0", "0 1000 0", "1 2 3 4",
        "-1 0 0", "1,2,3", "1 2 3garbage", "999999999999 0 0",
        "1.5 2 3", "1 2", "1 2 3 #comment", "1 2 +3"
    };
    unsigned i;
    good("0 128 128", 0, 128, 128);
    good("255 255 255", 255, 255, 255);
    good("\t12  34\t56 ", 12, 34, 56);
    for(i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
        assert(!theme_parse_rgb(invalid[i], rgb));
    assert(!theme_parse_rgb(NULL, rgb));
    assert(!theme_parse_rgb("1 2 3", NULL));
    puts("classic-theme RGB parser tests passed");
    return 0;
}
