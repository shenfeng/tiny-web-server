CC = c99
CFLAGS = -Wall -I . -O2

# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
# LIB = -lpthread

all: tiny

tiny: tiny.c lib.c lib.h rio.c rio.h
	$(CC) $(CFLAGS) -o tiny tiny.c rio.c lib.c $(LIB)

lib: lib.c lib.h rio.o
	$(CC) $(CFLAGS) -c lib.c

rio.o: rio.c rio.h
	$(CC) $(CFLAGS) -c rio.c

clean:
	rm -f *.o tiny *~
