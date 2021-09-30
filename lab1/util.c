#include "util.h"

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

int Socket(int __domain, int __type, int __protocol, int status, char *message)
{
    int sock = socket(__domain, __type, __protocol);
    if (sock == -1)
        errx(status, "%s", message);

    return sock;
}

ssize_t Sendto(int __fd, const void *__buf, size_t __n,
               int __flags, struct sockaddr * __addr,
               socklen_t __addr_len, int status, char *message)
{
    size_t res = sendto(__fd, __buf, __n, __flags, __addr, __addr_len);
    if (res != __n)
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

int Bind(int __fd, struct sockaddr * __addr, socklen_t __len, int status, char* message) {
    int res = bind(__fd, __addr, __len);
    if(res != 0) errx(status, "%s", message);
    return res;
}
