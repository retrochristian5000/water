# Welcome to Windows 98 compatibility frontend

This is an initial, clean-room Windows 98 Welcome navigation frontend,
not an extraction or copy of Microsoft's original executable. It neither
parses nor redistributes WELCOME.DAT, .WBM images, or multimedia assets.

The documented topics are Register Now, Connect to the Internet, Discover
Windows 98, and Maintain Your Computer.

| Topic | Program | Status |
| --- | --- | --- |
| Register Now | regwiz.exe /r | Separate registration wizard, may be absent |
| Connect to the Internet | icwconn1.exe | Water frontend, currently Internet Properties only |
| Discover Windows 98 | discover.exe | Tour component, often on Win98 CD media |
| Maintain Your Computer | tuneup.exe | Separate Maintenance Wizard, may be absent |

Clicking Begin launches only the selected external helper. Missing
components produce a notice instead of false success.

The optional startup checkbox controls **only** the current user's
HKCU\Software\Microsoft\Windows\CurrentVersion\Run value named
WaterWelcome. It never modifies Windows 98's HKLM Run entry named
Welcome. The /R switch respects this per-user opt-in.

## Manual regression matrix (not yet executed)

1. Build programs/welcome/welcome.exe with Water's PE Win32 toolchain.
2. Verify the four topics, keyboard navigation, Begin, and Close.
3. With helpers absent, confirm informative missing-component handling.
4. With Water's icwconn1 installed, confirm Internet Properties opens.
5. Toggle the startup checkbox; inspect HKCU Run/WaterWelcome and verify
   welcome.exe /R is quiet while disabled.
6. Compare on licensed Windows 98 FE media; investigate WELCOME.DAT and
   .WBM behavior separately before claiming full compatibility.
