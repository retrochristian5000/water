# RLE graphics: Windows 95 evidence and decoder safety

**Status:** Format documented for Windows 3.x/95-era GDI.
Water source audited and a compressed-stream preflight added to
MSRLE32. No original Windows 95 RTM codec binary or guest execution
has been inspected, and the Wine codec build has not been run.

## ABI and component boundaries

* GDI DIB: `BI_RLE8` represents 8-bit indexed bitmaps and `BI_RLE4`
  represents 4-bit indexed bitmaps. Water's GDI pathway is
  `dlls/win32u/dib.c:build_rle_bitmap`.
* Video for Windows: `dlls/msrle32/msrle32.c` implements an RLE
  video compressor/decompressor (VfW `ICM_COMPRESS`/`ICM_DECOMPRESS`)
  for key/delta frames. The legacy `dlls/msvideo.dll16` bridge is
  a separate calling ABI. Do not flatten VfW and GDI decoder semantics.
* `dlls/windowscodecs/bmpdecode.c` is a **later WIC** bitmap path.
  WIC is not Windows 95 RTM evidence and does not belong in RTM
  kernel, GDI, or installer header reconstructions.
* RLE is unrelated to DBLBUFF, DoubleSpace, CAB/LZ disk/installer
  compression and MBR/INT13 driver initialization.

## Format and concrete defect

RLE packets are `count,value`. Nonzero count is an encoded
run. Zero count escapes are `00 00` EOL, `00 01` end-of-image,
`00 02 dx dy` delta, and `00 n` (n >= 3) absolute literals.
For RLE8 absolute n pixels require n data bytes and alignment
padding if odd. For RLE4 they require ceil(n/2) packed data bytes
plus 16-bit alignment padding. Compressed DIB images must be
bottom-up (positive height), not top-down.

Water's `MSRLE32_DecompressRLE4` and `MSRLE32_DecompressRLE8`
previously consumed bytes until an EOI marker with no declared-input-
length check, and advanced output rows without checking vertical
limits. A truncated command, large delta or repeated EOL could
read/write beyond the intended frame.

`MSRLE32_ValidateCompressedStream` now performs a bounded pass
using `BITMAPINFOHEADER.biSizeImage` **before** the legacy
decoder or palette initialization. It validates positive dimensions,
packet completeness, literal and pad lengths, run width and delta
row/column ranges, and requires an EOI terminator. Malformed
streams return `ICERR_BADFORMAT` without entering the old loops.
This is a conservative source-level safety improvement, **not**
proof of original Microsoft codec error codes.

**Remaining gaps:** `biSizeImage` is a *declared* length, not a
proven accessible allocation size; callers must provide at least
that many actual bytes. Width/stride arithmetic, palette index
bounds, compressed frame reuse and subtle decode differences still
need separate testing. The guard does not harden the independent
GDI or WIC decoders; avoid changing unrelated user-mode APIs
merely because format names overlap.

## Test and version controls

`python3 -m unittest discover -s scripts/tests -p test_msrle_stream.py`

The source-based C99 harness tests valid encoded RLE4/RLE8,
absolute literals, padding, EOL/EOI, delta movement and malformed
truncated or out-of-range streams. It compiles the *actual*
validator function, not an unrelated Python reimplementation.
It is not a full Video for Windows integration/Win95 RTM guest test.

Next compare period Windows 95 RTM `MSRLE32`/VfW behavior with
original binary hashes and metadata, test native GDI
`BI_RLE4`/`BI_RLE8` separately, and audit decoder allocations
under memory sanitizers. Do not attribute later WIC behavior or
unverified malformed-stream quirks to Windows 95 RTM.

Sources:
- Win32-era BITMAPINFOHEADER: https://techshelps.github.io/Win32/html/05za4g.htm
- Microsoft GDI compression grammar: https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-compression
- Original Windows 3.1 help archive quoting RLE8: https://fourcc.org/pixel-format/rgb-bi_rle8/
