# DirectShow AX module build and export contract

**Scope:** PE DLL/.AX build and ABI validation for Water's DirectShow
modules; separate from a working DirectShow graph or television capture
driver. An \`.ax\` filename is **not** a different PE executable format.
It is a Windows DLL with a filter-specific extension; the Windows loader
and COM treat its exports and registrations as a DLL.

## Verified Water build ownership

- \`dlls/l3codecx.ax/Makefile.in\` sets \`MODULE = l3codecx.ax\`, defines
  C and IDL sources, and has \`l3codecx.ax.spec\`. The IDL emits class
  registration information; C code implements a class factory and
  \`IFilterMapper2::RegisterFilter\` for MPEG Layer-3 decoding.
- \`dlls/ksproxy.ax/Makefile.in\` sets \`MODULE = ksproxy.ax\` and
  provides \`ksproxy.ax.spec\`, but no functional KSProxy implementation.
  Stub exports must not be reported as functional TV hardware support.
- \`tools/makedep.c\` uses \`replace_extension(module, ".dll", ".spec")\`.
  Its \`replace_extension\` helper in \`tools/tools.h\` appends \`.spec\`
  if the name doesn't end in \`.dll\`. Therefore the existing .AX modules
  correctly use **\`l3codecx.ax.spec\` and \`ksproxy.ax.spec\`**, rather
  than arbitrary \`l3codecx.spec\` or renamed \`.dll\` files.
- \`tools/makedep.c\` invokes \`winegcc -shared\` for non-executables;
  \`tools/winegcc/winegcc.c\` preserves an explicitly supplied \`.ax\`
  output filename and lets the DLL build mode work for that extension.

## Added PE/COFF regression

\`scripts/tests/llvm-pe-dll.py\` now cross-links a **synthetic DirectShow
filter DLL named \`water_probe.ax\`** as well as the existing DLL and
client EXE test. The AX fixture explicitly uses x86 stdcall functions,
and a \`.def\` file exports the **undecorated** COM names:

- \`DllGetClassObject\`
- \`DllCanUnloadNow\`
- \`DllRegisterServer\`
- \`DllUnregisterServer\`

The test validates the PE32/PE32+ machine class, \`IMAGE_FILE_DLL\`,
export-directory DLL name **\`water_probe.ax\`**, exact named export
set, \`/NOENTRY\`, and the COFF import-library archive header.
It runs against **x86, x64 and ARM64**, optionally with ThinLTO or
Full LTO. These modern target ABIs are **format controls**; they do
not imply Windows 98 could run ARM64 DirectShow.

Run with matching LLVM tools:

\`\`\`sh
python3 scripts/tests/llvm-pe-dll.py /path/to/llvm/bin
python3 scripts/tests/llvm-pe-dll.py /path/to/llvm/bin --lto thin
python3 scripts/tests/llvm-pe-dll.py /path/to/llvm/bin --lto full
\`\`\`

### Unverified until a full Water build and runtime test

- Actual Water \`l3codecx.ax\` and \`ksproxy.ax\` binaries have not
  been built as part of this regression. It tests Clang/LLD's linking,
  not the full Wine \`makedep\` invocation or \`winebuild\` spec generation.
- \`DllRegisterServer\` existence is not evidence that COM class
  registration or Filter Mapper discovery works. \`InprocServer32\`
  must point to the **\`.ax\` module** (not a renamed DLL), and separate
  filter-category/pin registrations may be needed.
- Win9x i386 \`stdcall\` names must remain undecorated at the export
  boundary; COM methods and interface layout still require runtime
  testing. \`ksproxy.ax\` currently contains stub exports, not a BDA
  topology or streaming implementation.

## Sources

- Microsoft, [Implementing DllRegisterServer](https://learn.microsoft.com/en-us/windows/win32/directshow/implementing-dllregisterserver).
- Microsoft, [Layout of the Registry Keys](https://learn.microsoft.com/en-us/windows/win32/directshow/layout-of-the-registry-keys).
- Microsoft, [DLL Functions](https://learn.microsoft.com/en-us/windows/win32/directshow/dll-functions).
