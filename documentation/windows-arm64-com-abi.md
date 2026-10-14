# COM header ABI integrity: ordinary ARM64 Windows

## Scope and recovered source evidence (14 October 2026)

This continues Water's Windows PE and COM audits. Distinguish
`aarch64-pc-windows-msvc` (ordinary native Windows ARM64 COM), x64,
i386/Win9x, Windows GNU, and the ARM64EC hybrid ABI. A COM interface
pointer is a pointer-sized value; the C representation starts with a
vtable pointer and follows a fixed method order. GUID values and COM
interface method signatures are independent of the CPU used to
compile them, but pointer fields and aggregate alignment are not.

Source-of-truth paths:
- `include/unknwn.idl` defines the IUnknown base methods and its IID.
- `include/oaidl.idl` defines IDispatch, VARIANT, SAFEARRAY, and the
  wire-marshalling forms. Generated `unknwn.h`/`oaidl.h` are emitted
  by WIDL into a configured build, and are not handwritten in the repo.
- `include/objbase.h` defines the C versus C++ COM declaration
  macros. `include/combaseapi.h` implements IID_PPV_ARGS.
- `include/guiddef.h` owns 16-byte GUID and REF* definitions.
- `include/oleauto.h` owns Automation helper macros and functions.

## Confirmed missing SDK macro: DECLARE_INTERFACE_IID

Microsoft's SDK supports both:
- `DECLARE_INTERFACE_IID(iface, iid)` for a **root** interface,
  whose C++ type gets the UUID attribute.
- `DECLARE_INTERFACE_IID_(iface, base, iid)` for a **derived**
  interface, preserving C++ inheritance.

Water defined the second but **omitted the first**. This could
produce missing-macro compile failures for SDK-style custom COM
interfaces (or lead to declarations without the expected
`__uuidof` metadata). Repair in `include/objbase.h`:

- The C++ form declares the interface with `DECLSPEC_UUID(iid)`
  and `DECLSPEC_NOVTABLE`, without inventing a parent interface.
- The C / `CINTERFACE` form delegates to the existing
  `DECLARE_INTERFACE`, preserving the vtable pointer and manual
  method slots without changing COM machine layouts.
- Prior macro definitions are undefined alongside the other
  COM declaration macros to avoid stale definitions.

This adds **declaration syntax**, not an IID symbol definition,
class factory, registration or new Windows API. A true COM class
still needs its original IID, vtable implementation, and runtime
contract; new arbitrary test UUIDs are not historical Windows IIDs.

Reference Microsoft SDK `combaseapi.h` macro definitions:
https://github.com/tpn/winsdk-10/blob/master/Include/10.0.14393.0/um/combaseapi.h

## ABI controls added

`scripts/tests/com-abi-headers.c` asserts these contracts on
ordinary Windows ARM64, x64 and i386 using the generated WIDL
headers, with **C**, **native C++**, and **C++ CINTERFACE** modes:

| Contract | ARM64/x64 | i386 |
| --- | --- | --- |
| GUID | 16 bytes | 16 bytes |
| IUnknown C vtable | 3 pointer slots | 3 pointer slots |
| IDispatch C vtable | 7 pointer slots | 7 pointer slots |
| VARIANT (memory representation) | 24 bytes | 16 bytes |
| SAFEARRAY descriptor with one bound | 32 bytes | 24 bytes |
| SAFEARRAY.pvData offset | 16 | 12 |
| SAFEARRAY.rgsabound offset | 24 | 16 |
| DISPPARAMS | 24 bytes | 16 bytes |
| VARIANT_BOOL | 2 bytes | 2 bytes |

The fixture also compiles the restored root-interface macro,
checks a derived interface's explicit C/CINTERFACE slot layout
and C++ inheritance, and verifies `__uuidof` and
`IID_PPV_ARGS` for MSVC-ABI C++ interfaces. `IWaterRoot` is an
invented **test-only** GUID/interface and is not exposed as
a Water DLL COM registration.

Use a configured Water build that has already generated
`oaidl.h`, `unknwn.h`, and `objidlbase.h`, then run:

```sh
WHP_COM_GENERATED_INCLUDE=/path/to/build/include \
  sh scripts/tests/com-abi-headers.sh
```

The script intentionally does not substitute host SDK headers
when generated Water headers are missing. It exits 77 with a
skip explanation in that case. It supports
`WHP_COM_CC`/`WHP_COM_CXX` overrides.

## Verification and limits

The **restored macro's exact C++ UUID attribute form** was
independently compiled with installed Clang 17 targeting
`aarch64-pc-windows-msvc`, `x86_64-pc-windows-msvc`, and
`i686-pc-windows-msvc`. All three minimal self-contained
UUID/interface syntax probes passed.

The source patch and regression files are published to Water.
**Not verified:** running the regression against an actual
WIDL-generated Water header tree, generating the full project,
COM class registration, COM apartment initialization or
marshalling an actual runtime object between processes.
The test does not validate ARM64EC or COM's separate RPC/NDR
wire formats. Wire `VARIANT`/`SAFEARRAY` structures in IDL
must not be confused with in-process layouts.

Future investigations must separately verify COM server
exports, CLSIDs/IIDs, proxy/stub registration, marshaling,
`IDispatch::Invoke` argument types, and ARM64EC interface
interoperability. Missing IID/VTable signatures are not a
license to infer them from interface names, particularly for
the earlier partial DirectAnimation `danim.h` recovery.

Reference contracts:
- Microsoft IUnknown:
  https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nn-unknwn-iunknown
- Microsoft IDispatch:
  https://learn.microsoft.com/en-us/windows/win32/api/oaidl/nn-oaidl-idispatch
- Microsoft SAFEARRAY:
  https://learn.microsoft.com/en-us/windows/win32/api/oaidl/ns-oaidl-safearray
- Microsoft SDK `combaseapi.h`:
  https://github.com/tpn/winsdk-10/blob/master/Include/10.0.14393.0/um/combaseapi.h
