CC = gcc
.SUFFIXES: .c .o

all: oss

oss: oss.o
	gcc -Wall -g -o oss oss.o

.c.o:
	$(CC) -g -c $<

clean:
	rm -f *.o oss