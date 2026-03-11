#ifndef KERNEL_STDLIB_H
#define KERNEL_STDLIB_H

#include "stddef.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 2147483647

int abs(int x);
double strtod(const char* nptr, char** endptr);
void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void abort(void);

#endif
