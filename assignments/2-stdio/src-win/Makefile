all: build

build: so_stdio.obj
	link /nologo /dll /out:so_stdio.dll /implib:so_stdio.lib so_stdio.obj

so_stdio.obj: so_stdio.c
	cl /W3 /D_CRT_SECURE_NO_DEPRECATE /c so_stdio.c

clean:
	del /Q /F so_stdio.lib so_stdio.dll so_stdio.obj
