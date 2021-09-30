#include <err.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdbool.h>

void Getaddrinfo(char *node, char *service, struct addrinfo *hints, struct addrinfo **res, int status, char *message, bool fatal);

void getUDP_addrinfo(char *node, char *service, struct addrinfo **res, int status, char *message, bool fatal);

int Socket(int __domain, int __type, int __protocol, int status, char *message);

ssize_t Sendto(int __fd, const void *__buf, size_t __n,
               int __flags, struct sockaddr * __addr,
               socklen_t __addr_len, int status, char *message);

ssize_t Recvfrom(int __fd, void *__restrict __buf, size_t __n, int __flags,
                 struct sockaddr * __addr, socklen_t *__restrict __addr_len, int status, char *message);

int Bind(int __fd, struct sockaddr * __addr, socklen_t __len, int status, char* message);
