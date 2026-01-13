//----------------------------------------------------------------------
//
//  DiskImageUtils.c
//
//  Written by: Ken McLeod
//
//  Modification History:
//  Thu Jul 03 2025 (kcm) -- initial version
//
//----------------------------------------------------------------------

// CoreFoundation is only used for the date formatting code in
// DateStringForHFSDate. Will likely rewrite that function at some
// point to remove this dependency.
#include <CoreFoundation/CoreFoundation.h>
#include "DiskImageUtils.h"

void tabprint(int tabstop, char *format, ...) {
    FILE *stream = stdout;
    int i = 0, tabsize = 4;
    for (; i<tabstop*tabsize; i++) { fprintf(stream, " "); }
    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    fflush(stream);
}

#define kPBStr "##################################################"
#define kPBWidth 50

int progress(double percentComplete) {
    int val = (int) (percentComplete * 100);
    int lpad = (int) (percentComplete * kPBWidth);
    int rpad = kPBWidth - lpad;
    fprintf(stdout, "\r%3d%% [%.*s%*s]", val, lpad, kPBStr, rpad, "");
    fflush(stdout);
    return val;
}

static inline ushort rotl16(ushort n, unsigned int c) {
    const unsigned int mask = (CHAR_BIT*sizeof(n) - 1);
    c &= mask;
    return (n<<c) | (n>>( (-c)&mask ));
}

static inline ushort rotr16(ushort n, unsigned int c) {
    const unsigned int mask = (CHAR_BIT*sizeof(n) - 1);
    c &= mask;
    return (n>>c) | (n<<( (-c)&mask ));
}

ushort Checksum16(uchar *bytes, size_t length) {
    // C translation of routine used by boot code to verify driver
    uchar *p = bytes;
    ushort cksum = 0;
    int i;
    for (i=0 ; i<length; i++) {
        cksum += *p++;
        cksum = rotl16(cksum, 1);
    }
    if (cksum == 0) { cksum = 0xFFFF; }
    return cksum;
}

ushort ComputeChecksum(int fd, off_t driverOffset, off_t length) {
    ushort result = 0xFFFF;
    off_t count;
    void *p = 0;
    if ((count = lseek(fd, driverOffset, SEEK_SET)) == -1) { return 1; }
    p = malloc(length);
    if (!p) { goto done; }
    if ((count = read(fd, p, length)) < 0) { goto done; }
    result = Checksum16(p, length);
done:
    free(p);
    return result;
}

static double hfsEpoch = -3061152000.0; // 1904-01-01T00:00:00Z

void DateStringForHFSDate(uint32_t hfsDate, uint32_t maxLen, char *str) {
    CFAbsoluteTime cfTime = hfsDate + hfsEpoch;
    CFDateRef cfDate = CFDateCreate(kCFAllocatorDefault, cfTime);
    CFDateFormatterRef dateFormatter = CFDateFormatterCreate(kCFAllocatorDefault,
    CFLocaleCopyCurrent(), kCFDateFormatterLongStyle, kCFDateFormatterLongStyle);
    CFStringRef cfString = CFDateFormatterCreateStringWithDate(kCFAllocatorDefault, dateFormatter, cfDate);
    CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfString), kCFStringEncodingUTF8) + 1;
    if (str && maxLen >= length) {
        Boolean converted = CFStringGetCString(cfString, str, length, kCFStringEncodingUTF8);
        if (!converted) {
            char *errString = "ERROR";
            strncpy(str, errString, strlen(errString));
        }
    }
    CFRelease(cfString);
    CFRelease(dateFormatter);
    CFRelease(cfDate);
}

int ReadUShort(int fd, size_t offset, ushort *value) {
    off_t count;
    if ((count = lseek(fd, offset, SEEK_SET)) == -1) { return 1; }
    if ((count = read(fd, value, sizeof(ushort))) < 0) { return 1; }
    *value = (ushort) ntohs(*value);
    return 0;
}

int ReadULong(int fd, size_t offset, ulong *value) {
    off_t count;
    if ((count = lseek(fd, offset, SEEK_SET)) == -1) { return 1; }
    if ((count = read(fd, value, sizeof(ulong))) < 0) { return 1; }
    *value = (ulong) ntohl(*value);
    return 0;
}

int ReadDriverDescriptorRecord(int fd, size_t offset, DDRecord *ddr) {
    off_t count;
    if ((count = lseek(fd, offset, SEEK_SET)) == -1) { return 1; }
    if ((count = read(fd, ddr, sizeof(DDRecord))) < 0) { return 1; }
    // convert numeric values to host endian
    ddr->sbSig = (ushort) ntohs(ddr->sbSig);
    ddr->sbBlkSize = (ushort) ntohs(ddr->sbBlkSize);
    ddr->sbBlkCount = (ulong) ntohl(ddr->sbBlkCount);
    ddr->sbDevType = (ushort) ntohs(ddr->sbDevType);
    ddr->sbDevId = (ushort) ntohs(ddr->sbDevId);
    ddr->sbData = (ulong) ntohl(ddr->sbData);
    ddr->sbDrvrCount = (ushort) ntohs(ddr->sbDrvrCount);
    ddr->ddBlock = (ulong) ntohl(ddr->ddBlock);
    ddr->ddSize = (ushort) ntohs(ddr->ddSize);
    ddr->ddType = (ushort) ntohs(ddr->ddType);
    return 0;
}

int ReadPartitionMapEntry(int fd, size_t offset, Partition *pme) {
    off_t count;
    if ((count = lseek(fd, offset, SEEK_SET)) == -1) { return 1; }
    if ((count = read(fd, pme, sizeof(Partition))) < 0) { return 1; }
    // convert numeric values to host endian
    pme->pmSig = (ushort) ntohs(pme->pmSig);
    pme->pmSigPad = (ushort) ntohs(pme->pmSigPad);
    pme->pmMapBlkCnt = (ulong) ntohl(pme->pmMapBlkCnt);
    pme->pmPyPartStart = (ulong) ntohl(pme->pmPyPartStart);
    pme->pmPartBlkCnt = (ulong) ntohl(pme->pmPartBlkCnt);
    pme->pmLgDataStart = (ulong) ntohl(pme->pmLgDataStart);
    pme->pmDataCnt = (ulong) ntohl(pme->pmDataCnt);
    pme->pmPartStatus = (ulong) ntohl(pme->pmPartStatus);
    pme->pmLgBootStart = (ulong) ntohl(pme->pmLgBootStart);
    pme->pmBootSize = (ulong) ntohl(pme->pmBootSize);
    pme->pmBootAddr = (ulong) ntohl(pme->pmBootAddr);
    pme->pmBootAddr2 = (ulong) ntohl(pme->pmBootAddr2);
    pme->pmBootEntry = (ulong) ntohl(pme->pmBootEntry);
    pme->pmBootEntry2 = (ulong) ntohl(pme->pmBootEntry2);
    pme->pmBootCksum = (ulong) ntohl(pme->pmBootCksum);
    return 0;
}

int ReadBootBlockHeader(int fd, size_t offset, BootBlockHeader *bb) {
    off_t count;
    if ((count = lseek(fd, offset, SEEK_SET)) == -1) { return 1; }
    if ((count = read(fd, bb, sizeof(BootBlockHeader))) < 0) { return 1; }
    // convert numeric values to host endian
    bb->bbID = (ushort) ntohs(bb->bbID);
    bb->bbEntry = (ulong) ntohl(bb->bbEntry);
    bb->bbVersion = (ushort) ntohs(bb->bbVersion);
    bb->bbPageFlags = (ushort) ntohs(bb->bbPageFlags);
    bb->bbCntFCBs = (ushort) ntohs(bb->bbCntFCBs);
    bb->bbCntEvts = (ushort) ntohs(bb->bbCntEvts);
    bb->bb128KSHeap = (ulong) ntohl(bb->bb128KSHeap);
    bb->bb256KSHeap = (ulong) ntohl(bb->bb256KSHeap);
    bb->bbSysHeapSize = (ulong) ntohl(bb->bbSysHeapSize);
    bb->filler = (ushort) ntohs(bb->filler);
    bb->bbSysHeapExtra = (ulong) ntohl(bb->bbSysHeapExtra);
    bb->bbSysHeapFract = (ulong) ntohl(bb->bbSysHeapFract);
    return 0;
}

int ReadMasterDirectoryBlock(int fd, size_t offset, MasterDirectoryBlock *mdb) {
    int i;
    off_t count;
    if ((count = lseek(fd, offset, SEEK_SET)) == -1) { return 1; }
    if ((count = read(fd, mdb, sizeof(MasterDirectoryBlock))) < 0) { return 1; }
    // convert numeric values to host endian
    mdb->drSigWord = (ushort) ntohs(mdb->drSigWord);
    mdb->drCrDate = (ulong) ntohl(mdb->drCrDate);
    mdb->drLsMod = (ulong) ntohl(mdb->drLsMod);
    mdb->drAtrb = (ushort) ntohs(mdb->drAtrb);
    mdb->drNmFls = (ushort) ntohs(mdb->drNmFls);
    mdb->drVBMSt = (ushort) ntohs(mdb->drVBMSt);
    mdb->drAllocPtr = (ushort) ntohs(mdb->drAllocPtr);
    mdb->drNmAlBlks = (ushort) ntohs(mdb->drNmAlBlks);
    mdb->drAlBlkSiz = (ulong) ntohl(mdb->drAlBlkSiz);
    mdb->drClpSiz = (ulong) ntohl(mdb->drClpSiz);
    mdb->drAlBlSt = (ushort) ntohs(mdb->drAlBlSt);
    mdb->drNxtCNID = (ulong) ntohl(mdb->drNxtCNID);
    mdb->drFreeBks = (ushort) ntohs(mdb->drFreeBks);
    mdb->drVolBkUp = (ulong) ntohl(mdb->drVolBkUp);
    mdb->drVSeqNum = (ushort) ntohs(mdb->drVSeqNum);
    mdb->drWrCnt = (ulong) ntohl(mdb->drWrCnt);
    mdb->drXTClpSiz = (ushort) ntohs(mdb->drXTClpSiz);
    mdb->drCTClpSiz = (ulong) ntohl(mdb->drCTClpSiz);
    mdb->drNmRtDirs = (ushort) ntohs(mdb->drNmRtDirs);
    mdb->drFilCnt = (ulong) ntohl(mdb->drFilCnt);
    mdb->drDirCnt = (ulong) ntohl(mdb->drDirCnt);
    for (i=0; i<8; i++) {
        mdb->drFndrInfo[i] = (ulong) ntohl(mdb->drFndrInfo[i]);
    }
    mdb->drVCSize = (ushort) ntohs(mdb->drVCSize);
    mdb->drVBMCSize = (ulong) ntohl(mdb->drVBMCSize);
    mdb->drXTFlSize = (ulong) ntohl(mdb->drXTFlSize);
    mdb->drCTFlSize = (ulong) ntohl(mdb->drCTFlSize);
    return 0;
}

int ReadHFSPlusVolumeHeader(int fd, size_t offset, HFSPlusVolumeHeader *vh) {
    int i;
    off_t count;
    if ((count = lseek(fd, offset, SEEK_SET)) == -1) { return 1; }
    if ((count = read(fd, vh, sizeof(HFSPlusVolumeHeader))) < 0) { return 1; }
    // convert numeric values to host endian
    vh->signature = (ushort) ntohs(vh->signature);
    vh->version = (ushort) ntohs(vh->version);
    vh->attributes = (ulong) ntohl(vh->attributes);
    vh->lastMountedVersion = (ulong) ntohl(vh->lastMountedVersion);
    vh->journalInfoBlock = (ulong) ntohl(vh->journalInfoBlock);
    vh->createDate = (ulong) ntohl(vh->createDate);
    vh->modifyDate = (ulong) ntohl(vh->modifyDate);
    vh->backupDate = (ulong) ntohl(vh->backupDate);
    vh->checkedDate = (ulong) ntohl(vh->checkedDate);
    vh->fileCount = (ulong) ntohl(vh->fileCount);
    vh->dirCount = (ulong) ntohl(vh->dirCount);
    vh->blockSize = (ulong) ntohl(vh->blockSize);
    vh->totalBlocks = (ulong) ntohl(vh->totalBlocks);
    vh->freeBlocks = (ulong) ntohl(vh->freeBlocks);
    vh->nextAllocation = (ulong) ntohl(vh->nextAllocation);
    vh->resClumpSize = (ulong) ntohl(vh->resClumpSize);
    vh->dataClumpSize = (ulong) ntohl(vh->dataClumpSize);
    vh->nextCatalogID = (ulong) ntohl(vh->nextCatalogID);
    vh->writeCount = (ulong) ntohl(vh->writeCount);
    vh->writeCount = (ulong) ntohl(vh->writeCount);
    // vh->encodingsBitmap: do we care yet?
    for (i=0; i<8; i++) {
        vh->finderInfo[i] = (ulong) ntohl(vh->finderInfo[i]);
    }
    // allocationFile, extentsFile, etc.: do we care yet?
    return 0;
}

