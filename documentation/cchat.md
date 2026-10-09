# CCHAT.EXE: Water Comic Chat compatibility ledger

Status: **SOURCE-IMPLEMENTED / BUILD AND RUNTIME UNVERIFIED**.

## Original provenance and version boundary

Microsoft Comic Chat was a comic-strip IRC chat client. Microsoft has
published its historical source under the MIT license:
<https://github.com/microsoft/comic-chat>.
The `v2.1b` source (February 1998) is the closest currently available
source baseline to the Windows 98 FE files we have been investigating.
It was originally built with Visual C++ 4.x and MFC and includes the
21 base character set (including Dan) and 10 Art Pack characters:
<https://github.com/microsoft/comic-chat/blob/main/docs/v2.1b/README.md>.

Water's prior `programs/` inventory contained no `cchat.exe`.
The executable here is a **new independent LGPL-compatible
approximation**, not imported MFC code or a Microsoft binary.

## Implemented subset

- Win32/ANSI single-window GUI with server, port, nick, channel fields,
  connect/disconnect, input and sending.
- Winsock 2, IPv4, asynchronous `connect`/`recv`/`send`,
  a buffered partial-send queue, IRC registration, PING/PONG, JOIN,
  standard `PRIVMSG` and `NOTICE`, numeric errors and nick updates.
- Text commands: `/join #channel`, `/nick nickname`,
  `/me action`, `/quit`. Outgoing IRC lines are length-checked and
  carriage-return/linefeed injection is rejected; oversized server
  lines are discarded.
- GDI cartoon portraits and scrollable speech-bubble panels.
  Portraits are original simple geometry, **not** Dan or original
  `.AVB` assets. Conversation history is limited to 96 panels.
- Offline parser/validation test: run `cchat.exe --self-test`
  and check exit status 0. It does not connect to a server.

## Known missing Comic Chat compatibility

- **`DAN.AVB` and the 2.1/2.5 AVB art formats are not decoded.**
  Backdrop/art packaging, gestures, facial expressions and the
  original expert-system panel layouts are not recreated. One
  message per panel is a basic visual approximation.
- No OLE/COM automation, avatar editing, original installer,
  shared Comic Chat IRC metadata, multi-channel UI or historical
  versioned registry behavior.
- No TLS/STARTTLS/SASL, IPv6, DCC, proxy settings, file transfer,
  non-ASCII charset negotiation or persistent preferences.
  All network traffic is **unencrypted plaintext IRC**; do not
  transmit private information or credentials.
- Default server/channel fields are examples, not a claim that
  a particular network accepts registration or exposes that channel.
- Win9x hardware-specific GDI and Winsock performance, modern server
  interoperability, compilation and execution remain **unverified**.

## Next test and implementation gates

1. Compile the selected Water Win32 PE target; confirm that
   `cchat.exe` is built and imports `ws2_32`, `user32`, `gdi32`.
2. Execute `cchat.exe --self-test`; assert exit code 0.
3. Use a controlled plaintext IRC test server to exercise two
   independent clients, JOIN, PING/PONG, PRIVMSG, ACTION, nickname
   collision, disconnect and reconnect.
4. Confirm the GUI/scrolling on Windows 98 FE and on a current
   supported Wine/Water environment.
5. Compare the Microsoft `v2.1b` / `v2.5-beta-1` art loaders
   and the actual `DAN.AVB` bytes, recording hashes and native
   format details before implementing artwork decoding.
6. Expand panel layout and metadata only after protocol and asset
   behavior is supported by real tests.

Keep unsupported features explicit. Do not claim original Comic Chat
equivalence based only on producing a file named `CCHAT.EXE`.
