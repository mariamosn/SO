all: build

build: so_stdio.o xfile.o
	gcc -Wall -shared so_stdio.o xfile.o -o libso_stdio.so

so_stdio.o: so_stdio.c
	gcc -Wall -fPIC -c so_stdio.c

xfile.o: xfile.c
	gcc -Wall -fPIC -c xfile.c

clean:
	rm -f *.o libso_stdio.so

copy:
	cp libso_stdio.so ../checker-lin/libso_stdio.so
