# IVVIDEO.DLL — VivoActive H.263, not Intel Indeo

Status: **HISTORICAL IDENTITY CONFIRMED; CODEC NOT IMPLEMENTED**.
Audit date: October 9, 2026. Windows target: 1998-era Win32/VfW
(Video for Windows), separate from 16-bit Indeo and Windows Media codecs.

## Provenance and Windows 98 FE evidence

The original Windows 98 FE cabinet inventory lists:

| Source | Filename | Uncompressed size | Inventory timestamp |
| --- | --- | ---: | --- |
| `WIN98_53.CAB` | `IVVIDEO.DLL` | **225,280 bytes** | **May 11, 1998, 8:01 PM** |

The file appears amid other old Internet/NetShow-era multimedia
components. **Presence on installation media is not proof the DLL
was installed or enabled in a fresh default Windows 98 FE setup.**
No native FE file has been extracted or hashed as part of this audit.

Vivo Software's *VivoActive H.263* decoder is identified by the
Video for Windows registration:

```ini
[drivers32]
vidc.vivo=ivvideo.dll
```

A contemporary AVI codec survey traces this registration to the
Microsoft **NetShow 2.0 Player** distribution and identifies the
codec's FourCC as `VIVO`. The related, **separate audio** decoder
used `msacm.vivog723=vivog723.acm` for Vivo G.723/Siren.
Microsoft announced NetShow 2.0 beta in March 1997 and its release in
August 1997, including third-party multimedia technologies. Thus
this codec family predates Windows 98 FE.

RFC 2361 (June 1998), section B.102, independently registers
**Vivo H.263** with FourCC `VIVO` (also lowercase `vivo`).
Preserve literal on-disk FourCC spellings when reading file headers,
but make case-insensitive lookup behavior an *explicit compatibility
test*, not a reason to reassign the codec to Intel.

### Name trap: not Indeo

The filename begins `IV`, but **is not an Intel Indeo codec**.

| Family | Example driver | Video FourCC |
| --- | --- | --- |
| VivoActive H.263 | `IVVIDEO.DLL` | `VIVO` / `vivo` |
| Intel Indeo 3.1 / 3.2 | `IR32_32.DLL` | `IV31`, `IV32` |
| Intel Indeo 4.1 | `IR41_32.DLL` | `IV41` |
| Intel Indeo 5 | `IR50_32.DLL` | `IV50` |
| Microsoft H.263 (another NetShow codec) | `MSH263.DRV` | `M263` |

H.263 describes the **video coding family**; its presence does not
guarantee that an arbitrary generic H.263 decoder will handle Vivo's
bitstream packaging, frame headers, timestamps or AVI/ASF use correctly.
VivoActive's proprietary `.VIV` container is not identical to an AVI
stream carrying the `VIVO` FourCC.

Historical surveys caution that NetShow **player** distributions often
supplied decoding or keyed encoding, while NetShow **tools** had
different encode capabilities. Do not claim working encoding support
from this filename or a driver-registration string.

## Recovered Water support and ownership

The audited Water `master` tree has **no `dlls/ivvideo`** and no
source-level `IVVIDEO.DLL` implementation.

- `dlls/msvfw32/msvideo_main.c` already discovers codecs via
  `Software\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32` and
  falls back to `GetPrivateProfileSectionA("drivers32", ..., "system.ini")`.
  Its `ICInfo`, `ICOpen`, `ICOpenFunction` and `ICLocate` are the
  existing *VfW routing layer*. They are not the Vivo decompressor.
- `dlls/ir50_32/ir50.c` implements the independent Intel Indeo 5
  `IV50` codec path and is **not** the right owner for `VIVO`.
- `dlls/msvidc32` implements Microsoft Video 1 (`MSVC`), also distinct.

**Do not add `VIDC.VIVO=ir50_32.dll`, route `VIVO` to `IV50`,
register a nonexistent decoder, or return success for unimplemented
`ICM_DECOMPRESS` messages.** The existing VfW loader may be used
to load an authentic codec when installed in an appropriate guest,
without Water pretending to decode its frames.

## Implementation gates — keep the 1998 ABI separate

1. Extract the authorized 1998 `IVVIDEO.DLL` from the correct FE
   media set; record SHA-256, PE machine type, file/product versions,
   resource strings, import table, and **actual export names**.
2. Inspect `[drivers32]` entries and installation INF/NetShow
   configurations, distinguishing FE media, actual installed systems,
   IE/NetShow player, and NetShow tools.
3. Verify VfW's `DriverProc` presence/signature and callbacks:
   `DRV_LOAD`, `DRV_OPEN`, `DRV_CLOSE`, `ICM_GETINFO`,
   `ICM_DECOMPRESS_QUERY`, `ICM_DECOMPRESS_GET_FORMAT`,
   `ICM_DECOMPRESS_BEGIN`, `ICM_DECOMPRESS`,
   `ICM_DECOMPRESS_END`, and any native extensions; never infer
   exact exports from the generic VfW interface.
4. Obtain a lawful known-good `VIVO` sample and independently
   identify its coded frame format, valid resolution ranges and
   supported destination BITMAPINFO formats.
5. Implement decoding only where a real H.263/Vivo-compatible decoder
   is available. Test bounds, decompression failures, malformed
   compressed buffers, error codes and top-down/bottom-up DIBs.
6. Exercise `ICInfo`, `ICOpen` and `ICLocate` under Win9x
   `SYSTEM.INI` and NT registry personalities. Keep FourCC and
   installed-codec behavior distinct. Verify in a real Windows 98 FE
   profile before marking it IMPLEMENTED.

## Sources (provenance-separated)

- Windows 98 FE cabinet inventory (distribution, **not** a DLL binary):
  https://www.localhost.me.uk/support/windows98/pages/98cabcon.html
- Period AVI codec survey, `VIDC.VIVO`/NetShow 2.0:
  https://www.faqs.org/faqs/graphics/avi-faq/
  https://jmcgowan.com/aviweb.html
- June 1998 RFC 2361, B.102 (FourCC registry):
  https://www.rfc-editor.org/rfc/rfc2361
- Microsoft NetShow 2.0 beta announcement (1997-03-10):
  https://news.microsoft.com/source/1997/03/10/microsoft-announces-immediate-availability-of-netshow-2-0-beta-brings-multimedia-broadcast-and-communication-to-the-internet-and-intranets/
- Microsoft NetShow 2.0 release and Vivo partnership (1997-08-05):
  https://news.microsoft.com/source/1997/08/05/microsoft-announces-acquisition-of-vxtreme-rolls-out-streaming-multimedia-strategy-with-release-of-netshow-2-0/
- Microsoft KB142946, Win95 codec troubleshooting (Indeo control):
  https://jeffpar.github.io/kbarchive/kb/142/Q142946/
