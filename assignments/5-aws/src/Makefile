CFLAGS = /nologo /W4

build: aws.exe

aws.exe: aws.obj sock_util.obj http_parser.obj
	cl $(CFLAGS) /Fe$@ $**

aws.obj: aws.c
	cl $(CFLAGS) /Fo$@ /c $^

sock_util.obj: sock_util.c
	cl $(CFLAGS) /Fo$@ /c $^

http_parser.obj: http_parser.c
	cl $(CFLAGS) /Fo$@ /c $^

clean: del *.obj aws.exe