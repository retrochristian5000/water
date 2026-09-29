/*
 * Temporary KRNL386 -> EMM386 compatibility bridge.
 *
 * The UMB-provider implementation is owned by programs/emm386. KRNL386 still
 * links it because Water does not yet load a real EMM386.EXE from CONFIG.SYS.
 * Remove this bridge when the DOS boot chain owns EMM386 loading.
 */

#include "../../programs/emm386/umb.c"
