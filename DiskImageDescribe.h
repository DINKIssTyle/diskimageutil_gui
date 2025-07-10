//----------------------------------------------------------------------
//
//  DiskImageDescribe.h
//
//  Written by: Ken McLeod
//
//  Modification History:
//  Thu Jul 03 2025 (kcm) -- initial version
//
//----------------------------------------------------------------------

#ifndef __diskimagedescribe_h__
#define __diskimagedescribe_h__

#include "DiskImageUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

void DescribeHFSPlusVolume(int fd, size_t offset, int tab);
void DescribeHFSVolume(int fd, size_t offset, int tab);
void DescribePartitionMap(int fd, size_t fileSize, int tab);
void DescribeFile(const char *inPathname);

#ifdef __cplusplus
}
#endif

#endif /* __diskimagedescribe_h__ */

