/*
 * Precompiled header for the native WRC build.
 *
 * Keep this limited to stable headers shared by the ordinary WRC translation
 * units.  Generated parser sources and files with local preprocessor state
 * are excluded by tools/makedep or PCH_EXCLUDE.
 */

#ifndef __WRC_PCH_H
#define __WRC_PCH_H

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools.h"
#include "wrc.h"
#include "utils.h"

#endif
