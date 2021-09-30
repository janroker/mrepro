#include "util.h"
#include "unistd.h"

void Getaddrinfo(char *node, char *service, struct addrinfo *hints, struct addrinfo **res, int status, char *message, bool fatal)
{
    int error = getaddrinfo(node, service, hints, res);

    if (error)
    {
        if (fatal)
            errx(status, "%s\n%s", gai_strerror(error), message);
        else
            warnx("%s\n%s", gai_strerror(error), message);
    }
}

void getUDP_addrinfo(char *node, char *service, struct addrinfo **res, int status, char *message, bool fatal)
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    Getaddrinfo(node, service, &hints, res, status, message, fatal);
}

void GetTCP_addrinfo_passive(char *node, char *service, struct addrinfo **res, int status, char *message, bool fatal)
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    Getaddrinfo(node, service, &hints, res, status, message, fatal);
}

int Socket(int __domain, int __type, int __protocol, int status, char *message)
{
    int sock = socket(__domain, __type, __protocol);
    if (sock == -1)
        errx(status, "%s\n", message);

    return sock;
}

ssize_t Sendto(int __fd, const void *__buf, size_t __n,
               int __flags, const struct sockaddr *__addr,
               socklen_t __addr_len, int status, char *message)
{
    ssize_t res = sendto(__fd, __buf, __n, __flags, __addr, __addr_len);
    if (res != (ssize_t)__n)
    {
        errx(status, "%s", message);
    }
    return res;
}

ssize_t Recvfrom(int __fd, void *__restrict __buf, size_t __n, int __flags,
                 struct sockaddr *__addr, socklen_t *__restrict __addr_len, int status, char *message)
{
    int nread = recvfrom(__fd, __buf, __n, __flags, __addr, __addr_len);
    if (nread == -1)
    {
        errx(status, "%s", message);
    }
    return nread;
}

int Bind(int __fd, const struct sockaddr *__addr, socklen_t __len, int status, char *message)
{
    int res = bind(__fd, __addr, __len);
    if (res != 0)
        errx(status, "%s", message);
    return res;
}

int Listen(int sockfd, int backlog, int status, char *message)
{
    int res = listen(sockfd, backlog);
    if (res != 0)
        errx(status, "%s", message);

    return res;
}

int Accept(int __fd, struct sockaddr *__addr,
           socklen_t *__restrict __addr_len, int status, char *message)
{
    int res = accept(__fd, __addr, __addr_len);
    if (res == -1)
        warnx("%s\n", message);

    return res;
}

int Connect(int __fd, const struct sockaddr *__addr, socklen_t __len, int status, char *message)
{
    int res = connect(__fd, __addr, __len);
    if (res != 0)
        errx(status, "%s", message);

    return res;
}

ssize_t readn(int fd, void *vptr, size_t n, char *delimiter)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;
    ptr = vptr;
    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR)
                nread = 0; /* and call read() again */
            //return 0;
            else
                return (-1);
        }
        else if (nread == 0 || (delimiter != NULL && ptr[nread - 1] == *delimiter))
        {
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0 */
}

ssize_t writn(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;
    ptr = vptr;
    nleft = n;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0; /* and call write again */
            else
                return (-1); /* error */
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n);
}

ssize_t Writn(int fd, const void *vptr, size_t n)
{
    if (writn(fd, vptr, n) != ((ssize_t)n))
    {
        warnx("Failed or partial write");
        return -1;
    }

    return n;
}