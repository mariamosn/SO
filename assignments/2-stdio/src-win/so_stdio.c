#define DLL_EXPORTS
#include "so_stdio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
/*
#include <sys/wait.h>
#include <errno.h>
*/
#define BUF_LEN 4096

#define READ 1
#define WRITE 2
#define APPEND 4

typedef struct _so_file {
	HANDLE fd;
	char buffer[BUF_LEN];
	int buf_index;
	int buf_size;
	int mod;
	int error;
	int last;
	int eof;
	// pid_t pid;

} SO_FILE;

int xwrite(HANDLE fd, const void *buf, int count)
{
	int bytes_written = 0;

	while (bytes_written < count) {
		//ssize_t bytes_written_now = write(fd, buf + bytes_written,
		//								  count - bytes_written);
		int bytes_written_now;

		WriteFile(fd,
					(char *)buf + bytes_written,
					count - bytes_written,
					&bytes_written_now,
					NULL);

		if (bytes_written_now <= 0) /* I/O error */
			return -1;

		bytes_written += bytes_written_now;
	}

	return bytes_written;
}

// CreateFile
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int mod;
	HANDLE fd = INVALID_HANDLE_VALUE;
	SO_FILE *result;

	if (strncmp(mode, "r+", 2) == 0) {
		// fd = open(pathname, O_RDWR);
		fd = CreateFile(pathname,
					GENERIC_READ|GENERIC_WRITE,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
		mod = READ | WRITE;

	} else if (strncmp(mode, "r", 1) == 0) {
		// fd = open(pathname, O_RDONLY);
		fd = CreateFile(pathname,
					GENERIC_READ|GENERIC_WRITE,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
		mod = READ;

	} else if (strncmp(mode, "w+", 2) == 0) {
		// fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
		fd = CreateFile(pathname,
					GENERIC_READ|GENERIC_WRITE,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					CREATE_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
		mod = READ | WRITE;

	} else if (strncmp(mode, "w", 1) == 0) {
		// fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		fd = CreateFile(pathname,
					GENERIC_READ|GENERIC_WRITE,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					CREATE_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
		mod = WRITE;

	} else if (strncmp(mode, "a+", 2) == 0) {
		// fd = open(pathname, O_RDWR | O_CREAT | O_APPEND, 0644);
		fd = CreateFile(pathname,
					GENERIC_READ|FILE_APPEND_DATA,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
		mod = APPEND | READ | WRITE;

	} else if (strncmp(mode, "a", 1) == 0) {
		// fd = open(pathname, O_WRONLY | O_CREAT | O_APPEND, 0644);
		fd = CreateFile(pathname,
					GENERIC_READ|FILE_APPEND_DATA,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
		mod = APPEND | WRITE;
	} else {
		return NULL;
	}

	if (fd == INVALID_HANDLE_VALUE)
		return NULL;

	result = malloc(sizeof(SO_FILE));
	if (result == NULL) {
		CloseHandle(fd);
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

int so_fclose(SO_FILE *stream)
{
	int res;

	res = so_fflush(stream);
	if (res == SO_EOF && stream->last == WRITE) {
		stream->error = SO_EOF;
		free(stream);
		return SO_EOF;
	}

	res = CloseHandle(stream->fd);
	if (res == 0) {
		stream->error = SO_EOF;
		free(stream);
		return SO_EOF;
	}

	free(stream);
	return 0;
}

HANDLE so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_fflush(SO_FILE *stream)
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

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int res;

	if (stream->last == READ) {
		stream->buf_index = 0;
		stream->buf_size = 0;
	} else if (stream->last == WRITE) {
		res = so_fflush(stream);
		if (res == SO_EOF) {
			stream->error = SO_EOF;
			return -1;
		}
	}

	// res = lseek(stream->fd, offset, whence);
	res = SetFilePointer(stream->fd,
						offset,
						NULL,
						whence);
	if (res == -1) {
		stream->error = errno;
		return -1;
	}

	return 0;
}

long so_ftell(SO_FILE *stream)
{
	long crt_off;

	// crt_off = lseek(stream->fd, 0, SEEK_CUR);
	crt_off = SetFilePointer(stream->fd,
						0,
						NULL,
						FILE_CURRENT);
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

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	long long i;
	int crt;

	for (i = 0; i < size * nmemb; i++) {
		crt = so_fgetc(stream);
		if (crt == SO_EOF) {
			if (!stream->eof) {
				stream->error = SO_EOF;
				return 0;
			}
			break;
		}
		((char *)ptr)[i] = (char)crt;
	}
	stream->last = READ;

	return i / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	long long i;
	int crt, ret;
	char c;

	for (i = 0; i < size * nmemb; i++) {
		c = ((char *)ptr)[i];
		crt = 0;
		crt = (int)c;
		ret = so_fputc(crt, stream);
		if (ret == SO_EOF) {
			stream->error = SO_EOF;
			return 0;
		}
	}

	stream->last = WRITE;
	return nmemb;
}

int so_fgetc(SO_FILE *stream)
{
	int res = 0, bytes_read = 0, ret;

	// verifică permisiunile
	if ((stream->mod & READ) == 0) {
		stream->error = SO_EOF;
		return SO_EOF;
	}

	// dacă nu mai sunt date disponibile în buffer, acesta este repopulat
	if (stream->buf_index >= stream->buf_size || stream->buf_index < 0) {
		// bytes_read = read(stream->fd, stream->buffer, BUF_LEN);
		ret = ReadFile(stream->fd,
								stream->buffer,
								BUF_LEN,
								&bytes_read,
								NULL);
		if (ret == 0) {
			stream->error = SO_EOF;
			return SO_EOF;
		} else if (bytes_read == 0) {
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

int so_fputc(int c, SO_FILE *stream)
{
	int bytes_written, ret;

	// verifică permisiunile
	if ((stream->mod & WRITE) == 0) {
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

	if (c == SO_EOF)
		return 0;
	return c;
}

int so_feof(SO_FILE *stream)
{
	return stream->eof;
}

int so_ferror(SO_FILE *stream)
{
	return stream->error;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *stream = NULL;
	/*
	pid_t pid;
	int status, filedes[2];

	if (pipe(filedes))
		return NULL;

	stream = (SO_FILE *)malloc(sizeof(SO_FILE));
	if (stream == NULL) {
		CloseHandle(filedes[0]);
		CloseHandle(filedes[1]);
		return NULL;
	}

	stream->buf_index = 0;
	stream->buf_size = 0;
	stream->error = 0;
	stream->last = 0;
	stream->eof = 0;

	if (strncmp("r", type, 1) == 0) {
		stream->mod = READ;
		stream->fd = filedes[0];
	} else if (strncmp("w", type, 1) == 0) {
		stream->mod = WRITE;
		stream->fd = filedes[1];
	} else {
		CloseHandle(filedes[0]);
		CloseHandle(filedes[1]);
		free(stream);
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		free(stream);
		CloseHandle(filedes[0]);
		CloseHandle(filedes[1]);
		return NULL;
	} else if (pid == 0) {
		if (stream->mod & READ) {
			dup2(filedes[1], 1);
			CloseHandle(filedes[0]);
		} else {
			dup2(filedes[0], 0);
			CloseHandle(filedes[1]);
		}

		if (execl("/bin/sh", "sh", "-c", command, NULL) == -1) {
			free(stream);
			if (stream->mod & READ)
				CloseHandle(filedes[1]);
			else
				CloseHandle(filedes[0]);
			return NULL;
		}
	} else {
		if (stream->mod & READ)
			CloseHandle(filedes[1]);
		else
			CloseHandle(filedes[0]);

		stream->pid = pid;
	}
	*/
	return stream;
}

int so_pclose(SO_FILE *stream)
{
	/*
	pid_t pid = stream->pid;
	int ret, status;

	if (so_fflush(stream) == SO_EOF && stream->last == WRITE) {
		stream->error = SO_EOF;
		CloseHandle(stream->fd);
		free(stream);
		return SO_EOF;
	}

	CloseHandle(stream->fd);
	free(stream);

	ret = waitpid(pid, &status, 0);
	if (ret == -1 || !WIFEXITED(status))
		return SO_EOF;
	*/
	return 0;
}
