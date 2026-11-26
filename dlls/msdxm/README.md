# MSDXM compatibility boundary

The legacy desktop MediaPlayer class is documented under MSDXM.OCX (CLSID
22D6F312-B0F6-11D0-94AB-0080C74C7E95); later implementations use WMPDXM.DLL.
MSDXM.DLL also appears in historical binary records, but the precise export
table and version-specific relationship to the OCX remain unverified.

This WATER module is an experimental, unregistered DLL. Its class factory can
be called directly for a name-based IDispatch adapter backed by WMP.DLL.
The private dispatch IDs are not historical ABI values. No legacy dual
interface IID, type library, OLE hosting interface or COM registration is
advertised until those contracts are recovered. Do not redirect the legacy
CLSID to the modern WMP class.

References:
- https://learn.microsoft.com/en-us/previous-versions/windows/desktop/wmp/using-the-windows-media-player-7-or-later-object-model
- https://learn.microsoft.com/en-us/previous-versions/windows/desktop/wmp/detecting-windows-media-player-from-an-application
- https://sources.debian.org/src/wxpython3.0/3.0.1.1%2Bdfsg-2/src/msw/mediactrl_am.cpp
