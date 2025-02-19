/*********************************
*
* @File: common.h
* @Purpose: Common definitions and utility functions
* @Author: [Your Name]
* @Date: [YYYY-MM-DD]
*
*********************************/

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#define printF(x) write(1, x, strlen(x))

#define ERROR_MSG_CONFIG "Error opening config file\n"
#define ERROR_MSG_DIR "Error: Directory does not exist\n"

void nothing(int signum);

#endif
