//----------------------------------------------------------------------
//
//  DiskImageConvert.c
//
//  Written by: Ken McLeod
//
//  Modification History:
//  Sun Jul 06 2025 (kcm) -- initial version
//  Tue Jul 08 2025 (kcm) -- added option for read-only partition
//
//----------------------------------------------------------------------

#include "DiskImageUtils.h"
#include "DiskImageConvert.h"
#include "Driver.h"

static int WriteHFSVolumeAttributes(int fd, off_t hfsStart, int rw) {
    int result = 0;
    off_t offset = hfsStart + (512*2); // offset to MDB in the file
    off_t attrOffset = offset + 10; // offset to drAtrb field of MDB
    ushort volAttrs = 0;
    if ((result = ReadUShort(fd, attrOffset, &volAttrs)) != 0) {
        return result;
    }
    if (rw) { // volume should be writable: clear lock bits
        volAttrs &= ~(1 << HFSVolumeHardwareLockBit);
        volAttrs &= ~(1 << HFSVolumeSoftwareLockBit);
    } else { // volume should be read-only: set lock bits
        volAttrs |= (1 << HFSVolumeHardwareLockBit);
        volAttrs |= (1 << HFSVolumeSoftwareLockBit);
    }
    volAttrs = (ushort) htons(volAttrs);
    if ((offset = lseek(fd, attrOffset, SEEK_SET)) == -1) {
        return errno;
    }
    if ((offset = write(fd, &volAttrs, sizeof(volAttrs))) < 0) {
        return errno;
    }
    return result;
}

static int WriteHFSVolumeData(int ofd, int fd, off_t rdStart, off_t wrStart, size_t hfsLen, int rw) {
    int result = 0;
    off_t rdCount, wrCount;
    size_t chunkSize = 256*1024; // buffer 256K at a time
    char *buf = malloc(chunkSize);
    size_t bytesRemaining = hfsLen;
    double percentComplete;
    if ((rdCount = lseek(fd, rdStart, SEEK_SET)) == -1) { goto error; }
    if ((wrCount = lseek(ofd, wrStart, SEEK_SET)) == -1) { goto error; }
    while (bytesRemaining) {
        rdCount = chunkSize;
        if (bytesRemaining < rdCount) {
            rdCount = bytesRemaining;
        }
        if ((rdCount = read(fd, buf, rdCount)) < 0) { goto error; }
        if ((wrCount = write(ofd, buf, rdCount)) < 0) { goto error; }
        bytesRemaining -= wrCount;
        percentComplete = (double)(hfsLen-bytesRemaining)/hfsLen;
        progress(percentComplete);
    }
    fprintf(stdout, "\n");
    result = WriteHFSVolumeAttributes(ofd, wrStart, rw);
    if (!result) {
        char *str = (rw) ? "writable" : "read-only";
        tabprint(0, "Marked HFS volume as %s\n", str);
    }
    goto cleanup;
error:
    result = errno;
    if (!result) { result = -1; }
cleanup:
    free(buf);
    return result;
}

static int WriteDriverDescriptionRecord(int fd, size_t hfsLen) {
    DDRecord ddr = {0};
    off_t count;
    ushort blockSize = 0x200; // 512
    // ddr, partition map and driver occupy first 0xC000 bytes of file,
    // followed by the HFS volume data
    ulong totalBytes = (0xC000 + hfsLen);
    ulong totalBlks = totalBytes / blockSize;

    ddr.sbSig = (ushort) htons(0x4552); // 'ER'
    ddr.sbBlkSize = (ushort) htons(blockSize); // 512
    ddr.sbBlkCount = (ulong) htonl(totalBlks);
    ddr.sbDevType = (ushort) htons(1) ; // device type = 1
    ddr.sbDevId = (ushort) htons(1) ; // device id = 1
    ddr.sbDrvrCount = (ushort) htons(1);
    ddr.ddBlock = (ulong) htonl(0x40); // block 64
    ddr.ddSize = (ushort) htons(0x13); // needs 19 blocks
    ddr.ddType = (ushort) htons(1); // MacOS = 1

    // write driver descriptor record at offset 0
    if ((count = lseek(fd, 0, SEEK_SET)) == -1) { return errno; }
    if ((count = write(fd, &ddr, sizeof(DDRecord))) < 0) { return errno; }
    return 0;
}

/*
    pmPartStatus (from IM:Devices 3-26):
    Two words of status information about the partition.
    The low-order byte of the low-order word contains status information
    used only by the A/UX operating system:
        Bit Meaning
        0   Set if a valid partition map entry
        1   Set if partition is already allocated; clear if available
        2   Set if partition is in use; may be cleared after a system reset
        3   Set if partition contains valid boot information
        4   Set if partition allows reading
        5   Set if partition allows writing
        6   Set if boot code is position-independent
        7   Unused
*/

static int WriteApplePartitionMapEntry(int fd, int mapBlks) {
    Partition pme = {0};
    off_t count;
    pme.pmSig = (ushort) htons(0x504D); // 'PM'
    pme.pmMapBlkCnt = (ulong) htonl(mapBlks); // number of blocks in map
    pme.pmPyPartStart = (ulong) htonl(1); // this one starts at block 1
    pme.pmPartBlkCnt = (ulong) htonl(63); // 63 blocks in partition
    strncpy(pme.pmPartName, "Apple", 5);
    strncpy(pme.pmPartType, "Apple_partition_map", 19);
    pme.pmDataCnt = (ulong) htonl(63); // same as pmPartBlkCnt
    pme.pmPartStatus = (ulong) htonl(0x37); // see pmPartStatus flags

    // write partition map entry at offset 0x200
    if ((count = lseek(fd, 0x200, SEEK_SET)) == -1) { return errno; }
    if ((count = write(fd, &pme, sizeof(Partition))) < 0) { return errno; }
    return 0;
}

static int WriteDriverPartitionEntry(int fd, int mapBlks) {
    Partition pme = {0};
    off_t count;
    pme.pmSig = (ushort) htons(0x504D); // 'PM'
    pme.pmMapBlkCnt = (ulong) htonl(mapBlks); // number of blocks in map
    pme.pmPyPartStart = (ulong) htonl(64); // this one starts at block 64
    pme.pmPartBlkCnt = (ulong) htonl(32); // 32 blocks in partition
    strncpy(pme.pmPartName, "Macintosh", 9);
    strncpy(pme.pmPartType, "Apple_Driver43", 14);
    pme.pmDataCnt = (ulong) htonl(32); // same as pmPartBlkCnt
    pme.pmPartStatus = (ulong) htonl(0x7F); // see pmPartStatus flags
    pme.pmBootSize = (ulong) htonl(sizeof(_Apple_Driver43));
    pme.pmBootCksum = (ulong) htonl(0x0000F624); // 16-bit checksum
    strncpy(pme.pmProcessor, "68000", 5); // processor type

    // %%% mystery bytes. what is this doing? it seems to be necessary.
    pme.pmPad[1] = 0x01; pme.pmPad[2] = 0x06;
    pme.pmPad[11] = 0x01; pme.pmPad[13] - 0x07;

    // write driver partition entry at offset 0x400
    if ((count = lseek(fd, 0x400, SEEK_SET)) == -1) { return errno; }
    if ((count = write(fd, &pme, sizeof(Partition))) < 0) { return errno; }
    return 0;
}

static int WriteHFSPartitionEntry(int fd, int mapBlks, int writable, size_t hfsLen) {
    Partition pme = {0};
    off_t count;
    ulong flags = (writable) ? 0xB7 : 0x97; // writable if bit 5 set
    pme.pmSig = (ushort) htons(0x504D); // 'PM'
    pme.pmMapBlkCnt = (ulong) htonl(mapBlks); // number of blocks in map
    pme.pmPyPartStart = (ulong) htonl(96); // this one starts at block 96
    pme.pmPartBlkCnt = (ulong) htonl(hfsLen/0x200); // size in blocks
    strncpy(pme.pmPartName, "MacOS", 5);
    strncpy(pme.pmPartType, "Apple_HFS", 9);
    pme.pmDataCnt = (ulong) htonl(hfsLen/0x200); // same as pmPartBlkCnt
    pme.pmPartStatus = (ulong) htonl(flags); // see pmPartStatus flags

    // write HFS partition entry at offset 0x600
    if ((count = lseek(fd, 0x600, SEEK_SET)) == -1) { return errno; }
    if ((count = write(fd, &pme, sizeof(Partition))) < 0) { return errno; }
    return 0;
}

static int WriteDriverData(int fd) {
    off_t count;
    // driver goes after partition map at offset 0x8000 (32768)
    if ((count = lseek(fd, 0x8000, SEEK_SET)) == -1) { return errno; }
    if ((count = write(fd, &_Apple_Driver43[0], sizeof(_Apple_Driver43))) < 0) { return errno; }
    return 0;
}

static int WriteDeviceImage(int ofd, int fd, off_t hfsStart, size_t hfsLen, int rw) {
    int result = 0;
    // number of blocks (entries) in our partition map
    const int mapBlks = 3;
    // write driver descriptor record (block 0)
    tabprint(1, "Writing driver descriptor record\n");
    if ((result = WriteDriverDescriptionRecord(ofd, hfsLen)) != 0) {
        goto done;
    }
    // write partition map entry (block 1)
    tabprint(1, "Writing Apple partition map\n");
    if ((result = WriteApplePartitionMapEntry(ofd, mapBlks)) != 0) {
        goto done;
    }
    // write driver partition entry (block 2)
    tabprint(1, "Writing driver partition\n");
    if ((result = WriteDriverPartitionEntry(ofd, mapBlks)) != 0) {
        goto done;
    }
    // write HFS partition entry (block 3)
    tabprint(1, "Writing HFS partition\n");
    if ((result = WriteHFSPartitionEntry(ofd, mapBlks, rw, hfsLen)) != 0) {
        goto done;
    }
    // write driver: sizeof(_Apple_Driver43) at offset 0x8000 (32768)
    tabprint(1, "Writing driver data\n");
    if ((result = WriteDriverData(ofd)) != 0) {
        goto done;
    }
    // write HFS partition: hfsLen bytes at offset 0xC000 (49152)
    tabprint(0, "Writing HFS volume data\n");
    result = WriteHFSVolumeData(ofd, fd, hfsStart, 0xC000, hfsLen, rw);
done:
    return result;
}

// find the offset and length in bytes of the first HFS partition
static int ProbePartitionMap(int fd, size_t fileSize, off_t *hfsStart, size_t *hfsLen) {
    Partition pme;
    char ptype[34];
    const off_t kBlockSize = 512;
    off_t pmeOffset = kBlockSize; // partition map starts at block 1
    off_t partOffset; // in bytes
    size_t partLength; // in bytes
    memset(ptype, 0, sizeof(ptype));

    while (pmeOffset) {
        if (ReadPartitionMapEntry(fd, pmeOffset, &pme) != 0) { break; }
        if (pme.pmSig != 0x504D) { break; } // 'PM'
        partOffset = pme.pmPyPartStart * kBlockSize;
        partLength = pme.pmPartBlkCnt * kBlockSize;
        memcpy(ptype, (char*)pme.pmPartType, 32);
        if (!strncmp(ptype, "Apple_HFS", strlen(ptype))) {
            if (partOffset > fileSize) { return -1; } // bad
            while ((partLength >= kBlockSize) &&
                   ((partOffset + partLength) > fileSize)) {
                // truncate partition length to fit inside file length
                partLength -= kBlockSize;
            }
            *hfsStart = partOffset;
            *hfsLen = partLength;
            return 0;
        }
        pmeOffset += kBlockSize; // next partition map entry
    }
    return -1;
}

// find the offset and length in bytes of the HFS volume
static int ProbeFile(int fd, size_t *fileSize, off_t *hfsStart, size_t *hfsLen) {
    DDRecord ddr;
    ushort hfsSig = 0;
    int result = 0;
    struct stat sb = {0};
    if (fstat(fd, &sb) < 0) { return errno; }
    *fileSize = sb.st_size;
    if (ReadDriverDescriptorRecord(fd, 0, &ddr) != 0) { return -1; }

    // secondary check for HFS or HFS+ sig if this is a volume image
    if (!((ReadUShort(fd, 0x400, &hfsSig) == 0) &&
        (hfsSig == 0x4244 || hfsSig == 0x482B))) { // HFS or HFS+
        hfsSig = 0;
    }
    if (ddr.sbSig == 0x4552) { // 'ER'
        return ProbePartitionMap(fd, sb.st_size, hfsStart, hfsLen);
    } else if ((ddr.sbSig == 0x4C4B) || // 'LK'
               (ddr.sbSig == 0x0000 && hfsSig != 0))  {
        *hfsStart = 0;
        *hfsLen = *fileSize;
        return 0;
    }
    return -1; // can't get HFS volume
}

void ConvertFile(int iso, char *inPath, char *outPath, int rw) {
    struct stat sb = {0};
    int fd = -1, ofd = -1;
    int result;
    size_t fileSize;
    off_t hfsStart;
    size_t hfsLen;
    if ((fd = open(inPath, O_RDONLY, 0)) == -1) {
        tabprint(0, "Unable to open \"%s\" (%d)\n", inPath, errno);
        goto done;
    }
    result = ProbeFile(fd, &fileSize, &hfsStart, &hfsLen);
    tabprint(0, "Input file: \"%s\"\n", inPath);
    tabprint(0, "Input file size: %ld bytes\n", fileSize);
    if (result != 0) {
        tabprint(0, "Unable to find HFS volume (error %d)\n", result);
        goto done;
    } else {
        tabprint(0, "HFS volume found at offset %lld, length %lld\n", hfsStart, hfsLen);
    }
    tabprint(0, "Output file: \"%s\"\n", outPath);
    if ((ofd = open(outPath, O_RDWR | O_CREAT | O_TRUNC, 0600)) == -1) {
        tabprint(0, "Unable to create output file (%d)\n", outPath, errno);
        goto done;
    }
    if (iso) { // Apple partition map device image
        tabprint(0, "Writing Apple partition map device image\n");
        result = WriteDeviceImage(ofd, fd, hfsStart, hfsLen, rw);
    } else { // HFS volume image, just the raw bytes at offset 0
        tabprint(0, "Writing HFS volume data\n");
        result = WriteHFSVolumeData(ofd, fd, hfsStart, 0, hfsLen, rw);
    }
    if (fstat(ofd, &sb) < 0) { result = errno; }
    if (result == 0) {
        tabprint(0, "Wrote %lld bytes to output file.\n", sb.st_size);
    } else {
        tabprint(0, "An error occurred writing the image: %d\n", result);
    }
done:
    if (fd != -1) { close(fd); }
    if (ofd != -1) { close(ofd); }
}
