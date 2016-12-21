#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <strings.h>
#include <unistd.h>

#include "gertrude.h"

int serialfd=-1;

int compare_word(unsigned int offset,unsigned int w,unsigned char *data)
{
  int r=0;
  unsigned char w0=(w>>0)&0xff;
  unsigned char w1=(w>>8)&0xff;
  unsigned char w2=(w>>16)&0xff;
  unsigned char w3=(w>>24)&0xff;
  if (data[offset+0]!=w0) {
    printf("0x%08x : Saw %02x instead of %02x\n",offset+0,w0,data[offset+0]);
    r++;
  }
  if (data[offset+1]!=w1) {
    printf("0x%08x : Saw %02x instead of %02x\n",offset+1,w1,data[offset+1]);
    r++;
  }
  if (data[offset+2]!=w2) {
    printf("0x%08x : Saw %02x instead of %02x\n",offset+2,w2,data[offset+2]);
    r++;
  }
  if (data[offset+3]!=w3) {
    printf("0x%08x : Saw %02x instead of %02x\n",offset+3,w3,data[offset+3]);
    r++;
  }
  return r;
}

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

  for(int offset=0;offset<stat.st_size;offset+=1024) {

    // How many bytes in current block
    int count=1024;
    if (count>(stat.st_size-offset)) count=stat.st_size-offset;

    int errors=1;

    while(errors) {
      // Write data
      char cmd[1024];

      // Read back and verify
      snprintf(cmd,1024,"md.l %x 256\r\n",offset);
      write_all(fd,cmd,strlen(cmd));
      
      if (!errors) break;

      printf("Writing at 0x%08x\n",offset);      
      
      // Start memory write with advance
      snprintf(cmd,1024,"mw.l %x\r\n",offset);
      write_all(fd,cmd,strlen(cmd));

      unsigned char buf[1024];
      int r;
      unsigned long long expected_offset=0x1000000+offset;
      unsigned long long last_expected_offset=offset+1024-16;
      char mdline[1024];
      int mdlinelen=0;
      unsigned int addr,w1,w2,w3,w4;
      while(expected_offset<=last_expected_offset) {
	r=read_nonblock(fd,buf,1024);
	buf[r]=0;
	for(int k=0;k<r;k++)
	  if (buf[k]=='\n') {
	    // check line
	    mdline[mdlinelen]=0;
	    printf("saw '%s'\n",mdline);
	    if (sscanf(mdline,"%x: %x %x %x %x",
		       &addr,&w1,&w2,&w3,&w4)==5) {
	      printf("Valid line: %08x : %08x %08x %08x %08x\n",
		     addr,w1,w2,w3,w4);
	      errors+=compare_word(addr-0x1000000,w1,data);
	      errors+=compare_word(addr-0x1000000+4,w2,data);
	      errors+=compare_word(addr-0x1000000+8,w3,data);
	      errors+=compare_word(addr-0x1000000+12,w4,data);
	      if (addr==expected_offset) expected_offset+=16;
	    }
	    mdlinelen=0;
	  } else {
	    if (mdlinelen<1024) mdline[mdlinelen++]=buf[k];
	  }
	if (r<1) usleep(10000);
      }

      

      int question_marks_expected=0;
      int question_marks_received=0;
      
      for(int j=0;j<count;j++)
	{

	  question_marks_expected++;

	  // Read from serial port until we get a ? mark
	  unsigned char buf[1024];
	  int r=read_nonblock(fd,buf,1024);
	  buf[r]=0;
	  for(int k=0;k<r;k++) if (buf[k]=='?') question_marks_received++;
	  while (question_marks_expected>question_marks_received) {
	    printf("  Waiting for serial port to catch up...\n");
	    r=read_nonblock(fd,buf,1024);
	    buf[r]=0;
	    for(int k=0;k<r;k++) if (buf[k]=='?') question_marks_received++;
	    if (r<1) usleep(10000);
	  }

	  // Write bytes
	  snprintf(cmd,1024,"%02x%02x%02x%02x\n\r",
		   data[offset+j+0],
		   data[offset+j+1],
		   data[offset+j+2],
		   data[offset+j+3]);
	  write_all(fd,cmd,strlen(cmd));
	}
      
    }
  }
  
  
}
