
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(const char *msg) {
  perror(msg);
  exit(0);
}

int main(int argc, char *argv[]) {

  int sockfd,n;
  struct sockaddr_in servaddr,cliaddr;
  struct hostent *server;
  char sendline[65025];
  bzero(sendline, 65025);

  if (argc < 4) {
    printf("usage: %s server port reqID host...", argv[0]);
    exit(1);
  }

  //setup socket. mmm, boilerplate
  sockfd=socket(AF_INET,SOCK_DGRAM,0);
  server = gethostbyname(argv[1]);
  if (server == NULL) 
    error("ERROR no such host");

  bzero(&servaddr,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  bcopy((char*)server->h_addr,
        (char*)&servaddr.sin_addr.s_addr,
        server->h_length);
  servaddr.sin_port=htons(atoi(argv[2]));

  // add to each time we add a byte to reply
  unsigned char checksum = 0;

  //GID 1 byte
  sendline[3] = 14;
  checksum += sendline[3];

  //RID 1 byte
  sendline[4] = atoi(argv[3]);
  checksum += sendline[4];

  //hosts (~host~host~host...) from args
  int arg, index, j;
  for (index = 5, arg = 4; arg < argc; arg++) {
    sendline[index++] = '~';
    checksum += sendline[index-1];
    for (j = 0; argv[arg][j]; index++, j++) {
      sendline[index] = argv[arg][j];
      checksum += sendline[index];
    }
  }
  sendline[index] = '\0';

  //TML 2 bytes
  sendline[0] = (index >> 8) & 0xFF;
  sendline[1] = index & 0xFF;
  checksum += sendline[0];
  checksum += sendline[1];

  //Checksum 1 byte
  sendline[2] = ~checksum;


  //while we get corrupted response or 7 tries
  //  send datagram, check checksum & length each time
  char recvline[10000];
  int i = 0;
  int attempts = 0;
  unsigned char calc_cs;
  do {
    sendto(sockfd,sendline,index,0,
        (struct sockaddr *)&servaddr,sizeof(servaddr));
    n=recvfrom(sockfd,recvline,10000,0,NULL,NULL);
    calc_cs = 0;
    for (i=0; i < n; i++) {
      calc_cs += recvline[i];
    }
  } while (((recvline[0] | recvline[1] != n) || calc_cs != 0xFF)
      && attempts++ < 7);

  //if we got a valid response in trial 1-7
  //  host: ip
  if (attempts < 8) {
    for (i=5, arg=4; i < n; ) {
      int ips = recvline[i++];
      int j;
      printf("%s:\n", argv[arg++]);
      for (j=0; j < ips; j++) {
        printf("%d.%d.%d.%d\n",
            0xFF & recvline[i++], 0xFF & recvline[i++], 
            0xFF & recvline[i++], 0xFF & recvline[i++]);
      }
    }
  } else {
    printf("7 tries are up. Something went terribly wrong\n");
  }
}
