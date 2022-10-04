CFLAGS=/nologo /W4 /EHsc /Za /MD

build: so-cpp.exe

so-cpp.exe: tema1.obj hashmap.obj
	$(CPP) $(CFLAGS) /Fe$@ $**

tema1.obj: tema1.c

hashmap.obj: hashmap.c hashmap.h

clean:
	del /f /q *.obj *.exe

