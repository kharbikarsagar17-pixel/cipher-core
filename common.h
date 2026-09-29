/*
 * Common Header for Cipher-Core
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Common macros */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define BIT_SET(var, pos)   ((var) |= (1ULL << (pos)))
#define BIT_CLEAR(var, pos) ((var) &= ~(1ULL << (pos)))
#define BIT_CHECK(var, pos) (!!((var) & (1ULL << (pos))))

/* Error codes */
#define SUCCESS 0
#define ERROR -1
#define ERROR_INVALID_PARAM -2
#define ERROR_OUT_OF_MEMORY -3
#define ERROR_NOT_FOUND -4
#define ERROR_TIMEOUT -5
#define ERROR_PERMISSION_DENIED -6

#endif /* COMMON_H */
