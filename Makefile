CC = gcc
CFLAGS = -ansi -Wall -std=c99 -g
LIBS = -ledit
DEPS = libs/mpc/mpc.c
INCLUDES = -I libs/mpc/

all: caballa

caballa: caballa.c
	$(CC) $(CFLAGS) -o caballa caballa.c $(DEPS) $(LIBS) $(INCLUDES)

clean:
	rm caballa
