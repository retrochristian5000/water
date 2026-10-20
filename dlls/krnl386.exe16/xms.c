/*
 * Temporary KRNL386 -> HIMEM.SYS XMS compatibility bridge.
 *
 * The DOS XMS/HMA/A20 implementation now belongs to
 * programs/himem.sys/xms.c; EMM386 owns UMB requests 10h/11h.
 * KRNL386 includes the provider only to preserve existing DOS-based
 * Win16/NT WOW callers until their XMS loader/dispatch paths are ready.
 *
 * This compiles the same compatibility code into KRNL386 for now:
 * it does NOT build HIMEM.SYS or implement Windows 95's protected-mode
 * XMS takeover and retained real-mode XMS handle interoperability.
 */
#include "../../programs/himem.sys/xms.c"
