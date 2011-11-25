CC = c99
CFLAGS = -Wall -I . -O2

# LIB = -lpthread

all: tiny

tiny: tiny.c rio.c rio.h
	$(CC) $(CFLAGS) -o tiny tiny.c rio.c $(LIB)

rio.o: rio.c rio.h
	$(CC) $(CFLAGS) -c rio.c

clean:
	rm -f *.o tiny *~
