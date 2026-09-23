#include <stddef.h>
/* Freestanding aggregate copies emitted by GCC; no semihosting/libc dependency. */
void *memcpy(void *destination, const void *source, size_t size)
{
    unsigned char *d = destination;
    const unsigned char *s = source;
    size_t i;
    for (i = 0; i < size; i++) { d[i] = s[i]; }
    return destination;
}
void *memset(void *destination, int value, size_t size)
{
    unsigned char *d = destination;
    size_t i;
    for (i = 0; i < size; i++) { d[i] = (unsigned char)value; }
    return destination;
}
