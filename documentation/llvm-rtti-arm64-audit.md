# LLVM Microsoft C++ RTTI: Windows ARM64 ABI audit

## Evidence recovered (14 October 2026)

Water's Windows ARM64 PE path invokes the LLVM fork maintained at
`retrochristian5000/LLVM` through the
`toolchains/llvm-project` submodule. RTTI in C++ binaries is **not**
the same switch as `LLVM_ENABLE_RTTI` for building LLVM itself.

Target distinction:
- `aarch64-pc-windows-msvc`: Microsoft C++ ABI; RTTI Complete Object
  Locator (COL) uses image-relative 32-bit RVAs on 64-bit Windows.
- `x86_64-pc-windows-msvc`: same 64-bit COL representation, separate
  COFF relocation family.
- `i686-pc-windows-msvc`: five-field COL with direct 32-bit pointers.
- `aarch64-w64-windows-gnu`: different Itanium C++ RTTI and mangled
  symbols. Do not merge with MSVC RTTI just because both targets
  produce ARM64 Windows PE/COFF images.
- `arm64ec` hybrid RTTI and runtime interoperability need their own
  ABI checks; macOS `arm64e` PAC is an unrelated host ABI.

## Reproduction and verified invariants

Using the installed Clang 17 and LLD 17 on a Linux x86_64 test host,
a C++17 class with two polymorphic base classes was compiled for
`aarch64-pc-windows-msvc` and `x86_64-pc-windows-msvc`.
The generated ARM64 LLVM IR contained
`%rtti.CompleteObjectLocator = type { i32, i32, i32, i32, i32, i32 }`,
with locator signature 1 and the two vftable offsets 0 and 8.
The RTTI reference relocation entries in the ARM64 COFF object
were `IMAGE_REL_ARM64_ADDR32NB`, not incorrect 64-bit RVA fields.
The RTTI TypeDescriptor itself retained the appropriate 64-bit
vftable pointer. `__RTDynamicCast` had the Windows MSVC runtime
signature with a 32-bit vfptr delta.

A separately linked ARM64 PE32+ smoke DLL using test-only stub
implementations of external C++ runtime functions had
`Machine=0xAA64`, 4 KB section alignment, 512-byte file
alignment, and both COL records reachable from the final image.
Parsing the PE sections found two 24-byte COLs whose
`pSelf` RVAs pointed to their own locations, with `offset`
fields 0 and 8. Both resolved the intended Derived TypeDescriptor
and ClassHierarchyDescriptor.

**This is a structural Clang+LLD test, not a claim that the stubbed
DLL is usable on Windows.** A real Windows ARM64 C++ runtime with
`__RTDynamicCast`, `__RTtypeid`, exception behavior and
`type_info` semantics must still be exercised.

## Fork regression and chronology

At the time of the audit, the LLVM fork's
`clang/lib/AST/MicrosoftMangle.cpp` already included the
June 2026 fix that limits Microsoft RTTI TypeDescriptor names
for deeply nested template/lambda types. Do not reapply that patch
or blame this known corrected issue for a new failure without a
versioned reproduction. Upstream issue and fix:
https://github.com/llvm/llvm-project/issues/206313
https://github.com/llvm/llvm-project/pull/206317

The fork's comprehensive
`clang/test/CodeGenCXX/microsoft-abi-rtti.cpp` originally had
i386 and x86_64 RUN lines only. A new separate regression,
`clang/test/CodeGenCXX/microsoft-abi-rtti-arm64.cpp`, now checks:
- 64-bit MSVC-style 6-field COL signatures and
  image-relative RTTI, including multiple inheritance;
- 32-bit MSVC 5-field COL as a guard against unwanted ABI mixing;
- GNU Windows/Itanium RTTI independently;
- `__RTDynamicCast` and `__RTCastToVoid` declarations;
- actual ARM64 COFF `IMAGE_REL_ARM64_ADDR32NB` RTTI relocations.

Regression is committed on LLVM `main`:
https://github.com/retrochristian5000/LLVM/commit/fbb23cce1e131993ab88e8945cf8cce857bc1a6e

The local Clang 17 source fixture was compiled and its IR/object
inspected, but the new fork `lit` regression has **not yet been
executed with the rebuilt LLVM fork compiler**. This is a
coverage repair, not a confirmed CodeGen defect fixed by changing
runtime semantics. Do not invent an ABI patch without an actual
failing test.

## Submodule continuity guard

As audited, Water's `toolchains/llvm-project` gitlink still
references `3ca674b360059614fc3572fa81bdf25ea9b9f5d5`, while
the LLVM fork `main` is
`fbb23cce1e131993ab88e8945cf8cce857bc1a6e`.
The pinned commit is **68 first-parent-ancestry commits behind**
the fork branch in GitHub's compare view. An automatic pin
advance would silently include many unrelated compiler and
linker changes. It was deliberately **not** advanced as part
of this narrow RTTI investigation. Review or test that larger
LLVM upgrade separately before moving Water's gitlink.

Next validation: run the LLVM `lit` test in the fork's actual
bootstrap, then compile a Water C++ DLL with `-frtti` and
Microsoft-compatible runtime/import libraries, inspect RTTI RVAs
in the final PE image, and run `dynamic_cast`/ `typeid`
on Windows ARM64. Avoid `/GR-` or `-fno-rtti-data` for
modules that require polymorphic runtime type identification.
