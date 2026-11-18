/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

// --- 1. setup ---

#define PORT "3490" // the port client will be connecting to

#define MAXDATASIZE 100 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// --- 2. declarations and argument check ---

int main(int argc, char *argv[])
{
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  if (argc != 2)
  {
    fprintf(stderr, "usage: client hostname\n");
    exit(1);
  }

  // --- 3. preparing hints and getaddrinfo call ---
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); // error
    return 1;                                               // lookup problem code
  }

  // --- 4. loop through all results and connect to the first you can ---
  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    // attempt to create a socket
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      perror("client: socket"); // failed to create a socket; move to next node
      continue;
    }

    // convert node's address to text and print it
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: attempting connection to %s\n", s);

    // attempt to connect to socket
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      // on error:
      perror("client: connect"); // print error
      close(sockfd);             // close socket so it doesn't leak
      continue;                  // try next node
    }

    break; // end the loop on success
  }

  // --- 5. failure check, connected message, cleanup ---

  if (p == NULL) // every address failed
  {
    fprintf(stderr, "client: failed to connect\n");
    return 2; // connect problem code
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
  printf("client connected to %s\n", s);

  freeaddrinfo(servinfo); // all done with this structure

  // --- 6. read from socket and close ---

  if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
  {
    perror("recv"); // error receiving
    exit(1);
  }

  buf[numbytes] = '\0'; // turn bytes to string

  printf("client: received '%s'\n", buf);
  close(sockfd);

  return 0;
}
