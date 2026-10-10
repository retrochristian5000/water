# SYSTEM.DRV — Windows 98 FE Win16 system driver audit

Status: **SOURCE-AUDITED / DRIVE AND NULL-TIMER FIX STAGED**
(2026-10-09). This is not a native Windows 98 binary export-table audit.

## Distinguish the original distribution artifacts

The Windows 98 FE CAB inventory contains two distinct entries:

| Distribution | Bytes | Timestamp |
| --- | ---: | --- |
| Windows 98 FE main installation \`SYSTEM.DRV\` | 2,288 | 1998-05-11 20:01 |
| \`MINI.CAB\` legacy compatibility \`SYSTEM.DRV\` | 2,304 | 1996-12-18 17:42 |

These are *separate artifacts*. Differences in size do not establish
their actual NE export sets, function bytes, driver capabilities or ABI.

## Existing Water implementation

\`dlls/system.drv16/system.drv16.spec\` includes:
1 InquireSystem, 2 CreateSystemTimer, 3 KillSystemTimer,
4 EnableSystemTimers, 5 DisableSystemTimers, 6 GetSystemMSecCount,
7 Get80x87SaveSize, 8 Save80x87State, 9 Restore80x87State,
13 INQUIRELONGINTS (stub, explicitly annotated W1.1/W2.0),
and 20 A20_Proc (no-op). An older commented ordinal 14 is also
marked only for W1.1.

**Do not infer** that the historical Windows 1.x/2.x ordinal 13 or 14
survived in the 1998 driver, or that the original driver had no additional
exports. Verify the two authentic NE entry tables first.

Existing partial logic:
- \`InquireSystem(2)\` (single-drive logic) is unsupported;
- timer callbacks use Wine's Win16/WOW callback bridge;
- timer startup and teardown still use a thread lifecycle requiring
  testing; the existing stop uses \`TerminateThread\`;
- x87 save/restore uses an x86 operand-size override for the
  94-byte 16-bit \`FSAVE/FRSTOR\` image. Do not flatten it to an
  arbitrary host FPU-state representation.

## Scoped fixes

- \`InquireSystem(1, drive)\` now constructs a **rooted** \`X:\\\` path
  for \`GetDriveTypeW\`; previously it supplied drive-relative \`X:\`.
  Invalid indexes (26 and above) return \`DRIVE_UNKNOWN\` rather than
  manufacturing non-drive letters.
- \`CreateSystemTimer16\` now rejects a null Win16 callback. Previously
  the timer count increased but no live slot was recorded, preventing
  the count from returning to zero during normal unregister operations.
- All existing ordinal declarations, service mappings, x87 storage,
  and the unsupported one-drive operation remain unchanged.

The test \`scripts/tests/test_system_drv16.py\` compiles the **actual**
three relevant C function bodies against narrow API shims to exercise
drive path, unknown indexes, timer capacity, null callbacks, and
registration/unregistration. It additionally checks the committed
Win16 \`.spec\` ordinal mapping. It does not simulate timer APCs, WOW
segment callbacks, guest Windows 98 behavior or the authentic NE image.

\`\`\`sh
python3 scripts/tests/test_system_drv16.py
\`\`\`

## Further compatibility gates

1. Extract both original \`SYSTEM.DRV\` files and preserve CAB source,
   exact length, SHA-256 and DOS/NE header details.
2. Compare name and ordinal exports from *each* NE entry table with the
   Water \`.spec\`; preserve negative evidence and obsolete ordinals.
3. Confirm Windows 98 FE timer callback register/stack conventions
   in a reproducible 16-bit test program; verify nested and concurrent
   timer create, kill, disable, and cleanup operations.
4. Recover the single-drive \`InquireSystem(2)\` contract before
   attempting to emulate DOS single-drive media changes.
5. Audit the Win16 x87 save/restore buffer and A20 behavior on the
   correct guest ABI before modifying them.

## Sources

- Windows 98 CAB inventory: https://www.localhost.me.uk/support/windows98/pages/98cabcon.html
- Historical system driver API: Norton, *Writing Windows Device
  Drivers* (1992), available via bitsavers.org Windows 3.1 documents.
- Water implementation: \`dlls/system.drv16/system.c\` and
  \`dlls/system.drv16/system.drv16.spec\`.
