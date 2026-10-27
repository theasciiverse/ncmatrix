# Changes from upstream

NCMatrix for the Asciiverse catalog: current CMatrix (github.com/abishekvashok/cmatrix,
the `upstream` remote) with the network display of Thomas Bloedorn's
NCMatrix 1.0 (2005). CMatrix's history is kept whole, up to upstream master
`5c082c6` (2023-12-09, after 2.0); everything after it is ours, on branch
`asciiverse`. NCMatrix 1.0 is the `ncmatrix` remote (github.com/mayfrost/NCMatrix,
commit `87a794f`), identical to the original `ncmatrix-1.0.zip` from
Bloedorn's site (Internet Archive copy of 2015-08-04, sha256
`b9c25b47ddfbc9bd9d4256a9a69718a7e9e7d25be4f9fb9b2798c1dc886530e2`, md5
`a8889a16f94b6672c7a033376d6c75fc` as in the Arch package) but for a
leftover `Makefile.bak`. There is no later NCMatrix; the other GitHub
repositories named ncmatrix are copies of 1.0 or unrelated programs.
GPL version 3 or later (`COPYING`); `NOTICE` names who made what.

## The network display, from NCMatrix 1.0

As in Bloedorn's version: every so often the packet counters are read,
and the packets sent and received since the last reading are painted into
the rain as a run of that many cells down from a random place, bold, red
for sent and magenta for received; a new character in a cell takes the
rain's colour again. His options keep their letters: `-I` interface, `-R`
and `-T` the colours, `-H` how often.

Changed in carrying it over:

- Each cell keeps its colour (Bloedorn's `color` field) alongside current
  CMatrix's `is_head`, and the cells start cleared (CMatrix left them
  uninitialised).
- More than 80 packets painted nothing (his `NFILL_LIMIT - remainder` went
  negative); a run is now as long as the packets, up to the screen height.
- Runs land in the columns the rain falls in (he could pick odd columns,
  never drawn, or one past the right edge) and on a cell with a character
  when one can be found in eight tries.
- `-I` matches the interface name exactly (he used `strstr`, so `eth0`
  matched `veth0`); without `-I` every interface but loopback counts (his
  default `eth0` is absent on most current systems).
- `-H` counts screen updates between readings, default 4 (his counted
  calls inside the drawing loop, so its meaning changed with the window).
- Counters that go backwards (a reset interface) paint nothing.
- `-N` turns the display off, N while running turns it off and on.
- Linux reads `/proc/net/dev` as he did; macOS and the BSDs read
  `getifaddrs`; Windows reads `GetIfTable2` (real network cards only, as
  Windows stacks filter interfaces that repeat their counts; `-I` takes the
  name Windows shows, such as `Wi-Fi`).

## Other changes

- The binary, usage, version text and man page are NCMatrix's (the man
  page is installed as `ncmatrix.1`); the version text credits CMatrix's
  authors and Bloedorn.
- SIGTERM and SIGHUP quit and put the terminal back (SIGHUP left it
  without echo); locked mode (`-L`) still ignores Ctrl+C.
- `-s` no longer pushes the key that ended it back into the terminal
  (TIOCSTI): started from a launcher the key would land in the launcher,
  and most current kernels refuse it. `-DNCMATRIX_KEEP_KEY=ON` brings it
  back in the CMake build.
- CMake 3.10 is the minimum (2.8 is refused by CMake 4).
- An invalid colour (`-C`, `-R`, `-T`) exits with status 1 (CMatrix
  exited 0).

## Packaging

- `asciiverse/build.sh` builds `out/ncmatrix` with the compiler and
  curses library from the environment; `asciiverse/package.sh` packs
  `dist/ncmatrix-<target>.tar.gz` (.zip for Windows) with the man page,
  README.md, COPYING, NOTICE and this file;
  `.github/workflows/release.yml` builds static musl Linux (x86_64,
  aarch64), macOS (x86_64, arm64) and Windows x86_64 (MinGW, PDCurses
  wide, `asciiverse/win/ncurses.h` pointing at it).
- `asciiverse/record-preview.sh` records the catalog preview.
- Checked here: ASan and UBSan clean with `-I` (present and missing
  interface), `-N`, `-o`, `-r`, `-s`, `-c`, the N key and resizes from
  100x30 down to 1x1 and up to 160x50; q, SIGINT, SIGTERM and SIGHUP leave
  the terminal as it was; traffic shows on Wi-Fi during a download, and an
  unused port (no packets) shows none. Static musl build in Alpine. The
  Windows build, run under Wine 10 with the host's network, draws, shows
  traffic for all interfaces and for `-I wlp6s0`, and q quits with status
  0; real Windows is unchecked. macOS not built here.

## Ours

- **How to play it, at the top of README.md.** A short note on installing and
  starting it with av, or from a release archive; the original text follows
  unchanged.
