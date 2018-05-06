OBJS = myshell
CC = gcc
CFLAGS = -Wall

.PHONY : myshell
myshell : $(OBJS)
	$(CC) $(CFLAGS) myshell.c -o myshell
clean :
	rm myshell
