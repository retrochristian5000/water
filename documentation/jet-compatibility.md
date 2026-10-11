# Microsoft JET / Windows 98 and WebTV audit

Jet Red is the Access/DAO database-engine family; Jet Blue is ESENT. They are distinct, not interchangeable. Water already has dlls/esent/esent.spec but its Jet APIs are currently stubs. Do not misrepresent them as a working Jet Red implementation.

Target candidates:
- Jet Red 3.5/3.51: MSJET35.DLL and related DAO 3.5 / OLE DB components.
- Jet Red 4.0: MSJET40.DLL / MSJETOLEDB40.DLL and DAO 3.6.
- Jet Blue: ESENT.DLL (separate ESE API and database format).

Separate [WaveTop Data Broadcasting, WebTV for Windows and Intel Intercast evidence](windows98-broadcast.md) before attributing WebTV databases or runtime DLL dependencies to any of the three features.
The [Gemstar/WebTV EPG evidence](webtv-gemstar-epg.md) confirms guide licensing/data-loader concepts but **not** any Jet Red or ESENT dependency.

Windows 98 WebTV's electronic program guide and Intel Intercast support are documented; direct Jet dependence has NOT yet been verified. Inspect original extracted Win98/WebTV installer binaries and registration records before implementing an engine. Win98 targets PE32 i386; modern PE32+ ARM64 evidence must not be substituted.

Read-only import scan:
    python3 scripts/inspect-jet.py --json /path/to/extracted/webtv > jet-imports.json
    python3 -m unittest discover -s scripts/tests -p test_inspect_jet.py

The inspector reports direct and delay imports, image machine, and SHA-256. It does not execute binaries, decode CABs, or detect COM registration, dynamically constructed LoadLibrary imports, or transitive dependencies. No Jet import alone is not evidence that Jet was never used.

Sources:
- Microsoft, April 8, 1998 (WebTV/Intercast): https://news.microsoft.com/source/1998/04/08/intel-microsoft-to-align-efforts-on-interactive-tv-programming/
- Microsoft, June 25, 1998 (WaveTop): https://news.microsoft.com/source/1998/06/25/microsoft-windows-98-to-ship-with-wavephores-wavetop-consumer-data-broadcast-service/
- Microsoft, ESE / Jet Red-vs-Blue: https://learn.microsoft.com/en-us/windows/win32/extensible-storage-engine/extensible-storage-engine
- MS KB Q240377 (Jet 3.5): https://jeffpar.github.io/kbarchive/kb/240/Q240377/
