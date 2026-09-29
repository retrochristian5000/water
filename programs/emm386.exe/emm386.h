/*
 * EMM386 compatibility ownership for Water.
 */
#ifndef __WATER_EMM386_H
#define __WATER_EMM386_H

#include "windef.h"
#include "winnt.h"

void EMM386_XMSRequestUMB(I386_CONTEXT *context);
void EMM386_XMSReleaseUMB(I386_CONTEXT *context);

#endif /* __WATER_EMM386_H */
