# Network Neighborhood and My Network Places

## Windows-version boundary

Windows 95/98 called the virtual network folder Network Neighborhood.
Windows Me, Windows 2000 and XP used My Network Places.
Former Microsoft KB267732 documents a real difference: Windows 98
Network Neighborhood showed workgroup computers; Windows Me My
Network Places showed workgroups instead.
https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/267732

Both names refer to the classic network namespace CLSID
{208D2C60-3AEA-1069-A2D7-08002B30309D}. CSIDL_NETWORK identifies
the virtual root. CSIDL_NETHOOD identifies a different physical
shortcut folder and must not be confused with the network root.
https://learn.microsoft.com/en-us/windows/win32/shell/csidl

## Water state

dlls/shell32/shfldr_netplaces.c owns the network virtual folder;
dlls/mpr owns WNet provider functions and dlls/netapi32 owns management
APIs. This change repairs constructor reference counting and failed IID
cleanup, validates Shell output pointers, and handles the existing
Entire Network PIDL's display text and EntireNetwork parsing alias.

Workgroup/server/share enumeration in EnumObjects is still absent.
BindToObject for network-type PIDLs is not yet provider-aware.
Do not manufacture fake network entries while browsing is missing.

Future compatibility tests should separately compare Windows 98 FE,
Windows Me, Windows 2000 and XP default names, namespace registration,
icon display, workgroup grouping and WNet provider enumeration. Do not
create a second CLSID or change version behavior without those tests.
