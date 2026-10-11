#!/usr/bin/env python3
"""Source-level guards for Water's WinINet/Schannel TLS policy.

This checks policy wiring; it is not a substitute for a real HTTPS handshake
and certificate-validation test on each supported host and Windows ABI.
"""
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
net = (ROOT / "dlls/wininet/netconnection.c").read_text(encoding="utf-8")
schannel = (ROOT / "dlls/secur32/schannel.c").read_text(encoding="utf-8")
gnutls = (ROOT / "dlls/secur32/schannel_gnutls.c").read_text(encoding="utf-8")

# The WinINet credential and handshake paths must not create/use an
# unrequested weaker TLS credential after a failed handshake.
assert "compat_cred_handle" not in net
assert "have_compat_cred_handle" not in net
assert "netcon_secure_connect_setup(connection, TRUE)" not in net
connect = net.split("DWORD NETCON_secure_connect(", 1)[1].split(
    "static BOOL send_ssl_chunk(", 1
)[0]
assert connect.count("netcon_secure_connect_setup(connection)") == 1
assert "return netcon_secure_connect_setup(connection);" in connect

# Protocol registry keys are per client/server role, with a backend
# capability mask; TLS 1.3 support must not bypass Schannel policy.
assert re.search(
    r'\{\s*L"TLS 1\.3",\s*SP_PROT_TLS1_3_CLIENT,\s*'
    r'SP_PROT_TLS1_3_SERVER,\s*TRUE,\s*FALSE\s*\}',
    schannel,
)
assert "enabled & GNUTLS_CALL( get_enabled_protocols, NULL )" in schannel
assert "RegCloseKey(protocols_key);\n    }else {" in schannel
assert "SP_PROT_TLS1_3_CLIENT, \"VERS-TLS1.3\"" in gnutls
assert "SP_PROT_TLS1_3_SERVER, \"VERS-TLS1.3\"" in gnutls

# Earlier protocols remain expressible via explicit per-protocol registry
# policy; this change does not rewrite the chosen historical Windows profile.
for protocol in ("SSL 2.0", "SSL 3.0", "TLS 1.0", "TLS 1.1", "TLS 1.2"):
    assert f'L"{protocol}"' in schannel

print("PASS: WinINet does not downgrade TLS; Schannel TLS 1.3 policy is wired")
