//----------------------------------------------------------------------
//
//  DiskImageUtils.h
//
//  Written by: Ken McLeod
//
//  Modification History:
//  Thu Jul 03 2025 (kcm) -- initial version
//
//----------------------------------------------------------------------

#ifndef __diskimageutils_h__
#define __diskimageutils_h__

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <libkern/OSByteOrder.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t uchar;
typedef uint16_t ushort;
typedef uint32_t ulong;
typedef uint64_t ulonglong;

#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_RESET   "\x1b[0m"

// Partitioned disk structures, from SCSI.h
// Driver Descriptor Record is block 0, first 512 bytes of disk
typedef struct __attribute__((aligned(2), packed)) DDRecord {
    ushort sbSig; // device signature (0x4552, 'ER' for Erich Ringewald)
    ushort sbBlkSize; // block size of the device (usually 0x200, 512)
    ulong sbBlkCount; // number of blocks on the device
    ushort sbDevType; // (reserved)
    ushort sbDevId; // (reserved)
    ulong sbData; // (reserved)
    ushort sbDrvrCount; // number of driver descriptor entries
    ulong ddBlock; // first driver's starting block
    ushort ddSize; // size of the driver, in 512-byte blocks
    ushort ddType; // operating system type (MacOS = 1)
    uchar ddPad[486]; // ddBlock/ddSize/ddType entries for additional drivers (8 bytes each)
}   DDRecord;

typedef struct __attribute__((aligned(2), packed)) Partition {
    ushort pmSig; // partition signature (0x504D, 'PM' for Partition Map)
    ushort pmSigPad; // (reserved)
    ulong pmMapBlkCnt; // number of blocks in partition map
    ulong pmPyPartStart; // block number of first block of partition
    ulong pmPartBlkCnt; // number of blocks in partition
    uchar pmPartName[32]; // partition name string (optional; some special values)
    uchar pmPartType[32]; // partition type string (names beginning with "Apple_" are reserved)
    ulong pmLgDataStart; // first logical block of data area (for e.g. A/UX)
    ulong pmDataCnt; // number of blocks in data area (for e.g. A/UX)
    ulong pmPartStatus; // partition status information (used by A/UX)
    ulong pmLgBootStart; // first logical block of boot code
    ulong pmBootSize; // size of boot code, in bytes
    ulong pmBootAddr; // boot code load address
    ulong pmBootAddr2; // (reserved)
    ulong pmBootEntry; // boot code entry point
    ulong pmBootEntry2; // (reserved)
    ulong pmBootCksum; // boot code checksum
    uchar pmProcessor[16]; // processor type string ("68000", "68020", "68030", "68040")
    uchar pmPad[376]; // (reserved)
}   Partition;

// HFS volume structures
// Boot block header is at the start of block 0.
typedef struct __attribute__((aligned(2), packed)) BootBlockHeader {
    ushort bbID; // signature bytes, for HFS this must be 0x4C4B ('LK' for Larry Kenyon)
    ulong bbEntry; // entry point to boot code, expressed as a 68K BRA.S instruction
    ushort bbVersion; // flag byte and boot block version number
    ushort bbPageFlags; // "used internally"
    uchar bbSysName[16]; // system filename, usually "System" (stored as string with leading length byte)
    uchar bbShellName[16]; // Finder filename, usually "Finder"
    uchar bbDbg1Name[16]; // first debugger filename, usually "MacsBug"
    uchar bbDbg2Name[16]; // second debugger filename, usually "Disassembler"
    uchar bbScreenName[16]; // file containing startup screen, usually "StartUpScreen"
    uchar bbHelloName[16]; // name of startup program, usually "Finder"
    uchar bbScrapName[16]; // name of system scrap file, usually "Clipboard"
    ushort bbCntFCBs; // number of FCBs to allocate
    ushort bbCntEvts; // number of event queue elements
    ulong bb128KSHeap; // system heap size on 128K Mac
    ulong bb256KSHeap; // "used internally"
    ulong bbSysHeapSize; // system heap size on machines with >= 512K of RAM
    ushort filler; // reserved
    ulong bbSysHeapExtra; // minimum amount of additional System heap space required
    ulong bbSysHeapFract; // fraction of RAM to make available for system heap;
    // executable boot code, if any, follows this header
}   BootBlockHeader;

// HFS volume attributes (from Kernel Enumerations on developer website)
typedef enum HFSVolumeAttributes {
    HFSVolumeHardwareLockBit = 7,
    HFSVolumeUnmountedBit = 8,
    HFSVolumeSparedBlocksBit = 9,
    HFSVolumeNoCacheRequiredBit = 10,
    HFSVolumeJournaledBit = 13,
    HFSVolumeInconsistentBit = 14,
    HFSVolumeSoftwareLockBit = 15,
}   HFSVolumeAttributes;

// Master Directory Block (aka Volume Information Block)
// This is logical block 2 (at offset 0x400, using 512-byte blocks)
typedef struct __attribute__((aligned(2), packed)) MasterDirectoryBlock {
    ushort drSigWord; // signature (big-endian 0x4244, 'BD' for Big Disk)
    ulong drCrDate; // creation date
    ulong drLsMod; // last modified date
    ushort drAtrb; // volume attributes -- see HFSVolumeAttributes
    ushort drNmFls; // number of files in root directory
    ushort drVBMSt; // first block of volume bitmap
    ushort drAllocPtr; // start of next allocation search
    ushort drNmAlBlks; // number of allocation blocks in volume
    ulong drAlBlkSiz; // size in bytes of allocation blocks
    ulong drClpSiz; // clump size
    ushort drAlBlSt; // first allocation block in volume
    ulong drNxtCNID; // next unused catalog node ID
    ushort drFreeBks; // number of unused allocation blocks
    uchar drVN[28]; // volume name Pascal string, end padded with zeroes
    ulong drVolBkUp; // last backup date
    ushort drVSeqNum; // volume backup sequence number
    ulong drWrCnt; // // volume write count
    ushort drXTClpSiz;
    ulong drCTClpSiz;
    ushort drNmRtDirs;
    ulong drFilCnt;
    ulong drDirCnt;
    ulong drFndrInfo[8]; // [8 * long] / finderInfo[32 * byte]
    ushort drVCSize; // -> drEmbedSigWord
    ulong drVBMCSize; // drVBMCSize/drCtlCSize -> drEmbedExtent
    ulong drXTFlSize;
    uchar drXTExtRec[12];
    ulong drCTFlSize;
    uchar drCTExtRec[12];
}   MasterDirectoryBlock;

// HFS+ fork data structure
typedef struct __attribute__((packed)) HFSPlusForkData {
    ulonglong logicalSize;
    ulong clumpSize;
    ulong totalBlocks;
    uchar extents[64];
}   HFSPlusForkData;

// HFS+ volume header
typedef struct __attribute__((aligned(2), packed)) HFSPlusVolumeHeader {
    ushort signature; // 0x482B 'H+'
    ushort version;
    ulong attributes;
    ulong lastMountedVersion;
    ulong journalInfoBlock;
    ulong createDate;
    ulong modifyDate;
    ulong backupDate;
    ulong checkedDate;
    ulong fileCount;
    ulong dirCount;
    ulong blockSize;
    ulong totalBlocks;
    ulong freeBlocks;
    ulong nextAllocation;
    ulong resClumpSize;
    ulong dataClumpSize;
    ulong nextCatalogID;
    ulong writeCount;
    ulonglong encodingsBitmap;
    ulong finderInfo[8]; // 32 bytes
    HFSPlusForkData allocationFile;
    HFSPlusForkData extentsFile;
    HFSPlusForkData catalogFile;
    HFSPlusForkData attributesFile;
    HFSPlusForkData startupFile;
}   HFSPlusVolumeHeader;


void tabprint(int tabstop, char *format, ...);
int progress(double percentComplete);
ushort Checksum16(uchar *bytes, size_t length);
ushort ComputeChecksum(int fd, off_t driverOffset, off_t length);
void DateStringForHFSDate(uint32_t hfsDate, uint32_t maxLen, char *str);

int ReadUShort(int fd, size_t offset, ushort *value);
int ReadULong(int fd, size_t offset, ulong *value);
int ReadDriverDescriptorRecord(int fd, size_t offset, DDRecord *ddr);
int ReadPartitionMapEntry(int fd, size_t offset, Partition *pme);
int ReadBootBlockHeader(int fd, size_t offset, BootBlockHeader *bb);
int ReadMasterDirectoryBlock(int fd, size_t offset, MasterDirectoryBlock *mdb);
int ReadHFSPlusVolumeHeader(int fd, size_t offset, HFSPlusVolumeHeader *vh);

#ifdef __cplusplus
}
#endif

#endif /* __diskimageutils_h__ */

