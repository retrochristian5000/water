# FTP compatibility: IIS Server versus WinINet client

Audit date: **11 October 2026**. This records source-backed Windows FTP
service boundaries and a concrete WinINet interoperability repair.

## Distinct Windows versions and owners

| Family | FTP server evidence | Water consequence |
| --- | --- | --- |
| Windows NT Server 4.0 / IIS 4.0 | Older IIS architecture and inetinfo.exe hosting | Do not reuse Win9x client as server |
| Windows 2000 Server / IIS 5.0 | IIS 5.0 with inetinfo.exe, metabase | Guest services and FTP authentication belong to NT server personality |
| Windows Server 2003 / IIS 6.0 | Older FTP metabase under LM/MSFTPSVC | Not the modern ApplicationHost.config XML schema |
| Windows Server 2008 / IIS 7.0 | FTP 7 downloadable separately | Don't assert it shipped with the default OS |
| Windows Server 2008 R2 / IIS 7.5 | FTP 7.5 in-box feature; ftpsvc service and system.ftpServer XML | Different service/configuration path from IIS 4/5/6 |

Modern FTP 7 configuration replaces IIS 6 metabase settings, but doesn't
establish the older `inetinfo.exe` binaries' internal ownership. Likewise,
`HTTP.sys` is not an FTP server, and WinINet isn't an IIS FTP listener.

## Water code audit

- `dlls/wininet/ftp.c`: implements FTP **client** control/data sockets,
  active/passive transfer and LIST parsing.
- `dlls/wininet/tests/ftp.c`: existing client tests, many requiring remote
  FTP access; not an IIS service implementation.
- `programs/services` and `programs/svchost`: Windows Service Control
  Manager and service-host infrastructure, not the FTP service itself.
- No dedicated `inetinfo.exe`, `MSFTPSVC`, or `ftpsvc` server component
  was identified in the inspected Water module listings.

## Concrete interoperability repair

IIS FTP can serve MS-DOS-style directory listings with two-digit or
four-digit years. Water's previous `FTP_ParseNextFile` parser accepted
only an eight-character date and read just one token as the filename.
`FTP_ParseDOSListing` now parses both date widths, AM/PM (including
midnight/noon), directory markers, 32-bit-bounded sizes and full names
with spaces. Invalid input falls back to the existing UNIX parser;
unsupported 64-bit sizes are rejected rather than truncated. No
host network listener or anonymous upload service is added.

Check the actual Water helper with synthetic IIS records:

```sh
python3 -m unittest discover -s scripts/tests -p test_iis_ftp_listing.py -v
```

## FTP folder-view boundary (audit: 1 November 2026)

The FTP *client transport*, the FTP *URL protocol handler*, and the FTP
*Shell namespace folder* are three distinct responsibilities. A WinINet
`FtpFindFirstFileW` implementation is not by itself a working Explorer
folder, and rendering FTP data inside an HTML browser is not an
`IShellFolder` implementation.

### Historical personality and installation distinctions

- IE4's FTP browsing was browser-oriented. IE5 introduced an optional FTP
  Folders shell extension (associated with `MSIEFTP.DLL`), allowing both
  Internet Explorer and Windows Explorer to browse an FTP server using a
  folder interface. Windows 98 SE, Me and Windows 2000 included the helper.
- An IE6 version number alone is insufficient to infer the helper exists:
  Microsoft KB299893 documents cases where IE6 on Windows 98 RTM or NT4
  did not install the Graphical FTP Helper. Preserve the installed-feature
  distinction instead of hard-coding `IE >= 5` as meaning FTP folders.
- IE7 decoupled IE FTP browsing from the Windows Explorer shell view.
  KB941896 describes the browser's WinINet-based view and Explorer's
  separate folder-based implementation. In IE7 and later, the Internet
  Options setting refers to enabling the folder view *outside* IE.
- Do not equate passive/active FTP transport settings with the choice of
  folder versus web-oriented view; the behavior and historical defaults
  must be checked separately.

### Water's currently identified call paths

- `programs/explorer/explorer.c`: Explorer's address bar calls
  `ILCreateFromPathW`; its starting-folder path calls
  `IShellFolder_ParseDisplayName`. Both rely on Shell PIDLs.
- `dlls/shell32/pidl.c` and `dlls/shell32/shfldr_desktop.c`: Shell path
  parsing reaches `IEParseDisplayNameWithBCW` for a non-`shell:` URL.
- `dlls/shdocvw/shdocvw_main.c`: that exported function currently returns
  `E_FAIL` unconditionally. This is a reproducible *source-level*
  blocker for `ftp://` Shell navigation; it is not evidence of a
  completed runtime test.
- `dlls/ieframe/navigate.c` and `dlls/urlmon/ftp.c`: browser navigation
  and URLMON FTP transfer paths exist but do not instantiate an FTP
  namespace folder.
- `dlls/wininet/ftp.c`: supports FTP client directory enumeration,
  downloads, uploads, rename, delete and directory operations. Reuse
  this layer for transport rather than writing another FTP client.
- No `dlls/msieftp`-style module appears in the audited DLL module
  listing. `dlls/shell32/shfldr_netplaces.c` owns Network Places,
  **not** FTP directory contents.

### Implementation and acceptance boundaries

The missing owner is an FTP Shell namespace extension: parse a
`ftp://` display name into a stable PIDL; bind to an `IShellFolder`
for the URL and its child directories; enumerate `WIN32_FIND_DATA`
entries from WinINet; provide an `IShellView` (prefer the existing
Shell default view where applicable); and preserve parsing names,
folder attributes, error behavior and navigation state. File operations
must use appropriate WinINet FTP verbs, not a mapped local filesystem
path. Do not report success by making `IEParseDisplayNameWithBCW`
return a fabricated PIDL which cannot bind to a real folder object.

Regression fixtures should use a loopback/offline FTP server and check:
anonymous versus credential-prompt login; root and assigned-home paths;
folder traversal and parent navigation; names containing spaces;
DOS versus UNIX LIST records; empty directories; file download and
authorized upload; permission failures; and isolation of HTTP browsing.
Check IE5/IE6 with FTP Folders installed versus absent, then IE7+ browser
versus Windows Explorer routing. Never expose passwords in debug logs,
URL bar history, or regression snapshots.

**Status:** source audit only. The Shell folder implementation, end-to-end
FTP-folder regression tests, a compiled build, and native-version runtime
comparison remain outstanding. A change to URLMON or WinINet alone must
not be marked as a fix for Explorer's FTP folder view.

The server-component inventory above is dated 11 October 2026:
`programs/inetinfo` appears in the 1 November 2026 module list, so
the earlier server inventory must not be interpreted as a current
absence finding. This client-folder audit does not evaluate that service.

Additional references:
- Microsoft KB217888, *How to Install and Use FTP Folders*:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/217888
- Microsoft KB299893, IE6 Graphical FTP Helper installation boundary:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/299893
- Microsoft KB941896, IE7 and Explorer FTP behavior:
  https://mskb.pkisolutions.com/kb/941896
- Microsoft, *Understanding Shell Namespace Extensions*:
  https://learn.microsoft.com/en-us/windows/win32/shell/nse-works

## Open implementation work

Recover real NT4, Windows 2000 and Server 2003 IIS binaries/manifests
before creating an FTP service. Map registry service names, metabase
values, site roots, permissions, anonymous/integrated authentication,
and passive/active data socket control. Never implement an unprotected
host-accessible FTP listener as a placeholder; default to offline
or loopback test fixtures. Real IIS interoperability and full Water
build tests remain outstanding.

## Sources

- [Microsoft: IIS 4/5/6 architecture matrix](https://learn.microsoft.com/en-us/previous-versions/iis/6.0-sdk/ms525752%28v%3Dvs.90%29).
- [Microsoft: FTP 7 config and IIS 6 LM/MSFTPSVC metabase boundary](https://learn.microsoft.com/en-us/iis/configuration/system.ftpserver/).
- [Microsoft: Windows IIS FTP directory listing styles](https://learn.microsoft.com/en-us/iis/configuration/system.applicationhost/sites/site/ftpserver/directorybrowse).
- [Microsoft: IIS 7 FTP service and firewall](https://learn.microsoft.com/en-us/iis/publish/using-the-ftp-service/configuring-ftp-firewall-settings-in-iis-7).
