
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include <poll.h>
#include <sys/wait.h>
#include "zlib.h"

//global variables store state for exit function
struct termios orig_termios;

//wrapper function to reset terminal and exit process
void Exit(int n){
  if (tcsetattr(0, TCSANOW, &orig_termios) != 0) {
    fprintf(stderr, "System call tcsetattr failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }

  exit(n);
}

//wrapper functions to provide error checking for system calls
int Recv(int sockfd, void * buf, int len){
  int r = 0;
  if((r = recv(sockfd, buf, len, 0)) == -1){
    fprintf(stderr, "System call recv failed. Error value is: %s\n", strerror(errno));
    Exit(1);
  }
  return r;
}

void Send(int sockfd, void * buf, int len){
  if(send(sockfd, buf, len, 0) == -1){
    fprintf(stderr, "System call send failed. Error value is: %s\n", strerror(errno));
    Exit(1);
  }
}

void Close(int filen){
  if (close(filen) == -1){
    fprintf(stderr, "System call close failed. Error value is: %s\n", strerror(errno));
    Exit(1);
  }
}

int Read(int filen, char* buff, int n){
  int r = 0;
  if ((r = read(filen, buff, n)) == -1){
    fprintf(stderr, "System call read failed. Error value is: %s\n", strerror(errno));
    Exit(1);
  }
  return r;
}

void Write(int filen, char* buff, int n){
  if (write(filen, buff, n) == -1){
    fprintf(stderr, "System call write failed. Error value is: %s\n", strerror(errno));
    Exit(1);
  }
}

void Pipe(int * pipen){
  if (pipe(pipen) == -1){
    fprintf(stderr, "System call pipe failed. Error value is: %s\n", strerror(errno));
    Exit(1);
  }
}

void Dup2(int fd1, int fd2){
  if (dup2(fd1, fd2) == -1){
    fprintf(stderr, "System call pipe failed. Error value is: %s\n", strerror(errno));
    Exit(1);
  }
}

int main(int argc, char** argv){
  int buf_size = 1024;
  struct termios new_termios;
  int port_num;
  int pt = 0;
  int l = 0;
  int c = 0;
  char * log_file;
  struct option opts[] = {
    {"port", required_argument, NULL, 'p'},
    {"log", required_argument, NULL, 'l'},
    {"compress", no_argument, NULL, 'c'},
    {0, 0, 0, 0}
  };
  char optresult;
  int fd = 0;
  //get original terminal settings
  if (tcgetattr(fd, &orig_termios) != 0) {
    fprintf(stderr, "System call tcgetattr failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }

  new_termios = orig_termios;
  new_termios.c_iflag = ISTRIP;
  new_termios.c_oflag = 0;
  new_termios.c_lflag = 0;

  //parse options
  while ((optresult = getopt_long(argc, argv, "", opts, NULL)) != -1){
    switch (optresult){
    case 'p':
      pt = 1;
      port_num = atoi(optarg);
      break;
    case 'l':
      l = 1;
      log_file = optarg;
      break;
    case 'c':
      c = 1;
      break;
    case 0:
      break;
    case '?':
    default:
      fprintf(stderr, "Correct usage is ./client --port=<number> [--log=<filename>] [--compress]\n");
      exit(1);
    }
  }

  //check for required port number
  if (pt != 1){
    fprintf(stderr, "No port number argument. Correct usage is ./client --port=<number> [--log=<filename>] [--compress]\n");
    exit(1);
  }

  int filefd;
  if (l){
    if ((filefd = creat(log_file, 0644)) < 0){
      fprintf(stderr, "System call creat failed. Error value is: %s\n", strerror(errno));
      exit(1);
    }
  }

  //prepare compression data structures
  z_stream cstream;
  z_stream dstream;
  if(c){
    cstream.zalloc = Z_NULL;
    cstream.zfree = Z_NULL;
    cstream.opaque = Z_NULL;
    if (deflateInit(&cstream, Z_DEFAULT_COMPRESSION) != 0){
      fprintf(stderr, "deflateInit failed.\n");
      exit(1);
    }
    dstream.zalloc = Z_NULL;
    dstream.zfree = Z_NULL;
    dstream.opaque = Z_NULL;
    dstream.avail_in = 0;
    dstream.next_in = Z_NULL;
    if (inflateInit(&dstream) != 0){
      fprintf(stderr, "inflateInit failed.\n");
      exit(1);
    }
  }

  //create socket
  int sock_num;
  sock_num = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_num == -1){
    fprintf(stderr, "System call socket failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }

  //connect to server
  struct sockaddr_in server;
  struct hostent * host_ip = gethostbyname("localhost");
  memset((char *) &server, 0, sizeof(server));
  memcpy((char *) &server.sin_addr.s_addr, (char *) host_ip->h_addr, host_ip->h_length);
  server.sin_family = AF_INET;
  server.sin_port = htons(port_num);

  if (connect(sock_num, (struct sockaddr *)&server , sizeof(server)) == -1){
    fprintf(stderr, "System call connect failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }
  
  struct pollfd p[2];
  p[0].fd = 0;
  p[0].events = POLLIN | POLLHUP | POLLERR;
  p[1].fd = sock_num;
  p[1].events = POLLIN | POLLHUP | POLLERR;
  char buf[buf_size];
  char buf2[buf_size];
  char buf3[buf_size];
  char curr = 0x0;
  int count = 0;
  int done2 = 0;
  int count2 = 0;

  //set terminal settings
  if (tcsetattr(fd, TCSANOW, &new_termios) != 0) {
    fprintf(stderr, "System call tcsetattr failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }

  //loop through input from keyboard and send it to terminal and server, loop through input from server and send it to terminal
  while(!done2){
    if (poll(p, 2, 0) == -1){
      fprintf(stderr, "System call poll failed. Error value is: %s\n", strerror(errno));
      Exit(1);
    }
    if (p[0].revents == POLLIN){
      count = Read(0, buf, buf_size);
      int i = 0;
      int a = 0;
      char sent[buf_size];
      while ( i < count ){
	curr = buf[i];
	i+=1;
	//mapping cr -> nl
	if (curr == 0x0a || curr == 0x0d){
	  char newline = 0x0d;
	  Write(1, &newline, 1);
	  newline = 0x0a;
	  Write(1, &newline, 1);
	  buf[i] = newline;
	  if(!c && p[1].revents != POLLHUP && p[1].revents != POLLERR){
	    sent[a] = newline;
	    Send(sock_num, &newline, 1);
	    a+=1;
	  }
	}
	else{
	  Write(1, &curr, 1);
	  if (!c && p[1].revents != POLLHUP && p[1].revents != POLLERR){
	    Send(sock_num, &curr, 1);
	    sent[a] = curr;
	    a+=1;
	  }
	}
      }
      //compress sent bytes
      if (c){
	cstream.avail_in = count;
	cstream.next_in = (Bytef *) buf;
	cstream.avail_out = buf_size;
	cstream.next_out = (Bytef *) buf2;
	do{
	  deflate(&cstream, Z_SYNC_FLUSH);
	} while (cstream.avail_in > 0);
	count2 = buf_size - cstream.avail_out;
	if (p[1].revents != POLLHUP && p[1].revents != POLLERR){
	  Send(sock_num, buf2, count2);
	}
      }
      //report bytes sent
      if (l && count != 0){
	if (!c){
	char end = '\n';
	dprintf(filefd, "SENT %d bytes: ", a);
	Write(filefd, sent, a);
	Write(filefd, &end, 1);
	}
	if(c){
	  char end = '\n';
	  dprintf(filefd, "SENT %d bytes: ", count2);
	  Write(filefd, buf2, count2);
	  Write(filefd, &end, 1);
	}
      }
    }

    if (p[1].revents == POLLIN){
      count = Recv(sock_num, buf, buf_size);
      int i = 0;
      if (!c) {
	while (i < count){
	  curr = buf[i];
	  i+=1;
	  //map nl -> crnl
	  if (curr == 0x0a || curr == 0x0d){
	    char newline = 0x0d;
	    Write(1, &newline, 1);
	    newline = 0x0a;
	    Write(1, &newline, 1);
	  }
	  else{
	    Write(1, &curr, 1);
	  }
	}
      }
      //decompress data from server
      if (c) {
	i = 0;
	dstream.avail_in = count;
	dstream.next_in = (Bytef *) buf;
	dstream.avail_out = buf_size;
	dstream.next_out = (Bytef *) buf3;
	do{
	  inflate(&dstream, Z_SYNC_FLUSH);
	} while (dstream.avail_in > 0);
	while ((unsigned) i < buf_size - dstream.avail_out){
	  curr = buf3[i];
	  i+=1;
	  if (curr == 0x0a || curr == 0x0d){
	    char newline = 0x0d;
	    Write(1, &newline, 1);
	    newline = 0x0a;
	    Write(1, &newline, 1);
	  }
	  else{
	    Write(1, &curr, 1);
	  }
	}
      }
      //record received bytes
      if (l && count != 0){
	char end = '\n';
	dprintf(filefd, "RECEIVED %d bytes: ", count);
	Write(filefd, buf, count);
	Write(filefd, &end, 1);
      }
      if (count == 0){
	done2 = 1;
      }
    }

    if (p[1].revents == POLLHUP || p[1].revents == POLLERR){
      done2 = 1;
      count = Recv(sock_num, buf, buf_size);
      int i = 0;
      if (!c) {
      while (i < count){
	curr = buf[i];
	i+=1;
	if (curr == 0x0a || curr == 0x0d){
	  char newline = 0x0d;
	  Write(1, &newline, 1);
	  newline = 0x0a;
	  Write(1, &newline, 1);
	}
	else{
	  Write(1, &curr, 1);
	}
      }
      }
    
      if (c) {
	i = 0;
	dstream.avail_in = count;
	dstream.next_in = (Bytef *) buf;
	dstream.avail_out = buf_size;
	dstream.next_out = (Bytef *) buf3;
	inflate(&dstream, Z_SYNC_FLUSH);
	while ((unsigned) i < buf_size - dstream.avail_out){
	  curr = buf3[i];
	  i+=1;
	  if (curr == 0x0a || curr == 0x0d){
	    char newline = 0x0d;
	    Write(1, &newline, 1);
	    newline = 0x0a;
	    Write(1, &newline, 1);
	  }
	  else{
	    Write(1, &curr, 1);
	  }
	}
      }
      if (l && count != 0){
	char end = '\n';
	dprintf(filefd, "RECEIVED %d bytes: ", count);
	Write(filefd, buf, count);
	Write(filefd, &end, 1);
      }
    }
  }
  
  Exit(0);
}
