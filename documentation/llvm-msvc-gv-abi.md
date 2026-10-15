# Microsoft `/Gv` and ARM64EC calling-convention boundary

**Audit date:** October 15, 2026. Continuation of Water's LLVM/COFF
and 286/Win16 ABI evidence ledgers; no target family is reset.

## Microsoft contract

Microsoft `/Gv` sets the default calling convention to
`__vectorcall` for eligible nonmember, nonvariadic functions on
**x86 and x64** with SSE2 or newer. Explicit `__cdecl`, `__stdcall`,
or `__fastcall` overrides and `main`/varargs retain their normal
conventions. With C linkage, x86/x64 `__vectorcall` symbols have a
`name@@bytecount` suffix, unlike x86 `__stdcall`'s
`_name@bytecount`. Passing SIMD and floating-point parameters in
different registers makes the choice an actual *calling ABI*, not
just a spelling preference.

The ARM64EC ABI follows a bridge to x64 but **does not implement
x64 `__vectorcall`**. Microsoft says generating code for an
explicit ARM64EC `__vectorcall` function is unsupported. This
does **not** mean that the Clang frontend must refuse all declarations
of `__vectorcall` function-pointer types. LLVM added type parsing
for compatibility with the Microsoft STL in upstream PR #87725,
while deliberately retaining the code-generation error. Do not
simplify this into a target-wide parser rejection.

## Direct Clang 17 / LLD 17 evidence

With `clang-cl --target=<triple> /Gv /c` and a function of
signature `int whp_vector(int, double)`:

| Target | COFF definition symbol |
|---|---|
| i686-pc-windows-msvc | `whp_vector@@12` |
| x86_64-pc-windows-msvc | `whp_vector@@16` |
| aarch64-pc-windows-msvc | `whp_vector` |
| arm64ec-pc-windows-msvc | `#whp_vector` |

The ordinary ARM64/ARM64EC outputs demonstrate that Clang 17
**silently ignores `/Gv` on those targets**; they are not evidence
that ARM64EC secretly implements vectorcall. The official `/Gv`
reference restricts the option to x86/x64 and describes ARM
target handling as ignored. Whether Microsoft's particular
ARM64EC driver rejects an otherwise-ignored `/Gv` switch was
**not independently tested**. No LLVM driver behavior was
changed just to guess at that distinction.

Negative/exception controls: explicitly `__cdecl` x86 symbol
`_whp_cdecl` and x64 symbol `whp_cdecl`; varargs and `main`
did not acquire `@@` suffixes. `lld-link` built no-entry DLLs
and their `.lib` import archives for x86/x64 vectorcall
exports. Separate import clients referenced
`__imp_whp_vector@@12` and `__imp_whp_vector@@16`, then
linked successfully against the corresponding libraries.
The ARM64EC compiler accepted a `__vectorcall` *pointer typedef*
but rejected a function definition at object-generation time
with an unsupported calling-convention diagnostic. These
operations do **not** demonstrate a functional ARM64EC/x64
vectorcall call bridge.

## Source-confirmed Water defect and minimal correction

`tools/winebuild/parser.c` calls
`remove_stdcall_decoration()` for `.def` export names and
internal aliases. Before this fix, that helper in
`tools/winebuild/utils.c` recognized the **last** `@number`
on i386 without excluding a preceding `@`. For
`whp_vector@@12`, it could mistake vectorcall for stdcall,
mutate the stored name to `whp_vector@`, and generate an
incorrect internal link name. By contrast, on x64 and ARM64EC
the same helper already returned without changing the input;
**there was no demonstrated x64 suffix truncation**.

The fix makes the helper reject suffix parsing when the last
`@` is preceded by another `@`, leaving vectorcall names
unaltered. Regular x86 stdcall `name@bytes` parsing is
unchanged. This is a **symbol/DEF reconstruction** fix;
it does not add unsupported ARM64EC vectorcall machine code,
replace native ARM64 with x64, or affect Win16 far-call
semantics. Water's separate ARM64EC `/lib /machine:arm64ec`
and double-indexed import archive logic remain unchanged.

Test source:
`scripts/tests/gv-vectorcall-abi.sh` validates Clang COFF
symbol naming, x86/x64 import archives, ARM64EC type parsing
versus code-generation rejection, and optional real-Water
`.def` round-tripping.

```sh
sh scripts/tests/gv-vectorcall-abi.sh
sh scripts/tests/gv-vectorcall-abi.sh /path/to/built/winebuild
```

The standalone Clang/LLD 17 fixture operations were executed
successfully. The new script, and its optional newly built Water
`winebuild` round-trip portion, were **not executed** as a
whole in this round. Treat integration verification as pending.

## Protected-mode and hybrid ABI guard

Intel 80286 protected mode predates SSE2 and the MSVC x86
`__vectorcall` convention. An i386 PE32 COFF machine code
does not imply a Win16/NE selector, segment descriptor, or
80286 return-frame layout. Never route `/Gv` results into
Win16 far-call relays. Equally, a valid x64 `name@@N`
import library cannot make an ARM64EC function follow the
x64 vectorcall register convention automatically. Hybrid
entry/exit thunk support and OS dispatch still need distinct
ABI evidence.

## References

- Microsoft `/Gd /Gr /Gv /Gz`:
  https://learn.microsoft.com/en-us/cpp/build/reference/gd-gr-gv-gz-calling-convention
- Microsoft `__vectorcall`:
  https://learn.microsoft.com/en-us/cpp/cpp/vectorcall
- Microsoft ARM64EC conventions:
  https://learn.microsoft.com/en-us/cpp/build/arm64ec-windows-abi-conventions
- LLVM vectorcall type-parsing design (2024):
  https://github.com/llvm/llvm-project/pull/87725
