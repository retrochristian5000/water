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

Water currently includes `dxtrans.dll`, `mshtml.dll`, and `d3drm.dll`,
but does **not** yet provide `danim.dll`. The DirectAnimation ActiveX
control's class factories, COM/IDispatch type information, and behavior
engine therefore remain unimplemented. Do not register either documented
DirectAnimation CLSID to `dxtrans.dll`: its existing class factory does
not supply a DirectAnimation object.

The `dxtrans.dll` `DllGetClassObject` entry point now clears its
output pointer when it rejects an unsupported CLSID, instead of leaving
the caller with an invalid interface pointer. Its other unimplemented
exports have not thereby become DirectAnimation functionality.

## Future contract tests

Before introducing `danim.dll` activation or animation behavior,
recover the original DirectAnimation SDK IDL/type library and compare
Windows 98-era COM interfaces and registrations. Then test COM
instantiation, `PixelLibrary`/`MeterLibrary`, `Image` and `Sound`
properties, `Start`, and ActiveX hosting in `mshtml` separately.
A loadable placeholder without these behaviors does not reproduce the
Windows 98 preview experience.
