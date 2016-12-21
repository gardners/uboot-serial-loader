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
  unsigned char w3=(w>>0)&0xff;
  unsigned char w2=(w>>8)&0xff;
  unsigned char w1=(w>>16)&0xff;
  unsigned char w0=(w>>24)&0xff;

  //  printf("compare offset = %x\n",offset);
  
  if (data[offset+0]!=w0) {
    //    printf("0x%08x : Saw %02x instead of %02x\n",offset+0,w0,data[offset+0]);
    r++;
  }
  if (data[offset+1]!=w1) {
    //    printf("0x%08x : Saw %02x instead of %02x\n",offset+1,w1,data[offset+1]);
    r++;
  }
  if (data[offset+2]!=w2) {
    //    printf("0x%08x : Saw %02x instead of %02x\n",offset+2,w2,data[offset+2]);
    r++;
  }
  if (data[offset+3]!=w3) {
    //    printf("0x%08x : Saw %02x instead of %02x\n",offset+3,w3,data[offset+3]);
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
  close(fd);

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

      printf("Verifying at 0x%08x\n",offset);      
      errors=0;
      
      // Read back and verify
      snprintf(cmd,1024," \nmd.l %x 100\r\n",0x1000000+offset);
      write_all(serialfd,cmd,strlen(cmd));
            
      unsigned char buf[1024];
      int r;
      unsigned long long expected_offset=0x1000000+offset;
      unsigned long long last_expected_offset=offset+0x1000000+1024-16;
      char mdline[1024];
      int mdlinelen=0;
      unsigned int addr,w1,w2,w3,w4;
      if (0) printf("(1) expected_offset=%llx, last_expected_offset=%llx\n",
	     expected_offset,last_expected_offset);
      while(expected_offset<=last_expected_offset) {
	if (0) printf("(2) expected_offset=%llx, last_expected_offset=%llx\n",
	       expected_offset,last_expected_offset);

	r=read_nonblock(serialfd,buf,1024);
	buf[r]=0;
	for(int k=0;k<r;k++)
	  if (buf[k]=='\n') {
	    // check line
	    mdline[mdlinelen]=0;
	    // printf("saw '%s'\n",mdline);
	    if (sscanf(mdline,"%x: %x %x %x %x",
		       &addr,&w1,&w2,&w3,&w4)==5) {
	      if (addr==expected_offset) {
		errors+=compare_word(addr-0x1000000,w1,data);
		errors+=compare_word(addr-0x1000000+4,w2,data);
		errors+=compare_word(addr-0x1000000+8,w3,data);
		errors+=compare_word(addr-0x1000000+12,w4,data);
		expected_offset+=16;		
	      } else {
		printf("Invalid line: %08x : %08x %08x %08x %08x\n",
		       addr,w1,w2,w3,w4);
		errors+=4;
	      }
	    }
	    mdlinelen=0;
	  } else {
	    if (mdlinelen<1024) mdline[mdlinelen++]=buf[k];
	  }
	if (r<1) usleep(10000);
      }

      printf("Writing %d bytes at 0x%08x\n",count,offset);      

      // Start memory write with advance
      snprintf(cmd,1024,"mm.l %x\n",offset+0x1000000);
      write_all(serialfd,cmd,strlen(cmd));

      int question_marks_expected=1;
      int question_marks_received=0;
      
      for(int j=0;j<count;j+=4)
	{
	  // Read from serial port until we get a ? mark

	  if (0) fprintf(stderr,"?1 : e=%d, rx=%d\n",
		 question_marks_expected,question_marks_received);
	  
	  unsigned char buf[1024];
	  int r=read_nonblock(serialfd,buf,1024);
	  buf[r]=0;
	  for(int k=0;k<r;k++) if (buf[k]=='?') question_marks_received++;
	  while (question_marks_expected>question_marks_received) {
	    if (0) printf("  Waiting for serial port to catch up...\n");
	    r=read_nonblock(serialfd,buf,1024);
	    buf[r]=0;
	    for(int k=0;k<r;k++) if (buf[k]=='?') {
		question_marks_received++;
		if (0) fprintf(stderr,"?2 : e=%d, rx=%d\n",
			question_marks_expected,question_marks_received);
	      }
	    if (r<1) usleep(10000);
	  }

	  if (0) fprintf(stderr,"?3 : e=%d, rx=%d\n",
		 question_marks_expected,question_marks_received);

	  // Write bytes
	  snprintf(cmd,1024,"%02x%02x%02x%02x\n",
		   data[offset+j+0],
		   data[offset+j+1],
		   data[offset+j+2],
		   data[offset+j+3]);
	  write_all(serialfd,cmd,strlen(cmd));
	  question_marks_expected++;
	}
      write_all(serialfd,".\n",2);
      r=read_nonblock(serialfd,buf,1024);
    }
  }
  
  
}
