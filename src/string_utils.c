#include "string_utils.h"
#include "common.h"

void to_uppercase(char *psStr) {
    for (int nI = 0; psStr[nI]; nI++) {
        psStr[nI] = toupper((unsigned char)psStr[nI]);
    }
}

void sanitize_username(char *psUsername) {
    char *psSrc = psUsername;
    char *psDst = psUsername;
    while (*psSrc) {
        if (*psSrc != '&') {
            *psDst = *psSrc;
            psDst++;
        }
        psSrc++;
    }
    *psDst = '\0';
}