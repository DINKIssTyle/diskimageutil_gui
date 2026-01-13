//----------------------------------------------------------------------
//
//  DiskImageConvert.h
//
//  Written by: Ken McLeod
//
//  Modification History:
//  Sun Jul 06 2025 (kcm) -- initial version
//  Tue Jul 08 2025 (kcm) -- added option for read-only partition
//
//----------------------------------------------------------------------


#ifndef __diskimageconvert_h__
#define __diskimageconvert_h__

#ifdef __cplusplus
extern "C" {
#endif

void ConvertFile(int iso, char *inFilePath, char *outFilePath, int rw);


#ifdef __cplusplus
}
#endif

#endif /* __diskimageconvert_h__ */
