CC = gcc
CFLAGS = -std=c99 -ansi
SRCS = main.c
OBJS = $(SRCS: .c = .o)
PROG = cfind

all: $(OBJS)
	$(CC) $(CFLAGS) $(SRCS) -o $(PROG)

run: all
	./$(PROG)

clean:
	rm *.o

