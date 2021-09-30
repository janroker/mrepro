#include <unistd.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include "util.h"
#include "debug.h"

void handleErr(int status, char *message, int errLvl) {
    switch (errLvl) {
        case FATAL:
            err(status, "%s", message);
        case WARN:
            warn("%s", message);
            break;
        case DBG:
            debug_printf("%s\n", message);
            break;
        default:
            break;
    }
}

int Inet_pton(int af, const char *src, void *dst, int status, char *message, int errLvl) {
    int res = inet_pton(af, src, dst);
    if (res != 1)
        handleErr(status, message, errLvl);
    return res;
}

const char *Inet_ntop(int af, const void *src, char *dst, socklen_t len, int status, char *message, int errLvl) {
    const char *res = inet_ntop(af, src, dst, len);
    if (res == NULL)
        handleErr(status, message, errLvl);
    return res;
}

int
Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen,
            int flags, int status, char *message, int errLvl) {
    int res = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
    if (res != 0)
        handleErr(status, message, errLvl);
    return res;
}

int Getaddrinfo(char *node, char *service, struct addrinfo *hints, struct addrinfo **res, int status, char *message,
                int errLvl) {
    int error = getaddrinfo(node, service, hints, res);
    if (error != 0) {
        switch (errLvl) {
            case FATAL:
                err(status, "%s\n%s", gai_strerror(error), message);
            case WARN:
                warn("%s\n%s", gai_strerror(error), message);
                break;
            case DBG:
                debug_printf("%s\n%s\n", gai_strerror(error), message);
                break;
            default:
                break;
        }
    }
    return error;
}

int
GetUDP_addrinfo(char *node, char *service, struct addrinfo **res, int flags, int status, char *message, int errLvl) {
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = flags;

    return Getaddrinfo(node, service, &hints, res, status, message, errLvl);
}

int
GetTCP_addrinfo(char *node, char *service, struct addrinfo **res, int flags, int status, char *message, int errLvl) {
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = flags;

    return Getaddrinfo(node, service, &hints, res, status, message, errLvl);
}

int Socket(int domain, int type, int protocol, int status, char *message, int errLvl) {
    int sock = socket(domain, type, protocol);
    if (sock == -1)
        handleErr(status, message, errLvl);
    return sock;
}

/*
The sendto() function shall send a message through a connection-mode or
 connectionless-mode socket. If the socket is connectionless-mode,
 the message shall be sent to the address specified by dest_addr.
 If the socket is connection-mode, dest_addr shall be ignored.
*/
ssize_t
Sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len, int status,
       char *message, int errLvl) {
    ssize_t res = sendto(fd, buf, n, flags, addr, addr_len);
    if (res != (ssize_t) n)
        handleErr(status, message, errLvl);
    return res;
}

ssize_t Recvfrom(int fd, void *restrict buf, size_t n, int flags,
                 struct sockaddr *addr, socklen_t *restrict addr_len, int status, char *message, int errLvl) {
    ssize_t nread = recvfrom(fd, buf, n, flags, addr, addr_len);
    if (nread == -1)
        handleErr(status, message, errLvl);
    return nread;
}

int Bind(int fd, const struct sockaddr *addr, socklen_t len, int status, char *message, int errLvl) {
    int res = bind(fd, addr, len);
    if (res != 0)
        handleErr(status, message, errLvl);
    return res;
}

int Listen(int sockfd, int backlog, int status, char *message, int errLvl) {
    int res = listen(sockfd, backlog);
    if (res != 0)
        handleErr(status, message, errLvl);

    return res;
}

int Accept(int fd, struct sockaddr *addr, socklen_t *restrict addr_len, int status, char *message, int errLvl) {
    int res = accept(fd, addr, addr_len);
    if (res == -1)
        handleErr(status, message, errLvl);
    return res;
}

int Connect(int fd, const struct sockaddr *addr, socklen_t len, int status, char *message, int errLvl) {
    int res = connect(fd, addr, len);
    if (res != 0)
        handleErr(status, message, errLvl);
    return res;
}

ssize_t readn(int fd, void *vptr, size_t n, const char *delimiter) {
    size_t nleft;
    ssize_t nread;
    char *ptr;
    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0; /* and call read() again */
            else
                return (-1);
        } else if (nread == 0 || (delimiter != NULL && ptr[nread - 1] == *delimiter)) {
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0 */
}

size_t writn(int fd, const void *vptr, size_t n) {
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;
    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (errno == EINTR)
                nwritten = 0; /* and call write again */
            else
                return (-1); /* error */
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n);
}

size_t Writn(int fd, const void *vptr, size_t n, int status, char *message, int errLvl) {
    if (writn(fd, vptr, n) != n)
        handleErr(status, message, errLvl);
    return n;
}

ssize_t Recv(int socket, void *buffer, size_t length, int flags, int status, char *message, int errLvl) {
    ssize_t nread = recv(socket, buffer, length, flags);
    if (nread == -1)
        handleErr(status, message, errLvl);
    return nread;
}

ssize_t Send(int socket, const void *buffer, size_t length, int flags, int status, char *message, int errLvl) {
    ssize_t res = send(socket, buffer, length, flags);
    if (res != (ssize_t) length)
        handleErr(status, message, errLvl);
    return res;
}

int Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen, int status, char *message,
               int errLvl) {
    int res = setsockopt(fd, level, optname, optval, optlen);
    if (res != 0)
        handleErr(status, message, errLvl);
    return res;
}
