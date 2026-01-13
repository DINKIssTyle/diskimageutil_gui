FRAMEWORKS = -framework CoreFoundation
INCLUDES = DiskImageUtils.h DiskImageConvert.h DiskImageDescribe.h Driver.h
LIBRARIES =
SOURCES = DiskImageUtils.c DiskImageConvert.c DiskImageDescribe.c diskimageutil.c
OUTPUT = diskimageutil

all:
	cc -g ${FRAMEWORKS} ${INCLUDES} ${LIBRARIES} ${SOURCES} -o ${OUTPUT}

clean:
	rm -r "${OUTPUT}" "${OUTPUT}.dSYM"
