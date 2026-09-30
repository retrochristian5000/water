/*
 * Water Win16 guest profile selection.
 *
 * Guest identity is deliberately factored into orthogonal axes:
 *
 *   line    - DOS, pre-9x Windows, Win9x, or Windows NT
 *   version - version/build within that line
 *   ISA     - guest architecture, independent of the Water host
 *
 * Execution mode (standard/enhanced) is separate from identity.  Legacy
 * WATER_VDM_PERSONALITY strings remain accepted as compatibility aliases,
 * but consumers should gate behavior on the axes below rather than on a
 * monolithic personality name.
 */
#ifndef __WINE_WIN16_PROFILE_H
#define __WINE_WIN16_PROFILE_H

#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wine/vdm.h"

/* Legacy selector retained for launch-script compatibility. */
#define WATER_WIN16_PERSONALITY_ENV            "WATER_VDM_PERSONALITY"
#define WATER_WIN16_PERSONALITY_WIN30_STANDARD "win30-standard"
#define WATER_WIN16_PERSONALITY_WIN31_STANDARD "win31-standard"
#define WATER_WIN16_PERSONALITY_WIN95_OSR2     "win95-osr2"
#define WATER_WIN16_PERSONALITY_WINME           "winme"

/* Orthogonal guest identity axes. */
#define WATER_WIN16_LINE_ENV                    "WATER_VDM_LINE"
#define WATER_WIN16_VERSION_ENV                 "WATER_VDM_VERSION"
#define WATER_WIN16_ISA_ENV                     "WATER_VDM_ISA"
#define WATER_WIN16_MODE_ENV                    "WATER_WIN16_MODE"

#define WATER_WIN16_LINE_DOS                    "dos"
#define WATER_WIN16_LINE_PRE9X                  "pre9x"
#define WATER_WIN16_LINE_WIN9X                  "win9x"
#define WATER_WIN16_LINE_NT                     "nt"

#define WATER_WIN16_ISA_X86                     "x86"
#define WATER_WIN16_ISA_MIPS                    "mips"
#define WATER_WIN16_ISA_ALPHA                   "alpha"
#define WATER_WIN16_ISA_PPC                     "ppc"
#define WATER_WIN16_ISA_ARM                     "arm"
#define WATER_WIN16_ISA_ARM64                   "arm64"

#define WATER_WIN16_MODE_STANDARD               "standard"
#define WATER_WIN16_MODE_ENHANCED               "enhanced"

enum water_win16_line
{
    WATER_WIN16_LINE_UNKNOWN,
    WATER_WIN16_LINE_DOS_FAMILY,
    WATER_WIN16_LINE_PRE9X_FAMILY,
    WATER_WIN16_LINE_WIN9X_FAMILY,
    WATER_WIN16_LINE_NT_FAMILY
};

enum water_win16_isa
{
    WATER_WIN16_ISA_UNKNOWN,
    WATER_WIN16_ISA_X86_FAMILY,
    WATER_WIN16_ISA_MIPS_FAMILY,
    WATER_WIN16_ISA_ALPHA_FAMILY,
    WATER_WIN16_ISA_PPC_FAMILY,
    WATER_WIN16_ISA_ARM_FAMILY,
    WATER_WIN16_ISA_ARM64_FAMILY
};

enum water_win16_mode
{
    WATER_WIN16_MODE_UNKNOWN,
    WATER_WIN16_MODE_STANDARD_FAMILY,
    WATER_WIN16_MODE_ENHANCED_FAMILY
};

struct water_win16_profile
{
    enum water_win16_line line;
    enum water_win16_isa isa;
    enum water_win16_mode mode;
    WORD major;
    WORD minor;
    DWORD build;
};

static inline BOOL water_win16_parse_version( const char *value, WORD *major,
                                               WORD *minor, DWORD *build )
{
    char *end;
    unsigned long part;

    if (!value || !*value) return FALSE;

    part = strtoul( value, &end, 10 );
    if (end == value || part > 0xffff) return FALSE;
    *major = part;
    *minor = 0;
    *build = 0;

    if (!*end) return TRUE;
    if (*end++ != '.') return FALSE;

    value = end;
    part = strtoul( value, &end, 10 );
    if (end == value || part > 0xffff) return FALSE;
    *minor = part;

    if (!*end) return TRUE;
    if (*end++ != '.') return FALSE;

    value = end;
    part = strtoul( value, &end, 10 );
    if (end == value || *end) return FALSE;
    *build = part;
    return TRUE;
}

static inline void water_win16_apply_legacy_profile( struct water_win16_profile *profile,
                                                      const char *legacy )
{
    if (!legacy || !*legacy) return;

    if (!strcmp( legacy, WATER_WIN16_PERSONALITY_WIN30_STANDARD ))
    {
        profile->line = WATER_WIN16_LINE_PRE9X_FAMILY;
        profile->major = 3;
        profile->minor = 0;
        profile->build = 0;
        profile->mode = WATER_WIN16_MODE_STANDARD_FAMILY;
    }
    else if (!strcmp( legacy, WATER_WIN16_PERSONALITY_WIN31_STANDARD ))
    {
        profile->line = WATER_WIN16_LINE_PRE9X_FAMILY;
        profile->major = 3;
        profile->minor = 10;
        profile->build = 0;
        profile->mode = WATER_WIN16_MODE_STANDARD_FAMILY;
    }
    else if (!strcmp( legacy, WATER_WIN16_PERSONALITY_WIN95_OSR2 ))
    {
        profile->line = WATER_WIN16_LINE_WIN9X_FAMILY;
        profile->major = 4;
        profile->minor = 0;
        profile->build = 1111;
    }
    else if (!strcmp( legacy, WATER_WIN16_PERSONALITY_WINME ))
    {
        profile->line = WATER_WIN16_LINE_WIN9X_FAMILY;
        profile->major = 4;
        profile->minor = 90;
        profile->build = 3000;
    }
    else if (!strcmp( legacy, WATER_VDM_PERSONALITY_NT31_WOW ))
    {
        profile->line = WATER_WIN16_LINE_NT_FAMILY;
        profile->major = 3;
        profile->minor = 10;
        profile->build = 511;
    }
    else if (!strcmp( legacy, WATER_VDM_PERSONALITY_NT351_WOW ))
    {
        profile->line = WATER_WIN16_LINE_NT_FAMILY;
        profile->major = 3;
        profile->minor = 51;
        profile->build = 1057;
    }
    else if (!strcmp( legacy, WATER_VDM_PERSONALITY_NT5_WOW ))
    {
        profile->line = WATER_WIN16_LINE_NT_FAMILY;
        profile->major = 5;
        profile->minor = 0;
        profile->build = 0;
    }
}

static inline void water_win16_read_profile( struct water_win16_profile *profile )
{
    char value[32];
    DWORD len;

    /*
     * Preserve Water's long-standing default while making it explicit:
     * DOS-based Windows 4.00 build 950, with ISA/mode not inferred from host.
     */
    profile->line = WATER_WIN16_LINE_WIN9X_FAMILY;
    profile->isa = WATER_WIN16_ISA_UNKNOWN;
    profile->mode = WATER_WIN16_MODE_UNKNOWN;
    profile->major = 4;
    profile->minor = 0;
    profile->build = 950;

    len = GetEnvironmentVariableA( WATER_WIN16_PERSONALITY_ENV, value, sizeof(value) );
    if (len && len < sizeof(value)) water_win16_apply_legacy_profile( profile, value );

    len = GetEnvironmentVariableA( WATER_WIN16_LINE_ENV, value, sizeof(value) );
    if (len && len < sizeof(value))
    {
        if (!strcmp( value, WATER_WIN16_LINE_DOS ))
            profile->line = WATER_WIN16_LINE_DOS_FAMILY;
        else if (!strcmp( value, WATER_WIN16_LINE_PRE9X ))
            profile->line = WATER_WIN16_LINE_PRE9X_FAMILY;
        else if (!strcmp( value, WATER_WIN16_LINE_WIN9X ))
            profile->line = WATER_WIN16_LINE_WIN9X_FAMILY;
        else if (!strcmp( value, WATER_WIN16_LINE_NT ))
            profile->line = WATER_WIN16_LINE_NT_FAMILY;
    }

    len = GetEnvironmentVariableA( WATER_WIN16_VERSION_ENV, value, sizeof(value) );
    if (len && len < sizeof(value))
    {
        WORD major, minor;
        DWORD build;

        if (water_win16_parse_version( value, &major, &minor, &build ))
        {
            profile->major = major;
            profile->minor = minor;
            profile->build = build;
        }
    }

    len = GetEnvironmentVariableA( WATER_WIN16_ISA_ENV, value, sizeof(value) );
    if (len && len < sizeof(value))
    {
        if (!strcmp( value, WATER_WIN16_ISA_X86 ) || !strcmp( value, "i386" ))
            profile->isa = WATER_WIN16_ISA_X86_FAMILY;
        else if (!strcmp( value, WATER_WIN16_ISA_MIPS ))
            profile->isa = WATER_WIN16_ISA_MIPS_FAMILY;
        else if (!strcmp( value, WATER_WIN16_ISA_ALPHA ))
            profile->isa = WATER_WIN16_ISA_ALPHA_FAMILY;
        else if (!strcmp( value, WATER_WIN16_ISA_PPC ) || !strcmp( value, "powerpc" ))
            profile->isa = WATER_WIN16_ISA_PPC_FAMILY;
        else if (!strcmp( value, WATER_WIN16_ISA_ARM ))
            profile->isa = WATER_WIN16_ISA_ARM_FAMILY;
        else if (!strcmp( value, WATER_WIN16_ISA_ARM64 ) || !strcmp( value, "aarch64" ))
            profile->isa = WATER_WIN16_ISA_ARM64_FAMILY;
    }

    len = GetEnvironmentVariableA( WATER_WIN16_MODE_ENV, value, sizeof(value) );
    if (len && len < sizeof(value))
    {
        if (!strcmp( value, WATER_WIN16_MODE_STANDARD ))
            profile->mode = WATER_WIN16_MODE_STANDARD_FAMILY;
        else if (!strcmp( value, WATER_WIN16_MODE_ENHANCED ))
            profile->mode = WATER_WIN16_MODE_ENHANCED_FAMILY;
    }
}

static inline BOOL water_win16_profile_is_version( const struct water_win16_profile *profile,
                                                    WORD major, WORD minor )
{
    return profile->major == major && profile->minor == minor;
}

#define WATER_WIN16_KERNEL_ENV                  "WATER_KERNEL16_IMAGE"
#define WATER_WIN16_KERNEL_KRNL286              "krnl286"
#define WATER_WIN16_KERNEL_KRNL386              "krnl386"
#define WATER_WIN16_CPU_LEVEL_ENV               "WATER_X86_CPU_LEVEL"

/*
 * KRNL386 export projection policy.  This is deliberately not part of guest
 * identity: compatibility policy is a separate knob from line/version/ISA.
 */
#define WATER_WIN16_EXPORT_POLICY_ENV            "WATER_KERNEL16_EXPORT_POLICY"
#define WATER_WIN16_EXPORT_POLICY_COMPAT         "compatible"
#define WATER_WIN16_EXPORT_POLICY_NATIVE         "native"

#endif /* __WINE_WIN16_PROFILE_H */
