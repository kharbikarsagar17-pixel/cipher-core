/*
 * Rho Module - Regular Expression Engine
 * Part of Cipher-Core Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define RHO_VERSION "1.0.0"

static void rho_init(void) {
    printf("Rho regular expression engine initialized v%s\n", RHO_VERSION);
}

/* Simple pattern matching simulation */
static bool rho_match(const char *pattern, const char *text) {
    int p_len = strlen(pattern);
    int t_len = strlen(text);
    
    if (p_len == 0) return true;
    if (t_len == 0) return false;
    
    /* Handle wildcard '*' */
    if (pattern[0] == '*') {
        for (int i = 0; i <= t_len; i++) {
            if (rho_match(pattern + 1, text + i)) {
                return true;
            }
        }
        return false;
    }
    
    /* Handle exact match or single char wildcard '?' */
    if (pattern[0] == text[0] || pattern[0] == '?') {
        return rho_match(pattern + 1, text + 1);
    }
    
    return false;
}
