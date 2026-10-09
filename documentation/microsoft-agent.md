# Microsoft Agent and Windows XP Search Companion

## Historical evidence and scope

Microsoft Agent 2.0 was an animated-character and interaction subsystem, not
the filesystem search engine. Windows XP's **Search Companion** (accessed
from Explorer's Search pane) used Agent character technology for its
animated assistant. Rover, the yellow dog previously used in Microsoft Bob,
was the default search character. Merlin, the wizard, was an existing
Microsoft Agent character selectable in the XP companion, rather than a
character made specifically for the XP file-search application.

The former Microsoft Knowledge Base article **307980** (last modified
July 15, 2004) documents toggling the animated search companion and
selecting a different character. This proves user-visible XP behavior, not
the specific COM call sequence used by Explorer.

Sources:
- Microsoft Agent platform overview:
  https://learn.microsoft.com/en-us/windows/win32/lwef/microsoft-agent
- Microsoft KB307980, archived:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/307980
- XP Search Companion character distinctions (secondary source):
  https://agentpedia.tmafe.com/wiki/Search_Companion

## Owning components and COM identities

Keep these separate. None is a synonym for the other:

| Component | Role | Established identity |
| --- | --- | --- |
| `AgentCtl.dll` | In-process ActiveX control for scripting and hosting | CLSID `{D45FD31B-5C6E-11D1-9EC1-00C04FD7081F}` |
| `AgentSvr.exe` | Out-of-process Microsoft Agent COM server | CLSID `{D45FD2FC-5C6E-11D1-9EC1-00C04FD7081F}` (secondary/period developer evidence; verify on XP image) |
| XP Search Companion | Explorer-integrated local-file-search interface and character selection | **Not** the Agent ActiveX CLSID |
| Character assets | Character definitions and compressed animation resources | `.ACS` (local) or `.ACF` + `.ACA` (split/remote) |

Microsoft's documented ActiveX control class ID:
https://learn.microsoft.com/en-us/windows/win32/lwef/accessing-the-control-in-web-pages

Microsoft describes AgentCtl.dll registration and AgentSvr.exe /regserver:
https://learn.microsoft.com/en-us/windows/win32/lwef/installation-and-page-loading-problems

Period 2002 developer discussion of the server CLSID and `IAgentEx`:
https://groups.google.com/g/microsoft.public.msagent/c/G7SBAYkuPas

`IAgentCtl`, `IAgentCtlEx`, `IAgentCtlCharacter`, and the Agent server's
`IAgent`/`IAgentEx` must not be assigned guessed interface IDs or DISPIDs.
Independently compare the XP type libraries before generating ABI headers.

## Asset and filesystem distinctions

- For locally installed characters, Agent supports `*.ACS` files.
  `Characters.Load` supports a relative character name resolved in
  the system's localized `%WINDIR%\msagent\chars` directory or an
  absolute file location.
- `*.ACF` describes a character and references separately delivered
  `*.ACA` animations. Treat `*.AAF` as historical Agent 1.5-era
  material; **do not** assume all versions of `.ACS` have the same layout.
- XP Search Companion's default Rover asset is reported as
  `%WINDIR%\srchasst\chars\rover.acs`, while Merlin is an existing
  Microsoft Agent character. The XP-specific asset path is third-party
  evidence and needs checking against actual XP media.
- Character data is not the executable COM implementation. Do not
  copy copyrighted Microsoft .ACS files into Water merely to test an API.

Microsoft loading specification:
https://learn.microsoft.com/en-us/windows/win32/lwef/loading-a-character
https://learn.microsoft.com/en-us/windows/win32/lwef/iagent--load

Secondary source for the XP Rover registry/path:
https://www.windowspage.de/tipps/010257.html

## Water audit (2026-10-09)

On the `master` tree at `376a2013f8de95d87238254f6bad62e46b53b2e0`,
there is **no** `AgentCtl.dll`, `AgentSvr.exe`, `IAgentCtl` implementation,
or Microsoft Agent character loader. Water has `programs/explorer`, but
no verified XP Search Companion implementation. `programs/find` is not
the Explorer Search Companion and must not inherit its responsibilities.

Thus Agent is a *missing subsystem*, not a leftover implementation stub.
Adding a COM class factory returning `E_NOTIMPL`, or self-registering a
nonfunctional ActiveX control, does not restore XP search animations and
could interfere with a working native Agent installation.

## Implementation and verification order

1. Verify XP-era type library/interface GUIDs, method signatures,
   calling conventions, and in-process versus local-server COM activation
   on an authentic 32-bit Windows XP reference system.
2. Implement `AgentCtl.dll` character collections, character loading,
   request/event semantics, and matching `IDispatch` contracts; preserve
   local COM and browser ActiveX activation differences.
3. Implement the local `.ACS` loading and animation pipeline, starting
   with owned/test character fixtures. Treat the `.ACF`/`.ACA` pair
   as a separate network-loading path and respect versioned formats.
4. Implement or interoperate with the `AgentSvr.exe` local COM server,
   including client ownership and request cancellation.
5. Only then integrate the XP Search Companion's character selection,
   visibility controls, and animation with the actual Explorer search UI.
   File search must still work with animated characters disabled.
6. Test x86 XP runtime separately from any other Windows personality
   or ABI. A successful DLL load or COM activation is **not** proof that
   an animation played or that a file search worked.

No Microsoft Agent binaries, character assets, COM registration, or
executable stubs are added by this documentation-only audit.
