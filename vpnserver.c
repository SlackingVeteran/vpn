#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

#define PORT_NUMBER 4433
#define BUFF_SIZE 2000

struct sockaddr_in peerAddr;

int createTunDevice() {
   int tunfd;
   struct ifreq ifr;
   memset(&ifr, 0, sizeof(ifr));

   ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

   tunfd = open("/dev/net/tun", O_RDWR);
   ioctl(tunfd, TUNSETIFF, &ifr);

   return tunfd;
}

int initTCPServer() {
    int sockfd, new_socket, valread;
    struct sockaddr_in server;
    int opt = 1;
    char buff[1024] = {0};

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT_NUMBER);

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port PORT_NUMBER
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port PORT_NUMBER
    if (bind(sockfd, (struct sockaddr *)&server,
                                 sizeof(server))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Wait for the VPN client to "connect".
    bzero(buff, 1024);
    int peerAddrLen = sizeof(struct sockaddr_in);

    // Need to listen to connection, listen() marks socket referred by sockfd as a passive socket
    // Can only be used if sockfd type is SOCK_STREAM or SOCK_SEQPACKET
    if (listen(sockfd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // accept() then read() is used in TCP connection, UDP works with just recvfrom()
    if ((new_socket = accept(sockfd, (struct sockaddr *)&peerAddr,
                       (socklen_t*)&peerAddrLen))<0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    valread = read( new_socket , buff, 1024);

    printf("Connected with the client: %s\n", buff);
    return new_socket;
}

void tunSelected(int tunfd, int sockfd){
    int  len;
    char buff[BUFF_SIZE];

    printf("Got a packet from TUN\n");

    bzero(buff, BUFF_SIZE);
    len = read(tunfd, buff, BUFF_SIZE);

    // send() is used instead of sendto for tcp
    send(sockfd, buff, len, 0);
}

void socketSelected (int tunfd, int sockfd){
    int  valread;
    char buff[BUFF_SIZE];

    printf("Got a packet from the tunnel\n");

    bzero(buff, BUFF_SIZE);

    valread = read(sockfd, buff, BUFF_SIZE);
    write(tunfd, buff, valread);

}
int main (int argc, char * argv[]) {
   int tunfd, sockfd;

   tunfd  = createTunDevice();
   sockfd = initTCPServer();

   // Enter the main loop
   while (1) {
     fd_set readFDSet;

     FD_ZERO(&readFDSet);
     FD_SET(sockfd, &readFDSet);
     FD_SET(tunfd, &readFDSet);
     select(FD_SETSIZE, &readFDSet, NULL, NULL, NULL);

     if (FD_ISSET(tunfd,  &readFDSet)) tunSelected(tunfd, sockfd);
     if (FD_ISSET(sockfd, &readFDSet)) socketSelected(tunfd, sockfd);
  }
}
