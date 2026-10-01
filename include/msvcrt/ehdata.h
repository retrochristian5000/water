/*
 * MSVC C++ exception ABI data definitions
 *
 * This header exposes the subset of the MSVC exception metadata ABI needed by
 * C++ runtime libraries.  The corresponding runtime-side structures live in
 * dlls/msvcrt and use the same field layout.
 */

#ifndef __WINE_EHDATA_H
#define __WINE_EHDATA_H

#include <corecrt.h>
#include <winnt.h>

/*
 * MSVC stores exception type information as image-relative offsets on its
 * native 64-bit targets and on ARM.  CHPE keeps type information absolute.
 */
#ifndef _EH_RELATIVE_TYPEINFO
# if defined(_CHPE_X86_ARM64_EH_)
#  define _EH_RELATIVE_TYPEINFO 0
# elif defined(_M_AMD64) || defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64) || \
       defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
#  define _EH_RELATIVE_TYPEINFO 1
# else
#  define _EH_RELATIVE_TYPEINFO 0
# endif
#endif

#define EH_EXCEPTION_NUMBER       0xe06d7363u
#define EH_MAGIC_NUMBER1          0x19930520u
#define EH_MAGIC_NUMBER2          0x19930521u
#define EH_MAGIC_NUMBER3          0x19930522u
#define EH_PURE_MAGIC_NUMBER1     0x01994000u

#if _EH_RELATIVE_TYPEINFO
# define EH_EXCEPTION_PARAMETERS 4
#else
# define EH_EXCEPTION_PARAMETERS 3
#endif

#define CT_IsSimpleType           0x00000001u
#define CT_ByReferenceOnly        0x00000002u
#define CT_HasVirtualBase         0x00000004u
#define CT_IsWinRTHandle          0x00000008u
#define CT_IsStdBadAlloc          0x00000010u

#define TI_IsConst                0x00000001u
#define TI_IsVolatile             0x00000002u
#define TI_IsUnaligned            0x00000004u
#define TI_IsPure                 0x00000008u
#define TI_IsWinRT                0x00000010u

#pragma pack(push,4)

/* Generalized pointer-to-member displacement used by the MSVC EH ABI. */
typedef struct PMD
{
    int mdisp;
    int pdisp;
    int vdisp;
} PMD;

#if _EH_RELATIVE_TYPEINFO
typedef int PMFN;
#else
typedef void (__cdecl *PMFN)(void *);
#endif

typedef struct TypeDescriptor TypeDescriptor;

typedef const struct _s_CatchableType
{
    unsigned int properties;
#if _EH_RELATIVE_TYPEINFO
    int pType;
#else
    TypeDescriptor *pType;
#endif
    PMD thisDisplacement;
    int sizeOrOffset;
    PMFN copyFunction;
} CatchableType;

typedef const struct _s_CatchableTypeArray
{
    int nCatchableTypes;
#if _EH_RELATIVE_TYPEINFO
    int arrayOfCatchableTypes[];
#else
    CatchableType *arrayOfCatchableTypes[];
#endif
} CatchableTypeArray;

typedef const struct _s_ThrowInfo
{
    unsigned int attributes;
    PMFN pmfnUnwind;
#if _EH_RELATIVE_TYPEINFO
    int pForwardCompat;
    int pCatchableTypeArray;
#else
    int (__cdecl *pForwardCompat)(...);
    CatchableTypeArray *pCatchableTypeArray;
#endif
} ThrowInfo;

#pragma pack(pop)

#if defined(_WIN64)
#pragma pack(push,8)
#endif

/*
 * Overlay for EXCEPTION_RECORD used by MSVC C++ exceptions.  Its params field
 * occupies the first EH_EXCEPTION_PARAMETERS entries of ExceptionInformation.
 */
typedef struct EHExceptionRecord
{
    DWORD ExceptionCode;
    DWORD ExceptionFlags;
    struct _EXCEPTION_RECORD *ExceptionRecord;
    void *ExceptionAddress;
    DWORD NumberParameters;
    struct
    {
        DWORD magicNumber;
        void *pExceptionObject;
        ThrowInfo *pThrowInfo;
#if _EH_RELATIVE_TYPEINFO
        void *pThrowImageBase;
#endif
    } params;
} EHExceptionRecord;

#if defined(_WIN64)
#pragma pack(pop)
#endif

#if (defined(_M_ARM64) || defined(__aarch64__)) && !defined(_M_ARM64EC) && !defined(__arm64ec__)
C_ASSERT(sizeof(PMD) == 12);
C_ASSERT(sizeof(CatchableType) == 28);
C_ASSERT(sizeof(ThrowInfo) == 16);
C_ASSERT(FIELD_OFFSET(EHExceptionRecord, params) == 32);
C_ASSERT(FIELD_OFFSET(EHExceptionRecord, params.pExceptionObject) == 40);
C_ASSERT(FIELD_OFFSET(EHExceptionRecord, params.pThrowInfo) == 48);
C_ASSERT(FIELD_OFFSET(EHExceptionRecord, params.pThrowImageBase) == 56);
C_ASSERT(sizeof(EHExceptionRecord) == 64);
#endif

#define PER_CODE(per)       ((per)->ExceptionCode)
#define PER_NPARAMS(per)    ((per)->NumberParameters)
#define PER_MAGICNUM(per)   ((per)->params.magicNumber)
#define PER_PEXCEPTOBJ(per) ((per)->params.pExceptionObject)
#define PER_PTHROW(per)     ((per)->params.pThrowInfo)

#define PER_IS_MSVC_EH(per) \
    (PER_CODE(per) == EH_EXCEPTION_NUMBER && \
     PER_NPARAMS(per) == EH_EXCEPTION_PARAMETERS && \
     (PER_MAGICNUM(per) == EH_MAGIC_NUMBER1 || \
      PER_MAGICNUM(per) == EH_MAGIC_NUMBER2 || \
      PER_MAGICNUM(per) == EH_MAGIC_NUMBER3))

#define PER_IS_MSVC_PURE_OR_NATIVE_EH(per) \
    (PER_CODE(per) == EH_EXCEPTION_NUMBER && \
     PER_NPARAMS(per) == EH_EXCEPTION_PARAMETERS && \
     (PER_MAGICNUM(per) == EH_MAGIC_NUMBER1 || \
      PER_MAGICNUM(per) == EH_MAGIC_NUMBER2 || \
      PER_MAGICNUM(per) == EH_MAGIC_NUMBER3 || \
      PER_MAGICNUM(per) == EH_PURE_MAGIC_NUMBER1))

#endif /* __WINE_EHDATA_H */
