
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

//ignore sigpipes
void signal_handler(int signo){
  if (signo == SIGPIPE){

  }
}

//function to run the shell
void run_shell(int client_sock_num, int c);

//wrapper functions to check for errors and clean up code
int Recv(int sockfd, void * buf, int len){
  int r = 0;
  if((r = recv(sockfd, buf, len, 0)) == -1){
    fprintf(stderr, "System call recv failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }
  return r;
}

void Send(int sockfd, void * buf, int len){
  if(send(sockfd, buf, len, 0) == -1){
    fprintf(stderr, "System call send failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }
}

void Close(int filen){
  if (close(filen) == -1){
    fprintf(stderr, "System call close failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }
}

int Read(int filen, char* buff, int n){
  int r = 0;
  if ((r = read(filen, buff, n)) == -1){
    fprintf(stderr, "System call read failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }
  return r;
}

void Write(int filen, char* buff, int n){
  if (write(filen, buff, n) == -1){
    fprintf(stderr, "System call write failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }
}

void Pipe(int * pipen){
  if (pipe(pipen) == -1){
    fprintf(stderr, "System call pipe failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }
}

void Dup2(int fd1, int fd2){
  if (dup2(fd1, fd2) == -1){
    fprintf(stderr, "System call pipe failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }
}

//function to report exit status of shell
int ex(pid_t f){
  int status;
  pid_t waitr;
  if((waitr = waitpid(f, &status, WNOHANG)) == -1){
    fprintf(stderr, "System call waitpid failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }
  if (waitr > 0){
    int status1 = status & 0x007f;
    int status2 = (status & 0xff00) >> 8;
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", status1, status2);
    return 0;
  }
  return 1;
}

int main(int argc, char** argv){
  int port_num;
  int c = 0;
  int p = 0;
  struct option opts[] = {
    {"port", required_argument, NULL, 'p'},
    {"compress", no_argument, NULL, 'c'},
    {0, 0, 0, 0}
  };
  char optresult;

  //parse options
  while ((optresult = getopt_long(argc, argv, "", opts, NULL)) != -1){
    switch (optresult){
    case 'p':
      p = 1;
      port_num = atoi(optarg);
      break;
    case 'c':
      c = 1;
      break;
    case 0:
      break;
    case '?':
    default:
      fprintf(stderr, "Correct usage is ./server --port=<number> [--compress]\n");
      exit(1);
    }
  }

  //check for required port number
  if (p != 1){
    fprintf(stderr, "No port argument. Correct usage is ./server --port=<number> [--compress]\n");
    exit(1);
  }

  //create socket
  int sock_num;
  sock_num = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_num == -1){
    fprintf(stderr, "System call socket failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }

  //bind to socket
  struct sockaddr_in server, client;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_family = AF_INET;
  server.sin_port = htons(port_num);

  if (bind(sock_num,(struct sockaddr *)&server, sizeof(server)) == -1){
    fprintf(stderr, "System call bind failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }

  //listen and accept incoming requests
  if (listen(sock_num, 3) == -1){
    fprintf(stderr, "System call listen failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }

  int client_sock_num;
  int addr_size = sizeof(struct sockaddr_in);
  client_sock_num = accept(sock_num, (struct sockaddr*) &client, (socklen_t*) &addr_size);
  if (client_sock_num == -1){
    fprintf(stderr, "System call accept failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }

  //run the bash shell
  run_shell(client_sock_num, c);

  exit(0);
}

void run_shell(int client_sock_num, int c){
  int pipe1[2];
  int pipe2[2];
  Pipe(pipe1);
  Pipe(pipe2);
  pid_t f;

  //fork child process to run shell
  if ( (f = fork()) == -1){
    fprintf(stderr, "System call fork failed. Error value is: %s\n", strerror(errno));
    exit(1);
  }

  //child launches bash shell
  if (f == 0){
    Close(0);
    if (dup(pipe1[0]) == -1){
      fprintf(stderr, "System call dup failed. Error value is: %s\n", strerror(errno));
      exit(1);
    }

    Close(pipe2[0]);
    Close(pipe1[1]);
    Dup2(pipe2[1], 1);
    Dup2(pipe2[1], 2);
    if (execl("/bin/bash", "/bin/bash", (char *) NULL) == -1){
      fprintf(stderr, "System call execl failed. Error value is: %s\n", strerror(errno));
      exit(1);
    }
  }

  //parent blocks sigpipes
  signal(SIGPIPE, signal_handler);

  //prepare data structures for compression
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
  
  Close(pipe2[1]);
  Close(pipe1[0]);
  struct pollfd p[2];
  p[0].fd = client_sock_num;
  p[0].events = POLLIN | POLLHUP | POLLERR;
  p[1].fd = pipe2[0];
  p[1].events = POLLIN | POLLHUP | POLLERR;
  int buf_size = 1024;
  char buf[buf_size];
  char buf2[buf_size];
  char curr = 0x0;
  int count = 0;
  int report = 1;
  int done = 0;
  int done2 = 0;

  //loop through input from client and send it to bash shell, loop from input from shell and send it to client
  while(!done || !done2){
    if (poll(p, 2, 0) == -1){
      fprintf(stderr, "System call poll failed. Error value is: %s\n", strerror(errno));
      exit(1);
    }
    if (p[0].revents == POLLIN){
      count = Recv(client_sock_num, buf, buf_size);
      int i = 0;
      if (!c){
	while ( i < count ){
	curr = buf[i];
	i+=1;
	//checks for EOF
	if (curr == 0x04){
	  Close(pipe1[1]);
	  done = 1;
	  break;
	}
	//checks for interrupt command
	if (curr == 0x03){
	  if (kill(f, SIGINT) == -1){
	    fprintf(stderr, "System call kill failed. Error value is: %s\n", strerror(errno));
	    exit(1);
	  }
	  break;
	}
	if (p[1].revents != POLLHUP && p[1].revents != POLLERR) {
	   Write(pipe1[1], &curr, 1);
	}
	}
      }
      //handles compress input data
      if (c) {
	i = 0;
	dstream.avail_in = count;
	dstream.next_in = (Bytef *) buf;
	dstream.avail_out = buf_size;
	dstream.next_out = (Bytef *) buf2;
	do {
	  inflate(&dstream, Z_SYNC_FLUSH);
	} while (dstream.avail_in > 0);
	while ((unsigned) i < buf_size - dstream.avail_out){
	  curr = buf2[i];
	  i+=1;
	  if (curr == 0x04){
	    Close(pipe1[1]);
	    done = 1;
	    break;
	  }
	  if (curr == 0x03){
	    if (kill(f, SIGINT) == -1){
	      fprintf(stderr, "System call kill failed. Error value is: %s\n", strerror(errno));
	      exit(1);
	    }
	    break;
	  }
	  if (p[1].revents != POLLHUP && p[1].revents != POLLERR) {
	    if (curr == 0x0a || curr == 0x0d){
	      char newline = 0x0a;
	      Write(pipe1[1], &newline, 1);
	    }
	    else {
	      Write(pipe1[1], &curr, 1);
	    }
	  }
	}
      }
    }

    //if bash shell has returned data
    if (p[1].revents == POLLIN){
      count = Read(pipe2[0], buf, buf_size);
      //compresses data to be sent to client
      if (c){
	cstream.avail_in = count;
	cstream.next_in = (Bytef *) buf;
	cstream.avail_out = buf_size;
	cstream.next_out = (Bytef *) buf2;
	do {
	  deflate(&cstream, Z_SYNC_FLUSH);
	} while (cstream.avail_in > 0);
	int k = buf_size - cstream.avail_out;
	Write(client_sock_num, buf2, k);
      }
      if (!c){
	Write(client_sock_num, buf, count);
      }
    }

    if (p[1].revents == POLLHUP || p[1].revents == POLLERR){
      done2 = 1;
      count = Read(pipe2[0], buf, buf_size);
      if (c){
	cstream.avail_in = count;
	cstream.next_in = (Bytef *) buf;
	cstream.avail_out = buf_size;
	cstream.next_out = (Bytef *) buf2;
	do {
	  deflate(&cstream, Z_SYNC_FLUSH);
	} while(cstream.avail_in > 0);
	int k = buf_size - cstream.avail_out;
	Write(client_sock_num, buf2, k);
      }
      if (!c){
	Write(client_sock_num, buf, count);
      }
    }
    
    //checks for shell exit status
    if (report){
      report = ex(f);
    }
    
    if (done2){
      break;
    }
  }

  //forces a report from shell exit status
  if (report){
    int status;
    if(waitpid(f, &status, 0) == -1){
      fprintf(stderr, "System call waitpid failed. Error value is: %s\n", strerror(errno));
      exit(1);
    }
    int status1 = status & 0x007f;
    int status2 = (status & 0xff00) >> 8;
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n\r", status1, status2);
  }
  if (c){
    deflateEnd(&cstream);
    inflateEnd(&dstream);
  }
}
