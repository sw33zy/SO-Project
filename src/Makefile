CFLAGS= -W -Wall -Wextra -D_GNU_SOURCE
CC = gcc


all: argus argusd pipes
 
argus:
	$(CC) $(CFLAGS) -o argus argus.c

argusd:
	$(CC) $(CFLAGS) -o argusd argusd.c

pipes:
	mkfifo fifo
	mkfifo fifo2


runclient:
	./argus

runserver:
	./argusd

clean:
	-rm -f *.o *.txt *.pids argus argusd fifo fifo2	