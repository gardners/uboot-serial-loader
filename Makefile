all:	gertrude

gertrude:	main.c serial.c gertrude.h
	$(CC) -o gertrude main.c serial.c
