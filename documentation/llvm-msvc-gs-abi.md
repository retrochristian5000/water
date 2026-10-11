# Microsoft MSVC /GS and /Gs: separate compiler and runtime ABIs

Status (11 October 2026): **OPTION-TRANSLATION DEFECT FIXED AND PUSHED IN LLVM FORK; NEW LLVM BUILD/LIT TESTS NOT EXECUTED.** The available host Clang 17 was used for baseline target probes, not to assert that the updated fork passed. This topic builds on the [LLVM PE/COFF audit](llvm-pe-coff-abi.md) and [Microsoft event ABI audit](llvm-ms-events-abi.md).

See also the [`/F` executable stack-reserve ledger](llvm-msvc-f-stack-abi.md); `/F` controls a PE header value, not a stack-probe threshold or security cookie.

## Do not conflate case or meaning

| MSVC spelling | Behavior | ABI and runtime dependencies |
| --- | --- | --- |
| **`/GS`** (default) | Insert stack-buffer-overrun security cookies and checking code in selected functions; `/GS-` disables it | `__security_cookie`, `__security_check_cookie`, Windows CRT initialization (`__security_init_cookie`), and exception/SEH behavior |
| **`/Gs[size]`** | Set local-stack allocation threshold for invoking guard-page stack probes | `__chkstk` helper, x86 ESP versus x64 RSP allocation semantics, ARM64 prolog/unwind handling, Windows stack guard-page growth |

Microsoft says the 4096-byte default applies to x86/x64/ARM/ARM64. **Explicit bare `/Gs`** means zero on x64 but 4096 on x86/ARM/ARM64. **`/Gs0`** follows the same target split; 1–9 produce D9014 and have the same effective result. 10–2147483647 are explicit thresholds; 2147483648+ is C1049. The fork's new driver diagnostic is semantically equivalent but **does not claim identical MSVC D9014/C1049 numbers or wording**.

Source: [Microsoft /GS](https://learn.microsoft.com/en-us/cpp/build/reference/gs-buffer-security-check?view=msvc-170), [Microsoft /Gs](https://learn.microsoft.com/en-us/cpp/build/reference/gs-control-stack-checking-calls?view=msvc-170), [Microsoft __security_init_cookie](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/security-init-cookie?view=msvc-170).

## Source/root-cause audit

Water's LLVM fork previously declared `/Gs` and `/GsN` as direct aliases for generic `-mstack-probe-size` in `clang/include/clang/Options/Options.td`. The bare option hardcoded `4096` for **all** architectures; `/Gs0` passed zero for **all** architectures; values above INT32_MAX lacked MSVC validation.

Clang's `/GS` path was **already distinct** in `clang/lib/Driver/ToolChains/Clang.cpp`: `OPT__SLASH_GS`/`OPT__SLASH_GS_` controls the `-stack-protector 2` compiler mode.

Clang 17 baseline IR/object probe showed `__security_cookie`, `__security_check_cookie` and `__chkstk` references in COFF for i686/x64/ARM64. On ARM64, generic `-mstack-probe-size=0` did **not** emit the expected `__chkstk` call for a small stack frame; `-mstack-probe-size=1` did. The x86/x64 emitter treats zero differently. This confirms why using a generic zero threshold for non-x64 `/Gs0` is unsound.

In Water's inspected `dlls/msvcrt/msvcrt.spec` and `dlls/vcruntime140/vcruntime140.spec`, no security-cookie or chkstk exports were found. **This is a narrow inventory result, NOT proof that Water is unable to link or initialize these helpers**: CRT implementations, compiler runtimes, static libraries and alternate DLLs need independent symbol/import tests. Neither `__security_init_cookie` nor a stack probe may be replaced with an inert stub.

## LLVM fork correction

LLVM [36c9c27d](https://github.com/retrochristian5000/LLVM/commit/36c9c27d14de14e029c4d002ab44bfe2a097be97) replaces the bare alias with target-aware `/Gs` and `/GsN` translation while preserving the existing generic `-mstack-probe-size` control and independent `/GS` route. It handles:

- bare `/Gs`, `/Gs0`, `/Gs1`–`/Gs9` by x64-vs-other-target rules;
- explicit `/Gs10` through `/Gs2147483647`, with fatal error for larger or nonnumeric values;
- last-specified stack-probe option precedence, without changing `/GS` cookie checks.

LLVM [a561ab08](https://github.com/retrochristian5000/LLVM/commit/a561ab08e3b02cd83a910956d9bf7c75b9a31737) updates existing ambiguous driver tests to pin the target architecture. New `clang/test/Driver/cl-gs-stack-probe.c` checks x86/x64/ARM64 option translations, small-value warnings, overflow, combined flags and precedence.

Run after building Water's pinned LLVM fork:

```sh
llvm-lit -sv clang/test/Driver/cl-gs-stack-probe.c
llvm-lit -sv clang/test/Driver/cl-options.c
```

Compare actual object imports and Win32/Win64/ARM64 unwind info with:

```sh
llvm-readobj --file-headers --symbols --relocations path/to/test.obj
```

## Pending ABI validation (separate lanes)

1. **`/GS`:** verify the selected CRT initializes the process/module security cookie **before** any protected frame; validate x86 fastcall cookie helper symbol, x64 and ARM64 check conventions, SEH/unwind and compiler/runtime versions. Custom `/ENTRY` programs require explicit early initialization. Check that runtime overflows terminate rather than silently return.
2. **`/Gs`:** verify x86 helper can adjust ESP after probing while x64 `__chkstk` leaves stack adjustment to prologue; test ARM64 helper and unwind metadata. Probe at and around threshold and under large `alloca`; confirm guard-page commits on an actual Windows guest.
3. **Version/profile:** /GS and newer MSVC toolchains must not be retrojected into the Windows 98 **original** compiler/CRT environment without attestations. Water's Win9x x86 target, Win64 targets and macOS arm64 host are distinct ABI lanes.
4. **Security boundary:** do not globally suppress `/GS` or probes to fix unresolved CRT symbols. Audit boot-loader, compiler-rt and alternate CRT entry points instead. No full LLVM build, run-time guard page or Water PE executable test was performed in this round.

Microsoft also documents `#pragma check_stack` (32-bit x86/ARM32-specific behavior, not 64-bit) and `#pragma strict_gs_check` as separate surfaces; they were not implemented or changed in this patch.

References: [check_stack](https://learn.microsoft.com/en-us/cpp/preprocessor/check-stack?view=msvc-170), [strict_gs_check](https://learn.microsoft.com/en-us/cpp/preprocessor/strict-gs-check?view=msvc-170).
