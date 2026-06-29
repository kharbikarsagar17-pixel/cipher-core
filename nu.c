/*
 * Nu Module - String Processing
 * Part of Cipher-Core Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NU_VERSION "1.0.0"

static void nu_init(void) {
    printf("Nu string processing module initialized v%s\n", NU_VERSION);
}

static void nu_to_lower(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

static void nu_to_upper(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
}

static int nu_count_words(const char *str) {
    int count = 0;
    int in_word = 0;
    for (int i = 0; str[i]; i++) {
        if (!isspace(str[i]) && !in_word) {
            count++;
            in_word = 1;
        } else if (isspace(str[i])) {
            in_word = 0;
        }
    }
    return count;
}
