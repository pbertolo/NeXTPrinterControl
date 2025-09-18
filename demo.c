/*
 * demo.c - Handles the demo page printing for npctl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "demo.h"

#define DEMO_LOCAL "./demo.ps"
#define DEMO_SYSTEM "/usr/local/share/npctl/demo.ps"

int demo_page()
{
    FILE *fp;
    FILE *lp;
    char buf[1024];
    char cmd[512];
    int ret;

    /* Try local demo.ps first */
    fp = fopen(DEMO_LOCAL, "r");
    if (!fp) {
        /* Fallback to system-wide installed demo.ps */
        fp = fopen(DEMO_SYSTEM, "r");
        if (!fp) {
            fprintf(stderr, "Error: Could not open %s or %s\n",
                    DEMO_LOCAL, DEMO_SYSTEM);
            return 1;
        }
    }

    if (verbose)
        printf("[VERBOSE] Sending demo.ps to lpr...\n");

    /* Build lpr command */
    sprintf(cmd, "lpr");

    /* Open a pipe to lpr */
    lp = popen(cmd, "w");
    if (!lp) {
        perror("popen(lpr)");
        fclose(fp);
        return 1;
    }

    /* Copy demo.ps to lpr */
    while ((ret = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (fwrite(buf, 1, ret, lp) != ret) {
            perror("fwrite to lpr failed");
            fclose(fp);
            pclose(lp);
            return 1;
        }
    }

    fclose(fp);
    ret = pclose(lp);

    if (ret != 0) {
        fprintf(stderr, "lpr failed with exit code %d\n", ret);
        return 1;
    }

    if (verbose)
        printf("[VERBOSE] Demo page sent successfully.\n");

    return 0;
}
