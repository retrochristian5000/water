# DirectAnimation and Windows 98 compatibility

DirectAnimation is the DirectX Media animation runtime used by early
Internet Explorer content. It is not DirectShow, Direct3D Retained Mode,
modern Windows UI Animation, or the DirectX Transform core.

## Period evidence and entry points

- Microsoft announced DirectAnimation on December 10, 1997 as part of
  DirectX Media 5.1 and described Internet Explorer 4.0 integration and
  the planned inclusion in Windows 98.
  https://news.microsoft.com/source/1997/12/10/microsoft-releases-directanimation/
- Microsoft's archived DirectX Transform SDK article identifies
  `danim.dll` as the home of the DirectAnimation ActiveX control.
  It gives the control CLSID
  `{69AD90EF-1C20-11D1-8801-00C04FC29D46}` and describes
  `PixelLibrary`, `MeterLibrary`, and `DAStatics`.
  https://learn.microsoft.com/en-us/archive/msdn-magazine/2001/march/graphics-manipulate-digital-images-in-internet-explorer-with-the-directx-transform-sdk
- The period DirectAnimation SDK documents another viewer-control CLSID,
  `{B6FFC24C-7E13-11D0-9B47-00C04FC2F51D}`. Do not assume the
  controls have identical interface contracts.
  https://sistemas.afgcoahuila.gob.mx/software/Visual%20Basic%206.0%2032%20y%2064%20bits/Common/Tools/VB/Unsupprt/Danim/help/da/DA_E0003.htm

## Water ownership and outstanding gaps

Water includes `dxtrans.dll`, `mshtml.dll`, and `d3drm.dll`, and now
builds a separate `danim.dll` module. The new module recognizes the two
historically documented viewer CLSIDs and implements COM `IClassFactory`
identity, reference counting, server locks, and unload accounting.
**Viewer instance creation is not implemented**: `CreateInstance` returns
`E_NOTIMPL`, and the scripting `IDispatch`, `IOleObject`, animation
behaviors, and rendering remain absent.

`DllRegisterServer` deliberately returns `SELFREG_E_CLASS` until those
features work; this avoids hijacking a functional native DirectAnimation
registration with a non-rendering Water placeholder. `DllUnregisterServer`
does not remove any native registration. Do not register either documented
DirectAnimation CLSID to `dxtrans.dll`: its class factory does not supply
a DirectAnimation object.

The `dxtrans.dll` `DllGetClassObject` entry point now clears its
output pointer when it rejects an unsupported CLSID, instead of leaving
the caller with an invalid interface pointer. Its other unimplemented
exports have not thereby become DirectAnimation functionality.

## Public header recovery (October 13, 2026)

Water previously provided `dlls/danim` but no `include/danim.h`.
A January 31, 1999 DirectMedia SDK header inventory explicitly names
`danim.h` and inventories `CLSID_DAViewerControl`,
`CLSID_DAViewerControlWindowed`, `CLSID_DAStatics` and numerous other
DA class symbols. That inventory **does not publish their numeric GUIDs**
or the COM interfaces' vtable layouts:
https://groups.google.com/g/microsoft.public.multimedia.directx.dshow.programming/c/qTs0CFJCYG0

The March 27, 1998 Microsoft DirectAnimation SDK provides the
unwindowed viewer's CLSID
`{B6FFC24C-7E13-11D0-9B47-00C04FC2F51D}`:
https://sistemas.afgcoahuila.gob.mx/software/Visual%20Basic%206.0%2032%20y%2064%20bits/Common/Tools/VB/Unsupprt/Danim/help/da/DA_E0003.htm

A March 2001 Microsoft magazine article identifies
`{69AD90EF-1C20-11D1-8801-00C04FC29D46}` as a DirectAnimation
control in `danim.dll`; independent ActiveX registration inventories
label this the windowed variant:
https://learn.microsoft.com/en-us/archive/msdn-magazine/2001/march/graphics-manipulate-digital-images-in-internet-explorer-with-the-directx-transform-sdk
https://github.com/davestewart/maxscript/blob/master/3D/3ds2ae/03_3ds/dev/interface/ax%20property%20grids/axcontrols.ms

A Microsoft 2006 security bulletin lists dozens of CLSIDs under
`danim.dll`, and others separately under `daxctle.ocx` and
`lmrt.dll`. **This is later provenance, not a Windows 98 FE SDK ABI
manifest;** it must not be used to assign guessed 1998 names or
interface IIDs:
https://learn.microsoft.com/en-us/security-updates/securitybulletins/2006/ms06-067

The new *partial* `include/danim.h` declares only the two numerically
verified viewer class IDs:
`CLSID_DAViewerControl` and `CLSID_DAViewerControlWindowed`.
`libs/uuid/uuid.c` now defines them through the normal Wine
`DEFINE_GUID` mechanism. Both `dlls/danim/main.c` and its tests use
those public names instead of duplicate anonymous GUID constants.
The test validates the literal values and that the two IDs are distinct.

**Still missing:** original DirectAnimation IDL/TLB, `IID_IDAViewerControl`
(and other verified interface IIDs), automation `IDispatch` DISPIDs,
`IDAStatics` interface signatures, `DA_TIMER_SOURCE` enum values,
C/C++ COM vtables, and the actual `PixelLibrary` / `MeterLibrary`
implementations. The 1998 SDK readme mentions the
`IDAViewerControl::TimerSource` property and `DA_TIMER_SOURCE`
type, but doesn't establish the enum's numeric values:
https://sistemas.afgcoahuila.gob.mx/software/Visual%20Basic%206.0%2032%20y%2064%20bits/Common/Tools/VB/Unsupprt/Danim/relnotda.htm

Adding header declarations does **not** make the ActiveX viewer usable
or justify registering it on a Windows installation. Before filling in
vtable positions, GUIDs, or DISPIDs, capture versioned 1998-era
type-library/IDL evidence and compare variants. In particular,
do not silently merge the standard viewer with its windowed variant or
with Direct3D Retained Mode, DirectX Transform, or modern Windows
animation APIs.

## Future contract tests

Before introducing `danim.dll` activation or animation behavior,
recover the original DirectAnimation SDK IDL/type library and compare
Windows 98-era COM interfaces and registrations. Then test COM
instantiation, `PixelLibrary`/`MeterLibrary`, `Image` and `Sound`
properties, `Start`, and ActiveX hosting in `mshtml` separately.
A loadable placeholder without these behaviors does not reproduce the
Windows 98 preview experience.

## First-stage danim.dll implementation

`dlls/danim` is a **buildable, intentionally incomplete** COM server
foundation. Its four standard DLL exports and class-factory lifecycle are
covered by `dlls/danim/tests/main.c`. The regression tests keep an
expected-failure marker for constructing a viewer instance.

The 1998 SDK's VBScript example names `PixelLibrary`, `MeterLibrary`,
`Image`, `Sound`, and `Start` on the viewer control; these functions
are **not** operational in the first stage:
https://sistemas.afgcoahuila.gob.mx/software/Visual%20Basic%206.0%2032%20y%2064%20bits/Common/Tools/VB/Unsupprt/Danim/help/da/DA_E0003.htm

Recover a Windows 98 FE-era DirectAnimation type library before assigning
unverified DISPIDs, IIDs, registration values, or rendering behavior.
Later Windows XP danim.dll inventories contain more interfaces and exports
and are useful as cross-checks, not automatic Windows 98 specifications.
