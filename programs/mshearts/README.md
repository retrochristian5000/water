# MSHEARTS.EXE — initial Water compatibility game

This is a clean-room **offline** Hearts/Black Lady implementation. Windows 98
shipped Microsoft Hearts Network; its LAN/NetDDE protocol is **not** implemented.

Four players receive 13 cards. The human sits South ("You") and three
rule-based computer opponents fill West, North, and East. Passing rotates
left/right/across/hold; opening, following suit, broken hearts, first-trick
point restrictions, 26-point moon shots, and the 100-point game threshold
are implemented in `hearts.c`.

The Win32 game uses the **existing** `cards.dll` `cdtInit`, `cdtDrawExt`
and `cdtTerm` exports dynamically. If the DLL is unavailable, the same game
runs with GDI card rectangles and suit/rank labels. It does not redistribute
Microsoft card graphics or MSHEARTS.EXE assets.

## Build

The new `programs/mshearts` directory is registered in `configure.ac` and
uses Water's normal `MODULE = mshearts.exe` build.

## Portable rules regression test

From the repository root:

```sh
cc -std=c99 -Wall -Wextra -Werror -Iprograms/mshearts \
  programs/mshearts/hearts.c programs/mshearts/tests/logic.c -o /tmp/mshearts-test
/tmp/mshearts-test
```

The test exercises deterministic deals, all cards once, legal opening,
follow-suit rules, AI completion, 13 tricks/52 cards per hand, 26 raw penalty
points per hand, and eventual match completion. UI and actual Win32 runtime
still require manual smoke tests.

This is not an exact visual or AI reproduction of Microsoft's 1998 executable.
