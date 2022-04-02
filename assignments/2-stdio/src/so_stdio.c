#include "so_stdio.h"
#include "xfile.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
 
#include <sys/types.h>  /* open */
#include <sys/stat.h>	/* open */
#include <fcntl.h>      /* O_CREAT, O_RDONLY */
#include <unistd.h>     /* close, lseek, read, write */
#include <errno.h>

#define BUF_LEN 4096

#define READ 1
#define WRITE 2
#define APPEND 4

typedef struct _so_file {
    int fd;
    char buffer[BUF_LEN];
    int buf_index;
    int buf_size;
    int mod;
    int error;
    int last;
    int eof;

} SO_FILE;

FUNC_DECL_PREFIX SO_FILE *so_fopen(const char *pathname, const char *mode)
{
    int fd = -1, mod;
    SO_FILE *result;

    if (strncmp(mode, "r+", 2) == 0) {
        fd = open(pathname, O_RDWR);
        mod = READ | WRITE;

    } else if (strncmp(mode, "r", 1) == 0) {
        fd = open(pathname, O_RDONLY);
        mod = READ;

    } else if (strncmp(mode, "w+", 2) == 0) {
        fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
        mod = READ | WRITE;

    } else if (strncmp(mode, "w", 1) == 0) {
        fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        mod = WRITE;

    } else if (strncmp(mode, "a+", 2) == 0) {
        fd = open(pathname, O_RDWR | O_CREAT | O_APPEND, 0644);
        mod = APPEND | READ;

    } else if (strncmp(mode, "a", 1) == 0) {
        fd = open(pathname, O_WRONLY | O_CREAT | O_APPEND, 0644);
        mod = APPEND;
    }

    if (fd < 0)
        return NULL;

    result = malloc(sizeof(SO_FILE));
    if (result == NULL) {
        close(fd);
        return NULL;
    }

    result->fd = fd;
    result->buf_index = 0;
    result->buf_size = 0;
    result->mod = mod;
    result->error = 0;
    result->last = 0;
    result->eof = 0;

    return result;
}

FUNC_DECL_PREFIX int so_fclose(SO_FILE *stream)
{
    int res;

    res = so_fflush(stream);
    if (res == SO_EOF && stream->last == WRITE) {
        stream->error = SO_EOF;
        free(stream);
        return SO_EOF;
    }

    res = close(stream->fd);
    if (res == -1) {
        stream->error = SO_EOF;
        free(stream);
        return SO_EOF;
    }

    free(stream);
    return 0;
}

FUNC_DECL_PREFIX int so_fileno(SO_FILE *stream)
{
    return stream->fd;
}

FUNC_DECL_PREFIX int so_fflush(SO_FILE *stream)
{
    int bytes_written;

    if (stream->last != WRITE) {
        stream->error = SO_EOF;
        return SO_EOF;
    }

    bytes_written = xwrite(stream->fd, stream->buffer, stream->buf_index);
    if (bytes_written == -1) {
        stream->error = SO_EOF;
        return SO_EOF;
    }
    stream->buf_index = 0;
    stream->buf_size = 0;

    return 0;
}

FUNC_DECL_PREFIX int so_fseek(SO_FILE *stream, long offset, int whence)
{
    int res;

    if (stream->last == READ) {
        stream->buf_index = 0;
        stream->buf_size = 0;
    } else if (stream->last == WRITE) {
        so_fflush(stream);
    }

    res = lseek(stream->fd, offset, whence);
    if (res == -1) {
        stream->error = errno;
        return -1;
    }

    return 0;
}

FUNC_DECL_PREFIX long so_ftell(SO_FILE *stream)
{
    long crt_off;

    crt_off = lseek(stream->fd, 0, SEEK_CUR);
    if (crt_off == -1) {
        stream->error = SO_EOF;
        return SO_EOF;
    }

    if (stream->last == WRITE)
        crt_off += stream->buf_index;
    else if (stream->last == READ)
        crt_off -= (stream->buf_size - stream->buf_index);

    return crt_off;
}

FUNC_DECL_PREFIX
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    long long i;
    int crt;

    for (i = 0; i < size * nmemb; i++) {
        crt = so_fgetc(stream);
        if (crt == SO_EOF) {
            stream->error = SO_EOF;
            return 0;
        }
        ((char *)ptr)[i] = (char)crt;
    }
    stream->last = READ;

    return nmemb;
}

FUNC_DECL_PREFIX
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    long long i;
    int crt;
    char c;

    for (i = 0; i < size * nmemb; i++) {
        c = ((char *)ptr)[i];
        crt = 0;
        crt = (int)c;
        crt = so_fputc(crt, stream);
        if (crt == SO_EOF) {
            stream->error = SO_EOF;
            return 0;
        }
    }

    stream->last = WRITE;
    return nmemb;
}

FUNC_DECL_PREFIX int so_fgetc(SO_FILE *stream)
{
    int res = 0, bytes_read;

    // verifică permisiunile
    if (stream->mod & READ == 0) {
        stream->error = SO_EOF;
        return SO_EOF;
    }

    // dacă nu mai sunt date disponibile în buffer, acesta este repopulat
    if (stream->buf_index >= stream->buf_size || stream->buf_index < 0) {
        bytes_read = read(stream->fd, stream->buffer, BUF_LEN);
        if (bytes_read <= 0) {
            stream->error = SO_EOF;
            stream->eof = 1;
            return SO_EOF;
        }
        stream->buf_index = 0;
        stream->buf_size = bytes_read;
    }

    res = (int)((unsigned char) stream->buffer[stream->buf_index]);
    stream->buf_index++;
    stream->last = READ;

    return res;
}

FUNC_DECL_PREFIX int so_fputc(int c, SO_FILE *stream)
{
    int bytes_written, ret;

    // verifică permisiunile
    if (stream->mod & WRITE == 0) {
        stream->error = SO_EOF;
        return SO_EOF;
    }

    // dacă buffer-ul e plin
    if (stream->buf_index == BUF_LEN || stream->buf_index < 0) {
        ret = so_fflush(stream);
        if (ret == SO_EOF && stream->last == WRITE) {
            stream->error = SO_EOF;
            return SO_EOF;
        }
    }

    stream->buffer[stream->buf_index] = (char)c;
    stream->buf_index++;
    stream->last = WRITE;

    return c;
}

FUNC_DECL_PREFIX int so_feof(SO_FILE *stream)
{
    return stream->eof;
}

FUNC_DECL_PREFIX int so_ferror(SO_FILE *stream)
{
    return stream->error;
}

FUNC_DECL_PREFIX SO_FILE *so_popen(const char *command, const char *type)
{
    return NULL;
}

FUNC_DECL_PREFIX int so_pclose(SO_FILE *stream)
{
    return -1;
}
