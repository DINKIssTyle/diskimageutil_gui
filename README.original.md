# diskimageutil
**Convert and repair Apple HFS disk image files**

**Summary**

diskimageutil is a command-line tool that can convert between raw volume image (.dsk) and device image (.dmg or .iso) formats, producing a file which can be opened in an emulator. Run the tool in Terminal with no arguments to see its options.

**Introduction**

Disk images are files that hold the complete contents of a storage device, such as a floppy disk, hard drive, or CD-ROM. These files are frequently used as containers to archive information. They are also used by emulation software, which can mount and read them in a virtual machine as if they were a drive connected to that machine. Apple has traditionally provided tools (originally Disk Copy, then Disk Utility and `hdiutil`) to create and convert between disk image formats.

Unfortunately, some disk images which were created on "classic" versions of Mac OS are not able to be opened any more, even by Apple's own tools. Often this is because the HFS filesystem was used, and Apple decided to drop support for it in macOS Catalina (10.15). However, a number of disk images created from CD-ROMs turn out to have been mastered incorrectly, claiming to be a larger device than the actual file size would support. The CD-ROM driver allowed this, but emulators and disk image utilities will claim not to recognize the disk.

In addition, there are at least two different formats in use by emulators: raw HFS/HFS+ volume images, and device images. An emulator which expects one type generally cannot use the other. Apple has not provided a tool to create a volume image from a device image, or convert it to a device image.

There are some excellent utilities already written for this purpose, such as the full-featured Disk Jockey application. But if you are comfortable using Terminal and want to examine disk image files on the command line or process them in a script, then diskimageutil can be a useful tool.


**Usage**

    diskimageutil [-v] [-w] <verb> <file> [dstfile]
    <verb> is one of the following options:
        info      Prints type, size, and other info about <file>.
                  Use "-v info" to see more verbose detail.
        cvt2hfs   Converts input file to an HFS volume image.
                  If dstfile not specified, will create <file>.dsk.
        cvt2iso   Converts input file to an ISO device image.
                  If dstfile not specified, will create <file>.iso.
                  Use "-w cvt2iso" for a writable image (default is read-only)

**Examples**

    # Print info about contents of a disk image
        ./diskimageutil info "System 7.5.3.dmg"
    # Convert a disk image to a raw HFS volume
        ./diskimageutil cvt2hfs "System 7.5.3.iso" System753.dsk
    # Convert a disk image to an ISO device image
        ./diskimageutil cvt2iso MinivMac.dsk

**Notes**

Always keep a copy of your original source disk image, even if conversion is successful.

Use cvt2hfs to create a disk image for emulator software that expects a raw HFS volume, such as Mini vMac. Use cvt2iso for a device image that can be used with pre-10.15 versions of macOS/OS X, as well as in Basilisk, SheepShaver, Snow, QEMU, and other emulators.

Conversion to ISO format (even if the source image is already ISO) can repair readability problems with some device images, such as those made from old CD-ROMs. However, this process is lossy: it currently copies only the Apple_HFS partition, ignoring others. The intent is to make a working copy that can be used in an emulator, and is not a solution for archiving source media. ALWAYS keep your original disk image to avoid losing data!

**Limitations**

This program does not yet recognize many disk image formats, such as Disk Copy or DART images, or the GUID device partition scheme. It also does not yet correctly handle multiple HFS partitions in a device image. This software may contain bugs. Use at your own risk.

**Building**

This is a bare-bones "C" command-line tool. With Xcode's CLTools support installed, you should be able to build the tool by simply typing `make` while the diskimageutil directory is the current directory.

