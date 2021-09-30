#ifndef UTIL_H
#define UTIL_H

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "debug.h"

/////// error levels ///////
#define IGNORE 0
#define FATAL 1
#define DBG 2
#define WARN 3
/////// END error levels ///////

typedef uint8_t errLevel_t;
typedef uint8_t status_t;

int Inet_pton(int af, const char* src, void* dst, status_t status, errLevel_t errLvl, char* message);
const char* Inet_ntop(int af, const void* src, char* dst, socklen_t len, status_t status, errLevel_t errLvl, char* message);
int Getnameinfo(const struct sockaddr* sa, socklen_t salen, char* host, socklen_t hostlen, char* serv,
                socklen_t servlen, int flags, status_t status, errLevel_t errLvl, char* message);
int Getaddrinfo(char* node, char* service, struct addrinfo* hints, struct addrinfo** res, status_t status,
                errLevel_t errLvl, char* message);
int GetUDP_addrinfo(char* node, char* service, struct addrinfo** res, int flags, status_t status, errLevel_t errLvl, char* message);
int GetTCP_addrinfo(char* node, char* service, struct addrinfo** res, int flags, status_t status, errLevel_t errLvl, char* message);
int Socket(int domain, int type, int protocol, status_t status, errLevel_t errLvl, char* message);
ssize_t Sendto(int fd, const void* buf, size_t n, int flags, const struct sockaddr* addr, socklen_t addr_len,
               status_t status, errLevel_t errLvl, char* message);
// socklen_t restrict mora uvijek biti postavljen na sizeof(struct sockaddr) prije nego se šalje u poziv!!! inace addr nece biti postavljen
ssize_t Recvfrom(int fd, void* restrict buf, size_t n, int flags, struct sockaddr* addr, socklen_t* restrict addr_len,
                 status_t status, errLevel_t errLvl, char* message);
int Bind(int fd, const struct sockaddr* addr, socklen_t len, status_t status, errLevel_t errLvl, char* message);
int Listen(int sockfd, int backlog, status_t status, errLevel_t errLvl, char* message);
int Accept(int fd, struct sockaddr* addr, socklen_t* restrict addr_len, status_t status, errLevel_t errLvl, char* message);
// return nread, 0 for EOF or -1 for err
ssize_t readn(int fd, void* vptr, ssize_t n, const char* delimiter);
// return n or -1 for error
size_t Writn(int fd, const void* vptr, size_t n, status_t status, errLevel_t errLvl, char* message);
ssize_t Send(int socket, const void* buffer, size_t length, int flags, status_t status, errLevel_t errLvl, char* message);
ssize_t Recv(int socket, void* buffer, size_t length, int flags, status_t status, errLevel_t errLvl, char* message);
int Connect(int fd, const struct sockaddr* addr, socklen_t len, status_t status, errLevel_t errLvl, char* message);
int Setsockopt(int fd, int level, int optname, const void* optval, socklen_t optlen, status_t status, errLevel_t errLvl, char* message);
int Select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout, status_t status,
           errLevel_t errLvl, char* message);
// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr* sa);
size_t Strncpy(char* dest, const char* src, size_t n);
ssize_t freadn(void* buf, size_t size, size_t n, FILE* f);
ssize_t fwriten(const void* ptr, size_t size, size_t n, FILE* s);
int bindTCP(char* port, status_t status);
int bindUDP(char* port, status_t status);

#endif
