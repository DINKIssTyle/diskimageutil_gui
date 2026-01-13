package core

// Block0 (Driver Descriptor Record)
type DDRecord struct {
	SbSig       uint16 // device signature
	SbBlkSize   uint16 // block size of the device
	SbBlkCount  uint32 // number of blocks on the device
	SbDevType   uint16 // device type
	SbDevId     uint16 // device id
	SbData      uint32 // not used
	SbDrvrCount uint16 // driver descriptor count
	DdBlock     uint32 // first driver's starting block
	DdSize      uint16 // size of the driver, in 512-byte blocks
	DdType      uint16 // operating system type (MacOS = 1)
}

// Partition Map Entry
type Partition struct {
	PmSig         uint16    // partition signature
	PmSigPad      uint16    // reserved
	PmMapBlkCnt   uint32    // number of blocks in partition map
	PmPyPartStart uint32    // first physical block of partition
	PmPartBlkCnt  uint32    // number of blocks in partition
	PmPartName    [32]byte  // partition name
	PmPartType    [32]byte  // partition type
	PmLgDataStart uint32    // first logical block of data area
	PmDataCnt     uint32    // number of blocks in data area
	PmPartStatus  uint32    // partition status information
	PmLgBootStart uint32    // first logical block of boot code
	PmBootSize    uint32    // size of boot code, in bytes
	PmBootAddr    uint32    // boot code load address
	PmBootAddr2   uint32    // reserved
	PmBootEntry   uint32    // boot code entry point
	PmBootEntry2  uint32    // reserved
	PmBootCksum   uint32    // boot code checksum
	PmProcessor   [16]byte  // processor type
	PmPad         [376]byte // reserved
}

// Boot Block Header
type BootBlockHeader struct {
	BbID           uint16   // signature bytes (0x4C4B 'LK')
	BbEntry        uint32   // entry point to boot code
	BbVersion      uint16   // flag byte and version
	BbPageFlags    uint16   // used internally
	BbSysName      [16]byte // system filename
	BbShellName    [16]byte // Finder filename
	BbDbg1Name     [16]byte // debugger filename
	BbDbg2Name     [16]byte // debugger filename
	BbScreenName   [16]byte // startup screen filename
	BbHelloName    [16]byte // startup program filename
	BbScrapName    [16]byte // system scrap filename
	BbCntFCBs      uint16   // number of FCBs
	BbCntEvts      uint16   // number of event queue elements
	Bb128KSHeap    uint32   // system heap size on 128K Mac
	Bb256KSHeap    uint32   // used internally
	BbSysHeapSize  uint32   // system heap size
	Filler         uint16   // reserved
	BbSysHeapExtra uint32   // additional system heap space
	BbSysHeapFract uint32   // fraction of RAM for system heap
}

// Master Directory Block (MDB) for HFS
type MasterDirectoryBlock struct {
	DrSigWord  uint16           // volume signature
	DrCrDate   uint32           // date and time of volume creation
	DrLsMod    uint32           // date and time of last modification
	DrAtrb     uint16           // volume attributes
	DrNmFls    uint16           // number of files in root directory
	DrVBMSt    uint16           // first block of volume bitmap
	DrAllocPtr uint16           // used internally
	DrNmAlBlks uint16           // total number of allocation blocks
	DrAlBlkSiz uint32           // allocation block size
	DrClpSiz   uint32           // default clump size
	DrAlBlSt   uint16           // first allocation block in volume
	DrNxtCNID  uint32           // next unused catalog node ID
	DrFreeBks  uint16           // number of unused allocation blocks
	DrVolName  [28]byte         // volume name
	DrVolBkUp  uint32           // date and time of last backup
	DrVSeqNum  uint16           // volume sequence number
	DrWrCnt    uint32           // volume write count
	DrXTClpSiz uint32           // clump size for extents overflow file
	DrCTClpSiz uint32           // clump size for catalog file
	DrNmRtDirs uint16           // number of directories in root directory
	DrFilCnt   uint32           // number of files in volume
	DrDirCnt   uint32           // number of directories in volume
	DrFndrInfo [8]uint32        // information used by the Finder
	DrVCSize   uint16           // size (in blocks) of volume cache
	DrVBMCSize uint16           // size (in blocks) of volume bitmap cache
	DrCtlCSize uint16           // size (in blocks) of common volume cache
	DrXTFlSize uint32           // size of extents overflow file
	DrXTExtRec [3]ExtDescriptor // first 3 extents of extents overflow file
	DrCTFlSize uint32           // size of catalog file
	DrCTExtRec [3]ExtDescriptor // first 3 extents of catalog file
}

type ExtDescriptor struct {
	XdrStABN uint16 // first allocation block
	XdrNumAB uint16 // number of allocation blocks
}

// HFS Plus Volume Header
type HFSPlusVolumeHeader struct {
	Signature          uint16
	Version            uint16
	Attributes         uint32
	LastMountedVersion uint32
	JournalInfoBlock   uint32
	CreateDate         uint32
	ModifyDate         uint32
	BackupDate         uint32
	CheckedDate        uint32
	FileCount          uint32
	DirCount           uint32
	BlockSize          uint32
	TotalBlocks        uint32
	FreeBlocks         uint32
	NextAllocation     uint32
	ResClumpSize       uint32
	DataClumpSize      uint32
	NextCatalogID      uint32
	WriteCount         uint32
	EncodingsBitmap    uint64
	FinderInfo         [8]uint32
	AllocationFile     HFSPlusForkData
	ExtentsFile        HFSPlusForkData
	CatalogFile        HFSPlusForkData
	AttributesFile     HFSPlusForkData
	StartupFile        HFSPlusForkData
}

type HFSPlusForkData struct {
	LogicalSize uint64
	ClumpSize   uint32
	TotalBlocks uint32
	Extents     [8]HFSPlusExtentDescriptor
}

type HFSPlusExtentDescriptor struct {
	StartBlock uint32
	BlockCount uint32
}

const (
	BlockSize = 512
)
