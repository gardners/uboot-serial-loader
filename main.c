#include <stdio.h>
#include <stdlib.h>

int main(int argc,char **argv)
{
  if (argc!=4) {
    fprintf(stderr,
	    "Gertrude - the U-Boot telephone for when normal network connections aren't possible.\n"
	    "(C) 2016 Paul Gardner-Stephen. GPLv3\n"
	    "usage: gertrude <serial port> <file> <loadaddress (hex)>\n");
    fprintf(stderr,"       e.g.: gertrude /dev/cu.usbserial openwrt-foo-factory.bin 1000000\n\n");
    exit(-1);
  }

  char *serialport=argv[1];
  char *file=argv[2];
  long long loadaddress=strtoll(argv[3],NULL,16);

  printf("Attempting to load '%s' @ 0x%llx\n",file,loadaddress);
  
}
