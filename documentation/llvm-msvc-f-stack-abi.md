# Microsoft /F executable stack reserve: clang-cl to LLD PE/COFF

Status (11 October 2026): **SOURCE PATCHES PUSHED; REBUILT FORK AND NEW LIT/PE TESTS UNVERIFIED**. Related: [MSVC /GS versus /Gs](llvm-msvc-gs-abi.md), [PE/COFF ABI baseline](llvm-pe-coff-abi.md). No reset: reuses Water's previously pinned LLVM submodule and Windows architecture matrix.

## Distinct ABI contracts

| Option | Owns | Boundary |
| --- | --- | --- |
| `/Fnumber` or `/F number` (compiler) | Executable's PE `SizeOfStackReserve` | Converted into the linker's `/STACK:reserve`; joined or separate, decimal/hex/octal C-style notation, minimum 1 byte, rounded **up to four-byte alignment** |
| `/STACK:reserve[,commit]` (linker) | PE header reserve and commit | EXE only, 1 MiB reserve and 4 KiB commit defaults; explicit `/link /STACK` must override `/F`; `.def` `STACKSIZE` has its own override |
| `/Gs` (compiler) | Stack-probe threshold and `__chkstk` invocation | Does not reserve memory and must remain independent |
| `/GS` (compiler) | Security-cookie instrumentation | Does not set stack reserve or probe threshold |

A reserve is virtual address space, not immediate physical allocation. For PE32
(i386/Win9x) both reserve and commit fields are 32-bit, but PE32+ x64/ARM64
has 64-bit fields. Stack guard-page growth and runtime CRT helper calling
conventions remain separate tests. Microsoft explicitly describes earlier
Windows NT/95/98 commit behavior; do not import Windows 11 behavior
into older loaders without testing.

## Recovered defect and concrete patches

LLVM's `clang/include/clang/Options/Options.td` already recognized
`_SLASH_F` as `CLJoinedOrSeparate<"F">`, but
`clang/lib/Driver/ToolChains/MSVC.cpp` did **not** consume or forward it
to the linker. A local Clang 17 `clang-cl /F12345 -###` test warned
`argument unused during compilation`. The fork now converts valid
values to `-stack:VALUE` before `/link` arguments, rounds to
four-byte multiples, rejects zero and invalid notation, guards overflow,
and skips `/STACK` for DLL outputs. Explicit `/link /STACK` takes
precedence. It does **not** implement stack probes or security cookies.

Local cross-PE checks against system LLD 17 showed:
- `/STACK:5` produced reserve 5, so the compiler must round `/F5`
  before forwarding as `/STACK:8`.
- `/STACK:4294967297` **silently truncated to 1** in PE32 x86,
  while 64-bit PE32+ fields retained the full amount.
- In the patched fork, `lld/COFF/Writer.cpp` now rejects out-of-range
  PE32 reserve **and commit** fields before opening the output, including
  direct `/STACK` and `.def` paths.

Fork commit:
https://github.com/retrochristian5000/LLVM/commit/481c053c9f7df10cb292a33d0bdb8bd96d9069bf

## Run against the rebuilt pinned toolchain

```sh
llvm-lit -sv clang/test/Driver/cl-f-stack.c
llvm-lit -sv lld/test/COFF/stack-pe32-overflow.test
llvm-lit -sv lld/test/COFF/stack.test
python3 scripts/tests/llvm-pe-stack.py /path/to/pinned/llvm/bin
```

The Water Python harness builds freestanding x86, x64 and ARM64 executable
fixtures through **actual clang-cl plus matching lld-link**, then decodes
both PE fields and checks joined/separate numeric forms, explicit
linker precedence, invalid numbers and overflow. It does not run Windows
binaries or test guard-page handling. The baseline Clang/LLD 17 tests
exposed defects; this harness **has not passed against the rebuilt
modified fork yet**.

Outstanding: inspect Water's true Win9x PE32 executable entry point
and DOS/NE boundaries; verify `/F` with LTO, `.def` `STACKSIZE` override
and `/LD` behavior, and runtime stack-overflow handling under the
target Windows guest. Do not assume changing a PE reserve creates
or commits memory on a macOS arm64 host.

## Microsoft primary references

- [`/F` compiler switch](https://learn.microsoft.com/en-us/cpp/build/reference/f-set-stack-size?view=msvc-170)
- [`/STACK` executable stack allocations](https://learn.microsoft.com/en-us/cpp/build/reference/stack-stack-allocations?view=msvc-170)
- [`STACKSIZE` in a .def file](https://learn.microsoft.com/en-us/cpp/build/reference/stacksize?view=msvc-170)
- [Older `/STACK` wording referring to Windows NT/95/98](https://learn.microsoft.com/en-us/cpp/build/reference/stack?view=msvc-170)
