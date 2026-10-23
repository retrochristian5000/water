/* Copyright (C) 2026 Water contributors
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#include "rgb.h"
static int space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
int theme_parse_rgb(const char *text, unsigned char rgb[3])
{
    unsigned char values[3];
    unsigned i;
    if(!text || !rgb) return 0;
    for(i = 0; i < 3; i++)
    {
        unsigned value = 0, digits = 0;
        while(space((unsigned char)*text)) text++;
        while(*text >= '0' && *text <= '9')
        {
            value = value * 10 + (*text++ - '0');
            if(++digits > 3 || value > 255) return 0;
        }
        if(!digits || (i != 2 && !space((unsigned char)*text))) return 0;
        values[i] = value;
    }
    while(space((unsigned char)*text)) text++;
    if(*text) return 0;
    for(i = 0; i < 3; i++) rgb[i] = values[i];
    return 1;
}
