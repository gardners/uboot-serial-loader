all:	gertrude

gertrude:	main.c serial.c gertrude.h Makefile
	$(CC) -g -o gertrude main.c serial.c
