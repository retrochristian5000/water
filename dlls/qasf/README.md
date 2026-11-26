# ASF and historical media clients

ASF is a container, not a player or codec. Microsoft used the name Advanced
Streaming Format during the NetShow era and Advanced Systems Format in later
SDK documentation. Do not infer that all historical ASF revisions or codecs
are mutually compatible.

## Component boundaries

- programs/nsplayer: separate NetShow Player frontend (2.0 metadata).
- programs/wmplayer and dlls/wmp: Windows Media Player application and control.
- dlls/msdxm: experimental legacy player adapter, not the NetShow control.
- dlls/quartz: DirectShow graph building and shared file recognition.
- dlls/qasf: WM ASF Reader and DMO wrapper.
- dlls/wmvcore: Windows Media Format reader/writer API.
- dlls/wmasf: low-level ASF API exports (currently stubs).
- dlls/mfasfsrcsnk: later Media Foundation ASF source/sink integration.
- dlls/winegstreamer: shared parsing and codec backend.

The ASF Header Object GUID is 75B22630-668E-11CF-A6D9-00AA0062CE6C.
Its serialized 16-byte signature identifies the container only, not the
player, stream codec, DRM status, or streaming protocol. The Quartz signature
uses the existing asynchronous reader and splitter path; it does not claim
to match all Windows versions' source-filter selection. Microsoft documents
the older Windows Media Source Filter as the default through XP SP1 and
the newer WM ASF Reader as the subsequent default.

Keep ASX playlists, MMS transport, NetShow client/server interfaces, and
codec-specific decoding separate from ASF container identification. Do not
register later WMP COM interfaces as NetShow interfaces without evidence.
NetShow-era samples are still required to establish version-specific support;
the existing WMV1/WMA1 fixture does not establish that support.

## References

- https://news.microsoft.com/source/1997/03/10/microsoft-announces-immediate-availability-of-netshow-2-0-beta-brings-multimedia-broadcast-and-communication-to-the-internet-and-intranets/
- https://news.microsoft.com/source/1997/09/10/microsoft-industry-leaders-publish-asf-draft-specification-in-collaborative-effort-to-standardize-internet-streaming-media-formats/
- https://news.microsoft.com/source/1998/01/26/microsoft-announces-first-beta-of-netshow-3-0/
- https://learn.microsoft.com/en-us/windows/win32/medfound/asf-file-structure
- https://learn.microsoft.com/en-us/windows/win32/directshow/using-windows-media-in-directshow
