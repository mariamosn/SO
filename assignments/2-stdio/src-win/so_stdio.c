/*
 * Maria Moșneag
 * 333CA
 */

#define DLL_EXPORTS
#include "so_stdio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#define BUF_LEN 4096

#define READ 1
#define WRITE 2

/*
 * tip de date care descrie un fișier
 */
typedef struct _so_file {
	/*
	 * handle-ul fișierului
	 */
	HANDLE fd;
	/*
	 * buffer-ul asociat structurii pentru a eficientiza numărul de
	 * scrieri și de citiri
	 */
	char buffer[BUF_LEN];
	/*
	 * indexul primei poziții libere din buffer
	 */
	int buf_index;
	/*
	 * cantitatea curentă de date valide disponibile
	 */
	int buf_size;
	/*
	 * flag-ul memorează modul în care a fost deschis fișierul
	 */
	int mod;
	/*
	 * flag-ul este
	 * 0 în cazul în care nu au existat erori în cadrul operațiilor
	 * efectuate anterior asupra acestui fișier și
	 * o valoare diferită de 0 altfel
	 */
	int error;
	/*
	 * flag-ul are valoarea READ dacă ultima operație efectuată asupra
	 * fișierului a fost una de citire și WRITE în cazul în care ultima
	 * operație a fost o scriere
	 */
	int last;
	/*
	 * flag-ul este 0 dacă încă nu s-a ajuns la finalul fișierului și
	 * o valoare diferită de 0 altfel
	 */
	int eof;
	PROCESS_INFORMATION pi;

} SO_FILE;


/**
 * Lab #2, Simple I/O operations
 * Task #3, Linux
 * Full length read/write
 *
 * Write exactly count bytes or die trying.
 *
 * Return values:
 *  < 0     - error.
 *  >= 0    - number of bytes read.
 */
int xwrite(HANDLE fd, const void *buf, int count)
{
	int bytes_written = 0;

	while (bytes_written < count) {
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

/*
 * funcție similară fopen din libc, deschide un fișier
 * @pathname	= calea către fișier
 * @mode	= modul în care va fi deschis fișierul
 * @return	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 *		  sau NULL în caz de eroare
 */
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int mod;
	HANDLE fd = INVALID_HANDLE_VALUE;
	SO_FILE *result;

	/*
	 * verifică modul în care va fi deschis fișierul
	 */
	if (strncmp(mode, "r+", 2) == 0) {
		fd = CreateFile(pathname,
				GENERIC_READ|GENERIC_WRITE,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		mod = READ | WRITE;

	} else if (strncmp(mode, "r", 1) == 0) {
		fd = CreateFile(pathname,
				GENERIC_READ|GENERIC_WRITE,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		mod = READ;

	} else if (strncmp(mode, "w+", 2) == 0) {
		fd = CreateFile(pathname,
				GENERIC_READ|GENERIC_WRITE,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		mod = READ | WRITE;

	} else if (strncmp(mode, "w", 1) == 0) {
		fd = CreateFile(pathname,
				GENERIC_READ|GENERIC_WRITE,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		mod = WRITE;

	} else if (strncmp(mode, "a+", 2) == 0) {
		fd = CreateFile(pathname,
				GENERIC_READ|FILE_APPEND_DATA,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		mod = READ | WRITE;

	} else if (strncmp(mode, "a", 1) == 0) {
		fd = CreateFile(pathname,
				GENERIC_READ|FILE_APPEND_DATA,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		mod = WRITE;
	} else {
		return NULL;
	}

	if (fd == INVALID_HANDLE_VALUE)
		return NULL;

	/*
	 * inițializează structura SO_FILE
	 */
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

/*
 * funcție similară fclose din libc, închide un fișier
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= 0 în caz de succes
 *		  sau SO_EOF în caz de eroare
 */
int so_fclose(SO_FILE *stream)
{
	int res;

	/*
	 * înainte de a închide fișierul, în cazul în care ultima operație
	 * efectuată a fost o scriere, se va face flush buffer-ului intern
	 */
	res = so_fflush(stream);
	if (res == SO_EOF && stream->last == WRITE) {
		stream->error = SO_EOF;
		free(stream);
		return SO_EOF;
	}

	/*
	 * se închide fișierul
	 */
	res = CloseHandle(stream->fd);
	if (res == 0) {
		stream->error = SO_EOF;
		free(stream);
		return SO_EOF;
	}

	/*
	 * se eliberează memoria asociată structurii SO_FILE
	 */
	free(stream);

	return 0;
}

/*
 * funcția întoarce handle-ul asociat unui fișier
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= handle-ul asociat fișierului
 */
HANDLE so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

/*
 * funcție similară fflush din libc, forțează scrierea datelor din buffer-ul
 * intern în fișier în cazul în care ultima operație a fost una de scriere
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= 0 în caz de succes
 *		  sau SO_EOF în caz de eroare
 */
int so_fflush(SO_FILE *stream)
{
	int bytes_written;

	/*
	 * verifică dacă ultima operație a fost o scriere
	 */
	if (stream->last != WRITE) {
		stream->error = SO_EOF;
		return SO_EOF;
	}

	/*
	 * scrie conținutul buffer-ului intern în fișier
	 */
	bytes_written = xwrite(stream->fd, stream->buffer, stream->buf_index);
	if (bytes_written == -1) {
		stream->error = SO_EOF;
		return SO_EOF;
	}

	/*
	 * invalidează datele din buffer
	 */
	stream->buf_index = 0;
	stream->buf_size = 0;

	return 0;
}

/*
 * funcție similară fseek din libc, mută cursorul fișierului
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @offset	= offset-ul cu care va fi mutat fișierul față de "reper"
 * @whence	= "reperul" față de care se mută cursorul
 * @return	= 0 în caz de succes
 *		  sau -1 în caz de eroare
 */
int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int res;

	/*
	 * dacă ultima operație a fost o citire, se invalidează datele din
	 * buffer
	 */
	if (stream->last == READ) {
		stream->buf_index = 0;
		stream->buf_size = 0;

	/*
	 * dacă ultima operație a fost o scriere, se face flush
	 */
	} else if (stream->last == WRITE) {
		res = so_fflush(stream);
		if (res == SO_EOF) {
			stream->error = SO_EOF;
			return -1;
		}
	}

	/*
	 * se mută cursorul la poziția corespunzătoare
	 */
	res = SetFilePointer(stream->fd, offset, NULL, whence);
	if (res == -1) {
		stream->error = errno;
		return -1;
	}

	return 0;
}

/*
 * funcție similară ftell din libc, întoarce poziția curentă
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= poziția curentă în cadrul fișierului (offset-ul față de
 *		  începutul fișierului)
 *		  sau -1 în caz de eroare
 */
long so_ftell(SO_FILE *stream)
{
	long crt_off;

	/*
	 * determină poziția curentă reală, aceasta depinzând însă de starea
	 * buffer-ului intern
	 */
	crt_off = SetFilePointer(stream->fd, 0, NULL, FILE_CURRENT);
	if (crt_off == -1) {
		stream->error = SO_EOF;
		return SO_EOF;
	}

	/*
	 * se determină poziția curentă, ținând cont de offset-ul
	 * din cadrul buffer-ului intern;
	 * dacă ultima operație a fost o scriere, înseamnă că în buffer
	 * se află caractere în plus, care nu au apucat încă să fie
	 * scrise în fișier;
	 * dacă ultima operație a fost o citire, înseamnă că în buffer
	 * se află mai multe caractere, care nu au fost încă citite
	 */
	if (stream->last == WRITE)
		crt_off += stream->buf_index;
	else if (stream->last == READ)
		crt_off -= (stream->buf_size - stream->buf_index);

	return crt_off;
}

/*
 * funcție similară fread din libc, citește mai multe elemente
 * @ptr		= un pointer către zona de memorie în care vor fi
 *		  stocate datele citite
 * @size	= dimensiunea unui element citit
 * @nmemb	= numărul de elemente citite
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= numărul de elemente citite în caz de succes
 *		  sau 0 în caz de eroare
 */
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	long long i;
	int crt;

	/*
	 * elementele sunt citite byte cu byte
	 */
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

	return (size_t)(i / size);
}

/*
 * funcție similară fwrite din libc, scrie mai multe elemente
 * @ptr		= un pointer către zona de memorie de unde vor fi
 *		  luate datele scrise
 * @size	= dimensiunea unui element citit
 * @nmemb	= numărul de elemente citite
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= numărul de elemente scrise în caz de succes
 *		  sau 0 în caz de eroare
 */
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	long long i;
	int crt, ret;
	unsigned char c;

	/*
	 * elementele sunt scrise byte cu byte
	 */
	for (i = 0; i < size * nmemb; i++) {
		c = ((unsigned char *)ptr)[i];
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

/*
 * funcție similară fgetc din libc, citește un byte din fișier
 * @stream	= un pointer către o structură de tip SO_FILE care
 *		  conține datele fișierului
 * @return	= caracterul citit, extins la int în caz de succes
 *		  sau SO_EOF în caz de eroare
 */
int so_fgetc(SO_FILE *stream)
{
	int res = 0, bytes_read = 0, ret;

	/*
	 * verifică modul în care a fost deschis fișierul
	 */
	if ((stream->mod & READ) == 0) {
		stream->error = SO_EOF;
		return SO_EOF;
	}

	/*
	 * dacă nu mai sunt date disponibile în buffer,
	 * acesta este repopulat
	 */
	if (stream->buf_index >= stream->buf_size ||
		stream->buf_index < 0) {
		ret = ReadFile(stream->fd, stream->buffer, BUF_LEN,
				&bytes_read, NULL);
		if (ret == 0) {
			stream->error = SO_EOF;
			return SO_EOF;
		} else if (bytes_read == 0) {
			/*
			 * dacă am ajuns la finalul fișierului, se setează
			 * flag-ul corespunzător din SO_FILE
			 */
			stream->eof = 1;
			return SO_EOF;
		}

		stream->buf_index = 0;
		stream->buf_size = bytes_read;
	}

	/*
	 * se determină caracterul citit și se actualizează buffer-ul
	 * intern în mod corespunzător
	 */
	res = (int)((unsigned char) stream->buffer[stream->buf_index]);
	stream->buf_index++;
	stream->last = READ;

	return res;
}

/*
 * funcție similară fputc din libc, scrie un element
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= caracterul scris extins la int în caz de succes
 *		  sau SO_EOF în caz de eroare
 */
int so_fputc(int c, SO_FILE *stream)
{
	int ret;

	/*
	 * verifică permisiunile
	 */
	if ((stream->mod & WRITE) == 0) {
		stream->error = SO_EOF;
		return SO_EOF;
	}

	/*
	 * dacă buffer-ul este plin, se face flush
	 */
	if (stream->buf_index == BUF_LEN || stream->buf_index < 0) {
		ret = so_fflush(stream);
		if (ret == SO_EOF && stream->last == WRITE) {
			stream->error = SO_EOF;
			return SO_EOF;
		}
	}

	/*
	 * se introduce caracterul în buffer-ul intern
	 */
	stream->buffer[stream->buf_index] = (char)c;
	stream->buf_index++;
	stream->last = WRITE;

	return c;
}

/*
 * funcție similară feof din libc,
 * verifică dacă s-a ajuns la finalul fișierului
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= 0 dacă nu s-a ajuns la finalul fișierului
 *		  sau o valoare nenulă altfel
 */
int so_feof(SO_FILE *stream)
{
	return stream->eof;
}

/*
 * funcție similară ferror din libc, verifică dacă s-a întâlnit
 * vreo eroare în urma unei operații efectuate cu fișierul
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= 0 dacă nu s-a întâlnit vreo eroare
 *		  sau o valoare nenulă altfel
 */
int so_ferror(SO_FILE *stream)
{
	return stream->error;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *stream = NULL;

	return stream;
}

int so_pclose(SO_FILE *stream)
{
	return 0;
}
