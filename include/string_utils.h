#ifndef __STRING_UTILS_H__
#define __STRING_UTILS_H__

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

void to_uppercase(char *psStr);
void sanitize_username(char *psUsername);

#endif