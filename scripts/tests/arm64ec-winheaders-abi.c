/*
 * Microsoft Windows SDK architecture-macro and ABI layout contract.
 * Compile against Water's own Windows headers, never a host SDK.
 * ARM64EC is an x64-compatible data ABI, not a native ARM64 context.
 */
#include <windows.h>
#include <stddef.h>

#if defined(__arm64ec__) || defined(_M_ARM64EC)
# if !defined(_ARM64EC_) || !defined(_AMD64_)
#  error ARM64EC must expose both SDK _ARM64EC_ and _AMD64_ markers
# endif
# if defined(_ARM64_) || defined(_X86_)
#  error ARM64EC must not take the native ARM64 or i386 header branch
# endif
_Static_assert(sizeof(CONTEXT) == sizeof(AMD64_CONTEXT), "ARM64EC uses x64 CONTEXT");
_Static_assert(sizeof(CONTEXT) == 0x4d0, "ARM64EC/x64 CONTEXT layout");
_Static_assert(_Alignof(CONTEXT) == 16, "ARM64EC/x64 CONTEXT alignment");
_Static_assert(sizeof(ARM64EC_NT_CONTEXT) == sizeof(AMD64_CONTEXT), "EC context overlay size");
_Static_assert(_Alignof(ARM64EC_NT_CONTEXT) == 16, "EC context overlay alignment");
_Static_assert(offsetof(ARM64EC_NT_CONTEXT, X0) == offsetof(AMD64_CONTEXT, Rcx),
               "ARM64EC X0 maps to x64 RCX");
_Static_assert(offsetof(ARM64EC_NT_CONTEXT, X1) == offsetof(AMD64_CONTEXT, Rdx),
               "ARM64EC X1 maps to x64 RDX");
_Static_assert(offsetof(ARM64EC_NT_CONTEXT, Sp) == offsetof(AMD64_CONTEXT, Rsp),
               "ARM64EC SP maps to x64 RSP");
_Static_assert(offsetof(ARM64EC_NT_CONTEXT, Pc) == offsetof(AMD64_CONTEXT, Rip),
               "ARM64EC PC maps to x64 RIP");
_Static_assert(sizeof(RUNTIME_FUNCTION) == 12, "ARM64EC uses x64 runtime-function layout");
_Static_assert(sizeof(MEMORY_BASIC_INFORMATION) == 48, "ARM64EC uses 64-bit memory layout");
#elif defined(__aarch64__) || defined(_M_ARM64)
# if !defined(_ARM64_) || defined(_ARM64EC_) || defined(_AMD64_)
#  error Native ARM64 must not inherit ARM64EC/x64 SDK architecture markers
# endif
_Static_assert(sizeof(CONTEXT) == 0x390, "native ARM64 context layout");
_Static_assert(sizeof(RUNTIME_FUNCTION) == 8, "native ARM64 runtime-function layout");
#elif defined(__x86_64__) || defined(_M_AMD64)
# if !defined(_AMD64_) || defined(_ARM64_) || defined(_ARM64EC_)
#  error Native x64 must expose only the AMD64 SDK architecture marker
# endif
_Static_assert(sizeof(CONTEXT) == 0x4d0, "x64 context layout");
_Static_assert(sizeof(RUNTIME_FUNCTION) == 12, "x64 runtime-function layout");
#elif defined(__i386__) || defined(_M_IX86)
# if defined(_AMD64_) || defined(_ARM64EC_) || defined(_ARM64_)
#  error i386 must not inherit 64-bit SDK architecture markers
# endif
# if !defined(_X86_)
#  error i386 must preserve the Windows x86 SDK architecture marker
# endif
_Static_assert(sizeof(void *) == 4, "i386 pointer size");
#else
# error Target architecture unsupported by this ABI test
#endif

/* Never use the internal ARM64EC COFF object-machine value as a PE image
 * machine constant. Microsoft winnt.h intentionally omits that identifier. */
#ifdef IMAGE_FILE_MACHINE_ARM64EC
# error ARM64EC internal COFF machine must not be exposed as PE image machine
#endif
int whp_header_probe(void) { return 0; }
