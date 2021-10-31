CC = gcc
.SUFFIXES: .c .o

all: oss user

oss: oss.o user.o process_table.o utils.o
	gcc -Wall -g -o oss oss.o process_table.o utils.o

user: user.o utils.o
	gcc -Wall -g -o user user.o utils.o

process_table: process_table.o utils.o
	gcc -Wall -g -o process_table process_table.o utils.o

utils: utils.o
	gcc -Wall -g -o utils utils.o

.c.o:
	$(CC) -g -c $<

clean:
	rm -f *.o oss user process_table utils