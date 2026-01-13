package core

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"
)

const (
	hfsEpochOffset = 2082844800 // Seconds between Jan 1 1904 and Jan 1 1970
)

func DateStringForHFSDate(hfsDate uint32) string {
	if hfsDate == 0 {
		return ""
	}
	// Convert HFS date (seconds since 1904) to Unix time (seconds since 1970)
	unixTime := int64(hfsDate) - hfsEpochOffset
	t := time.Unix(unixTime, 0).UTC() // C code used CoreFoundation default, let's use UTC or Local?
	// C code used kCFDateFormatterLongStyle which is locale dependent.
	// Let's use a standard format for now.
	return t.Format("January 2, 2006 at 3:04:05 PM MST")
}

func tabPrint(sb *strings.Builder, tab int, format string, a ...interface{}) {
	indent := strings.Repeat("    ", tab)
	sb.WriteString(indent + fmt.Sprintf(format, a...))
}

func describeHFSPlusVolume(f *os.File, offset int64, tab int, sb *strings.Builder) {
	vh, err := ReadBigEndian[HFSPlusVolumeHeader](f, offset)
	if err != nil {
		tabPrint(sb, tab, "Error reading HFS+ volume header\n")
		return
	}

	tabPrint(sb, tab, "Created: %s\n", DateStringForHFSDate(vh.CreateDate))
	tabPrint(sb, tab, "Last modified: %s\n", DateStringForHFSDate(vh.ModifyDate))

	capacityBytes := uint64(vh.BlockSize) * uint64(vh.TotalBlocks)
	capacityMB := float64(capacityBytes) / (1024.0 * 1024.0)
	tabPrint(sb, tab, "Capacity: %.1f MB (%d bytes)\n", capacityMB, capacityBytes)

	usedBytes := uint64(vh.BlockSize) * (uint64(vh.TotalBlocks) - uint64(vh.FreeBlocks))
	usedMB := float64(usedBytes) / (1024.0 * 1024.0)
	tabPrint(sb, tab, "Used: %.1f MB (%d bytes)\n", usedMB, usedBytes)

	freeBytes := uint64(vh.BlockSize) * uint64(vh.FreeBlocks)
	freeMB := float64(freeBytes) / (1024.0 * 1024.0)
	tabPrint(sb, tab, "Free: %.1f MB (%d bytes)\n", freeMB, freeBytes)
}

func describeHFSVolume(f *os.File, offset int64, tab int, sb *strings.Builder, verbose bool) {
	bb, err := ReadBigEndian[BootBlockHeader](f, offset)
	if err != nil {
		tabPrint(sb, tab, "Error reading HFS boot blocks\n")
		return
	}

	mdbOffset := offset + (512 * 2)
	mdb, err := ReadBigEndian[MasterDirectoryBlock](f, mdbOffset)
	if err != nil {
		tabPrint(sb, tab, "Error reading volume information block\n")
		return
	}

	if verbose {
		tabPrint(sb, tab, "Boot block signature: 0x%04X", bb.BbID)
		if bb.BbID == 0 {
			tabPrint(sb, 0, " (non-bootable volume)\n")
		} else if bb.BbID == 0x4C4B { // 'LK'
			tabPrint(sb, 0, " 'LK' (bootable volume)\n")
		} else {
			tabPrint(sb, 0, " (expected 0x4C4B)\n")
		}
		tabPrint(sb, tab, "Boot block version: 0x%04X\n", bb.BbVersion)

		tabPrint(sb, tab, "Volume signature: 0x%04X ", mdb.DrSigWord)
		if mdb.DrSigWord == 0x4244 {
			tabPrint(sb, 0, "'BD' (HFS volume)\n")
		} else if mdb.DrSigWord == 0x482B {
			tabPrint(sb, 0, "'H+' (HFS+ volume)\n")
		} else {
			tabPrint(sb, 0, "(unrecognized format)\n")
		}
	}

	if mdb.DrSigWord == 0x4244 { // HFS
		nameLen := int(mdb.DrVolName[0])
		if nameLen > 27 {
			nameLen = 27 // Safety
		}
		name := string(mdb.DrVolName[1 : 1+nameLen])
		tabPrint(sb, tab, "Volume: %s\n", name)

		tabPrint(sb, tab, "Created: %s\n", DateStringForHFSDate(mdb.DrCrDate))
		tabPrint(sb, tab, "Last modified: %s\n", DateStringForHFSDate(mdb.DrLsMod))

		capacityBytes := uint64(mdb.DrAlBlkSiz) * uint64(mdb.DrNmAlBlks)
		capacityMB := float64(capacityBytes) / (1024.0 * 1024.0)
		tabPrint(sb, tab, "Capacity: %.1f MB (%d bytes)\n", capacityMB, capacityBytes)

		usedBytes := uint64(mdb.DrAlBlkSiz) * uint64(uint32(mdb.DrNmAlBlks)-uint32(mdb.DrFreeBks))
		usedMB := float64(usedBytes) / (1024.0 * 1024.0)
		tabPrint(sb, tab, "Used: %.1f MB (%d bytes)\n", usedMB, usedBytes)

		freeBytes := uint64(mdb.DrAlBlkSiz) * uint64(mdb.DrFreeBks)
		freeMB := float64(freeBytes) / (1024.0 * 1024.0)
		tabPrint(sb, tab, "Free: %.1f MB (%d bytes)\n", freeMB, freeBytes)

	} else if mdb.DrSigWord == 0x482B { // HFS+
		describeHFSPlusVolume(f, mdbOffset, tab, sb)
	}
}

func describePartitionMap(f *os.File, fileSize int64, tab int, sb *strings.Builder) {
	var pme Partition
	var pmeOffset int64 = 512
	i := 0

	for pmeOffset > 0 {
		p, err := ReadBigEndian[Partition](f, pmeOffset)
		if err != nil {
			break
		}
		pme = p

		if pme.PmSig != 0x504D {
			break
		}

		partOffset := int64(pme.PmPyPartStart) * 512
		partLength := int64(pme.PmPartBlkCnt) * 512

		pName := string(bytes.TrimRight(pme.PmPartName[:], "\x00"))
		pType := string(bytes.TrimRight(pme.PmPartType[:], "\x00"))

		tabPrint(sb, tab, "\n")
		tabPrint(sb, tab, "Partition %d: %s (%s)\n", i, pName, pType)
		tabPrint(sb, tab+1, "Size: %d bytes (offset %d to %d)", partLength, partOffset, partOffset+partLength)

		if (partOffset + partLength) > fileSize {
			tabPrint(sb, 0, " [TRUNCATED]")
		}
		tabPrint(sb, 0, "\n")

		if strings.HasPrefix(pType, "Apple_Driver") {
			drvOffset := partOffset
			drvLength := int64(pme.PmBootSize)

			cksum, _ := ComputeChecksum(f, drvOffset, int(drvLength))
			tabPrint(sb, tab+1, "Code: %d bytes (offset %d in file)\n", drvLength, drvOffset)
			tabPrint(sb, tab+1, "Checksum: 0x%08X", pme.PmBootCksum)

			if pme.PmBootCksum == 0 {
				if strings.HasPrefix(pName, "Maci") {
					tabPrint(sb, 0, " (driver will not load)")
				}
			} else {
				tabPrint(sb, 0, " (computed 0x%04X) ", cksum)
				// Note: C code computed Checksum16 but compared with 32-bit PmBootCksum?
				// Driver.h defines 16-bit checksum. PmBootCksum is uint32.
				// C code: ulong cksum = ComputeChecksum(...) (returns ushort).
				// C code: if (cksum == pme.pmBootCksum)
				// The comparison logic in C seems to compare a ushort promoted to ulong with a ulong.
				if uint32(cksum) == pme.PmBootCksum {
					tabPrint(sb, 0, " [VERIFIED]")
				} else {
					tabPrint(sb, 0, " [VERIFY FAILED]")
				}
			}
			tabPrint(sb, 0, "\n")
		}

		if strings.HasPrefix(pType, "Apple_HFS") {
			describeHFSVolume(f, partOffset, tab+1, sb, true) // Always verbose for partitions?
		}

		pmeOffset += 512
		i++
	}
}

func DescribeFile(inPath string, verbose bool) (string, error) {
	var sb strings.Builder

	f, err := os.Open(inPath)
	if err != nil {
		return "", err
	}
	defer f.Close()

	info, err := f.Stat()
	if err != nil {
		return "", err
	}
	fileSize := info.Size()

	name := filepath.Base(inPath)
	tabPrint(&sb, 0, "Checking file \"%s\"\n", name)
	tabPrint(&sb, 0, "File size: %d bytes\n", fileSize)

	ddr, err := ReadBigEndian[DDRecord](f, 0)
	if err != nil {
		return sb.String(), err
	}

	var hfsSig uint16
	hfsSig, _ = ReadBigEndian[uint16](f, 0x400)
	if !((hfsSig == 0x4244) || (hfsSig == 0x482B)) {
		hfsSig = 0
	}

	if ddr.SbSig == 0x4552 && verbose { // 'ER'
		length := int64(ddr.SbBlkSize) * int64(ddr.SbBlkCount)
		if length > 0 {
			tabPrint(&sb, 0, "Device size: %d bytes", length)
			if length > fileSize {
				tabPrint(&sb, 0, " [TRUNCATED]")
			}
			tabPrint(&sb, 0, "\n")
		} else {
			tabPrint(&sb, 0, "Device size: (not specified)\n")
		}
		tabPrint(&sb, 0, "Device signature: 0x%04X 'ER'\n", ddr.SbSig)
	}

	if ddr.SbSig == 0x4552 { // 'ER'
		tabPrint(&sb, 0, "File format: Apple Partition Map disk image\n")
		describePartitionMap(f, fileSize, 1, &sb)
	} else if ddr.SbSig == 0x4C4B { // 'LK'
		tabPrint(&sb, 0, "File format: Apple HFS volume image (bootable)\n")
		describeHFSVolume(f, 0, 1, &sb, verbose)
	} else if ddr.SbSig == 0x0000 && hfsSig != 0 {
		tabPrint(&sb, 0, "File format: Apple HFS volume image (not bootable)\n")
		describeHFSVolume(f, 0, 1, &sb, verbose)
	} else {
		tabPrint(&sb, 0, "File is not a recognized disk image format.\n")
		tabPrint(&sb, 0, "Currently this utility only recognizes raw HFS or Apple Partition Map format.\n")
	}

	return sb.String(), nil
}
