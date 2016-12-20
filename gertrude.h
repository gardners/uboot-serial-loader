int set_nonblock(int fd);
int set_block(int fd);
ssize_t read_nonblock(int fd, void *buf, size_t len);
ssize_t write_all(int fd, const void *buf, size_t len);
int serial_setup_port_with_speed(int fd,int speed);
int serial_setup_port(int fd);

  
