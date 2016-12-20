#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <strings.h>

#include "gertrude.h"

int serialfd=-1;

int main(int argc,char **argv)
{
  if (argc!=3) {
    fprintf(stderr,
	    "Gertrude - the U-Boot telephone for when normal network connections aren't possible.\n"
	    "(C) 2016 Paul Gardner-Stephen. GPLv3\n"
	    "usage: gertrude <serial port> <valid uImage file> \n");
    fprintf(stderr,"       e.g.: gertrude /dev/cu.usbserial openwrt-foo-uImage.bin\n\n");
    exit(-1);
  }

  char *serial_port=argv[1];
  char *file=argv[2];
  
  serialfd = open(serial_port,O_RDWR);
  if (serialfd<0) {
    perror("Opening serial port in main");
    exit(-1);
  }
  if (serial_setup_port(serialfd))
    {
      fprintf(stderr,"Failed to setup serial port. Exiting.\n");
      exit(-1);
    }
  fprintf(stderr,"Serial port open as fd %d\n",serialfd);

  int fd=open(file,O_RDONLY);
  if (fd<0) {
    fprintf(stderr,"Cannot read from '%s'\n",file);
  }
  struct stat stat;
  if (fstat(fd, &stat) == -1) {
    perror("stat");
    exit(-1);
  }
  unsigned char *data = mmap(NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (data==MAP_FAILED) {
    fprintf(stderr,"Could not mmap() file.\n");
    perror("mmap");
    exit(-1);
  }

  // Check uImage magic number
  unsigned char uImageMagic[4]={0x27,0x05,0x19,0x56};
  if (bcmp(uImageMagic,data,4)) {
    fprintf(stderr,"Bad magic number: You should only load valid uImage files.\n");
    exit(-1);
  }

  unsigned long long loadaddress=(data[16]<<24LL)+(data[17]<<16)+(data[18]<<8)+(data[19]<<0);
  loadaddress&=0xffffffff;
  unsigned long long imagebytes=(data[12]<<24LL)+(data[13]<<16)+(data[14]<<8)+(data[15]<<0);
  imagebytes&=0xffffffff;

  unsigned long long entryaddress=(data[20]<<24LL)+(data[21]<<16)+(data[22]<<8)+(data[23]<<0);
  entryaddress&=0xffffffff;
  
  printf("Attempting to load '%s' @ 0x%llx (length = 0x%llx, entry point = 0x%llx)\n",
	 file,loadaddress,imagebytes,entryaddress);
  
}
