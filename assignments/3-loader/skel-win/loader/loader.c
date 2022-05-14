/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <Windows.h>

#define DLL_EXPORTS
#include "loader.h"
#include "exec_parser.h"

#define MAPPED 1
#define NOT_MAPPED 0

static so_exec_t *exec;
static struct sigaction old_sa;
HANDLE fd;
PVOID handler;

int in_segm_check(char *address)
{
	int i;

	for (i = 0; i < exec->segments_no; i++) {
		char *segm_start, *segm_end;

		segm_start = (char *)exec->segments[i].vaddr;
		segm_end = segm_start + exec->segments[i].mem_size;

		if (address >= segm_start && address < segm_end)
			return i;
	}

	return -1;
}

void init_aux_data(int *page_sz, int *file_sz, int *mem_sz,
			int *segm_start_addr, int *segm_perm,
			int *page_start, int *page_end,
			int *file_off, char *address, int segm_no,
			int *page_no_in_segm)
{
	int segm_off;
	SYSTEM_INFO si;

    GetSystemInfo(&si);
	*page_sz = si.dwPageSize;

	*file_sz = exec->segments[segm_no].file_size;
	*mem_sz = exec->segments[segm_no].mem_size;
	*segm_start_addr = exec->segments[segm_no].vaddr;
	segm_off = exec->segments[segm_no].offset;
	*segm_perm = exec->segments[segm_no].perm;

	*page_no_in_segm = ((int)address - *segm_start_addr) / *page_sz;
	*page_start = *page_no_in_segm * *page_sz;
	*page_end = (*page_no_in_segm + 1) * *page_sz;
	*file_off = segm_off + *page_start;
}

LONG CALLBACK VectoredHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
	int ret, file_sz, mem_sz;
	int segm_no, segm_start_addr, segm_perm, file_off;
	int page_sz, page_start, page_end, page_no_in_segm;
	char *address;
	LPVOID mmap_res;

	if (ExceptionInfo.ExceptionRecord.ExceptionCode !=
			EXCEPTION_ACCESS_VIOLATION &&
		ExceptionInfo.ExceptionRecord.ExceptionCode !=
			EXCEPTION_DATATYPE_MISALIGNMENT)
		return EXCEPTION_CONTINUE_SEARCH;

	address = ExceptionInfo.ExceptionRecord.ExceptionInformation;

	/*
	 * Dacă page fault-ul nu este într-un segment cunoscut,
	 * se rulează handler-ul default.
	 */
	ret = in_segm_check(address);
	if (ret == -1)
		return EXCEPTION_CONTINUE_SEARCH;
	segm_no = ret;

	init_aux_data(&page_sz, &file_sz, &mem_sz, &segm_start_addr,
			&segm_perm, &page_start, &page_end, &file_off,
			address, segm_no, &page_no_in_segm);

	/*
	 * Dacă page fault-ul este generat într-o pagină deja
	 * mapată (segmentul nu are permisiunile necesare),
	 * se rulează handler-ul default.
	 */
	if (((char *)(exec->segments[segm_no].data))[page_no_in_segm] == MAPPED)
		return EXCEPTION_CONTINUE_SEARCH;

	/*
	 * Pagina este mapată la adresa aferentă.
	 */
	if (page_start < file_sz) {
		/*
		mmap_res VirtualAlloc(
			(LPVOID)segm_start_addr + page_start,
			(SIZE_T) page_sz,
			MEM_COMMIT | MEM_RESERVE,
			segm_perm
			);
		*/
		hFileMap = CreateFileMapping(
				fd,
				NULL,
				// TODO
				PAGE_READWRITE,
				0,
				page_sz,
				NULL);
		DIE(hFileMap == NULL, "CreateFileMapping");
		/*
		mmap_res = mmap((LPVOID)segm_start_addr + page_start, page_sz,
				segm_perm, MAP_PRIVATE | MAP_FIXED,
				fd, file_off);
		*/
		DIE(mmap_res == NULL, "mmap error");

		if (page_end >= file_sz && page_end < mem_sz)
			memset((void *)segm_start_addr + file_sz, 0,
					page_end - file_sz);
		else if (page_end >= file_sz)
			memset((void *)segm_start_addr + file_sz, 0,
					mem_sz - file_sz);
	} else {
		mmap_res VirtualAlloc(
			(LPVOID)segm_start_addr + page_start,
			(SIZE_T) page_sz,
			MEM_COMMIT | MEM_RESERVE,
			segm_perm
			);
		/*
		mmap_res = mmap((void *)segm_start_addr + page_start, page_sz,
				segm_perm,
				MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
		*/
		DIE(mmap_res == MAP_FAILED, "mmap error");
	}

	((char *)(exec->segments[segm_no].data))[page_no_in_segm] = MAPPED;

}

void allocate_aux_data()
{
	int i, page_sz;
	SYSTEM_INFO si;

    GetSystemInfo(&si);
	page_sz = si.dwPageSize;

	for (i = 0; i < exec->segments_no; i++) {
		if (exec->segments[i].mem_size % page_sz > 0)
			exec->segments[i].data =
				calloc(exec->segments[i].mem_size / page_sz + 1,
					sizeof(char));
		else
			exec->segments[i].data =
				calloc(exec->segments[i].mem_size / page_sz,
					sizeof(char));
		DIE(exec->segments[i].data == NULL, "calloc error");
	}
}

// TODO
int so_init_loader(void)
{
	/* initialize on-demand loader */

	handler = AddVectoredExceptionHandler(CALL_FIRST, VectoredHandler);
	DIE(handler == NULL, "AddVectoredExceptionHandler error");

	allocate_aux_data();

	return 0;
}

void free_exec()
{
	int i, j, ret;
	int pages_in_segm, page_sz, mem_sz, segm_start_addr, page_start;
	SYSTEM_INFO si;

    GetSystemInfo(&si);
	page_sz = si.dwPageSize;


	for (i = 0; i < exec->segments_no; i++) {
		mem_sz = exec->segments[i].mem_size;
		pages_in_segm = mem_sz / page_sz +
				(int)(mem_sz % page_sz != 0);

		for (j = 0; j < pages_in_segm; j++) {
			if (((char *)exec->segments[i].data)[j] == MAPPED) {
				segm_start_addr = exec->segments[i].vaddr;
				page_start = j * page_sz;
				ret = munmap((void *)segm_start_addr +
						page_start, page_sz);
				DIE(ret == -1, "munmap error");
			}

		}

		free(exec->segments[i].data);
	}

	free(exec->segments);
	free(exec);
	RemoveVectoredExceptionHandler(handler);
}


int so_execute(char *path, char *argv[])
{
	DWORD ret;

	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	fd = CreateFile(
			path,
			FILE_READ_DATA,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
	// fd = open(path, O_RDONLY, 0644);
	DIE(fd == INVALID_HANDLE_VALUE, "opening file error");

	so_start_exec(exec, argv);

	free_exec();

	rc = CloseHandle(fd);
	DIE(rc == FALSE, "CloseHandle");

	return -1;
}
