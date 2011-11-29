CC = c99
CFLAGS = -Wall -I . -O2

# LIB = -lpthread

all: tiny

tiny: tiny.c
	$(CC) $(CFLAGS) -o tiny tiny.c $(LIB)

clean:
	rm -f *.o tiny *~
