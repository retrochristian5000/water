# IIS 5 FTP service boundary

The historical target of inetinfo.exe is Windows 2000 IIS 5, not IIS 7+.
MSFTPSVC is the SCM service name, not an alias for the WinINet FTP client.
The original FTP binary's export contract has not been recovered; ftp.h is
private WATER code, not a replacement DLL ABI.

The FTP control-line parser is an offline foundation tested independently
of the service. It accepts one bounded ASCII line ending in CRLF and
identifies a subset of RFC 959 commands without storing credentials or
opening files or sockets. The 512-byte limit is a provisional WATER limit,
not a verified IIS 5 default. The parser does not handle Telnet negotiation,
UTF-8, stream reassembly, authentication or transfer semantics.

## Configuration and startup

Preserve the legacy IIS metabase hierarchy (/LM/MSFTPSVC and
/LM/MSFTPSVC/<site ID>) rather than the later applicationHost.config schema.
The legacy metabase SDK describes ServerBindings, MaxConnections,
ConnectionTimeout and AllowAnonymous at FTP nodes. Do not fabricate the
metabase API or default to anonymous access.

The SCM entry for MSFTPSVC remains stopped with ERROR_CALL_NOT_IMPLEMENTED.
No listener, service installation, file publication or firewall changes
are performed. A complete backend must support explicit administrator
configuration, authorization, a confined publishing root, resource limits
and clean service shutdown before it reports SERVICE_RUNNING.

## References

- RFC 959: https://www.rfc-editor.org/rfc/rfc959.html
- Legacy IIS metabase: https://learn.microsoft.com/en-us/previous-versions/iis/6.0-sdk/ms524773(v=vs.90)
- MaxConnections: https://learn.microsoft.com/en-us/previous-versions/iis/6.0-sdk/ms524491(v=vs.90)
- ConnectionTimeout: https://learn.microsoft.com/en-us/previous-versions/iis/6.0-sdk/ms525597(v=vs.90)
- AllowAnonymous: https://learn.microsoft.com/en-us/previous-versions/iis/6.0-sdk/ms524882(v=vs.90)

The SDK references above are for IIS 6; exact IIS 5 defaults and binary
compatibility require separate historical verification.
