/* Copyright (C) 2026 Water contributors
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#include "settings.h"
#include <stdio.h>

int serial_parse_settings(const char *port, const char *baud, struct serial_settings *out)
{
    unsigned port_num = 0;
    unsigned long speed = 0;
    const char *p;

    if (!port || !baud || !out) return 0;
    if (!((port[0] == 'C' || port[0] == 'c') &&
          (port[1] == 'O' || port[1] == 'o') &&
          (port[2] == 'M' || port[2] == 'm'))) return 0;
    p = port + 3;
    if (*p < '1' || *p > '9') return 0; /* COM0 and zero padding are invalid */
    while (*p >= '0' && *p <= '9') {
        port_num = port_num * 10 + (*p++ - '0');
        if (port_num > 256) return 0;
    }
    if (*p) return 0;

    if (!*baud) return 0;
    for (p = baud; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
        speed = speed * 10 + (*p - '0');
        if (speed > 4000000) return 0;
    }
    if (!speed) return 0;
    snprintf(out->device, sizeof(out->device), "\\\\.\\COM%u", port_num);
    out->baud = speed;
    return 1;
}
