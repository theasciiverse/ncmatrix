#!/bin/sh
# Builds ncmatrix into out/ without CMake or autotools. The compiler and
# curses library come from the environment, so one script serves every
# target (see .github/workflows/release.yml):
#
#   asciiverse/build.sh                                                  # Linux
#   LIBS=-lncursesw LDFLAGS="-static -s" asciiverse/build.sh             # Alpine, static
#   CFLAGS="-O2 -D_XOPEN_SOURCE_EXTENDED" LIBS=-lncurses asciiverse/build.sh   # macOS
#   CC=x86_64-w64-mingw32-gcc CFLAGS="-O2 -Iasciiverse/win -I$PDC -DPDC_WIDE -DPDC_FORCE_UTF8" \
#     LIBS="$PDC/wincon/pdcurses.a -liphlpapi" LDFLAGS="-static -s" EXE=.exe asciiverse/build.sh
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--O2}"
LDFLAGS="${LDFLAGS:-}"
LIBS="${LIBS:-$(pkg-config --libs ncursesw 2>/dev/null || echo -lncursesw)}"
EXE="${EXE:-}"
VERSION="${VERSION:-2.0}"
DEFS="-DEXCLUDE_CONFIG_H -DVERSION=\"$VERSION\" -DHAVE_NCURSES_H -DHAVE_USE_DEFAULT_COLORS -DHAVE_GETOPT_H"
case "$EXE" in
  .exe) ;;
  *) DEFS="$DEFS -DHAVE_SYS_IOCTL_H -DHAVE_UNISTD_H -DHAVE_TERMIOS_H" ;;
esac
mkdir -p out
$CC $CFLAGS $DEFS -Wall cmatrix.c $LDFLAGS $LIBS -o "out/ncmatrix$EXE"
echo "built ncmatrix$EXE"
