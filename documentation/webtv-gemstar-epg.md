# GEMSTAR and WebTV for Windows electronic program guide (EPG)

Status: **confirmed Microsoft/Gemstar contractual connection and
contemporary attribution to the Windows PC WebTV guide; Win98 binary mapping
pending**. This is a guide/data-system ledger, not a declaration that a
working Gemstar decoder exists in Water.

Related: [WebTV / WaveTop / Intel Intercast boundaries](windows98-broadcast.md);
[Closed Caption Decoder](closed-caption-decoder.md);
[JET/ESENT audit](jet-compatibility.md).

## Contemporaneous source recovery

| Date | Witness | Narrow supported finding | Limits |
| --- | --- | --- | --- |
| 9 January 1998 (filed 12 January) | Gemstar International, StarSight Telecast and Microsoft, *Definitive Agreement*, exhibit 10.3 to SEC 8-K, accession `0001017062-98-000045` | The agreement expressly gives **Windows 98 and WebTV Plus** as Microsoft EPG legacy product examples (§5.7.1); covers Gemstar data loaders, guide credit, data transport, technical cooperation | Commercial terms include redactions and exceptions; commitments are not proof of a particular version's installed files or UI |
| Windows 98 documentation, updated 11 February 1999 | Microsoft, *Turn your computer into a TV* | Explicit **WebTV for Windows** Program Guide; tuner, channel scanning and updating listings; Windows PC is different from WebTV set-top service | Does not itself identify Gemstar DLLs or data format |
| January 2000 print issue | *The Computer Paper*, vol. 13 no. 1, Maarten Heilbron, *WebTV for Windows* | Identifies a **Gemstar EPG** displayed through WebTV for Windows on TV-tuner-equipped PCs; contrasts set-top **WebTV Plus** guide, described as receiving listings from **TVData** | Journalistic review, not original binary; TVData attribution concerns the reviewed set-top product, not all versions |
| Later ATI multimedia software manuals (e.g. MMC 9.0, 2004) | ATI, *TV Listings* | Distinct **Gemstar GUIDE Plus+** application for Windows/ATI Multimedia Center, Internet-downloaded listings | Not proof that 1998 WebTV for Windows embedded ATI's GUIDE Plus+ executable; **do not merge brands** |
| 1999 Windows Millennium development builds | BetaWiki's binary inventory leads | Reports `gemstar.sys` and in some builds `wstdecod.dll` | Later-build secondary inventory **does not establish Windows 98 RTM/SE file presence**; inspect originals |

### What the 9 January 1998 agreement actually specifies

- §2 defines an EPG, `Data Loader`, `Gemstar Data Loader`, and `Gemstar
  Protocol(s)` distinctly. A loader receives and stores data for display;
  a patent/trademark license alone does not imply it is a DLL named Gemstar.
- §4.10 says **Microsoft** was responsible for implementing country-specific
  Gemstar Data Loaders with Gemstar providing data-format/protocol guidance.
- §5.1 describes loader-inclusion obligations, but §5.7 preserves exceptions
  for earlier **Windows 98 / WebTV Plus** versions. **Do not assume every RTM,
  SE, OEM or later upgrade image has identical Gemstar binaries.**
- §5.2.1(c) calls for Gemstar credit in an About box or equivalent guide
  screen. It provides an attribution search clue, **not an authenticated
  screenshot**. Some other logo-display terms are conditional or redacted.
- §§5.4/5.6 and appendix B describe TV-listing data delivery, VBI transport,
  security and format information. The contract also defines an Internet
  `Web Server` for distributing EPG data (§2) and only agrees to discuss
  establishing one (§5.8). Consequently, **VBI and Internet are distinct
  candidate transports**; contract definitions don't prove both shipped.
- Appendix B says Gemstar supplied technical information for VBI extraction,
  data decoding and loader interoperability. No decoder API, file format or
  executable is fully specified by the public text.

### Software / transport boundaries

1. Windows 98 tuner and WDM capture layer selects the channel and may expose
   video, sound and VBI streams.
2. The **closed-caption** `ccdecode.sys` and Line 21 renderer serve
   accessibility captions; they must not be rebranded as an EPG loader.
3. **WaveTop** is a WavePhore/PBS broadcast data-content service: shared
   broadcast infrastructure does not imply Gemstar ownership of WaveTop.
4. **WebTV for Windows** is the TV application/EPG presentation surface.
   Gemstar licensing/data-loader integration and program-listings storage are
   a separate functionality within or supporting the guide.
5. **WebTV Plus** set-top receivers are a separate hardware/service family.
   The 2000 print witness explicitly distinguishes its guide from the PC's.
6. **ATI GUIDE Plus+** is a distinct ATI TV-software integration of
   Gemstar-branded guide technology. Do not infer the same EXE, version,
   installer, transport or database format as WebTV for Windows.
7. **JET Red / ESENT / TVData**: neither EPG attribution nor a contract's
   definition of storing data identifies Microsoft JET file formats.
   Inspect actual database signatures and imports rather than guessing.

## Inspect original media before creating Water APIs

For a clearly dated, licensed Windows 98 **RTM / Second Edition / OEM**
image, preserve setup CAB/INF provenance, architecture and SHA-256. Do not
mix in later Windows Me, Neptune or ATI Multimedia Center upgrade files.

Check separately:
- Installed WebTV Program Guide applications and DLLs, EPG/guide About-box
  resources, Gemstar credit or patent text, COM registrations, and setup INF
  entries. Check installed programs as well as installation archives.
- `gemstar.sys` **as a candidate filename only**: original presence,
  file version, architecture, INF `AddService`, KS category/pins and filter
  function all remain unverified for Windows 98. A 1999 WinMe build filename
  does not prove a 1998 Win98 device driver.
- TV tuner/crossbar/VBI path and potential data-loader output; if Internet
  retrieval is observed, record the networking call path separately.
- Per-version guide storage files, unique magic bytes and indexes. Use
  `python3 scripts/inspect-jet.py --all --json /path/to/extracted/webtv`
  for PE import evidence only; a `jet_*` absence or match alone does not
  decide guide storage semantics.
- UI actions: channel lineup, station identifiers, ZIP/location selection,
  search by actor/show/category, current/next times, tune-from-listing,
  missing/stale guide data, and tunerless behavior. Sources describe
  functions but original API behavior and binary ownership are still open.
- Distinguish external service unavailability in modern times from DLL
  loading, decoder ABI, tuner routing and offline cached-guide failures.

Do **not** add an empty `gemstar.sys`, a fake GUI, a guessed COM CLSID,
or use a later Windows 10/Media Center EPG contract to model Win98.
Implement only independently verified interfaces with i386 Win9x layouts,
registration, error behavior and controlled sample inputs.

## Evidence ledger: resolve contradictions instead of merging them

| Proposition | Assessment |
| --- | --- |
| Windows 98/WebTV appears in Microsoft–Gemstar EPG agreement | **Confirmed** from 1998 exhibit (§5.7.1) |
| WebTV for Windows had an EPG with Gemstar attribution | **Confirmed by contemporary 2000 review** |
| WebTV for Windows and WebTV Plus used identical guide/data supplier | **Contradicted as a blanket claim** by the 2000 reviewer (PC Gemstar vs set-top TVData) |
| Microsoft defined/undertook Gemstar loader support | **Confirmed contract terms**, installed implementation per release unknown |
| Both VBI and Internet guide transports ran in Win98 RTM | **Unknown**; supported as architectural possibilities, not binary proof |
| `gemstar.sys` was in original Windows 98 | **Unknown**; 1999 WinMe secondary leads only |
| Gemstar guide required Jet Red or ESENT | **Unknown** |
| WaveTop / CC decoder implement the Gemstar program guide | **Not established** |

## Sources

1. [Gemstar International, StarSight, Microsoft: Definitive Agreement (9 Jan 1998; filed 12 Jan 1998), exhibit 10.3, SEC 8-K accession 0001017062-98-000045, reproduced by Law Insider](https://www.lawinsider.com/contracts/g20R5480IbR) — especially definitions in §2, §§4.10, 5.1, 5.2, 5.7, 5.8 and appendix B. Some text redacted.
2. [Microsoft Windows 98: *Turn your computer into a TV* (last updated 11 Feb 1999; mirrored from Microsoft's site)](https://zx.net.nz/mirror/www.microsoft.com/windows98/usingwindows/fun/articles/902Feb/tv.html).
3. [Maarten Heilbron, *WebTV for Windows*, *The Computer Paper* vol. 13 no. 1 (Jan 2000), Computer History Museum archived scan](https://archive.computerhistory.org/resources/access/text/2023/11/102803730-05-001-acc.pdf). The current archive hosting date is **not** the print article's creation date.
4. [ATI Multimedia Center 9.0 user guide, TV Listings (later Windows GUIDE Plus+ comparison; hosted ATI/AMD PDF)](https://drivers.amd.com/manuals/mmc90.pdf).
5. [BetaWiki, Windows Me build 2394 (1999) `gemstar.sys` lead](https://betawiki.net/wiki/Windows_Me_build_2394) — community compiled-file report, not verified Windows 98 media.
