/**
 * SO
 * Lab #2, Simple I/O operations
 *
 * Task #3, Linux
 *
 * Full length read/write
 */

#ifndef XFILE_UTIL_H
#define XFILE_UTIL_H

#include <unistd.h>

/**
 * Write exactly count bytes or die trying.
 *
 * Return values:
 *  < 0     - error.
 *  >= 0    - number of bytes read.
 */
ssize_t xwrite(int fd, const void *buf, size_t count);

#endif
