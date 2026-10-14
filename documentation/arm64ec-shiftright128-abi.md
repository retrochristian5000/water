# ARM64EC `__shiftright128`: shift-count ABI boundary

## Investigation: 14 October 2026

Water's `include/winnt.h` exposes a compatibility helper named
`ShiftRight128(low, high, shift)`. For ARM64EC, the same header maps
the **x64-named** `__shiftright128` macro to this helper:

```c
#if defined(__x86_64__)
# if defined(__arm64ec__)
#  define __shiftright128 ShiftRight128
# else
#  define ShiftRight128 __shiftright128
# endif
#endif
```

Clang 17 with `--target=arm64ec-pc-windows-msvc` defines both
`__x86_64__` and `__arm64ec__`, matching ARM64EC's x64 source
compatibility. The same target uses **ARM64 machine code** and does
not automatically implement the x64 hardware intrinsic. The LLVM
fork's `clang/lib/Headers/intrin0.h` intentionally declares
`__shiftright128` only for
`defined(__x86_64__) && !defined(__arm64ec__)`.
An arbitrary unguarded declaration on ARM64EC can instead become
an unresolved external symbol, so the Water header's explicit
alias matters.

### Confirmed bug and correction

Microsoft documents the x64
`unsigned __int64 __shiftright128(unsigned __int64 low,
unsigned __int64 high, unsigned char shift)` intrinsic as returning
the low half of a 128-bit right shift with **shift count modulo 64**.
In particular, `__shiftright128(0, 1, 64) == 0`, **not 1**.

Microsoft's published requirement table lists **x64**, not ARM64EC.
The ARM64EC compatibility alias is Water's own implementation; its
semantic parity is established by comparison with the documented
x64 contract, not by assuming Microsoft provides an ARM64EC
instruction with that name.

The prior Water inline helper used:

```c
return ((unsigned __int128)hi << 64 | lo) >> shift;
```

This violated the x64 contract for `shift >= 64`:
- `shift=64` incorrectly returned `hi`;
- `shift=65..127` returned upper-half bits rather than the
  modulo-64 combination;
- `shift=128..255` could shift a 128-bit value by its width or more,
  which is undefined behavior in C/C++.

Corrected expression:

```c
return ((unsigned __int128)hi << 64 | lo) >> (shift & 63);
```

The public function name, argument order, `DWORD64` return, `BYTE`
count, and x86/x64 native-intrinsic paths are unchanged. The
six-bit mask is deliberate: **modulo 64, not modulo 128**.

### Local reproduction and verification levels

Clang 17 LLVM IR for the original ARM64EC fallback contained an
unmasked `lshr i128` with its shift argument zero-extended from
`i8`. With the corrected helper, it contains
`and i8 %count, 63` before the logical 128-bit shift.

A portable C executable built and run in the tool container with
Clang 17 passed **6,400 combinations**: five low-half samples,
five high-half samples, and every `BYTE` shift from 0 through 255.
The expected reference computes
`s = shift & 63`, then for `s > 0`
`(low >> s) | (high << (64 - s))`, with a special case for
`s == 0` to avoid a 64-bit C shift by 64.

Separate freestanding compile probes for
`arm64ec-pc-windows-msvc` and `aarch64-pc-windows-msvc`
confirmed a 64-bit return, count masking and successful ARM64
instruction generation without needing an x64
`__shiftright128` runtime symbol. The native x64
`__shiftright128` intrinsic lowers to `llvm.fshr.i64`, which
implements the modulo-width behavior.

The committed regression extracts **the actual inline body** from
Water's local `include/winnt.h` (not an independently rewritten
copy), compiles and runs it for all 6,400 input combinations, then
compiles that body for ARM64EC and ordinary ARM64 Windows and checks
that the count mask survives. It also checks the x64 intrinsic
lowering and Water's ARM64EC alias. Run it in a Water checkout:

```sh
python3 scripts/tests/shift-right128-arm64ec.py
```

**Verification boundaries:** The standalone compiler/semantic probes
passed. The committed full extraction regression has not yet run
against an actual local Water checkout, and a real Windows ARM64EC
process has not executed the helper. No change was made to the LLVM
fork's intrinsic lowering, because these probes located the
difference in Water's own fallback. Do not confuse this bug with
ARM64EC PE metadata, PAC, ARM64X hybrid linking, or 128-bit data
alignment; it concerns shift-count semantics.

## Provenance

- Microsoft: [`__shiftright128` reference](https://learn.microsoft.com/en-us/cpp/intrinsics/shiftright128)
- Microsoft: [`__shiftleft128` example](https://learn.microsoft.com/en-us/cpp/intrinsics/shiftleft128)
- Water: `include/winnt.h`, `include/msvcrt/intrin.h`
- LLVM fork: `clang/lib/Headers/intrin0.h`

Follow-up: test actual Water header compilation under ARM64EC and
native ARM64 from its configured build, including the public
`__shiftright128` macro route; then confirm Win64 C++ callers
receive consistent inline expansion and no external x64 intrinsic
dependency. Keep any ARM64EC LLVM compiler bug report separate from
this confirmed Water header implementation error.
