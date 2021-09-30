#include "util.h"

void handleErr(status_t status, errLevel_t errLvl, char* message) {
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

int Inet_pton(int af, const char* src, void* dst, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Inet_pton fail";

   int res = inet_pton(af, src, dst);
   if (res != 1)
      handleErr(status, errLvl, message);
   return res;
}

const char* Inet_ntop(int af, const void* src, char* dst, socklen_t len, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Inet_ntop fail";

   const char* res = inet_ntop(af, src, dst, len);
   if (res == NULL)
      handleErr(status, errLvl, message);
   return res;
}

int Getnameinfo(const struct sockaddr* sa, socklen_t salen, char* host, socklen_t hostlen, char* serv,
                socklen_t servlen, int flags, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Getnameinfo fail";

   int res = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
   if (res != 0)
      handleErr(status, errLvl, message);
   return res;
}

int Getaddrinfo(char* node, char* service, struct addrinfo* hints, struct addrinfo** res, status_t status,
                errLevel_t errLvl, char* message) {
   if (!message)
      message = "Getaddrinfo fail";

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

int GetUDP_addrinfo(char* node, char* service, struct addrinfo** res, int flags, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "GetUDP_addrinfo fail";

   struct addrinfo hints;

   memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_INET;
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags    = flags;

   return Getaddrinfo(node, service, &hints, res, status, errLvl, message);
}

int GetTCP_addrinfo(char* node, char* service, struct addrinfo** res, int flags, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "GetTCP_addrinfo fail";

   struct addrinfo hints;

   memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags    = flags;

   return Getaddrinfo(node, service, &hints, res, status, errLvl, message);
}

int Socket(int domain, int type, int protocol, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Socket fail";

   int sock = socket(domain, type, protocol);
   if (sock == -1)
      handleErr(status, errLvl, message);
   return sock;
}

/*
The sendto() function shall send a message through a connection-mode or
 connectionless-mode socket. If the socket is connectionless-mode,
 the message shall be sent to the address specified by dest_addr.
 If the socket is connection-mode, dest_addr shall be ignored.
*/
ssize_t Sendto(int fd, const void* buf, size_t n, int flags, const struct sockaddr* addr, socklen_t addr_len,
               status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Sendto fail";

   ssize_t res = sendto(fd, buf, n, flags, addr, addr_len);
   if (res != (ssize_t)n)
      handleErr(status, errLvl, message);
   return res;
}

ssize_t Recvfrom(int fd, void* restrict buf, size_t n, int flags, struct sockaddr* addr, socklen_t* restrict addr_len,
                 status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Recvfrom fail";

   ssize_t nread = recvfrom(fd, buf, n, flags, addr, addr_len);
   if (nread == -1)
      handleErr(status, errLvl, message);
   return nread;
}

int Bind(int fd, const struct sockaddr* addr, socklen_t len, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Bind fail";

   int res = bind(fd, addr, len);
   if (res != 0)
      handleErr(status, errLvl, message);
   return res;
}

int Listen(int sockfd, int backlog, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Listen fail";

   int res = listen(sockfd, backlog);
   if (res != 0)
      handleErr(status, errLvl, message);

   return res;
}

int Accept(int fd, struct sockaddr* addr, socklen_t* restrict addr_len, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Accept fail";

   int res = accept(fd, addr, addr_len);
   if (res == -1)
      handleErr(status, errLvl, message);
   return res;
}

int Connect(int fd, const struct sockaddr* addr, socklen_t len, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Connect fail";

   int res = connect(fd, addr, len);
   if (res != 0)
      handleErr(status, errLvl, message);
   return res;
}

ssize_t readn(int fd, void* vptr, ssize_t n, const char* delimiter) {
   ssize_t nleft;
   ssize_t nread;
   char* ptr;
   ptr   = vptr;
   nleft = n;
   while (nleft > 0) {
      if ((nread = read(fd, ptr, nleft)) < 0) {
         //            if (errno == EINTR)
         //                nread = 0; /* and call read() again */
         //            else
         return (-1);
      } else if (nread == 0) {
         break; /* EOF */
      }
      nleft -= nread;

      if (delimiter != NULL && ptr[nread - 1] == *delimiter) {
         nleft += 1;
         break;
      }

      ptr += nread;
   }
   return (n - nleft); /* return >= 0 */
}

size_t writn(int fd, const void* vptr, size_t n) {
   size_t nleft;
   ssize_t nwritten;
   const char* ptr;
   ptr   = vptr;
   nleft = n;
   while (nleft > 0) {
      if ((nwritten = write(fd, ptr, nleft)) <= 0) {
         //            if (errno == EINTR)
         //                nwritten = 0; /* and call write again */
         //            else
         return (-1); /* error */
      }
      nleft -= nwritten;
      ptr += nwritten;
   }
   return (n);
}

size_t Writn(int fd, const void* vptr, size_t n, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Writn fail";

   if (writn(fd, vptr, n) != n)
      handleErr(status, errLvl, message);
   return n;
}

ssize_t Recv(int socket, void* buffer, size_t length, int flags, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Recv fail";

   ssize_t nread = recv(socket, buffer, length, flags);
   if (nread == -1)
      handleErr(status, errLvl, message);
   return nread;
}

ssize_t Send(int socket, const void* buffer, size_t length, int flags, status_t status, errLevel_t errLvl, char* message) {
   if (!message)
      message = "Send fail";

   ssize_t res = send(socket, buffer, length, flags);
   if (res != (ssize_t)length)
      handleErr(status, errLvl, message);
   return res;
}

int Setsockopt(int fd, int level, int optname, const void* optval, socklen_t optlen, status_t status, errLevel_t errLvl,
               char* message) {
   if (!message)
      message = "Setsockopt fail";

   int res = setsockopt(fd, level, optname, optval, optlen);
   if (res != 0)
      handleErr(status, errLvl, message);
   return res;
}

int Select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout, status_t status,
           errLevel_t errLvl, char* message) {
   if (!message)
      message = "Select fail";

   int res = select(nfds, readfds, writefds, exceptfds, timeout);
   if (res == -1)
      handleErr(status, errLvl, message);
   return res;
}

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr* sa) {
   if (sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in*)sa)->sin_addr);
   }

   return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

size_t Strncpy(char* dest, const char* src, size_t n) {
   size_t i;
   for (i = 0; i < n && src[i] != '\0'; i++)
      dest[i] = src[i];

   size_t len = i;

   for (; i < n; i++)
      dest[i] = '\0';

   return len;
}

ssize_t freadn(void* buf, size_t size, size_t n, FILE* f) {
   size_t nleft;
   size_t nread;
   char* ptr;
   ptr   = buf;
   nleft = n;
   while (nleft > 0) {
      if ((nread = fread(ptr, size, nleft, f)) == 0) {
         if (ferror(f) != 0) {
            return (-1);
         }
         break;
      }
      nleft -= nread;
      ptr += nread;
   }
   return (n - nleft); /* return >= 0 */
}

ssize_t fwriten(const void *ptr, size_t size, size_t n, FILE *s){
   size_t res = 0;
   size_t numLeft = n;
   while(1){
      res = fwrite(ptr, size, n, s);
      if(res == 0){
         if (ferror(s) != 0) {
            return (-1);
         }
      }
      numLeft -= res;
      if(numLeft == 0) return n;
   }
}
