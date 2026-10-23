# Windows 9x sample MIDI evidence and Water-owned demonstration

## Historical identity

Windows 95 and Windows 98 distributed an RMI sample named
`Dance of the Sugar-Plum Fairy.rmi` (also observed in case variants).
The work is Pyotr Ilyich Tchaikovsky's *Dance of the Sugar Plum Fairy* from
*The Nutcracker*. The sample sequencing distributed with Windows 95 was
credited courtesy of Midisoft Corporation; it is **not** automatically
public domain simply because the 1892 musical composition is.

Evidence:
- Raymond Chen, Microsoft, "What's the story of the onestop.mid file in
  the Media directory?" (2013):
  https://devblogs.microsoft.com/oldnewthing/20130212-00/?p=5263
- Contemporary discussion with Windows 95/98 filename inventory (2007):
  https://groups.google.com/g/microsoft.public.win95.general.discussion/c/iPtnAJJQ8JA
- Windows 95 soundtrack/credits transcription:
  https://vgmdb.net/release/23694
- Library of Congress RMID format description:
  https://www.loc.gov/preservation/digital/formats/fdd/fdd000120.shtml

## Water edition (new data, not original Microsoft sample)

`Dance of the Sugar-Plum Fairy (Water).rmi` is a newly coded,
simplified **demonstration arrangement** based on public-domain
Tchaikovsky motifs, featuring GM celesta, pizzicato strings and bass.
It is not an extract from, an exact transcription of, or a replacement
for Microsoft's Midisoft-supplied binary. Its arrangement, phrasing and
length differ from the historical sample. Water can distribute this new
asset without copying the 1995 sequenced MIDI data.

Format: RIFF type `RMID`, with a `data` chunk containing standard MIDI
file type 1, four tracks, a 2/4 time signature, and 60 quarter notes/min.
Approximate generated playback length: 96 seconds.

SHA-256:
`c57f8f5d1cbcf925a562289e4585174c11b43b8d492f9e942f8a9a40f3c15c41`

Regenerate or validate with no third-party Python packages:

```sh
python3 scripts/generate-sugar-plum-rmi.py --check
# Regeneration only if intentionally changing the asset:
python3 scripts/generate-sugar-plum-rmi.py
```

## Correct Water subsystem and testing scope

The existing `dlls/mciseq/mcimidi.c` already handles a RIFF `RMID`
container, and `loader/wine.inf.in` maps `rmi` to the `Sequencer` MCI
extension. Do not add redundant RIFF decoders in WINMM or replace MCI
with Windows XP `uxtheme.dll` or modern media playback paths.

**Opt-in test installation:** copy this file, preserving its distinctive
Water-edition filename, into the chosen guest's
`C:\\Windows\\Media` directory, then open it with a sequencer-capable
MCI player. Never overwrite a native Windows sample file.

This asset is **source-tracked**, but not automatically installed into
all Wine/Water prefixes. Classic Win9x media provisioning must stay
separate from NT/XP profiles. The Water sample has not been verified
on a real Windows 95/98 guest, sound device, or live MCI driver.

A third-party `.rmi` download with the historical name is not by itself
evidence of redistribution permission or binary identity. To compare
the Microsoft original later, use a licensed user's copy, record
filename/size/hashes, and keep it outside this repo.
