//----------------------------------------------------------------------
//
//  diskimageutil.c
//
//  Written by: Ken McLeod
//
//  Modification History:
//  Thu Jul 03 2025 (kcm) -- initial version
//
//----------------------------------------------------------------------

#include "DiskImageConvert.h"
#include "DiskImageDescribe.h"
#include "DiskImageUtils.h"

const char *kVersionStr = "Version 1.0, 09 Jul 2025";
int verbose = 0;

static void usage(const char *arg0) {
    fprintf(stderr, "%s\n\n", kVersionStr);
    fprintf(stderr, "Usage: %s [-v] [-w] <verb> <file> [dstfile]\n", arg0);
    fprintf(stderr, "<verb> is one of the following options:\n");
    fprintf(stderr, "  info      Prints type, size, and other info about <file>.\n");
    fprintf(stderr, "            Use \"-v info\" to see more verbose detail.\n");
    fprintf(stderr, "  cvt2hfs   Converts input file to an HFS volume image.\n");

    fprintf(stderr, "            If dstfile not specified, will create <file>.dsk.\n");
    fprintf(stderr, "  cvt2iso   Converts input file to an ISO device image.\n");
    fprintf(stderr, "            If dstfile not specified, will create <file>.iso.\n");
    fprintf(stderr, "            Use \"-w cvt2iso\" for a writable image (default is read-only)\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  # Print info about contents of a disk image\n");
    fprintf(stderr, "    %s info \"System 7.5.3.dmg\"\n", arg0);
    fprintf(stderr, "  # Convert a disk image to a raw HFS volume\n");
    fprintf(stderr, "    %s cvt2hfs \"System 7.5.3.iso\" System753.dsk\n", arg0);
    fprintf(stderr, "  # Convert a disk image to an ISO device image\n");
    fprintf(stderr, "    %s cvt2iso MinivMac.dsk\n", arg0);
    fprintf(stderr, "\nNotes:\n");
    fprintf(stderr, "  Always keep a copy of your original source disk image, even if conversion is successful.\n\n");
    fprintf(stderr, "  Use cvt2hfs to create a disk image for emulator software that expects a raw HFS volume, such as Mini vMac. Use cvt2iso for a device image that can be used with pre-10.15 versions of macOS/OS X, as well as in Basilisk, SheepShaver, Snow, QEMU, and other emulators.\n\n");
    fprintf(stderr, "  Conversion to ISO format (even if the source image is already ISO) can repair readability problems with some device images, such as those made from old CD-ROMs. However, this process is lossy: it currently copies only the Apple_HFS partition, ignoring others. The intent is to make a working copy that can be used in an emulator, and is not a solution for archiving source media. ALWAYS keep your original disk image to avoid losing data!\n");
    fprintf(stderr, "\nLimitations:\n");
    fprintf(stderr, "  This program does not yet recognize many disk image formats, such as Disk Copy or DART images, or the GUID device partition scheme. It also does not yet correctly handle multiple HFS partitions in a device image. This software may contain bugs. Use at your own risk.\n");
    fflush(stderr);
}

int main (int argc, char **argv)
{
    int idx, minArgs=3, rw=0;
    char *path;

    /* need at least 3 arguments: app, verb, file */
    if (argc < minArgs) { goto usage_error_exit; }

    for (idx = 1; idx < argc; idx++) {
        if (!strcmp(argv[idx], "-v")) {
            ++verbose;
            /* re-check arg count to make sure we have enough */
            if (argc < ++minArgs) { goto usage_error_exit; }
        } else if (!strcmp(argv[idx], "-w")) {
            ++rw;
            /* re-check arg count to make sure we have enough */
            if (argc < ++minArgs) { goto usage_error_exit; }
        } else if (!strcmp(argv[idx], "info")) {
            DescribeFile(argv[++idx]);
        } else if (!strcmp(argv[idx], "cvt2hfs") ||
                   !strcmp(argv[idx], "cvt2iso")) {
            int iso = (!strcmp(argv[idx], "cvt2iso")) ? 1 : 0;
            int pathLen = strlen(argv[++idx]);
            char *buf = (char*)malloc(pathLen+6);
            buf[0]='\0';
            strncpy(buf, argv[idx], pathLen);
            strncpy(buf+pathLen, (iso) ? ".iso" : ".dsk", 4);
            ConvertFile(iso, argv[idx],(idx+1 < argc) ? argv[idx+1] : buf, rw);
            free(buf);
            ++idx;
        } else {
            fprintf(stderr, "\nInvalid parameter: %s\n\n", argv[idx]);
            goto usage_error_exit;
        }
    }
    return 0;
usage_error_exit:
    usage(argv[0]);
    return 1;
}
