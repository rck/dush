CC = gcc
#CC = clang
CFLAGS = -std=c99 -pedantic -Wall -c 
LFLAGS =
 
all: dush
 
dush: dush.o
	$(CC) $(LFLAGS) -o $@ $^
 
%.o: %.c
	$(CC) $(CFLAGS) $^
 
clean:
	rm -f dush dush.o
