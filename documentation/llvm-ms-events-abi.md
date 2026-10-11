# Microsoft C++ event extensions: __hook, __unhook, __event, __raise

Status: **UNSUPPORTED EVENT GENERATION CONFIRMED; FAIL-CLOSED ATTRIBUTE GUARD PATCHED;
LLVM BUILD AND LIT REGRESSIONS NOT YET EXECUTED** (11 October 2026).

This is an extension/ABI audit, not an implementation of Microsoft events.
See [LLVM PE/COFF ABI ledger](llvm-pe-coff-abi.md) for previous
`__interface`/novtable fixes. Water's LLVM fork is
[retrochristian5000/LLVM](https://github.com/retrochristian5000/LLVM).

## Evidence recovered from the pinned LLVM source

| Surface | Direct observation | Consequence |
| --- | --- | --- |
| `clang/include/clang/Basic/TokenKinds.def` | Has `KEYWORD(__interface, KEYMS)` but **no keywords** `__hook`, `__unhook`, `__event`, `__raise` | Adding only tokens would be insufficient without parser, Sema and code generation |
| `clang/lib/Parse/ParseDeclCXX.cpp` and `SemaDeclCXX.cpp` | Implement several `__interface` rules but not Microsoft's unified event operators | Native interface support does not imply event support |
| `clang/lib/Parse/ParseDeclCXX.cpp::ParseMicrosoftAttributes` | Previously **silently discarded unknown single-bracket MS attributes**, including `[event_source(native)]`, `[event_receiver(com)]` | A source/receiver class could compile with no generated event state or COM subscription logic |
| Clang 17 local Windows-target smoke tests | The `__interface` control compiled; `__event`/ `__raise` were rejected; `__hook` and `__unhook` produced *undeclared identifier* errors; an attributed class **without events** compiled without warning and lacked an AST event attribute | Verified baseline behavior on i386/x64/ARM64 targets; these local tests predate the patch |
| LLVM `clang/lib/CodeGen/MicrosoftCXXABI.cpp` | Prior `__interface` implicit `novtable` fix exists | **Separate ABI issue** already repaired; do not duplicate |
| Microsoft Learn `__hook`, `__unhook`, `__event`, `__raise` and `event_source` docs | Native event dispatch versus COM connection point semantics documented | Requires real ABI/layout and lifetime tests on MSVC originals |

## Microsoft language contracts and ABI fault surfaces

- **Native event source:** `[event_source(native)]`, `__event` and
  `__raise` must produce a callable multicast event implementation;
  `__hook` associates receiver methods and `__unhook` disconnects
  them. Source layout depends on `optimize=size` vs `optimize=speed`.
  Explicitly test native storage offsets, x86 member-pointer
  representations, handler order, dispatch/reentrancy and detach semantics.
  Microsoft warns this native model is **deprecated**.
- **COM event source:** `[event_source(com)]` plus attributed interfaces
  are a different path: `IConnectionPointContainer`, `IConnectionPoint`,
  `Advise`, `Unadvise`, event IID, lifetime/refcount and apartment
  behavior must match. `[event_receiver(com, layout_dependent=true)]`
  changes signature/name matching. **Do not synthesize this using a
  generic C++ callback list or conflate it with `IDispatch` alone.**
- **Managed events:** Microsoft also documents managed uses. Native
  `-fms-extensions`/Windows COFF builds are not evidence that Clang has
  C++/CLI delegate/event support.
- **Version and dialect:** the Microsoft event attributes are explicitly
  incompatible with `/permissive-`; preserve MSVC compiler flags and
  compiler version before claiming a binary ABI mismatch. Class pointer
  sizes and member-call conventions differ between i386, x64 and ARM64.
- **Operator forms:** `__hook` accepts event/receiver and COM
  interface forms; `__unhook` additionally supports a source-only
  detach. Both return a `long` error status under documented native/COM
  usage. `__raise` must reject non-events. None of these semantics is
  currently implemented in this LLVM fork.

## Actual protective source fix, not a fake implementation

LLVM fork commit
[dc8ecdbd](https://github.com/retrochristian5000/LLVM/commit/dc8ecdbd2b82859e2c89a5eddfae123e0c94b36d)
makes `ParseMicrosoftAttributes` emit a hard error for explicitly named
`event_source` or `event_receiver` instead of **silently** discarding
them. It excludes HLSL, retains the compatibility treatment of other
unknown Microsoft bracket attributes and does not invent a
`__hook` builtin, COM subscription, or event runtime.
Follow-up commit
[8ad49c70](https://github.com/retrochristian5000/LLVM/commit/8ad49c70a037910e60bb3b86de8f93acac4057a9)
corrects the `-verify` diagnostic regexes.

A new `clang/test/Parser/ms-event-unsupported.cpp` declares native
and COM event-source/receiver attributes, and checks the explicit
diagnostic with six combinations: Windows i386, x64, ARM64, each in
C++11 and C++26. Unknown Microsoft attributes remain intentionally
nonfatal to avoid unrelated compatibility breakage.

### Verification discipline

After rebuilding Water's **pinned** LLVM fork, run:

```sh
llvm-lit -sv clang/test/Parser/ms-event-unsupported.cpp
llvm-lit -sv clang/test/SemaCXX/ms-interface.cpp
llvm-lit -sv clang/test/CodeGenCXX/ms-interface-implicit-novtable.cpp
```

The existing Clang 17 smoke tests establish **pre-patch failure modes**.
No full rebuilt LLVM compiler, official MSVC side-by-side layout capture,
LLVM lit run, Water bootstrap or Windows guest runtime test has been
completed for this event change. Branch/gitlink integrity verifies
publication, **not** compiler correctness.

The next implementation needs a staged event representation, semantic
checks, MSVC cross-compiled/original-output comparison, native multicast
code generation, and **separate** COM connection point synthesis.
Do not silently use a guessed ABI or substitute `std::function`
under the same keyword.

## Sources

- Microsoft, [`__hook`](https://learn.microsoft.com/en-us/cpp/cpp/hook?view=msvc-170).
- Microsoft, [`__unhook`](https://learn.microsoft.com/en-us/cpp/cpp/unhook?view=msvc-170).
- Microsoft, [`__event`](https://learn.microsoft.com/en-us/cpp/cpp/event?view=msvc-170).
- Microsoft, [`__raise`](https://learn.microsoft.com/en-us/cpp/cpp/raise?view=msvc-170).
- Microsoft, [`event_source`](https://learn.microsoft.com/en-us/cpp/windows/attributes/event-source?view=msvc-170).
- Microsoft, [`event_receiver`](https://learn.microsoft.com/en-us/cpp/windows/attributes/event-receiver?view=msvc-170).
- Microsoft, [Event handling in COM](https://learn.microsoft.com/en-us/cpp/cpp/event-handling-in-com?view=msvc-170).
- Microsoft, [Event handling](https://learn.microsoft.com/en-us/cpp/cpp/event-handling?view=msvc-170).
