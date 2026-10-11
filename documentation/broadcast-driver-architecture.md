# Broadcast Driver Architecture (BDA) and Windows 98 broadcast architecture

Status: **platform/owner boundaries established from dated Microsoft sources and
Water's master source; working BDA/WDM broadcast reception is NOT confirmed**.
Target guest personalities: Windows 98 RTM/SE i386 and later Windows 2000/XP
separately. BDA means **Broadcast Driver Architecture**, but the historically
named 1997 **Windows 98 Broadcast Architecture** and later **digital BDA**
must not silently be treated as identical implementations.

Related: [WebTV / WaveTop / Intercast](windows98-broadcast.md),
[Gemstar program guide](webtv-gemstar-epg.md),
[Closed Caption Decoder](closed-caption-decoder.md),
[JET/ESENT](jet-compatibility.md).

## Separate chronological layers

| Date / target | Attested architecture | What is *not* proved |
| --- | --- | --- |
| **8 October 1997** (Windows 98 announcement) | Microsoft explicitly calls it the *Microsoft Broadcast Architecture in Windows 98*: TV tuner access, television viewing, data broadcasts and an electronic program guide; WavePhore announced WaveTop integration | That the entire later DVB/ATSC BDA Network Provider and IP Sink collection was bundled in Win98 RTM |
| **25 June 1998** | WaveTop ships with Windows 98: its broadcast signal delivers content through analog VBI and the client decodes/caches it | That digital MPEG-2 demux, BDA IP Sink, or the GEMSTAR EPG storage backend owns WaveTop |
| **8 September 2000** | Microsoft announces *Microsoft TV Technologies* and a **new** broadcast driver architecture for worldwide digital TV, including DVB/ATSC/ARIB | That all this digital-TV functionality already shipped in 1998 |
| **Microsoft TV Technologies SDK guidance (DirectShow 9 era)** | Windows 98SE/Me/2000 applications cannot use its XP Video Control; must build digital graphs and store tune requests themselves | Win98 RTM built-in support for every later digital BDA filter |
| **Windows 2000 / XP / Server 2003** | Microsoft's *BDA IP Sink* specifically lists these OS targets; analog SLIP or digital MPE can carry packets into it | BDA IP Sink availability on Windows 98 |
| **Windows Vista / 2008** | Vista removed BDA IP Sink, BDA SLIP Deframer, CC Decoder and NABTS/FEC VBI Codec; Protected BDA (PBDA) was announced in 2008 | Presence of these components in every earlier Windows installation |

A later WDK page discussing BDA driver builds specifies `USE_MAPSYM=1` for
debugging on Windows 98/Me. That demonstrates **some BDA-related toolchain
provision for Win9x**, not which device classes or filters were present on
Win98 retail media. Keep the package version and hardware driver version
alongside OS version when testing.

## Data-flow variants; do not merge them

### 1998 analog-TV / VBI layer (feature-level, not a recovered binary graph)

```text
TV tuner / analog capture
        |
        +--> live video/audio --> WebTV for Windows TV window
        |
        +--> digitized VBI --> CC decoder --> Line 21 decoder --> captions
        |
        +--> digitized VBI --> broadcast data decoding --> WaveTop cache/content
        |
        +--> guide-data source (VBI and/or Internet; unverified by binary)
                                        --> GEMSTAR guide loader --> WebTV EPG
```

The caption branch follows Microsoft's description of CC and Line 21 filters,
not proof that `CCDECODE.SYS` shipped on a specific Win98 CD. Gemstar's
loader/data source is a **documented contract/functionality**, not proof of
a `GEMSTAR.SYS` Win98 RTM binary. An arrow labeled VBI does not make WaveTop,
closed captions and Gemstar interchangeable data formats.

### Later documented analog *BDA-compatible* data graph

```text
analog VBI --> NABTS/FEC VBI codec --> BDA SLIP Deframer
          --> BDA IP Sink --> networking stack/Winsock
```

Microsoft documents this chain for its BDA data-service filters. The
BDA IP Sink is **version-scoped to Windows 2000/XP/Server 2003**, so this
diagram must **not** be asserted as Windows 98 RTM's actual implementation.
The SLIP deframer can also participate in *non-BDA analog graphs*; the word
BDA in the filter name is not proof that every analog tuner used digital BDA.

### Digital BDA graph (separate from analog WebTV assumptions)

```text
tune request --> BDA Network Provider (control plane; NO signal throughput)
                      |
              KsProxy / WDM tuner-demodulator-capture
                      |
                MPEG-2 transport stream
                      |
              MPEG-2 Demultiplexer
                 /       |          \
           audio/video   PSI/SI    data IP
                         |           |
            Transport Information    MPE
                  Filter (TIF)       |
                         |         BDA IP Sink
            Guide Store Loader       |
              / tune requests      Winsock
```

The network provider coordinates tuner state and demux PID routing; actual
media is transported through hardware/filter pins, **not** through the
Network Provider's control output. Guide Store Loaders using the digital TIF
are distinct from the *Gemstar Data Loader* contract of January 1998 until
interoperability is proved from original sources.

## Water source ownership audit (master examined 11 October 2026)

| Existing path | What is actually present | Status |
| --- | --- | --- |
| `include/bdaiface.idl` | `IBDA_NetworkProvider`, `IBDA_DeviceControl`, `IBDA_Topology`, `IBDA_IPSinkInfo` and a later-named `IBDA_GuideDataDeliveryService` interface | **Declarations**, not backend implementation or 1998 provenance |
| `include/bdatypes.h`; `include/bdaiface_enums.h` | BDA structs/enums | Header/API inventory only |
| `include/ks.h`; `include/ksmedia.h` | A limited KS base and media type definitions | Not an operational BDA AVStream driver |
| `include/ksuuids.h` | `AM_KSCATEGORY_VBICODEC`, MPEG-2 GUID identifiers | GUID existence does not register usable filters |
| `dlls/ksproxy.ax/ksproxy.ax.spec` | `DllGetClassObject`, `KsOpenDefaultDevice`, `KsSynchronousDeviceControl` are stubs | **Major KSProxy blocker** |
| `dlls/ksuser/ksuser.spec` | `KsCreatePin`, `KsCreateTopologyNode`, clock/allocator exports are stubs | **Major KS interface blocker** |
| `dlls/qcap/capturegraph.c` | `ICaptureGraphBuilder2::RenderStream` returns `E_NOTIMPL` for `PIN_CATEGORY_VBI` | **Known analog VBI graph gap** |
| `dlls/qdvd` | DVD graph/navigation support but no confirmed Line 21 renderer | Captions path remains incomplete |

Do not create a `bda.sys`, fake device driver, `GEMSTAR.SYS`, generic EPG
DLL or COM registration simply because an interface is declared.
Retain Windows 98 i386 PE32 and driver model, Windows 2000/XP WDM/BDA
personality, and modern ARM64 build-host/guest distinctions. The presence
of `IBDA_GuideDataDeliveryService` is **not** proof that Gemstar's 1998
WebTV guide used that interface.

## Prioritized investigation and regression contracts

1. **Original installation evidence first:** for dated Win98 RTM, Win98SE,
   OEM and separate DirectX/driver additions, hash CAB/INF/SYS/DLL/AX files;
   record the install-time feature, tuner vendor/hardware IDs, registry
   KS categories and PE/LE/NE format. A later SDK header is not an install
   manifest. Do not infer a dependency from a filename.
2. **Minimal device enumeration:** test `ICreateDevEnum` and KSProxy
   for an existing, well-defined mock WDM capture filter. Validate
   category GUIDs, COM lifetime and error/absence semantics before
   promising tuner interoperability.
3. **Analog input before digital:** implement/verify capture stream pin
   enumeration and media types, sample/timestamp delivery, and a controlled
   `PIN_CATEGORY_VBI` route. Do not replace `E_NOTIMPL` until the
   implementation actually manages tee/sink-to-sink behavior and tests it.
4. **Split payload codecs:** route synthetic VBI samples to caption Line 21,
   NABTS-data, or other format-specific decoders only after format,
   framing and error handling are known. Guard against malformed, oversized
   or adversarial broadcast samples.
5. **Guide path independently:** audit Gemstar program-guide binaries,
   `TVX.EXE` candidates, guide resources and transport provenance.
   Never assume BDA TIF supplies an analog 1998 Gemstar data store or JET DB.
6. **Post-2000 digital BDA independently:** validate topology
   `IBDA_NetworkProvider` + `IBDA_DeviceControl` interfaces, tune requests,
   KsProxy device pin lifetimes, transport packets and PSI parsing with
   test fixtures; maintain explicit version gates for IP Sink and VMR.
7. **Security:** do not deliver arbitrary decoded broadcast IP packets
   into host Winsock as a shortcut. Microsoft's BDA IP Sink documentation
   warns of source-routing/broadcast-injection denial-of-service risks.
   Model guest networking and its filtering separately from host networking.
8. **Cross-check:** preserve missing/negative results; only assert runtime
   success after build, synthetic graph tests and era-correct OS/media tests.

### Evidence ledger

| Question | Status |
| --- | --- |
| BDA expands to Broadcast Driver Architecture? | **Confirmed** |
| Windows 98 had a broadcast architecture tied to WebTV and WaveTop? | **Confirmed**, Microsoft 1997/1998 |
| The newer BDA digital graph was announced by 2000? | **Confirmed**, Microsoft announcement |
| Windows 98SE software could construct TV graphs under SDK guidance? | **Confirmed** for supported runtime arrangements; installed filter inventory still needed |
| Windows 98 RTM shipped BDA IP Sink? | **Unsubstantiated** and **not in Microsoft's stated IP Sink OS availability** |
| Water implements functional KSProxy + VBI capture today? | **No**, source audit identifies stubs and `E_NOTIMPL` |
| Gemstar guide and digital BDA Guide Store Loader are the same API? | **Unknown**; do not merge |
| WaveTop, WebTV, captions, EPG use one identical VBI format? | **Not established** |
| Win98-era versions, original driver dependencies, tuner IDs, COM CLSIDs? | **Unknown** pending media/INF and graph capture |

## Sources, ranked by relevance

- [Microsoft/WavePhore, 8 Oct 1997: Windows 98 Broadcast Architecture](https://news.microsoft.com/source/1997/10/08/microsoft-to-include-wavephores-wavetop-software-in-windows-98/) — dated primary announcement.
- [Microsoft/WavePhore, 25 Jun 1998: WaveTop VBI delivery](https://news.microsoft.com/source/1998/06/25/microsoft-windows-98-to-ship-with-wavephores-wavetop-consumer-data-broadcast-service/) — dated primary announcement.
- [Microsoft, 8 Sep 2000: new Microsoft TV Technologies broadcast driver architecture](https://news.microsoft.com/source/2000/09/08/microsoft-announces-microsoft-tv-technologies-in-windows/) — dated primary announcement.
- [Microsoft, BDA network provider](https://learn.microsoft.com/en-us/previous-versions/ms779697%28v%3Dvs.85%29) — DirectShow SDK, later digital architecture.
- [Microsoft, BDA filters](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mstv/bda-filters) — named data paths and filter types.
- [Microsoft, BDA IP Sink](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mstv/bda-ip-sink-filter) — Windows 2000/XP/2003 scope and source-routing risk.
- [Microsoft, TV Technologies internals](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mstv/microsoft-tv-technologies-internals) — KsProxy, demux, TIF, guide loaders; Win98SE manual graph note.
- [Microsoft, Building BDA Drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/building-bda-drivers) — historical Win98/Me debugging note, not install evidence.
- [Microsoft, DirectShow: What's New](https://learn.microsoft.com/en-us/windows/win32/directshow/whats-new-in-directshow) — removal of BDA IP Sink, SLIP, CC and NABTS filters in Vista.
- [Microsoft, PBDA announcement, 2008](https://news.microsoft.com/source/2008/09/11/microsoft-launches-new-worldwide-platform-for-broadcast-tv-on-the-pc/) — later separate protected-driver architecture.
