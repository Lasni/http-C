/*
** listener.c -- a datagram sockets "server" demo
** UDP listener is connectionless - accepts anything
*/

// --- 1. setup ---

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MYPORT "4950" // the port users will be connecting to

#define MAXBUFLEN 100

// get sockaddr IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(void)
{

  // --- 2. declare variables, set up hints, call getaddrinfo ---

  int sockfd; // unlike in TCP, here one socket receives everything
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  struct sockaddr_storage their_addr;
  char buf[MAXBUFLEN];
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET6; // or set to AF_INET to use IPv4
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); // error
    return 1;                                               // address lookup failed
  }

  // --- 3. loop through results and bind to the first available ---
  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      perror("listener: socket"); // socket error
      continue;
    }

    if ((bind(sockfd, p->ai_addr, p->ai_addrlen)) == -1)
    {
      perror("listener: bind"); // bind error
      continue;
    }

    break;
  }

  // --- 4. failure check, cleanup ---
  if (p == NULL)
  {
    fprintf(stderr, "listener: failed to bind socket\n");
    return 2; // address couldn't be used
  }

  freeaddrinfo(servinfo);
  printf("listener: waiting to recvfrom...\n");

  // --- 5. recvfrom ---
  // In TCP the connection already identifies the other side
  // UDP can accept anything, so we need to know the sender's address
  addr_len = sizeof their_addr;
  if ((numbytes = recvfrom(sockfd,
                           buf,
                           MAXBUFLEN - 1,
                           0,
                           (struct sockaddr *)&their_addr, // sender's address
                           &addr_len)) == -1)
  {
    perror("recvfrom");
    exit(1);
  }

  // --- 6. print out results and close the socket  ---
  // inet_ntop converts raw address bytes to printable (127 0 0 1 -> 127.0.0.1)
  // get_in_addr gets the pointer to the raw bytes
  printf("listener: got packet from %s\n",
         inet_ntop(their_addr.ss_family,
                   get_in_addr((struct sockaddr *)&their_addr),
                   s,
                   sizeof s));

  printf("listener: packet is %d bytes long\n", numbytes);
  buf[numbytes] = '\0';
  printf("listener: packet containes \"%s\"\n", buf);

  close(sockfd);

  return 0;
}