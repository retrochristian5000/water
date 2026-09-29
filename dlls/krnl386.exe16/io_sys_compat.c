/*
 * Temporary KRNL386 -> Windows 9x IO.SYS compatibility bridge.
 *
 * The implementation is owned by programs/io.sys. KRNL386 still links it
 * because Water does not yet boot a real-mode IO.SYS image before WIN.COM.
 * Remove this bridge when the Win9x boot chain owns that startup phase.
 */

#include "../../programs/io.sys/config.c"
