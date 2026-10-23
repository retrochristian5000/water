# HyperTerminal compatibility: initial serial terminal

This is an **experimental direct-serial terminal application** for Water's
Win32 Windows 95/98 compatibility work. It is an original implementation,
**not** Hilgraeve's licensed HyperTerminal, and it does not reproduce the
Microsoft-bundled HyperTerminal feature set or claim hypertrm.exe binary,
UI, or saved-connection compatibility.

The program is registered as hypertrm.exe by Water's program build system.
Run it, select COM1 through COM256 and an explicit baud rate, and press
Connect. It opens the Win32 \\.\COMn device with CreateFileA, configures
8 data bits, no parity, one stop bit, and no flow control using GetCommState
and SetCommState, and exchanges raw bytes through ReadFile and WriteFile.
The display handles simple ASCII text and CR/LF; typing sends bytes directly.
Win32 ANSI UI APIs are used to avoid introducing a Unicode-only GUI dependency
into the Windows 95 personality.

**Current limitations:** No ANSI/VT100 escape-sequence emulator, modem dialing,
TAPI, networking/Telnet, file transfer (X/Y/ZMODEM, Kermit), serial port list,
connection wizard, .ht file format, terminal scripting, local-echo control,
RX ring buffer, hardware flow control selector, proper international code-page
handling, or UI-thread-safe asynchronous serial I/O. The input control uses
synchronous one-byte writes (maximum configured write timeout 1000ms).
Avoid assuming this is a complete HyperTerminal replacement.

Serial port configuration and emulation must remain distinct: the existing
serialui.dll/kernel32 COM API implementations and tapi32.dll own the
underlying Windows behaviors. Do not work around those layers in the app.

## Verification

The parser is testable without Windows headers or a guest VM:

    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
       -I programs/hypertrm programs/hypertrm/settings.c \
       programs/hypertrm/tests/settings_test.c -o settings-test
    ./settings-test

Manual guest testing required: a connected or emulated COM-port pair; ensure
8-N-1, verify text in both directions and disconnect/reconnect. Check separate
Windows 95 RTM and Windows 98 personalities. No claim of guest testing is made
until those test runs have actually been performed.
