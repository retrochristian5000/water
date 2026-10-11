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
