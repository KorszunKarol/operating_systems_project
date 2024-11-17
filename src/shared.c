/*********************************
*
* @File: shared.c
* @Purpose: Implementation of shared functionality
* @Author: Karol Korszun
* @Date: 2024-03-19
*
*********************************/

#include "shared.h"
#include "common.h"
#include <stdlib.h>
#include <sys/stat.h>

void vWriteLog(const char* psMsg) {
    if (psMsg) {
        write(STDOUT_FILENO, psMsg, strlen(psMsg));
        fsync(STDOUT_FILENO);  // Ensure output is flushed
    }
}

int nStringToInt(const char* psStr) {
    return atoi(psStr);
}

int nCreateDirectory(const char* psPath) {
    struct stat st = {0};
    if (stat(psPath, &st) == -1) {
        return mkdir(psPath, 0755);
    }
    return 0;
}