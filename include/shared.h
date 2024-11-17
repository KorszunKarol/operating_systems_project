/*********************************
*
* @File: shared.h
* @Purpose: Shared definitions and functions
* @Author: Karol Korszun
* @Date: 2024-03-19
*
*********************************/

#ifndef __SHARED_H__
#define __SHARED_H__

void vWriteLog(const char* psMsg);
int nStringToInt(const char* psStr);
int nCreateDirectory(const char* psPath);

#endif