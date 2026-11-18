#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"

#define BACKLOG 10

void sigchld_handler(int s)
{
  (void)s;

  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)sa;
    return &(ipv4->sin_addr);
  }

  else
  {
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)sa;
    return &(ipv6->sin6_addr);
  }
}

int main(void)
{

  // --- 1. ---
  int sockfd, new_fd; // listen on sockfd, new connections on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address info
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  // --- 2. ---
  memset(&hints, 0, sizeof hints); // set memory to 0
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  // if error, print it out and exit
  // else servinfo gets populated (single item with ->ai_next, aka linked list)
  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // --- 3. ---
  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next)
  {

    // create socket file descriptor or -1 and jumps to next node on failure
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      perror("server: socket");
      continue;
    }

    // sets a socket option or -1 and exits completely on failure
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
      perror("setsockopt");
      exit(1);
    }

    // binds socket to address and port or -1 and closes the sockfd on failure
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    // if all 3 calls succeeded the loop ends
    // sockfd is a bound socket ready to listen
    // p points at the node that worked
    break;
  }

  // --- 4. ---
  freeaddrinfo(servinfo); // free the linked list

  // check whether the loop succeeded
  // if loop ran past the last node p became NULL (every node failed)
  // print error and exit
  if (p == NULL)
  {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  // if we have a bound socket, turn it into a listening one
  // on error print out and exit
  if (listen(sockfd, BACKLOG) == -1)
  {
    perror("listen");
    exit(1);
  }

  // --- 5. ---
  // handler that reaps all dead processes via waitpid
  sa.sa_handler = sigchld_handler; // reference a cleaner function
  sigemptyset(&sa.sa_mask);        // empty garbage mask that could block signals
  sa.sa_flags = SA_RESTART;        // restart the parent's accept() waiting when interrupted by child
  // take action specified in &sa (sa_handler) when a SIGCHLD signal arrives or exit on error
  if (sigaction(SIGCHLD, &sa, NULL) == -1)
  {
    perror("sigaction");
    exit(1);
  }

  // --- 6. ---
  printf("Server: waiting for connections...\n");

  // main accept loop
  while (1) // run forever
  {
    sin_size = sizeof their_addr; // assign appropriate size each time
    new_fd = accept(
        sockfd,                         // socket file descriptor
        (struct sockaddr *)&their_addr, // cast to sockaddr for compiler compliance
        &sin_size);                     // size

    if (new_fd == -1) // error out
    {
      perror("accept");
      continue;
    }

    inet_ntop(
        their_addr.ss_family,                        // IPv4 or IPv6
        get_in_addr((struct sockaddr *)&their_addr), // takes raw bytes, cast to sockaddr
        s,                                           // text buffer for ip address
        sizeof s                                     // size
    );
    printf("server: got connection from %s\n", s);

    // --- 7. ---
    pid_t pid = fork();
    if (pid == -1) // failed fork
    {
      perror("fork");
      close(new_fd);
    }
    else if (pid == 0) // child process. Gets copies of orig. sockfd and new_fd
    {
      close(sockfd); // child doesn't need the listener, only new_fd client
      if (send(new_fd, "Hello, world!", 13, 0) == -1)
      {
        perror("send"); // error
      }
      close(new_fd); // done with this client
      exit(0);       // exit child which triggers the SIGCHLD signal for cleanup
    }
    else // parent process
    {
      close(new_fd); // parent doesn't need new_fd
    }
  }
  return 0;
}