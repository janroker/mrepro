#ifndef UTIL_H
#define UTIL_H

#include <sys/socket.h>
#include <netdb.h>

/////// error levels ///////
#define IGNORE 0
#define FATAL 1
#define DBG 2
#define WARN 3
/////// END error levels ///////

int Inet_pton(int af, const char *src, void *dst, int status, char *message, int errLvl);

const char *Inet_ntop(int af, const void *src, char *dst, socklen_t len, int status, char *message, int errLvl);

int
Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv,
            socklen_t servlen, int flags, int status, char *message, int errLvl);

int Getaddrinfo(char *node, char *service, struct addrinfo *hints, struct addrinfo **res, int status, char *message,
                int errLvl);

int GetUDP_addrinfo(char *node, char *service, struct addrinfo **res, int flags, int status, char *message,
                    int errLvl);

int GetTCP_addrinfo(char *node, char *service, struct addrinfo **res, int flags, int status, char *message,
                    int errLvl);

int Socket(int domain, int type, int protocol, int status, char *message, int errLvl);

ssize_t
Sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len, int status,
       char *message, int errLvl);

ssize_t Recvfrom(int fd, void *restrict buf, size_t n, int flags, struct sockaddr *addr, socklen_t *restrict addr_len,
                 int status, char *message, int errLvl);

int Bind(int fd, const struct sockaddr *addr, socklen_t len, int status, char *message, int errLvl);

int Listen(int sockfd, int backlog, int status, char *message, int errLvl);

int Accept(int fd, struct sockaddr *addr, socklen_t *restrict addr_len, int status, char *message, int errLvl);

// return nread, 0 for EOF or -1 for err
ssize_t readn(int fd, void *vptr, size_t n, const char *delimiter);

// return n or -1 for error
size_t Writn(int fd, const void *vptr, size_t n, int status, char *message, int errLvl);

ssize_t Send(int socket, const void *buffer, size_t length, int flags, int status, char *message, int errLvl);

ssize_t Recv(int socket, void *buffer, size_t length, int flags, int status, char *message, int errLvl);

int Connect(int fd, const struct sockaddr *addr, socklen_t len, int status, char *message, int errLvl);

int
Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen, int status, char *message,
           int errLvl);

#endif