# WIN87EM.DLL — Windows 98 FE / Win16 x87 compatibility audit

**Status: AUDITED; x87 rounding/integer-pop repaired; save/info/restore
explicitly unsupported.** This is not a complete Windows 3.x/9x emulator.

## Historical distribution and ABI

- WIN87EM.DLL is a **Win16 floating-point coprocessor/emulation DLL**.
  Microsoft's Windows 98 FE disk inventory lists a 12,800-byte copy.
  Windows 3.0/3.1 applications may import it even with a hardware FPU.
- Water has these existing Win16 ordinal exports in
  dlls/win87em.dll16/win87em.dll16.spec:
  1 __fpMath (-register); 3 __WinEm87Info; 4 __WinEm87Restore;
  5 __WinEm87Save (all three latter exports are Pascal -ret16).
  No new ordinals or Win32 aliases were introduced.
- The Windows 3.1 SDK documents these three APIs under spellings
  __Win87EmInfo / __Win87EmRestore / __Win87EmSave, with an **int**
  return of zero on success, nonzero on failure. Preserve the actual
  Water module's published export spellings until the original
  Windows 98 NE entry table is verified.
- The SDK's Win87EmSaveArea combines **94 bytes** from the x87 FSAVE
  format **plus an emulator-state block and register stack**.
  SYSTEM.DRV's 94-byte save buffer is not a substitute. An accurate
  size and layout must not be guessed from 94 alone.

## Confirmed source gaps

- Original ordinal-1 __fpMath subcommand 6 used two FSTCW reads,
  an OR into a write-only inline-asm operand and no explicit
  rounding-bit selection based on AX; it was susceptible to incorrect
  x87 control changes. Subcommand 7 hardcoded a zero result because
  FISTP was commented out, leaving the x87 stack unchanged.
- Source previously emitted unconditional x86 inline assembly even
  on Apple arm64 hosts. The affected instructions are now guarded.
- Ordinals 3, 4, 5 were silent FIXME stubs declared as **void**, despite
  -ret16 and the SDK's integer return contract. They now return a
  deterministic **nonzero error** while implementation remains
  unsupported, and do not corrupt user-provided state buffers.

## Current scoped behavior

- The native x86/x86_64 fast path can round ST0 using the AX rounding
  bits (0x0c00) while saving/restoring the original x87 control word.
- It can pop ST0 as a signed 32-bit integer with the selected rounding
  mode, returning DX:AX via Water's existing guest context.
- On non-x86 hosts, the helper reports unsupported rather than
  attempting to execute x86 instructions or claim a guest x87 context.
- These helpers manipulate the **host x87** stack only; they are not
  the guest FPU abstraction for TCG, ARM64e, or other non-x86 hosts.
  The future guest path needs an explicit virtual x87 state owner.
- The save/info/restore functions intentionally **do not report
  success** until the native emulator SaveEmArea ABI is implemented.
  This is an honest error path, not full removal of those stubs.

## Verification

    python3 scripts/tests/test_win87em_fpu.py

The portable C test builds the actual private FPU helper, pushes
floating-point values, and checks rounding, signed integer pop,
control-word restoration, null output and non-x86 fallback. A separate
source/spec check prevents silent renumbering and wrong return types.
These are host-level tests only, not Windows 98 FE guest execution.

Remaining:
1. Inspect an original 12,800-byte Windows 98 NE image and compare
   its exports, module flags, and SDK differences.
2. Recover the full Win87EmSaveArea layout and state ownership; then
   implement __WinEm87Info / Save / Restore with accurate status and
   no uninitialized or overlong writes.
3. Verify __fpMath command 6/7 behavior against real Win16 samples,
   including exception/overflow and signed boundary cases.
4. Integrate with the VDM/TCG guest x87 state rather than the host
   x87 register stack, including ARM64e support.
5. Preserve differences between SYSTEM.DRV x87 and WIN87EM.DLL APIs.

## Source basis

- Microsoft KB Q70758, WIN87EM.DLL in Windows 3.x:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/70758
- Microsoft Windows 3.1 SDK (1992), Win87EmSaveArea and API structures:
  https://www.bitsavers.org/pdf/microsoft/windows_3.1/Microsoft_Windows_3.1_SDK_1992/PC28915-0492_Programmers_Reference_Volume_1_199204.pdf
- Windows 98 FE distribution listing, Microsoft KB Q191057:
  https://helparchive.huntertur.net/document/106792
- Water: dlls/win87em.dll16/win87em.c, dlls/system.drv16/system.c,
  dlls/krnl386.exe16/fpu.c.
