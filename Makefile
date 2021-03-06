CC = gcc
.SUFFIXES: .c .o .h

all: oss user

oss: oss.o user.o process_table.o utils.o queue.o circular_queue.o config.h
	gcc -Wall -g -o oss oss.o process_table.o utils.o queue.o circular_queue.o

user: user.o utils.o queue.o process_table.o
	gcc -Wall -g -o user user.o utils.o queue.o process_table.o

process_table: process_table.o utils.o
	gcc -Wall -g -o process_table process_table.o utils.o

utils: utils.o
	gcc -Wall -g -o utils utils.o

queue: queue.o
	gcc -Wall -g -o queue queue.o

circular_queue: circular_queue.o
	gcc -Wall -g -o circular_queue circular_queue.o

.c.o:
	$(CC) -g -c $<

clean:
	rm -f *.o oss user process_table utils queue circular_queue