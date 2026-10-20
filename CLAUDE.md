# NCMatrix (Asciiverse packaging)

Current CMatrix with Thomas Bloedorn's network display from NCMatrix 1.0
(2005) carried over, for the Asciiverse catalog (kind `art`, id
`ncmatrix`). C, ncurses. GPL-3.0-or-later. `NOTICE` holds the credits; keep
them whole. Changes in `PATCHES.md`.

- Remotes: `upstream` is github.com/abishekvashok/cmatrix (base `5c082c6`,
  2023-12-09); `ncmatrix` is github.com/mayfrost/NCMatrix (Bloedorn's 1.0,
  identical to the archived zip); `origin` is
  github.com/theasciiverse/ncmatrix (private). Branch `asciiverse`.
- The network code is `color_by_name`, `net_counters` (Linux
  /proc/net/dev, macOS and BSD getifaddrs, Windows GetIfTable2) and
  `net_paint` in `cmatrix.c`, read every `net_poll` updates in the main
  loop.
- Build: `asciiverse/build.sh` (see its header for each platform); CMake
  also works and builds `ncmatrix`. Windows needs PDCurses built WIDE=Y
  and `ws2ipdef.h` before `iphlpapi.h` (MinGW declares GetIfTable2 only
  then).
- Release: tag `v2.0+20231209-av.<n>`; the workflow builds static musl
  Linux, macOS and Windows on `*-av.*` tags.
- av starts it as `ncmatrix -s -a -b`: any key quits.
- Preview: `asciiverse/record-preview.sh out/ncmatrix asciiverse/ncmatrix.cast`
  (downloads run while it records).
