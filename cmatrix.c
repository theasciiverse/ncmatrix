/*
    cmatrix.c

    Copyright (C) 1999-2017 Chris Allegretta
    Copyright (C) 2017-Present Abishek V Ashok
    Copyright (C) 2005 Thomas Bloedorn (the network display, from his
    NCMatrix 1.0, GPL version 1 or later)

    This file is part of cmatrix.

    cmatrix is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cmatrix is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cmatrix. If not, see <http://www.gnu.org/licenses/>.

*/

#define NCURSES_WIDECHAR 1

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <locale.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2ipdef.h> /* before iphlpapi.h, for GetIfTable2 */
#include <windows.h>
#include <iphlpapi.h>
#endif

#ifndef EXCLUDE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#ifdef _WIN32
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#elif defined(HAVE_TERMIO_H)
#include <termio.h>
#endif

#ifdef __CYGWIN__
#define TIOCSTI 0x5412
#endif

/* Matrix typedef */
typedef struct cmatrix {
    int val;
    bool is_head;
    int color;  /* NCMatrix: a traffic colour, 0 for the rain's own */
    bool net;   /* NCMatrix: painted by network traffic, drawn bold */
} cmatrix;

/* Global variables */
int console = 0;
int xwindow = 0;
int lock = 0;
cmatrix **matrix = (cmatrix **) NULL;
int *length = NULL;  /* Length of cols in each line */
int *spaces = NULL;  /* Spaces left to fill */
int *updates = NULL; /* What does this do again? */
#ifndef _WIN32
volatile sig_atomic_t signal_status = 0; /* Indicates a caught signal */
#endif

/* NCMatrix: the network display, after Thomas Bloedorn's NCMatrix 1.0
 * (2005). Packets sent and received since the last reading are painted
 * into the rain as bold runs, in the transmit and receive colours. */
int net_on = 1;
char *net_nic = NULL;      /* NULL: every interface but loopback */
int net_poll = 4;          /* screen updates between readings */
int tx_color = COLOR_RED;
int rx_color = COLOR_MAGENTA;

int va_system(char *str, ...) {

    va_list ap;
    char buf[133];

    va_start(ap, str);
    vsnprintf(buf, sizeof(buf), str, ap);
    va_end(ap);
    return system(buf);
}

/* What we do when we're all set to exit */
void finish(void) {
    curs_set(1);
    clear();
    refresh();
    resetty();
    endwin();
    if (console) {
#ifdef HAVE_CONSOLECHARS
        va_system("consolechars -d");
#elif defined(HAVE_SETFONT)
        va_system("setfont");
#endif
    }
    exit(0);
}

/* What we do when we're all set to exit */
void c_die(char *msg, ...) {

    va_list ap;

    curs_set(1);
    clear();
    refresh();
    resetty();
    endwin();

    if (console) {
#ifdef HAVE_CONSOLECHARS
        va_system("consolechars -d");
#elif defined(HAVE_SETFONT)
        va_system("setfont");
#endif
    }

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    exit(0);
}

void usage(void) {
    printf(" Usage: ncmatrix -[abBcfhlsmNVxk] [-u delay] [-C color] [-t tty] [-M message]\n");
    printf("                 [-I interface] [-H updates] [-R color] [-T color]\n");
    printf(" -a: Asynchronous scroll\n");
    printf(" -b: Bold characters on\n");
    printf(" -B: All bold characters (overrides -b)\n");
    printf(" -c: Use Japanese characters as seen in the original matrix. Requires appropriate fonts\n");
    printf(" -f: Force the linux $TERM type to be on\n");
    printf(" -l: Linux mode (uses matrix console font)\n");
    printf(" -L: Lock mode (can be closed from another terminal)\n");
    printf(" -o: Use old-style scrolling\n");
    printf(" -h: Print usage and exit\n");
    printf(" -n: No bold characters (overrides -b and -B, default)\n");
    printf(" -s: \"Screensaver\" mode, exits on first keystroke\n");
    printf(" -x: X window mode, use if your xterm is using mtx.pcf\n");
    printf(" -V: Print version information and exit\n");
    printf(" -M [message]: Prints your message in the center of the screen. Overrides -L's default message.\n");
    printf(" -u delay (0 - 10, default 4): Screen update delay\n");
    printf(" -C [color]: Use this color for matrix (default green)\n");
    printf(" -r: rainbow mode\n");
    printf(" -m: lambda mode\n");
    printf(" -k: Characters change while scrolling. (Works without -o opt.)\n");
    printf(" -t [tty]: Set tty to use\n");
    printf(" -I [interface]: Show this network interface's traffic (default: all but loopback)\n");
    printf(" -H [updates]: Screen updates between network readings (default 4)\n");
    printf(" -R [color]: Color for received packets (default magenta)\n");
    printf(" -T [color]: Color for transmitted packets (default red)\n");
    printf(" -N: No network display (N while running turns it off and on)\n");
}

void version(void) {
    printf(" NCMatrix version %s (compiled %s, %s)\n",
        VERSION, __TIME__, __DATE__);
    printf("CMatrix by Chris Allegretta, Abishek V Ashok and contributors:\n");
    printf("  https://github.com/abishekvashok/cmatrix\n");
    printf("Network display from NCMatrix 1.0 (2005) by Thomas Bloedorn\n");
}

/* A colour by name, or -1 */
int color_by_name(const char *name) {
    static const struct { const char *name; int color; } colors[] = {
        {"green", COLOR_GREEN}, {"red", COLOR_RED}, {"blue", COLOR_BLUE},
        {"white", COLOR_WHITE}, {"yellow", COLOR_YELLOW}, {"cyan", COLOR_CYAN},
        {"magenta", COLOR_MAGENTA}, {"black", COLOR_BLACK},
    };
    size_t k;
    for (k = 0; k < sizeof(colors) / sizeof(colors[0]); k++) {
        if (!strcasecmp(name, colors[k].name)) {
            return colors[k].color;
        }
    }
    return -1;
}

/* NCMatrix: the packets sent and received so far, summed over net_nic or
 * every interface but loopback. Returns 0 if they cannot be read. */
int net_counters(unsigned long long *tx, unsigned long long *rx) {
    *tx = *rx = 0;
#if defined(__linux__)
    /* /proc/net/dev: "name: rx bytes packets errs drop fifo frame
     * compressed multicast, tx bytes packets ..." */
    char line[512];
    int found = 0;
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        char *name = line;
        unsigned long long v[10];
        if (!colon) {
            continue;
        }
        *colon = '\0';
        while (*name == ' ') {
            name++;
        }
        if (net_nic ? strcmp(name, net_nic) != 0 : strcmp(name, "lo") == 0) {
            continue;
        }
        if (sscanf(colon + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7],
                   &v[8], &v[9]) == 10) {
            *rx += v[1];
            *tx += v[9];
            found = 1;
        }
    }
    fclose(f);
    return found;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    struct ifaddrs *ifs, *a;
    int found = 0;
    if (getifaddrs(&ifs) != 0) {
        return 0;
    }
    for (a = ifs; a; a = a->ifa_next) {
        struct if_data *d;
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_LINK || !a->ifa_data) {
            continue;
        }
        if (net_nic ? strcmp(a->ifa_name, net_nic) != 0 : (a->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }
        d = (struct if_data *) a->ifa_data;
        *rx += d->ifi_ipackets;
        *tx += d->ifi_opackets;
        found = 1;
    }
    freeifaddrs(ifs);
    return found;
#elif defined(_WIN32)
    /* real network cards only: Windows stacks filter interfaces on each
     * one that repeat its counts; -I takes the name shown in Windows
     * ("Wi-Fi", "Ethernet") */
    MIB_IF_TABLE2 *table;
    ULONG k;
    int found = 0;
    if (GetIfTable2(&table) != NO_ERROR) {
        return 0;
    }
    for (k = 0; k < table->NumEntries; k++) {
        MIB_IF_ROW2 *r = &table->Table[k];
        if (net_nic) {
            char name[256];
            if (wcstombs(name, r->Alias, sizeof(name)) == (size_t) -1
                || strcasecmp(name, net_nic) != 0) {
                continue;
            }
        } else if (r->Type == IF_TYPE_SOFTWARE_LOOPBACK
                   || !r->InterfaceAndOperStatusFlags.HardwareInterface) {
            continue;
        }
        *rx += r->InUcastPkts + r->InNUcastPkts;
        *tx += r->OutUcastPkts + r->OutNUcastPkts;
        found = 1;
    }
    FreeMibTable(table);
    return found;
#else
    return 0;
#endif
}

/* NCMatrix: paints a run of up to `packets` cells, down from a random cell
 * that shows a character, in a traffic colour. */
void net_paint(unsigned long long packets, int color) {
    int i = 1, j = 0, tries, n;
    if (packets == 0 || LINES < 1 || COLS < 1) {
        return;
    }
    n = packets > (unsigned long long) LINES ? LINES : (int) packets;
    /* the rain falls in the even columns; land on a character if we can */
    for (tries = 0; tries < 8; tries++) {
        i = rand() % LINES + 1;
        j = (rand() % ((COLS + 1) / 2)) * 2;
        if (matrix[i][j].val != ' ' && matrix[i][j].val != -1) {
            break;
        }
    }
    for (; n > 0 && i <= LINES; n--, i++) {
        matrix[i][j].color = color;
        matrix[i][j].net = true;
    }
}


/* nmalloc from nano by Big Gaute */
void *nmalloc(size_t howmuch) {
    void *r;

    if (!(r = malloc(howmuch))) {
        c_die("CMatrix: malloc: out of memory!");
    }

    return r;
}

/* Initialize the global variables */
void var_init() {
    int i, j;

    if (matrix != NULL) {
        free(matrix[0]);
        free(matrix);
    }

    matrix = nmalloc(sizeof(cmatrix *) * (LINES + 1));
    matrix[0] = nmalloc(sizeof(cmatrix) * (LINES + 1) * COLS);
    memset(matrix[0], 0, sizeof(cmatrix) * (LINES + 1) * COLS);
    for (i = 1; i <= LINES; i++) {
        matrix[i] = matrix[i - 1] + COLS;
    }

    if (length != NULL) {
        free(length);
    }
    length = nmalloc(COLS * sizeof(int));

    if (spaces != NULL) {
        free(spaces);
    }
    spaces = nmalloc(COLS* sizeof(int));

    if (updates != NULL) {
        free(updates);
    }
    updates = nmalloc(COLS * sizeof(int));

    /* Make the matrix */
    for (i = 0; i <= LINES; i++) {
        for (j = 0; j <= COLS - 1; j += 2) {
            matrix[i][j].val = -1;
        }
    }

    for (j = 0; j <= COLS - 1; j += 2) {
        /* Set up spaces[] array of how many spaces to skip */
        spaces[j] = (int) rand() % LINES + 1;

        /* And length of the stream */
        length[j] = (int) rand() % (LINES - 3) + 3;

        /* Sentinel value for creation of new objects */
        matrix[1][j].val = ' ';

        /* And set updates[] array for update speed. */
        updates[j] = (int) rand() % 3 + 1;
    }

}

#ifndef _WIN32
void sighandler(int s) {
    signal_status = s;
}
#endif

void resize_screen(void) {
#ifdef _WIN32
    BOOL result;
    HANDLE hStdHandle;
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

    hStdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdHandle == INVALID_HANDLE_VALUE)
        return;
#else
    char *tty;
    int fd = 0;
    int result = 0;
    struct winsize win;

    tty = ttyname(0);
    if (!tty) {
        return;
#endif
#ifdef _WIN32
    result = GetConsoleScreenBufferInfo(hStdHandle, &csbiInfo);
    if (!result)
        return;
    LINES = csbiInfo.dwSize.Y;
    COLS = csbiInfo.dwSize.X;
#else
    }
    fd = open(tty, O_RDWR);
    if (fd == -1) {
        return;
    }
    result = ioctl(fd, TIOCGWINSZ, &win);
    if (result == -1) {
        return;
    }

    COLS = win.ws_col;
    LINES = win.ws_row;
#endif

    if (LINES < 10) {
        LINES = 10;
    }
    if (COLS < 10) {
        COLS = 10;
    }

#ifdef HAVE_RESIZETERM
    resizeterm(LINES, COLS);
#ifdef HAVE_WRESIZE
    if (wresize(stdscr, LINES, COLS) == ERR) {
        c_die("Cannot resize window!");
    }
#endif /* HAVE_WRESIZE */
#endif /* HAVE_RESIZETERM */

    var_init();
    /* Do these because width may have changed... */
    clear();
    refresh();
}

int main(int argc, char *argv[]) {
    int i, y, z, optchr, keypress;
    int j = 0;
    int count = 0;
    int screensaver = 0;
    int asynch = 0;
    int bold = 0;
    int force = 0;
    int firstcoldone = 0;
    int oldstyle = 0;
    int random = 0;
    int update = 4;
    int highnum = 0;
    int mcolor = COLOR_GREEN;
    int rainbow = 0;
    int lambda = 0;
    int randnum = 0;
    int randmin = 0;
    int pause = 0;
    int classic = 0;
    int changes = 0;
    char *msg = "";
    char *tty = NULL;
    int net_tick = 0;
    int net_seen = 0;
    unsigned long long net_tx = 0, net_rx = 0;

    srand((unsigned) time(NULL));
    setlocale(LC_ALL, "");

    /* Many thanks to morph- (morph@jmss.com) for this getopt patch */
    opterr = 0;
    while ((optchr = getopt(argc, argv, "abBcfhlLnrosmxkNVM:u:C:t:I:H:R:T:")) != EOF) {
        switch (optchr) {
        case 's':
            screensaver = 1;
            break;
        case 'a':
            asynch = 1;
            break;
        case 'b':
            if (bold != 2) {
                bold = 1;
            }
            break;
        case 'B':
            bold = 2;
            break;
        case 'C':
        case 'R':
        case 'T': {
            int color = color_by_name(optarg);
            if (color < 0) {
                fprintf(stderr, " Invalid color selection\n Valid "
                       "colors are green, red, blue, "
                       "white, yellow, cyan, magenta " "and black.\n");
                exit(1);
            }
            if (optchr == 'C') {
                mcolor = color;
            } else if (optchr == 'R') {
                rx_color = color;
            } else {
                tx_color = color;
            }
            break;
        }
        case 'I':
            net_nic = optarg;
            break;
        case 'H':
            net_poll = atoi(optarg) < 1 ? 1 : atoi(optarg);
            break;
        case 'N':
            net_on = 0;
            break;
        case 'c':
            classic = 1;
            break;
        case 'f':
            force = 1;
            break;
        case 'l':
            console = 1;
            break;
        case 'L':
            lock = 1;
            //if -M was used earlier, don't override it
            if (0 == strncmp(msg, "", 1)) {
                msg = "Computer locked.";
            }
            break;
        case 'M':
            msg = strdup(optarg);
            break;
        case 'n':
            bold = -1;
            break;
        case 'h':
        case '?':
            usage();
            exit(0);
        case 'o':
            oldstyle = 1;
            break;
        case 'u':
            update = atoi(optarg);
            break;
        case 'x':
            xwindow = 1;
            break;
        case 'V':
            version();
            exit(0);
        case 'r':
            rainbow = 1;
            break;
        case 'm':
            lambda = 1;
            break;
        case 'k':
            changes = 1;
            break;
        case 't':
            tty = optarg;
            break;
        }
    }

    /* Clear TERM variable on Windows */
#ifdef _WIN32
    _putenv_s("TERM", "");
#endif

    if (force && strcmp("linux", getenv("TERM"))) {
#ifdef _WIN32
        SetEnvironmentVariableW(L"TERM", L"linux");
#else
        /* setenv is much more safe to use than putenv */
        setenv("TERM", "linux", 1);
#endif
    }
    if (tty) {
        FILE *ftty = fopen(tty, "r+");
        if (!ftty) {
            fprintf(stderr, "cmatrix: error: '%s' couldn't be opened: %s.\n",
                    tty, strerror(errno));
            exit(EXIT_FAILURE);
        }
        SCREEN *ttyscr;
        ttyscr = newterm(NULL, ftty, ftty);
        if (ttyscr == NULL)
            exit(EXIT_FAILURE);
        set_term(ttyscr);
    } else
        initscr();
    savetty();
    nonl();
#ifdef _WIN32
    raw();
#else
    cbreak();
#endif
    noecho();
    timeout(0);
    leaveok(stdscr, TRUE);
    curs_set(0);
#ifndef _WIN32
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGHUP, sighandler);
    signal(SIGWINCH, sighandler);
    signal(SIGTSTP, sighandler);
#endif

if (console) {
#ifdef HAVE_CONSOLECHARS
        if (va_system("consolechars -f matrix") != 0) {
            c_die
                (" There was an error running consolechars. Please make sure the\n"
                 " consolechars program is in your $PATH.  Try running \"consolechars -f matrix\" by hand.\n");
        }
#elif defined(HAVE_SETFONT)
        if (va_system("setfont matrix") != 0) {
            c_die
                (" There was an error running setfont. Please make sure the\n"
                 " setfont program is in your $PATH.  Try running \"setfont matrix\" by hand.\n");
        }
#else
        c_die(" Unable to use both \"setfont\" and \"consolechars\".\n");
#endif
}
    if (has_colors()) {
        start_color();
        /* Add in colors, if available */
#ifdef HAVE_USE_DEFAULT_COLORS
        if (use_default_colors() != ERR) {
            init_pair(COLOR_BLACK, -1, -1);
            init_pair(COLOR_GREEN, COLOR_GREEN, -1);
            init_pair(COLOR_WHITE, COLOR_WHITE, -1);
            init_pair(COLOR_RED, COLOR_RED, -1);
            init_pair(COLOR_CYAN, COLOR_CYAN, -1);
            init_pair(COLOR_MAGENTA, COLOR_MAGENTA, -1);
            init_pair(COLOR_BLUE, COLOR_BLUE, -1);
            init_pair(COLOR_YELLOW, COLOR_YELLOW, -1);
        } else {
#else
        { /* Hack to deal the after effects of else in HAVE_USE_DEFAULT_COLOURS */
#endif
            init_pair(COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
            init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
            init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
            init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
            init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
            init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
            init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
            init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
        }
    }

    /* Set up values for random number generation */
    if (classic) {
        /* Half-width kana characters. In the movie they are y-axis flipped, and
         * they appear alongside latin characters and numerals, but this is the
         * closest we can do with a standard unicode set and a single number
         * range */
        randmin = 0xff66;
        highnum = 0xff9d;
    } else if (console || xwindow) {
        randmin = 166;
        highnum = 217;
    } else {
        randmin = 33;
        highnum = 123;
    }
    randnum = highnum - randmin;

    var_init();

    while (1) {
#ifndef _WIN32
        /* Check for signals */
        if (signal_status == SIGINT || signal_status == SIGQUIT) {
            if (lock != 1)
                finish();
            /* exits */
        }
        /* asked to stop, or the terminal went away: put it back and go,
         * locked or not */
        if (signal_status == SIGTERM || signal_status == SIGHUP) {
            finish();
        }
        if (signal_status == SIGWINCH) {
            resize_screen();
            signal_status = 0;
        }

        if (signal_status == SIGTSTP) {
            if (lock != 1)
                    finish();
        }
#endif

        count++;
        if (count > 4) {
            count = 1;
        }

        if ((keypress = wgetch(stdscr)) != ERR) {
            if (screensaver == 1) {
#ifdef USE_TIOCSTI
                char *str = malloc(0);
                size_t str_len = 0;
                do {
                    str = realloc(str, str_len + 1);
                    str[str_len++] = keypress;
                } while ((keypress = wgetch(stdscr)) != ERR);
                size_t i;
                for (i = 0; i < str_len; i++)
                    ioctl(STDIN_FILENO, TIOCSTI, (char*)(str + i));
                free(str);
#endif
                finish();
            } else {
                switch (keypress) {
#ifdef _WIN32
                case 3: /* Ctrl-C. Fall through */
#endif
                case 'q':
                    if (lock != 1)
                        finish();
                    break;
                case 'a':
                    asynch = 1 - asynch;
                    break;
                case 'b':
                    bold = 1;
                    break;
                case 'B':
                    bold = 2;
                    break;
                case 'L':
                    lock = 1;
                    break;
                case 'n':
                    bold = 0;
                    break;
                case '0': /* Fall through */
                case '1': /* Fall through */
                case '2': /* Fall through */
                case '3': /* Fall through */
                case '4': /* Fall through */
                case '5': /* Fall through */
                case '6': /* Fall through */
                case '7': /* Fall through */
                case '8': /* Fall through */
                case '9':
                    update = keypress - 48;
                    break;
                case '!':
                    mcolor = COLOR_RED;
                    rainbow = 0;
                    break;
                case '@':
                    mcolor = COLOR_GREEN;
                    rainbow = 0;
                    break;
                case '#':
                    mcolor = COLOR_YELLOW;
                    rainbow = 0;
                    break;
                case '$':
                    mcolor = COLOR_BLUE;
                    rainbow = 0;
                    break;
                case '%':
                    mcolor = COLOR_MAGENTA;
                    rainbow = 0;
                    break;
                case 'r':
                     rainbow = 1;
                     break;
                case 'm':
                     lambda = !lambda;
                     break;
                case '^':
                    mcolor = COLOR_CYAN;
                    rainbow = 0;
                    break;
                case '&':
                    mcolor = COLOR_WHITE;
                    rainbow = 0;
                    break;
                case 'p':
                case 'P':
                    pause = (pause == 0)?1:0;
                    break;
                case 'N':
                    net_on = !net_on;
                    break;

                }
            }
        }
        if (net_on && pause == 0 && ++net_tick >= net_poll) {
            unsigned long long tx, rx;
            net_tick = 0;
            if (net_counters(&tx, &rx)) {
                /* counters that went back (a reset interface) paint nothing */
                if (net_seen && tx >= net_tx && rx >= net_rx) {
                    net_paint(tx - net_tx, tx_color);
                    net_paint(rx - net_rx, rx_color);
                }
                net_tx = tx;
                net_rx = rx;
                net_seen = 1;
            }
        }

        for (j = 0; j <= COLS - 1; j += 2) {
            if ((count > updates[j] || asynch == 0) && pause == 0) {

                /* I don't like old-style scrolling, yuck */
                if (oldstyle) {
                    for (i = LINES - 1; i >= 1; i--) {
                        matrix[i][j].val = matrix[i - 1][j].val;
                        matrix[i][j].color = matrix[i - 1][j].color;
                        matrix[i][j].net = matrix[i - 1][j].net;
                    }
                    matrix[0][j].color = 0;
                    matrix[0][j].net = false;
                    random = (int) rand() % (randnum + 8) + randmin;

                    if (matrix[1][j].val == 0) {
                        matrix[0][j].val = 1;
                    } else if (matrix[1][j].val == ' '
                             || matrix[1][j].val == -1) {
                        if (spaces[j] > 0) {
                            matrix[0][j].val = ' ';
                            spaces[j]--;
                        } else {

                            /* Random number to determine whether head of next column
                               of chars has a white 'head' on it. */

                            if (((int) rand() % 3) == 1) {
                                matrix[0][j].val = 0;
                            } else {
                                matrix[0][j].val = (int) rand() % randnum + randmin;
                            }
                            spaces[j] = (int) rand() % LINES + 1;
                        }
                    } else if (random > highnum && matrix[1][j].val != 1) {
                        matrix[0][j].val = ' ';
                    } else {
                        matrix[0][j].val = (int) rand() % randnum + randmin;
                    }

                } else { /* New style scrolling (default) */
                    if (matrix[0][j].val == -1 && matrix[1][j].val == ' '
                        && spaces[j] > 0) {
                        spaces[j]--;
                    } else if (matrix[0][j].val == -1
                        && matrix[1][j].val == ' ') {
                        length[j] = (int) rand() % (LINES - 3) + 3;
                        matrix[0][j].val = (int) rand() % randnum + randmin;

                        spaces[j] = (int) rand() % LINES + 1;
                    }
                    i = 0;
                    y = 0;
                    firstcoldone = 0;
                    while (i <= LINES) {

                        /* Skip over spaces */
                        while (i <= LINES && (matrix[i][j].val == ' ' ||
                               matrix[i][j].val == -1)) {
                            i++;
                        }

                        if (i > LINES) {
                            break;
                        }

                        /* Go to the head of this column */
                        z = i;
                        y = 0;
                        while (i <= LINES && (matrix[i][j].val != ' ' &&
                               matrix[i][j].val != -1)) {
                            matrix[i][j].is_head = false;
                            if (changes) {
                                if (rand() % 8 == 0)
                                    matrix[i][j].val = (int) rand() % randnum + randmin;
                            }
                            i++;
                            y++;
                        }

                        if (i > LINES) {
                            matrix[z][j].val = ' ';
                            matrix[z][j].color = 0;
                            matrix[z][j].net = false;
                            continue;
                        }

                        matrix[i][j].val = (int) rand() % randnum + randmin;
                        matrix[i][j].is_head = true;
                        matrix[i][j].color = 0;
                        matrix[i][j].net = false;

                        /* If we're at the top of the column and it's reached its
                           full length (about to start moving down), we do this
                           to get it moving.  This is also how we keep segments not
                           already growing from growing accidentally =>
                         */
                        if (y > length[j] || firstcoldone) {
                            matrix[z][j].val = ' ';
                            matrix[z][j].color = 0;
                            matrix[z][j].net = false;
                            matrix[0][j].val = -1;
                        }
                        firstcoldone = 1;
                        i++;
                    }
                }
            }
            /* A simple hack */
            if (!oldstyle) {
                y = 1;
                z = LINES;
            } else {
                y = 0;
                z = LINES - 1;
            }
            for (i = y; i <= z; i++) {
                move(i - y, j);

                if (matrix[i][j].val == 0 || (matrix[i][j].is_head && !rainbow)) {
                    if (console || xwindow) {
                        attron(A_ALTCHARSET);
                    }
                    attron(COLOR_PAIR(COLOR_WHITE));
                    if (bold) {
                        attron(A_BOLD);
                    }
                    if (matrix[i][j].val == 0) {
                        if (console || xwindow) {
                            addch(183);
                        } else {
                            addch('&');
                        }
                    } else if (matrix[i][j].val == -1) {
                        addch(' ');
                    } else {
                        addch(matrix[i][j].val);
                    }

                    attroff(COLOR_PAIR(COLOR_WHITE));
                    if (bold) {
                        attroff(A_BOLD);
                    }
                    if (console || xwindow) {
                        attroff(A_ALTCHARSET);
                    }
                } else {
                    if (rainbow) {
                        int randomColor = rand() % 6;

                        switch (randomColor) {
                            case 0:
                                mcolor = COLOR_GREEN;
                                break;
                            case 1:
                                mcolor = COLOR_BLUE;
                                break;
                            case 2:
                                mcolor = COLOR_BLACK;
                                break;
                            case 3:
                                mcolor = COLOR_YELLOW;
                                break;
                            case 4:
                                mcolor = COLOR_CYAN;
                                break;
                            case 5:
                                mcolor = COLOR_MAGENTA;
                                break;
                       }
                    }
                    int pair = matrix[i][j].color ? matrix[i][j].color : mcolor;
                    attron(COLOR_PAIR(pair));
                    if (matrix[i][j].net) {
                        attron(A_BOLD);
                    }
                    if (matrix[i][j].val == 1) {
                        if (bold) {
                            attron(A_BOLD);
                        }
                        addch('|');
                        if (bold) {
                            attroff(A_BOLD);
                        }
                    } else {
                        if (console || xwindow) {
                            attron(A_ALTCHARSET);
                        }
                        if (bold == 2 ||
                            (bold == 1 && matrix[i][j].val % 2 == 0)) {
                            attron(A_BOLD);
                        }
                        if (matrix[i][j].val == -1) {
                            addch(' ');
                        } else if (lambda && matrix[i][j].val != ' ') {
                            addstr("λ");
                        } else {
                            /* addch doesn't seem to work with unicode
                             * characters and there was no direct equivalent.
                             * So, construct a c-style string with the character
                             * and print that.
                             */
                            wchar_t char_array[2];
                            char_array[0] = matrix[i][j].val;
                            char_array[1] = 0;
                            addwstr(char_array);
                        }
                        if (bold == 2 ||
                            (bold == 1 && matrix[i][j].val % 2 == 0)) {
                            attroff(A_BOLD);
                        }
                        if (console || xwindow) {
                            attroff(A_ALTCHARSET);
                        }
                    }
                    if (matrix[i][j].net) {
                        attroff(A_BOLD);
                    }
                    attroff(COLOR_PAIR(pair));
                }
            }
        }

        //check if -M and/or -L was used
        if (msg[0] != '\0') {
            //Add our message to the screen
            int msg_x = LINES/2;
            int msg_y = COLS/2 - strlen(msg)/2;
            int i = 0;

            //Add space before message
            move(msg_x-1, msg_y-2);
            for (i = 0; i < strlen(msg)+4; i++)
                addch(' ');

            //Write message
            move(msg_x, msg_y-2);
            addch(' ');
            addch(' ');
            addstr(msg);
            addch(' ');
            addch(' ');

            //Add space after message
            move(msg_x+1, msg_y-2);
            for (i = 0; i < strlen(msg)+4; i++)
                addch(' ');
        }

        napms(update * 10);
    }
    finish();
}
