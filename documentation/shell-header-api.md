# Shell Win32 declarations: AppUserModelID

Water implements `SetCurrentProcessExplicitAppUserModelID` and
`GetCurrentProcessExplicitAppUserModelID` in `dlls/shcore/main.c`, and
exports both through `dlls/shcore/shcore.spec` and
`dlls/shell32/shell32.spec`. Before this change neither API appeared in
`include/shobjidl_core.idl` or `include/shobjidl.idl`.

The canonical signatures are:

- `HRESULT WINAPI SetCurrentProcessExplicitAppUserModelID(PCWSTR)`
- `HRESULT WINAPI GetCurrentProcessExplicitAppUserModelID(PWSTR *)`

Both are **Windows 7 or later**, exported from Shell32. They are not
Windows 98 or Windows XP APIs. The getter returns a COM-allocated string
which the caller frees with `CoTaskMemFree`. Both declarations now live
in `shobjidl_core.idl` so generated `shobjidl_core.h` and its importing
`shobjidl.h` share the API. The preexisting shcore tests exercise runtime
set/get behavior using dynamic loading; they now also type-check the
generated public declarations without adding a static Shell32 dependency
on systems that predate Windows 7.

Microsoft ABI and availability:
- https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-setcurrentprocessexplicitappusermodelid
- https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-getcurrentprocessexplicitappusermodelid

This is a **header-coverage repair**, not a new Win32 implementation.
Future audits should distinguish documented public exports from hidden
ordinal exports, forwarded functions, and stubs, and inspect all relevant
generated IDL headers before declaring an API missing.
