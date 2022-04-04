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

#ifdef DLL_IMPORTS
#define DLL_DECLSPEC __declspec(dllimport)
#else
#define DLL_DECLSPEC __declspec(dllexport)
#endif

// #include <unistd.h>
#include <windows.h>

/**
 * Read exactly count bytes or die trying.
 *
 * Return values:
 *  < 0     - error.
 *  >= 0    - number of bytes read.
 */
// ssize_t xread(int fd, void *buf, size_t count);

/**
 * Write exactly count bytes or die trying.
 *
 * Return values:
 *  < 0     - error.
 *  >= 0    - number of bytes read.
 */
DLL_DECLSPEC ssize_t xwrite(HANDLE fd, const void *buf, size_t count);

#endif
