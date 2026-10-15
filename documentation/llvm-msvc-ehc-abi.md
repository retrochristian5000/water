# MSVC /EHc: ARM64EC and Windows C++ exception-boundary audit

**15 October 2026 — World History Project / Water.**
State: **LLVM SOURCE MATCH CONFIRMED; WATER WRAPPER PATCH PUBLISHED;
DIRECT CLANG 17 TEST PASSED; WATER BUILD/INTEGRATION UNVERIFIED.**
Continue from the existing /arch, /Gv, /DLL, /SUBSYSTEM and COFF
library ABI ledgers. Neither Win16/286 nor Apple arm64e inherits
Windows ARM64EC compiler flags.

## Microsoft contract

Microsoft `/EHc` is a *source-level unwinding assumption*:
`extern "C"` declarations are assumed not to throw **C++**
exceptions when synchronous `/EHs` is enabled.
It is **ignored alone**, is overridden by `/EHa`, and
`/EHc-` cancels the assumption. Thus:

| Options | Mode and C-linkage behavior |
| --- | --- |
| `/EHs` | C++ unwinding enabled; `extern "C"` may throw |
| `/EHsc` or `/EHs /EHc` | C++ unwinding enabled; C-linkage calls assumed `nounwind` |
| `/EHc` alone | No exception-mode effect |
| `/EHsc-` or `/EHs /EHc-` | Equivalent to `/EHs` for this assumption |
| `/EHa` (even after `/EHc`) | Asynchronous model; C-linkage assumption ignored |

Microsoft warns that throwing through a declaration that
`/EHc` treats as nothrow invokes undefined behavior. This
should not be applied globally in Water merely to reduce
exception tables. Some C ABI callbacks are wrappers around
C++ implementations or can encounter Windows structured
exceptions. C++ exceptions and SEH are **not** interchangeable.

## Source recovery

The pinned LLVM fork's
`clang/lib/Driver/ToolChains/Clang.cpp` already implements
`parseClangCLEHFlags()` with `s`, `a`, `c`, modifier
cancellation and `/EHc` only when synchronous exceptions
are enabled. It forwards `-fexternc-nounwind` internally
for C++ compilation when `EH.Synch && EH.NoUnwindC`.
No LLVM AArch64 calling-convention, PAC, hybrid thunk or
exception-handler edit was justified by this evidence.

**Water-specific gap:** the GCC-style
`tools/winegcc/winegcc.c` parser previously treated
slash-prefixed `/EHsc`, `/EHs` and `/EHc` as
positional input files. It could not forward the Microsoft
exception model into Clang's GNU driver mode. Simply
passing `-fexternc-nounwind` to the GNU driver is
also wrong: Clang 17 accepts it only as an internal cc1
option, via `-Xclang -fexternc-nounwind`.

**Targeted repair published:** `winegcc` now consumes
`/EHs`, `/EHc`, `/EHc-`, `/EHsc`,
`/EHsc-` and `/EHa` with ordered modifier semantics.
The PE C++ compiler gets `-fexceptions` and
`-fcxx-exceptions` when appropriate; `-Xclang
-fexternc-nounwind` is emitted only for active
synchronous /EHs plus /EHc. It rejects non-Windows-PE
and Win16 `-m16` use, and requires Clang for active
exception-mode forwarding instead of sending Clang-only
flags into GCC. The `/EHa` mapping uses
`-fasync-exceptions`, but **does not establish full
Microsoft asynchronous SEH cleanup equivalence** in
LLVM. `/EHr` remains unimplemented and is explicitly
rejected by this wrapper; it is separate from /EHc
and should be audited independently.

## Direct Clang 17 evidence

The standalone fixture
`scripts/tests/ehc-arm64ec-ir.sh` executed successfully
for i686, x64, native ARM64, and ARM64EC Windows targets.

A C++ function owning a destructor-bearing stack object
and calling an unresolved `extern "C"` function was
compiled at O0 with Microsoft's SEH/C++ exception model.
With `/EHs`-equivalent Clang flags, LLVM IR contained
`invoke void @whp_c_api()` and
`@__CxxFrameHandler3`. With `/EHsc`-equivalent flags,
the call became `call void @whp_c_api()` with a
`nounwind` call attribute, and no exceptional edge
for that C call.

Separate Clang-cl 17 `-###` tests on all four targets
confirmed that `/EHsc` selects `-fexternc-nounwind`,
`/EHs` does not, standalone `/EHc` is ignored,
`/EHa` selects async exceptions, and `/EHsc-`
cancels the c modifier. The ARM64EC object fixture
with a catch handler referenced `__CxxFrameHandler3`
and retained the ARM64EC-decorated C++ function symbol.
These are **compiler/IR/object checks**, not an
end-to-end Windows-on-Arm exception propagation test.

`tools/winegcc/tests/msvc-eh.sh` was also added for
the wrapper's full option matrix and target guards.
It requires a freshly compiled `winegcc`, unavailable
to this session, so it has not been executed.

Run from the Water repository root:

```sh
sh scripts/tests/ehc-arm64ec-ir.sh
sh tools/winegcc/tests/msvc-eh.sh /path/to/built/winegcc
```

## Scope and unverified ABI concerns

The flag does **not** change the ARM64EC calling convention,
its x64-compatible data layout, the thunk dispatch helper
names or the final PE machine type. Windows ARM64EC
unwinding also requires correct `.pdata` and `.xdata`
metadata across hybrid entry/exit thunks, including
Microsoft's `save_any_reg` operation for SIMD-register
restoration. The `/EHc` IR tests **do not verify**
that cross-architecture runtime-unwind path, and they
do not resolve prior `__os_arm64x_dispatch_ret` issues.

Win16 MZ/NE, Intel 80286 protected-mode descriptors,
and 16-bit selector/call frames are independent from
modern `/EHc` and C++ SEH metadata.

## Primary sources

- Microsoft /EH exception model:
  https://learn.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model
- Microsoft exception specification behavior:
  https://learn.microsoft.com/en-us/cpp/cpp/exception-specifications-throw-cpp
- Microsoft ARM64EC ABI/unwind notes:
  https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
- Microsoft ARM64 exception metadata:
  https://learn.microsoft.com/en-us/cpp/build/arm64-exception-handling
