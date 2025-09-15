/*
 * npctl.c  -  Multi-function NeXT Laser Printer N2000 control utility
 * Target: NeXTSTEP 3.3 (m68k)
 */
 
 /*
 * Copyright (C) 2025 Paolo Bertolo
 *
 * This file is part of NeXTPrinterControl.
 *
 * NeXTPrinterControl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * NeXTPrinterControl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NeXTPrinterControl.  
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <mach/boolean.h>
#include <bsd/dev/m68k/npio.h>

#define PRINTER_DEV "/dev/np0"
#define BUF_SZ 4096

#ifndef ENOINIT
#define ENOINIT 66
#endif

static int verbose = 0;

/* ------------------ System call prototypes for K&R ------------------ */
extern int open();
extern int close();
extern int read();
extern int write();
extern int ioctl();
extern int unlink();
extern unsigned int sleep();
extern int geteuid();

/* For mkstemp, provide a fallback for old systems */
#ifndef HAVE_MKSTEMP
extern int mkstemp();
#endif


/* ---------------------- Usage -------------------------- */
static void usage(p)
    char *p;
{
    fprintf(stderr,
        "Usage:\n"
        "  %s [-v] on\n"
        "  %s [-v] off\n"
        "  %s [-v] reset\n"
        "  %s [-v] status\n"
        "  %s [-v] setdpi <300|400>\n"
        "  %s [-v] setmargins <left> <top> <width> <height>\n"
        "  %s [-v] print <file.ps>\n"
        "  %s [-v] demo\n",
        p, p, p, p, p, p, p, p);
}

/* ----------------- Error explain ----------------------- */
static void explain_errno(msg)
    const char *msg;
{
    switch (errno) {
    case ENOINIT: fprintf(stderr, "%s: Margins not set before write\n", msg); break;
    case EDEVERR: fprintf(stderr, "%s: Printer is in error state\n", msg); break;
    case ENXIO:   fprintf(stderr, "%s: Unknown command or device\n", msg); break;
    case EIO:     fprintf(stderr, "%s: I/O error talking to printer\n", msg); break;
    case EBUSY:   fprintf(stderr, "%s: Printer already in use\n", msg); break;
    default:      fprintf(stderr, "%s: %s\n", msg, strerror(errno)); break;
    }
}

/* ------------------ Open printer ---------------------- */
static int open_printer(flags)
    int flags;
{
    int fd;
    fd = open(PRINTER_DEV, flags, 0);
    if (fd < 0) {
        explain_errno("open");
    } else if (verbose) {
        printf("[VERBOSE] Opened printer device %s\n", PRINTER_DEV);
    }
    return fd;
}

/* ------------------ Power printer -------------------- */
static int set_power(on)
    int on;
{
    int fd, ret;
    struct npop op;

    fd = open_printer(O_RDWR);
    if (fd < 0) return -1;

    memset(&op, 0, sizeof(op));
    op.np_op = NPSETPOWER;
    op.np_power = on ? 1 : 0;

    if (verbose)
        printf("[VERBOSE] Power %s printer...\n", on ? "on" : "off");

    ret = ioctl(fd, NPIOCPOP, &op);
    if (ret == -1) {
        explain_errno("ioctl(NPSETPOWER)");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/* --------------- Reset printer ----------------------- */
static int reset_printer()
{
    if (set_power(0) < 0) return -1;
    sleep(1);
    return set_power(1);
}

/* --------------- Set resolution ---------------------- */
static int set_resolution(dpi)
    int dpi;
{
    int fd;
    struct npop op;

    fd = open_printer(O_RDWR);
    if (fd < 0) return -1;

    memset(&op, 0, sizeof(op));
    op.np_op = NPSETRESOLUTION;
    op.np_resolution = (dpi == 400) ? DPI400 : DPI300;

    if (verbose)
        printf("[VERBOSE] Setting resolution to %d dpi\n", dpi);

    if (ioctl(fd, NPIOCPOP, &op) == -1) {
        explain_errno("ioctl(NPSETRESOLUTION)");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/* ---------------- Set margins ------------------------ */
static int set_margins(left, top, width, height)
    int left, top, width, height;
{
    int fd;
    struct npop op;

    fd = open_printer(O_RDWR);
    if (fd < 0) return -1;

    memset(&op, 0, sizeof(op));
    op.np_op = NPSETMARGINS;
    op.np_margins.left = left;
    op.np_margins.top = top;
    op.np_margins.width = width;
    op.np_margins.height = height;

    if (verbose)
        printf("[VERBOSE] Setting margins l=%d t=%d w=%d h=%d\n", left, top, width, height);

    if (ioctl(fd, NPIOCPOP, &op) == -1) {
        explain_errno("ioctl(NPSETMARGINS)");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/* ---------------- Query status ----------------------- */
static void query_status(fd, verbose_flag)
    int fd;
    int verbose_flag;
{
    struct npop op;
    int code;
    const char *name;
    static const char *sizes[] = { "No cassette", "A4", "Letter", "B5", "Legal" };

    memset(&op, 0, sizeof(op));
    op.np_op = NPGETSTATUS;
    if (ioctl(fd, NPIOCPOP, &op) < 0) {
        explain_errno("ioctl(NPGETSTATUS)");
        return;
    }

    printf("Printer status:\n");
    if (op.np_status.flags == 0)
        printf("  - Ready (no errors)\n");
    if (op.np_status.flags & NPPAPERDELIVERY)
        printf("  - Printing (paper in path)\n");
    if (op.np_status.flags & NPDATARETRANS)
        printf("  - Data retransmit requested (pages=%u)\n", op.np_status.retrans);
    if (op.np_status.flags & NPCOLD)
        printf("  - Warming up (fixing assembly not hot)\n");
    if (op.np_status.flags & NPNOCARTRIDGE)
        printf("  - No cartridge\n");
    if (op.np_status.flags & NPNOPAPER)
        printf("  - No paper\n");
    if (op.np_status.flags & NPPAPERJAM)
        printf("  - Paper jam\n");
    if (op.np_status.flags & NPDOOROPEN)
        printf("  - Door open\n");
    if (op.np_status.flags & NPNOTONER)
        printf("  - Toner low\n");
    if (op.np_status.flags & NPHARDWAREBAD)
        printf("  - Hardware failure\n");
    if (op.np_status.flags & NPMANUALFEED)
        printf("  - Manual feed selected\n");
    if (op.np_status.flags & NPFUSERBAD)
        printf("  - Fixing assembly malfunction\n");
    if (op.np_status.flags & NPLASERBAD)
        printf("  - Laser/beam detect problem\n");
    if (op.np_status.flags & NPMOTORBAD)
        printf("  - Scanning motor malfunction\n");

    if (verbose_flag)
        printf("  [Raw flags=0x%08x]\n", op.np_status.flags);

    memset(&op, 0, sizeof(op));
    op.np_op = NPGETPAPERSIZE;
    if (ioctl(fd, NPIOCPOP, &op) < 0) {
        explain_errno("ioctl(NPGETPAPERSIZE)");
    } else {
        code = op.np_size;
        if (code >= 0 && code <= 4)
            name = sizes[code];
        else
            name = "Unknown";
        printf("Paper size: %s (code %d)\n", name, code);
    }
}

/* ---------------- Print file via lpr ----------------- */
static int print_file(path)
    char *path;
{
    char cmd[512];
    int ret;

    if (verbose)
        printf("[VERBOSE] Sending %s to lpr...\n", path);

    sprintf(cmd, "lpr %s", path);
    ret = system(cmd);

    if (ret != 0) {
        fprintf(stderr, "Failed to print %s via lpr (exit code %d)\n", path, ret);
        return -1;
    }

    if (verbose)
        printf("[VERBOSE] lpr finished successfully.\n");

    return 0;
}

/* ---------------- Demo page -------------------------- */
static int demo_page()
{
    char tmp[] = "/tmp/npdemoXXXXXX";
    int fd = mkstemp(tmp);
    FILE *f;

    if (fd < 0) {
        perror("mkstemp");
        return -1;
    }
    f = fdopen(fd, "w");
    if (!f) {
        perror("fdopen");
        close(fd);
        return -1;
    }

    fprintf(f,
        "%%!PS-Adobe-2.0\n"
        "%%%%Title: npctl demo\n"
        "/Courier findfont 18 scalefont setfont\n"
        "72 720 moveto (NeXT Laser Printer Demo Page) show\n"
        "72 700 moveto (Generated by npctl) show\n"
        "newpath 100 500 moveto 300 500 lineto 300 700 lineto 100 700 lineto closepath stroke\n"
        "showpage\n");
    fclose(f);

    if (verbose)
        printf("[VERBOSE] Printing demo page...\n");

    print_file(tmp);
    unlink(tmp);
    return 0;
}

/* -------------------- MAIN --------------------------- */
int main(argc, argv)
    int argc;
    char **argv;
{
    char *cmd; 
    int dpi, left, top, width, height; 
    int argi = 1;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-v") == 0) {
        verbose = 1;
        argi++;
        if (argc < 3) { usage(argv[0]); return 1; }
    }

    cmd = argv[argi];

    if (geteuid() != 0)
        fprintf(stderr, "Warning: you are not root - some ioctls may fail.\n");

    if (strcmp(cmd, "on") == 0) return set_power(1) ? 2 : 0;
    else if (strcmp(cmd, "off") == 0) return set_power(0) ? 3 : 0;
    else if (strcmp(cmd, "reset") == 0) return reset_printer() ? 8 : 0;
    else if (strcmp(cmd, "status") == 0) {
        int fd = open_printer(O_RDWR);
        if (fd < 0) return 4;
        query_status(fd, verbose);
        close(fd);
        return 0;
    }
    else if (strcmp(cmd, "setdpi") == 0) {
        if (argc <= argi + 1) { usage(argv[0]); return 1; }
        dpi = atoi(argv[argi + 1]);
        if (dpi != 300 && dpi != 400) {
            fprintf(stderr, "Only 300 and 400 DPI supported.\n");
            return 1;
        }
        return set_resolution(dpi) ? 5 : 0;
    }
    else if (strcmp(cmd, "setmargins") == 0) {
        if (argc <= argi + 4) { usage(argv[0]); return 1; }
        left = atoi(argv[argi + 1]);
        top = atoi(argv[argi + 2]);
        width = atoi(argv[argi + 3]);
        height = atoi(argv[argi + 4]);
        return set_margins(left, top, width, height) ? 6 : 0;
    }
    else if (strcmp(cmd, "print") == 0) {
        if (argc <= argi + 1) { usage(argv[0]); return 1; }
        return print_file(argv[argi + 1]) ? 7 : 0;
    }
    else if (strcmp(cmd, "demo") == 0) {
        return demo_page() ? 9 : 0;
    }
    else {
        usage(argv[0]);
        return 1;
    }
}
