package core

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"os"
)

const (
	HFSVolumeHardwareLockBit = 7
	HFSVolumeSoftwareLockBit = 15
)

type ProgressFunc func(percent float64)

func WriteHFSVolumeAttributes(f *os.File, hfsStart int64, rw bool) error {
	offset := hfsStart + (512 * 2) // offset to MDB
	attrOffset := offset + 10      // offset to drAtrb

	// Read current attributes
	var volAttrs uint16
	if _, err := f.Seek(attrOffset, 0); err != nil {
		return err
	}
	if err := binary.Read(f, binary.BigEndian, &volAttrs); err != nil {
		return err
	}

	if rw {
		volAttrs &= ^uint16(1 << HFSVolumeHardwareLockBit)
		volAttrs &= ^uint16(1 << HFSVolumeSoftwareLockBit)
	} else {
		volAttrs |= (1 << HFSVolumeHardwareLockBit)
		volAttrs |= (1 << HFSVolumeSoftwareLockBit)
	}

	if _, err := f.Seek(attrOffset, 0); err != nil {
		return err
	}
	return binary.Write(f, binary.BigEndian, volAttrs)
}

func WriteHFSVolumeData(ofd, ifd *os.File, rdStart, wrStart int64, hfsLen int64, rw bool, progress ProgressFunc) error {
	chunkSize := int64(256 * 1024)
	buf := make([]byte, chunkSize)
	bytesRemaining := hfsLen

	if _, err := ifd.Seek(rdStart, 0); err != nil {
		return err
	}
	if _, err := ofd.Seek(wrStart, 0); err != nil {
		return err
	}

	for bytesRemaining > 0 {
		toRead := chunkSize
		if bytesRemaining < toRead {
			toRead = bytesRemaining
		}

		n, err := ifd.Read(buf[:toRead])
		if err != nil {
			return err
		}

		if _, err := ofd.Write(buf[:n]); err != nil {
			return err
		}

		bytesRemaining -= int64(n)
		if progress != nil {
			percent := float64(hfsLen-bytesRemaining) / float64(hfsLen)
			progress(percent)
		}
	}

	if err := WriteHFSVolumeAttributes(ofd, wrStart, rw); err != nil {
		return err
	}
	return nil
}

func WriteDriverDescriptorRecord(f *os.File, hfsLen int64) error {
	blockSize := int64(512)
	totalBytes := 0xC000 + hfsLen
	totalBlks := totalBytes / blockSize

	ddr := DDRecord{
		SbSig:       0x4552, // 'ER'
		SbBlkSize:   512,
		SbBlkCount:  uint32(totalBlks),
		SbDevType:   1,
		SbDevId:     1,
		SbDrvrCount: 1,
		DdBlock:     0x40, // 64
		DdSize:      0x13, // 19
		DdType:      1,    // MacOS
	}

	return WriteBigEndian(f, 0, ddr)
}

func WriteApplePartitionMapEntry(f *os.File, mapBlks uint32) error {
	pme := Partition{
		PmSig:         0x504D, // 'PM'
		PmMapBlkCnt:   mapBlks,
		PmPyPartStart: 1,
		PmPartBlkCnt:  63,
		PmDataCnt:     63,
		PmPartStatus:  0x37,
	}
	copy(pme.PmPartName[:], "Apple")
	copy(pme.PmPartType[:], "Apple_partition_map")

	return WriteBigEndian(f, 0x200, pme)
}

func WriteDriverPartitionEntry(f *os.File, mapBlks uint32) error {
	pme := Partition{
		PmSig:         0x504D, // 'PM'
		PmMapBlkCnt:   mapBlks,
		PmPyPartStart: 64,
		PmPartBlkCnt:  32,
		PmDataCnt:     32,
		PmPartStatus:  0x7F,
		PmBootSize:    uint32(len(AppleDriver43)), // Should be checking sizeof(_Apple_Driver43)
		PmBootCksum:   0x0000F624,
	}
	copy(pme.PmPartName[:], "Macintosh")
	copy(pme.PmPartType[:], "Apple_Driver43")
	copy(pme.PmProcessor[:], "68000")

	pme.PmPad[1] = 0x01
	pme.PmPad[2] = 0x06
	pme.PmPad[11] = 0x01
	pme.PmPad[13] = 0x07 // Check index math from C

	return WriteBigEndian(f, 0x400, pme)
}

func WriteHFSPartitionEntry(f *os.File, mapBlks uint32, writable bool, hfsLen int64) error {
	flags := uint32(0x97)
	if writable {
		flags = 0xB7
	}

	pme := Partition{
		PmSig:         0x504D, // 'PM'
		PmMapBlkCnt:   mapBlks,
		PmPyPartStart: 96,
		PmPartBlkCnt:  uint32(hfsLen / 0x200),
		PmDataCnt:     uint32(hfsLen / 0x200),
		PmPartStatus:  flags,
	}
	copy(pme.PmPartName[:], "MacOS")
	copy(pme.PmPartType[:], "Apple_HFS")

	return WriteBigEndian(f, 0x600, pme)
}

func WriteDriverData(f *os.File) error {
	if _, err := f.Seek(0x8000, 0); err != nil {
		return err
	}
	_, err := f.Write(AppleDriver43)
	return err
}

func WriteDeviceImage(ofd, ifd *os.File, hfsStart, hfsLen int64, rw bool, progress ProgressFunc) error {
	mapBlks := uint32(3)

	if err := WriteDriverDescriptorRecord(ofd, hfsLen); err != nil {
		return err
	}
	if err := WriteApplePartitionMapEntry(ofd, mapBlks); err != nil {
		return err
	}
	if err := WriteDriverPartitionEntry(ofd, mapBlks); err != nil {
		return err
	}
	if err := WriteHFSPartitionEntry(ofd, mapBlks, rw, hfsLen); err != nil {
		return err
	}
	if err := WriteDriverData(ofd); err != nil {
		return err
	}

	// Write HFS at 0xC000
	return WriteHFSVolumeData(ofd, ifd, hfsStart, 0xC000, hfsLen, rw, progress)
}

func ProbePartitionMap(f *os.File, fileSize int64) (int64, int64, error) {
	var pme Partition
	var pmeOffset int64 = 512

	for pmeOffset > 0 {
		p, err := ReadBigEndian[Partition](f, pmeOffset)
		if err != nil {
			return 0, 0, err
		}
		pme = p

		if pme.PmSig != 0x504D {
			break
		}

		partOffset := int64(pme.PmPyPartStart) * 512
		partLength := int64(pme.PmPartBlkCnt) * 512
		pType := string(bytes.TrimRight(pme.PmPartType[:], "\x00"))

		if pType == "Apple_HFS" {
			if partOffset > fileSize {
				return 0, 0, fmt.Errorf("partition start beyond file size")
			}
			for partLength >= 512 && (partOffset+partLength) > fileSize {
				partLength -= 512
			}
			return partOffset, partLength, nil
		}
		pmeOffset += 512
	}
	return 0, 0, fmt.Errorf("HFS partition not found")
}

func ProbeFile(f *os.File) (int64, int64, int64, error) {
	info, err := f.Stat()
	if err != nil {
		return 0, 0, 0, err
	}
	fileSize := info.Size()

	ddr, err := ReadBigEndian[DDRecord](f, 0)
	if err != nil {
		return 0, 0, 0, err
	}

	var hfsSig uint16
	hfsSig, _ = ReadBigEndian[uint16](f, 0x400)

	if !((hfsSig == 0x4244) || (hfsSig == 0x482B)) { // 'BD' (HFS) or 'H+' (HFS+)
		hfsSig = 0
	}

	if ddr.SbSig == 0x4552 { // 'ER'
		start, len, err := ProbePartitionMap(f, fileSize)
		return fileSize, start, len, err
	} else if ddr.SbSig == 0x4C4B || (ddr.SbSig == 0 && hfsSig != 0) { // 'LK' or Raw
		return fileSize, 0, fileSize, nil
	}

	return 0, 0, 0, fmt.Errorf("unknown format")
}

func ConvertFile(iso bool, inPath, outPath string, rw bool, progress ProgressFunc) error {
	fd, err := os.Open(inPath)
	if err != nil {
		return err
	}
	defer fd.Close()

	fileSize, hfsStart, hfsLen, err := ProbeFile(fd)
	if err != nil {
		return err
	}

	fmt.Printf("Input file size: %d bytes\n", fileSize)
	fmt.Printf("HFS volume found at offset %d, length %d\n", hfsStart, hfsLen)

	// Truncate/create output
	ofd, err := os.OpenFile(outPath, os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0644)
	if err != nil {
		return err
	}
	defer ofd.Close()

	if iso {
		err = WriteDeviceImage(ofd, fd, hfsStart, hfsLen, rw, progress)
	} else {
		err = WriteHFSVolumeData(ofd, fd, hfsStart, 0, hfsLen, rw, progress)
	}

	if err == nil {
		// Log success
		fmt.Printf("Wrote to output file.\n")
	}
	return err

}
