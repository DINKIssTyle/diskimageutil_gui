//----------------------------------------------------------------------
//
//  DiskImageDescribe.c
//
//  Written by: Ken McLeod
//
//  Modification History:
//  Thu Jul 03 2025 (kcm) -- initial version
//
//----------------------------------------------------------------------

#include "DiskImageUtils.h"
#include "DiskImageDescribe.h"

extern int verbose;
const char *kVerifiedStr = "✔ VERIFIED";
const char *kFailedStr = "✖ VERIFY FAILED";
const char *kTruncedStr = "✖ TRUNCATED";

void DescribeHFSPlusVolume(int fd, size_t offset, int tab) {
    HFSPlusVolumeHeader vh;
    int len;
    char name[32];
    char date[255];
    memset(name, 0, sizeof(name));
    memset(date, 0, sizeof(date));

    if (ReadHFSPlusVolumeHeader(fd, offset, &vh) != 0) {
        tabprint(tab, "Error reading HFS+ volume header\n");
        return;
    }
    DateStringForHFSDate(vh.createDate, sizeof(date)-1, date);
    tabprint(tab, "Created: %s\n", date);
    DateStringForHFSDate(vh.modifyDate, sizeof(date)-1, date);
    tabprint(tab, "Last modified: %s\n", date);

    tabprint(tab, "Capacity: %.1f MB (%ld bytes)\n",
        (vh.blockSize*vh.totalBlocks) / (1024.0*1024.0),
        (vh.blockSize*vh.totalBlocks));
    tabprint(tab, "Used: %.1f MB (%ld bytes)\n",
        (vh.blockSize*(vh.totalBlocks-vh.freeBlocks)) / (1024.0*1024.0),
        (vh.blockSize*(vh.totalBlocks-vh.freeBlocks)));
    tabprint(tab, "Free: %.1f MB (%ld bytes)\n",
        (vh.blockSize*vh.freeBlocks) / (1024.0*1024.0),
        (vh.blockSize*vh.freeBlocks));
}

void DescribeHFSVolume(int fd, size_t offset, int tab) {
    BootBlockHeader bb;
    MasterDirectoryBlock mdb;
    off_t mdbOffset = offset + (512*2);
    int len;
    char name[32];
    char date[255];
    memset(name, 0, sizeof(name));
    memset(date, 0, sizeof(date));

    if (ReadBootBlockHeader(fd, offset, &bb) != 0) {
        tabprint(tab, "Error reading HFS boot blocks\n");
        return;
    }
    if (verbose) {
        ushort sig = htons(bb.bbID);
        memcpy(name, (char*)&sig, 2);
        name[2] = 0;
        tabprint(tab, "Boot block signature: 0x%04X", bb.bbID);
        if (bb.bbID == 0) {
            tabprint(0, " (non-bootable volume)\n");
        } else if (bb.bbID == 0x4C4B) { // 'LK'
            tabprint(0, " '%s' (bootable volume)\n", name);
        } else {
            tabprint(0, " '%s' (expected 0x4C4B)\n", name);
        }
        tabprint(tab, "Boot block version: 0x%04X\n",bb.bbVersion);
    }
    if (ReadMasterDirectoryBlock(fd, mdbOffset, &mdb) != 0) {
        tabprint(tab, "Error reading volume information block\n");
        return;
    }
    if (verbose) {
        ushort sig = htons(mdb.drSigWord);
        memcpy(name, (char*)&sig, 2);
        name[2] = 0;
        tabprint(tab, "Volume signature: 0x%04X '%s' ", mdb.drSigWord, name);
        if (mdb.drSigWord == 0x4244) { // 'BD' for HFS
            tabprint(0, "(HFS volume)\n");
        } else if (mdb.drSigWord == 0x482B) { // 'H+' for HFS+
            tabprint(0, "(HFS+ volume)\n");
        } else {
            tabprint(0, "(unrecognized format)\n");
        }
    }
    if (mdb.drSigWord == 0x4244) { // 'BD' for HFS
        memcpy(name, (char*)mdb.drVN+1, 27);
        if ((len = mdb.drVN[0]) > 27) { len = 28; }
        name[len] = 0;
        tabprint(tab, "Volume: %s\n", name);

        DateStringForHFSDate(mdb.drCrDate, sizeof(date)-1, date);
        tabprint(tab, "Created: %s\n", date);
        DateStringForHFSDate(mdb.drLsMod, sizeof(date)-1, date);
        tabprint(tab, "Last modified: %s\n", date);

        tabprint(tab, "Capacity: %.1f MB (%ld bytes)\n",
            (mdb.drAlBlkSiz*mdb.drNmAlBlks) / (1024.0*1024.0),
            (mdb.drAlBlkSiz*mdb.drNmAlBlks));
        tabprint(tab, "Used: %.1f MB (%ld bytes)\n",
            (mdb.drAlBlkSiz*(mdb.drNmAlBlks-mdb.drFreeBks)) / (1024.0*1024.0),
            (mdb.drAlBlkSiz*(mdb.drNmAlBlks-mdb.drFreeBks)));
        tabprint(tab, "Free: %.1f MB (%ld bytes)\n",
            (mdb.drAlBlkSiz*mdb.drFreeBks) / (1024.0*1024.0),
            (mdb.drAlBlkSiz*mdb.drFreeBks));
    } else if (mdb.drSigWord == 0x482B) { // 'H+' for HFS+
        DescribeHFSPlusVolume(fd, mdbOffset, tab);
    }
}

void DescribePartitionMap(int fd, size_t fileSize, int tab) {
    Partition pme;
    ulong cksum;
    char pname[34], ptype[34];
    const off_t kBlockSize = 512;
    off_t pmeOffset = kBlockSize; // partition map starts at block 1
    off_t partOffset; // in bytes
    size_t partLength; // in bytes
    int i = 0;
    memset(pname, 0, sizeof(pname));
    memset(ptype, 0, sizeof(ptype));

    while (pmeOffset) {
        if (ReadPartitionMapEntry(fd, pmeOffset, &pme) != 0) { break; }
        if (pme.pmSig != 0x504D) { break; } // 'PM'
        partOffset = pme.pmPyPartStart * kBlockSize;
        partLength = pme.pmPartBlkCnt * kBlockSize;
        memcpy(pname, (char*)pme.pmPartName, 32);
        memcpy(ptype, (char*)pme.pmPartType, 32);
        tabprint(tab, "\n");
        tabprint(tab, "Partition %d: %s (%s)\n", i, pname, ptype);
        tabprint(tab+1, "Size: %ld bytes (offset %ld to %ld)",
                partLength, partOffset, partOffset + partLength);
        if ((partOffset + partLength) > fileSize) {
            tabprint(0, ANSI_RED " %s" ANSI_RESET, kTruncedStr);
        }
        tabprint(0, "\n");
        if (!strncmp(ptype, "Apple_Driver", 12)) {
            off_t drvOffset = pme.pmPyPartStart * kBlockSize;
            off_t drvLength = pme.pmBootSize;
            // compute 16-bit checksum used by Apple_Driver* drivers
            // (note that the stored value is actually 32 bits)
            ulong cksum = ComputeChecksum(fd, drvOffset, drvLength);
            tabprint(tab+1, "Code: %ld bytes (offset %ld in file)\n",
                    drvLength, drvOffset);
            tabprint(tab+1, "Checksum: 0x%08X", pme.pmBootCksum);
            if (!pme.pmBootCksum) {
                // boot code only enforces check if name prefix is 'Maci'
                if (!strncmp(pname, "Maci", 4)) {
                    tabprint(0, " (driver will not load)");
                }
            } else { // there is a saved checksum to verify
                tabprint(0, " (computed 0x%08X) ", cksum);
                if (cksum == pme.pmBootCksum) {
                    tabprint(0, ANSI_GREEN "%s" ANSI_RESET, kVerifiedStr);
                } else {
                    tabprint(0, ANSI_RED "%s" ANSI_RESET, kFailedStr);
                }
            }
            tabprint(0, "\n");
        }
        if (!strncmp(ptype, "Apple_HFS", strlen(ptype))) {
            DescribeHFSVolume(fd, pme.pmPyPartStart * kBlockSize, tab+1);
        }
        pmeOffset += kBlockSize; // next partition map entry
        i++;
    }
}

void DescribeFile(const char *inPathname) {
    DDRecord ddr;
    ushort hfsSig = 0;
    struct stat sb = {0};
    int tab = 1;
    int fd;
    char *name = basename((char*)inPathname);
    tabprint(0, "Checking file \"%s\"\n",
            (name) ? name : inPathname);

    if ((fd = open(inPathname, O_RDONLY, 0)) == -1) { goto done; }
    if (fstat(fd, &sb) < 0) { goto done; }
    tabprint(0, "File size: %ld bytes\n", sb.st_size);
    if (ReadDriverDescriptorRecord(fd, 0, &ddr) != 0) { goto done; }

    // secondary check for HFS or HFS+ sig if this is a volume image
    if (!((ReadUShort(fd, 0x400, &hfsSig) == 0) &&
        (hfsSig == 0x4244 || hfsSig == 0x482B))) { // HFS or HFS+
        hfsSig = 0;
    }
    if (ddr.sbSig == 0x4552 && verbose) { // 'ER'
        size_t length = ddr.sbBlkSize * ddr.sbBlkCount;
        ushort sig = htons(ddr.sbSig);
        memcpy(name, (char*)&sig, 2);
        name[2] = 0;
        if (length > 0) {
            tabprint(0, "Device size: %ld bytes", length);
            if (length > sb.st_size) {
                tabprint(0, ANSI_RED " %s" ANSI_RESET, kTruncedStr);
            }
            tabprint(0, "\n");
        } else {
            tabprint(0, "Device size: (not specified)\n");
        }
        tabprint(0, "Device signature: 0x%04X '%s'\n", ddr.sbSig, name);
    }
    if (ddr.sbSig == 0x4552) { // 'ER'
        tabprint(0, "File format: Apple Partition Map disk image\n");
        DescribePartitionMap(fd, sb.st_size, tab);
    } else if (ddr.sbSig == 0x4C4B) { // 'LK'
        tabprint(0, "File format: Apple HFS volume image (bootable)\n");
        DescribeHFSVolume(fd, 0, tab);
    } else if (ddr.sbSig == 0x0000 && hfsSig != 0) {
        tabprint(0, "File format: Apple HFS volume image (not bootable)\n");
        DescribeHFSVolume(fd, 0, tab);
    } else {
        tabprint(0, "File is not a recognized disk image format.\n");
        tabprint(0, "Currently this utility only recognizes raw HFS or Apple Partition Map format.\n");
    }
done:
    if (fd != -1) {
        close(fd);
    }
}

