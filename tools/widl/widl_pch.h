/*
 * Precompiled header for the native WIDL build.
 *
 * Keep this limited to stable headers shared by most WIDL translation units.
 * Sources with generated code or per-file preprocessor state are excluded by
 * tools/makedep.
 */

#ifndef __WIDL_PCH_H
#define __WIDL_PCH_H

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "widl.h"
#include "utils.h"

#endif
