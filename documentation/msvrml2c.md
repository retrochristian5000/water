# MSVRML2C.OCX — Windows 98 FE VRML 2.0 Viewer

Status: **FIRST-STAGE COM FACTORY IMPLEMENTED / VIEWER NOT IMPLEMENTED**.
This is evidence tracking and a non-registering COM ABI starting point.
Compile and runtime tests **have not been executed**.

## Historical identity and chronology

- Microsoft's January 27, 1997 announcement identifies the IE VRML 2.0
  Viewer as a licensed derivative of **Intervista WorldView 2.0**.
  The original technology used Direct3D, DirectSound and DirectInput.
  <https://news.microsoft.com/source/1997/01/27/microsoft-licenses-vrml-2-0-c-implementation-from-intervista-software/>
- Microsoft confirmed integration with **Internet Explorer 4.0 in July
  1997**, and discussed it in its August 4 VRML authoring-tools release.
  <https://news.microsoft.com/source/1997/08/04/microsoft-releases-vrml-2-0-authoring-tools-cd/>
- The **Windows 98 First Edition** original cabinet listing records
  `MSVRML2C.OCX`, **2,191,360 bytes**, dated May 11, 1998, listed in
  `WIN98_62.CAB` and spanning into `WIN98_63.CAB`. The setup inventory
  separately identifies `VRML2C.INF`, **4,202 bytes**.
  <https://www.localhost.me.uk/support/windows98/pages/98cabcon.html>
- Microsoft's KB188139 confirms that Windows 98 exposed the VRML 2.0
  Viewer as an optional Windows Setup / Internet Tools component.
  Its IE4 installer could also register the viewer separately on
  Windows 95. Distribution on media is **not** proof of installation.
  <https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/188139>
- The period IE5 installable-components table lists **VRML 2.0 Viewer**
  identifier `{90A7533D-88FE-11D0-9DBE-0000C0411FC3}`.
  Registry records independently call it `VRMLBrowser` in
  `MSVRML2C.OCX`. This is strong evidence of a class identity, but
  it is **not** a recovered full 1998 type library, IID or DISPIDs.
  <https://documentation.help/HTMLREF/installable.htm>
  <https://www.bleepingcomputer.com/forums/t/371471/nasty-rootkit-no-updates-no-installs/>
- Do **not** confuse the Microsoft VRML 2.0 Viewer with
  `danim.dll` (DirectAnimation), `d3drm.dll` (Direct3D Retained Mode),
  `dxtrans.dll`, or the original IE3 **Java-based** Liquid Reality
  initiative. The underlying technologies overlap but the component
  identities and activation contracts are different.

## Safety and interoperability evidence

Microsoft KB188139 documents a historical integration defect:
the installed VRML viewer could incorrectly claim `.tar` and `.gz`
files, causing a VRML parse error instead of normal downloads.
**Do not reproduce those archive extension associations** or hijack
existing mime/file handlers without explicit compatible behavior.

## Water implementation and remaining gaps

Water already has `d3drm.dll`, `danim.dll`, `ole32.dll`,
`oleaut32.dll` and `mshtml.dll`. There was no
`dlls/msvrml2c.ocx` or verified VRML scene parser/viewer.

The new `dlls/msvrml2c.ocx` module exports only the four standard
COM in-process server DLL functions:

- `DllGetClassObject`: recognizes the period VRMLBrowser CLSID,
  returns `IClassFactory` / `IUnknown` on its factory and rejects
  unknown CLSIDs/IIDs with correct failure HRESULTs and cleared
  output pointers;
- `DllCanUnloadNow`: accounts for live factories and server locks;
- `DllRegisterServer`: explicitly returns `SELFREG_E_CLASS`
  while no functional viewer exists. This **does not** register the
  control or displace a functional native viewer;
- `DllUnregisterServer`: non-destructive no-op since Water has not
  registered the CLSID.

`IClassFactory::CreateInstance` does **not** fabricate a viewer:
nonaggregated requests return `E_NOTIMPL` and aggregated requests
return `CLASS_E_NOAGGREGATION`. The module cannot yet display VRML.

Remaining requirements include the 1998 native OCX PE export table,
type library, class/automation IIDs, property DISPIDs, browser host
interfaces (`IOleObject`, `IOleInPlaceObject` as applicable), VRML 2.0
scene parsing and validation, shapes and materials, navigation,
time/event routing, externally referenced resources, any custom
Intervista-specific APIs, DirectX rendering, and legitimate install
paths/registration. Do not guess ABI structure layouts from IE5/XP.

## Targeted tests and next gates

- `dlls/msvrml2c.ocx/tests/main.c` covers the four exported COM
  entry points, factory QI, non-aggregation, lifetime/unload,
  lock/unlock and **intentional lack of viewer activation**.
- First run a Water build on Win32 PE32 x86, and run the test suite
  before claiming build or runtime support.
- Next, extract the genuine FE `MSVRML2C.OCX` and `VRML2C.INF`
  from authorized media and record SHA-256, version resources,
  typelib, registration entries and actual DLL exports.
- With a verified contract, implement at least a standards-compliant
  VRML 2.0 header/scene loading path and a basic rendered object.
  Only then consider enabling class creation and registration.
- Compare with IE4-on-Windows-95 and later versions, preserving
  chronology and negative evidence. No feature is complete merely
  because the OCX module builds or the COM factory exists.
