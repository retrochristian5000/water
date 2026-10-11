# Windows 98 broadcast components: WebTV, WaveTop and Intercast

Status: **source-confirmed feature distinction; binary/API attribution pending**.
Platform: Windows 98 (1998), i386 Win9x PE32 with analog TV tuner.
Scope: this ledger records component boundaries, *not* new APIs or working emulation.
Related: [JET investigation](jet-compatibility.md).

## Distinguish the three clients and the common infrastructure

| Named feature | Maker / position | Documented purpose | Evidence |
| --- | --- | --- | --- |
| WebTV for Windows | Microsoft Windows 98 component | TV viewing, interactive programming, electronic program guide | [Microsoft/Intel, 8 Apr 1998](#sources); [ATI TV tuner manual](#sources) |
| WaveTop Data Broadcasting | WavePhore client/service bundled with Windows 98 | VBI-broadcast delivery of selected news, weather, stocks, entertainment, and software to a PC, then local display/cache | [Microsoft/WavePhore, 8 Oct 1997 and 25 Jun 1998](#sources) |
| Intel Intercast | Intel interactive-TV enhancement, integrated with WebTV for Windows | HTML enhancements delivered alongside broadcast television | [Microsoft/Intel, 8 Apr 1998](#sources); [Microsoft KB Q238961](#sources) |
| Windows 98 Broadcast Architecture | Shared OS/hardware layer | Tuner access and reception of television/data; platform used by WaveTop and WebTV | [Microsoft/WavePhore, 8 Oct 1997](#sources) |

**Installation-level separation is attested:** the contemporary ATI ALL-IN-WONDER
PRO manual and AITech's compatibility instructions identify *WaveTop Data
Broadcasting* and *WebTV for Windows* as two distinct check boxes beneath the
WebTV/Windows Setup group. A shared installer group does **not** prove that the
two runtime components are inseparable, and it does not prove a shared
database backend.

### WaveTop's distinct transport

The October 1997 and June 1998 Microsoft/WavePhore releases describe data
embedded in an analog television signal's **vertical blanking interval (VBI)**.
WaveTop software decodes the received data, caches content, and displays it
through a browser-style interface. WaveTop relied on the Windows 98 Broadcast
Architecture, a tuner board, and PBS National Datacast's US broadcast network.
Its client was also described as supporting IP and Winsock; this is not proof
that WaveTop requires an ordinary Internet download for received content.

Named WaveTop content channels in the June 1998 release: NewsTop, StockTop,
KidsTop, TechTop, FamilyTop, FunTop. These are **service/content labels**, not
Windows DLLs or APIs. Do not infer executable names from them.

WebTV for Windows instead includes TV viewing and an electronic program
guide. ATI describes program-guide updates through Internet or cable delivery;
Intel Intercast adds program-specific interactive HTML. Shared VBI/tuner
facilities can support different clients without merging their application
logic, content formats, or registration data.

## Historical chronology (BC/AD convention irrelevant to these dates)

- **8 October 1997:** Microsoft and WavePhore announce WaveTop inclusion and
  Windows 98 Broadcast Architecture; promise tuner/VBI reception.
- **8 April 1998:** Microsoft and Intel announce Intercast integration beginning
  with *WebTV for Windows*, not a rename of WaveTop.
- **25 June 1998:** Microsoft and WavePhore announce shipping of WaveTop in
  Windows 98; describe PBS distribution, caching, and named content channels.
- **1998-era ATI manual:** installation lists two separate check boxes:
  *WaveTop Data Broadcasting* and *WebTV for Windows*.

Do not merge the 1997 service *reach projection* with the 1998 reported
operational coverage; they are distinct dated assertions.

## Original-media inventory: leads only, not verified imports

A secondary Windows 98 CAB directory listing names **wavetop.cab** and
**wavetop.exe**, **waveui.exe**, **waveum.exe**, **tvx.exe**,
**tvwakeup.exe**, and **vidsvr.exe**. These are **inspection candidates**;
their ownership, producer versions, binary hashes, and cross-dependencies
remain **unverified** until reproduced from a dated original CD/OEM image.
Do not classify `waveui.exe` as WebTV or WaveTop solely from spelling.

WavePhore VBI receiver DLL names reported in contemporary or later software
indexes are also **unverified for a specific Windows 98 release**. Treat every
filename as an intake lead until original-image validation.

## Reproducible investigation — preserve the component boundary

1. Record retail/OEM edition, release/date, setup language, CD/CAB name,
   original SHA-256, and device/tuner driver version. The US WaveTop service
   and its availability must not be generalized to every localized edition.
2. Examine Windows Setup INF files, optional-component declarations and
   separate installed-file lists for the two check boxes. Note shared files
   **once** with provenance; don't fabricate unrelated partitions.
3. Extract originals using a read-only workflow and inspect executable type,
   PE32 i386 headers, import/delay-import tables, COM registration, and
   `LoadLibrary` strings. Absence from the import table is not proof of
   absence of a runtime or COM dependency. Watch for Win16/NE and LE/VxD
   components that a PE-only scan cannot decode.
4. Scan each *verified* package/file set separately for Jet Red vs Jet Blue:
   `python3 scripts/inspect-jet.py --all --json /path/to/extracted/wavetop`
   and `python3 scripts/inspect-jet.py --all --json /path/to/extracted/webtv`.
   Compare JSON reports, SHA-256 values, and actual import lists. The scanner
   covers PE direct and delay imports only.
5. On a Windows 98 test machine, verify each checkbox combination,
   installation order, unregister/uninstall changes, and tuner/no-tuner
   behavior with immutable before/after snapshots. Distinguish missing
   broadcast signal from failure to initialize the DLL or UI.
6. Only after actual binaries identify imports, map the owning Windows 98
   APIs and ABIs into Water. Do not fake WaveTop, WebTV or MSJET exports
   without signatures, ordinals and behavior evidence.

## Open evidence and negative results

| Question | Status |
| --- | --- |
| Two different Windows Setup components? | **Confirmed** by period vendor instructions |
| Shared Windows 98 broadcast architecture? | **Confirmed** by Microsoft announcements |
| WaveTop delivery via VBI with cache and browser interface? | **Confirmed** in Microsoft/WavePhore release |
| Intercast enhancement integrated with WebTV? | **Confirmed** in Microsoft/Intel announcement |
| WebTV binary directly imports WaveTop? | **Unknown** — no original PE/registration analysis |
| WaveTop uses Jet Red, or WebTV uses Jet Red? | **Unknown** — no original import/COM analysis |
| Either component uses Jet Blue/ESENT? | **Unknown** |
| Exact original setup files, DLL exports and service interfaces? | **Unknown** — inspect original media |
| Both clients run independently when installed alone? | **Unknown** — separate check boxes do not prove runtime independence |

## Sources

- [Microsoft/WavePhore, 8 October 1997: agreement and broadcast architecture](https://news.microsoft.com/source/1997/10/08/microsoft-to-include-wavephores-wavetop-software-in-windows-98/) — corporate primary announcement.
- [Microsoft/Intel, 8 April 1998: Intercast and WebTV](https://news.microsoft.com/source/1998/04/08/intel-microsoft-to-align-efforts-on-interactive-tv-programming/) — corporate primary announcement.
- [Microsoft/WavePhore, 25 June 1998: shipping and transport details](https://news.microsoft.com/source/1998/06/25/microsoft-windows-98-to-ship-with-wavephores-wavetop-consumer-data-broadcast-service/) — corporate primary announcement.
- [ATI ALL-IN-WONDER PRO manual, Windows 98 section](https://drivers.amd.com/manuals/Aiwpro.pdf) — period manufacturer install procedure; lists two check boxes.
- [AITech, Net-TV 98 / WebTV install notes](https://www.aitech.com/support/addendum.htm) — manufacturer support instructions; lists two check boxes.
- [Microsoft KB Q238961 archived text, Intercast installation](https://www.betaarchive.com/wiki/index.php?title=Microsoft_KB_Archive%2F238961) — later archival transcription; confirm against original media.
- [Third-party Windows 98 CAB directory listing](https://www.localhost.me.uk/support/windows98/pages/98cabcon.html) — secondary filename leads; **not authenticated originals**.
