package core

import (
	"encoding/binary"
	"os"
)

func Checksum16(data []byte) uint16 {
	var cksum uint16 = 0
	for _, b := range data {
		cksum += uint16(b)
		cksum = (cksum << 1) | (cksum >> 15)
	}
	if cksum == 0 {
		return 0xFFFF
	}
	return cksum
}

func ComputeChecksum(f *os.File, offset int64, length int) (uint16, error) {
	b := make([]byte, length)
	_, err := f.ReadAt(b, offset)
	if err != nil {
		return 0, err
	}
	return Checksum16(b), nil
}

func ReadBigEndian[T any](f *os.File, offset int64) (T, error) {
	var val T
	if _, err := f.Seek(offset, 0); err != nil {
		return val, err
	}
	err := binary.Read(f, binary.BigEndian, &val)
	return val, err
}

func WriteBigEndian(f *os.File, offset int64, val interface{}) error {
	if _, err := f.Seek(offset, 0); err != nil {
		return err
	}
	return binary.Write(f, binary.BigEndian, val)
}
