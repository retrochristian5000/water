# Closed Caption Decoder (CCDECODE) — Windows 98 broadcast audit

Status: **historical driver/DirectShow interfaces confirmed; retail Windows 98
CCDECODE.SYS presence and operational support not yet confirmed from original
media**. Maintain this distinction until dated Windows 98 CAB/INF binaries are
available for inspection. See [WaveTop/WebTV boundaries](windows98-broadcast.md)
and [BDA / analog-versus-digital broadcast layers](broadcast-driver-architecture.md).

## Name and owner boundaries

- **CC Decoder / Closed Caption Decoder**: Microsoft documents a *kernel-mode
  WDM stream-class filter* in `AM_KSCATEGORY_VBICODEC`. It takes raw
  television VBI waveforms on a **VBI** input, or already-extracted captions
  on its **HWCC** input (pass-through). Its primary output provides caption
  data to the Line 21 Decoder. Applications enumerate the WDM component via
  the System Device Enumerator rather than instantiating it directly as a
  user-mode COM DLL.
- **Line 21 Decoder**: a *different DirectShow filter* whose
  `CLSID_Line21Decoder` implementation is documented as used by Windows
  95/98/Me/2000; `CLSID_Line21Decoder2` is XP-era. It accepts line-21
  EIA/CEA-608 byte pairs and draws caption bitmaps for video overlay.
  Microsoft names `qdvd.dll` as its executable module.
- **Tuner/capture**: a device driver can emit `PIN_CATEGORY_VBI` raw samples,
  `PIN_CATEGORY_CC` extracted byte pairs or
  `PINNAME_VIDEO_CC_CAPTURE` hardware-sliced data. A direct CC pin can
  bypass the WDM CC decoder and feed Line 21 rendering.
- **VBI broadcast services**: WaveTop carried digital data through the VBI;
  that is shared *transport*, not proof that its content was caption data or
  that WaveTop/WebTV directly imported `ccdecode.sys`. Distinguish CC
  (US NTSC line 21), NABTS/data services and WST/PAL Teletext.

## Water owner/source audit (master as of 11 Oct 2026)

| Path | Observed implementation boundary |
| --- | --- |
| `dlls/qcap/capturegraph.c` | `ICaptureGraphBuilder2::RenderStream` returns `E_NOTIMPL` for `PIN_CATEGORY_VBI` (Tee/Sink-to-Sink unsupported) |
| `dlls/ksproxy.ax/ksproxy.ax.spec` | `DllGetClassObject` and kernel-streaming helper exports are stubs |
| `dlls/ksuser/ksuser.spec` | KS user-mode DLL export signatures exist as stubs |
| `dlls/qdvd/qdvd_main.c` and `qdvd_classes.idl` | Registers DVD navigator/graph builder, not Line 21 decoder |
| `include/ksmedia.h` | Limited KS audio/video structures; does not yet provide WDM VBI caption descriptors |

There is no functioning `ccdecode.sys` driver implementation in this audited
Water tree. Do not add fake `.sys` exports or a `qdvd.dll` registration
claim before implementing and testing filter pin formats, streaming and ABI.

## Original-media / ABI recovery workflow

1. Extract a dated, licensed Windows 98 RTM/SE/OEM CD or DirectX package
   using a separate, read-only CAB extractor. Preserve original archive
   hashes; distinguish the system image from subsequently installed DirectX.
2. Inventory matching `ccdecode.inf`, `ccdecode.sys`, and other INF
   references with:
   `python3 scripts/inspect-caption-driver.py /path/to/extracted > caption-driver.json`.
   This tool reads files only; it records SHA-256, candidate INF sections and
   lines, and PE machine/class for a `CCDECODE.SYS` candidate. An INF
   reference alone does **not** prove a file is installed or functional.
3. Resolve hardware IDs, `AddService`, `CopyFiles`, KS filter categories,
   pin GUIDs, original architecture and Setup installation state from the
   original INF/registry/driver. Compare Win98/98SE vs 2000/XP/DirectX files
   separately. Do not classify 2002 packages as 1998 originals.
4. Reconstruct separate VBI, hardware-decoded caption and direct CC paths;
   trace data to `IAMLine21Decoder` and verify actual bitmap overlay.
   Preserve the device's Win9x i386 ABI; ARM64 PE32+ host tests are *format
   controls*, not historical Win9x driver evidence.
5. Regression-test byte-pair parity, caption channels, control codes, fields,
   timing, no-caption and malformed samples against authentic EIA-608 data,
   then tuner-independent synthetic sources before an actual tuner.

### Verification status

The inspection script's synthetic tests do not validate an actual codec or
Windows 98 driver. Neither the original CCDECODE.SYS, its Windows 98 install
history, nor its WebTV-specific deployment has been demonstrated in this
round.

## References

- Microsoft, [CC Decoder Filter](https://learn.microsoft.com/en-us/windows/win32/directshow/cc-decoder-filter).
- Microsoft, [Line 21 Decoder Filter](https://learn.microsoft.com/en-us/windows/win32/directshow/line-21-decoder-filter).
- Microsoft, [Viewing Closed Captions](https://learn.microsoft.com/en-us/windows/win32/directshow/viewing-closed-captions).
- Microsoft, [WST Codec Filter](https://learn.microsoft.com/en-us/windows/win32/directshow/wst-codec-filter).
- Microsoft, [DirectShow redistribution component lists](https://learn.microsoft.com/en-us/windows/win32/directshow/setting-up-the-build-environment); `ccdecode.sys` is listed in a DirectX `bda.cab`, but this alone does not date its Win98 inclusion.
- AITech, [Net-TV 98 WebTV WDM driver and overlay notes](https://www.aitech.com/support/addendum.htm).
