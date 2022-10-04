CC = gcc
CFLAGS = -Wall -g -fPIC

build: libscheduler.so

libscheduler.so: so_scheduler.o linkedlist.o
	$(CC) -shared $^ -o $@

so_scheduler.o: so_scheduler.c
	$(CC) $(CFLAGS) -c $< -o $@ 

linkedlist.o: linkedlist.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o libscheduler.so