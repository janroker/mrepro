#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>

#include "debug.h"
#include "util.h"

#define RRQ_CODE 1
#define WRQ_CODE 2
#define DATA_CODE 3
#define ACK_CODE 4
#define ERROR_CODE 5

#define NOT_DEFINED 0
#define FILE_NOT_FOUND 1
#define ACCESS_VIOLATION 2
#define DISK_FULL 3
#define ILLEGAL_TFTP_OPERATION 4
#define UNKNOWN_PORT 5
#define FILE_ALREADY_EXISTS 6

#define MAX_FILENAME_L 255
#define BUF_LEN \
   (2 + MAX_FILENAME_L + 10 + 1 + 8 + 1 + 1) // 2 okteta | naziv_datoteke + /tftpboot/ | 1 oktet | nacin prijenosa | 1 oktet | + 1 viska za provjeru

static bool demon = false;

void recvfrom_alarm(int sig) { return; }

typedef struct Rq {
   uint16_t code;
   char data[];
} Rq;

struct Data {
   uint16_t code;
   uint16_t block_no;
   char data[512];
};

struct Ack {
   uint16_t code;
   uint16_t block_no;
};

struct Error {
   uint16_t code;
   uint16_t errCode;
};

void usage();
void job(char* port);
void handleRequest(Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen);
void myLog(bool error, char* fmt, ...);
void processRequest(int sock, Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen);
size_t nextStringStartingPoint(const char* str, size_t currPos, size_t len);
bool validateFilename(char* filename, bool* startsWith);
void sendErr(int sock, struct sockaddr_storage* recvAddr, socklen_t socklen, uint16_t errCode);
void transfer(int sock, struct sockaddr_storage* recvAddr, socklen_t socklen, char filename[266], bool octet);
// returns bytes not transfered to buf_tmp
size_t convertCRLF(const char bufFrom[512], char bufTo[512], size_t len1, size_t len2, size_t* numToSend, bool isFirst);

bool fillFilename(int sock, const Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen, bool startsWith,
                  size_t next, char filename[MAX_FILENAME_L + 10 + 1]);
bool checkExistance(int sock, const Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen, const char* filename);
bool checkAccess(int sock, Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen, const char* filename);
void receiveData(int sock, struct sockaddr_storage* recvAddr, socklen_t socklen, char filename[266], bool octet);
// return 0 ok
// return 1 bad args
// return 2 getaddrinfo fail
// return 3 socket creation fail
// return 4 bind fail
// return 5 recvfrom fail
// return 6 sendTo fail
// return 7 Fork fail
int main(int argc, char* argv[]) {
   char c;
   while ((c = getopt(argc, argv, "d")) != -1) {
      switch (c) {
      case 'd': {
         demon = true;
         debug_printf("isDaemon\n");
         break;
      }
      default: {
         usage();
         return 1;
      }
      }
   }

   argc -= optind;
   argv += optind;
   if (argc != 1) {
      usage();
      return 1;
   }
   char* portNameOrNumber = argv[0];

   job(portNameOrNumber);

   return 0;
}

void job(char* port) {
   struct addrinfo* res;
   GetUDP_addrinfo(NULL, port, &res, AI_PASSIVE, 2, FATAL, NULL);
   int firstSock = Socket(res->ai_family, res->ai_socktype, res->ai_protocol, 3, FATAL, NULL);
   Bind(firstSock, res->ai_addr, res->ai_addrlen, 4, FATAL, NULL);

   if (demon) {
      if (daemon(0, 0) < 0)
         errx(8, "Daemon fail");
   }

   char buf[BUF_LEN];
   struct sockaddr_storage recvAddr;
   socklen_t socklen;

   bool isChild = false;
   while (true) {
      size_t bufSize = sizeof(buf);
      ssize_t rez    = Recvfrom(firstSock, buf, bufSize, 0, (struct sockaddr*)&recvAddr, &socklen, 5, FATAL, NULL);

      if (rez == bufSize)
         buf[bufSize - 1] = '\0';
      else
         buf[rez] = '\0';

      int pid;
      if ((pid = fork()) == 0) {
         isChild = true;
         break;
      } else if (pid < 0)
         errx(7, "Fork failed");
   }

   close(firstSock);
   freeaddrinfo(res);

   if (isChild) {
      if (demon)
         openlog("jr51484:MrePro tftpserver", LOG_PID, LOG_FTP);

      handleRequest((Rq*)buf, &recvAddr, socklen);

      if (demon)
         closelog();
   }
}

void handleRequest(Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen) {
   struct addrinfo* res;
   GetUDP_addrinfo(NULL, "0", &res, AI_PASSIVE, 2, FATAL, NULL);
   int sock = Socket(res->ai_family, res->ai_socktype, res->ai_protocol, 3, FATAL, NULL);

   char addrStr[INET6_ADDRSTRLEN];
   Inet_ntop(recvAddr->ss_family, get_in_addr((struct sockaddr*)recvAddr), addrStr, sizeof addrStr, IGNORE, WARN, NULL);
   myLog(false, "%s->%s\n", addrStr, request->data);

   request->code = ntohs(request->code);
   debug_printf("Recv code %d\n", request->code);
   if (request->code == RRQ_CODE || request->code == WRQ_CODE) {
      processRequest(sock, request, recvAddr, socklen);
   } else {
      sendErr(sock, recvAddr, socklen, ILLEGAL_TFTP_OPERATION);
      debug_printf("Received invalid request code: %d", request->code);
   }

   freeaddrinfo(res);
   close(sock);
}

void processRequest(int sock, Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen) {
   bool startsWith;
   if (!validateFilename(request->data, &startsWith)) { // Invalid name... // ok je jer smo na buf naljepili \0
      sendErr(sock, recvAddr, socklen, FILE_NOT_FOUND);
      return;
   }

   size_t next = nextStringStartingPoint(request->data, 0, BUF_LEN);
   if (next == BUF_LEN) { // err
      sendErr(sock, recvAddr, socklen, NOT_DEFINED);
      return;
   }

   char filename[MAX_FILENAME_L + 10 + 1]; // max + "/ftftboot/" + \0
   if (!fillFilename(sock, request, recvAddr, socklen, startsWith, next, filename))
      return;

   if (!checkExistance(sock, request, recvAddr, socklen, filename))
      return;

   if (!checkAccess(sock, request, recvAddr, socklen, filename))
      return;

   size_t filenameSize = strlen(filename);
   char rqType[BUF_LEN - filenameSize];
   Strncpy(rqType, request->data + next, BUF_LEN - filenameSize);
   rqType[BUF_LEN - filenameSize - 1] = '\0';

   debug_printf("Received filename %s; Command %s;\n", filename, rqType);

   bool type = false; // netascii
   if (strcasecmp(rqType, "netascii") == 0)
      type = false;
   else if (strcasecmp(rqType, "octet") == 0)
      type = true;
   else {
      sendErr(sock, recvAddr, socklen, ILLEGAL_TFTP_OPERATION);
      return;
   }

   if (request->code == RRQ_CODE)
      transfer(sock, recvAddr, socklen, filename, type);
   else
      receiveData(sock, recvAddr, socklen, filename, type);
}

size_t convertFromCRLF(char data[512], size_t received) {
   char data2[512];
   int pos2 = 0;
   for (int i = 0; i < received; i++) {
      if (data[i] != '\r') {
         data2[pos2] = data[i];
         pos2++;
      }
   }
   memcpy(data, data2, pos2);
   return pos2;
}

void receiveData(int sock, struct sockaddr_storage* recvAddr, socklen_t socklen, char filename[266], bool octet) {
   FILE* f = fopen(filename, "wb");
   struct Data data;
   int ackNum          = 0;
   int counter         = 1;
   bool isLast         = false;
   ssize_t receivedNum = 516;
   struct Ack ack;
   ack.code = htons(ACK_CODE);

   fd_set rset;
   FD_ZERO(&rset);
   FD_SET(sock, &rset);

   sigset_t sigset_alrm, sigset_empty;
   sigemptyset(&sigset_alrm);
   sigemptyset(&sigset_empty);
   sigaddset(&sigset_alrm, SIGALRM);

   signal(SIGALRM, recvfrom_alarm);
   sigprocmask(SIG_BLOCK, &sigset_alrm, NULL);

   while (true) {
      ack.block_no = htons(ackNum);
      Sendto(sock, &ack, sizeof ack, 0, (struct sockaddr*)recvAddr, socklen, 6, FATAL, NULL);

      if (isLast) // done
         break;

      alarm(5);
      int n = pselect(sock + 1, &rset, NULL, NULL, NULL, &sigset_empty);
      if (n < 0) {
         if (errno == EINTR) {
            debug_printf("istekla vremenska kontrola\n");
            break;
         } else
            errx(5, "recvfrom file error");
      } else if (n != 1)
         errx(5, "pselect file error");

      receivedNum = Recvfrom(sock, &data, sizeof data, 0, NULL, NULL, 5, FATAL, NULL);

      if (receivedNum < 516)
         isLast = true;

      if (!octet)
         receivedNum = convertFromCRLF(data.data, receivedNum - 4) + 4;

      if (ntohs(data.code) != DATA_CODE) {
         sendErr(sock, recvAddr, socklen, ILLEGAL_TFTP_OPERATION);
         return;
      }

      ackNum = ntohs(data.block_no);
      if (ackNum == counter) {
         counter++;

         if (fwriten(data.data, sizeof(char), receivedNum - 4, f) == -1) {
            if (errno == ENOSPC)
               sendErr(sock, recvAddr, socklen, DISK_FULL);
            else
               sendErr(sock, recvAddr, socklen, NOT_DEFINED);

            return;
         }
      }
   }

   fclose(f);
}

bool checkAccess(int sock, Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen, const char* filename) {
   if (request->code == RRQ_CODE) {
      if (access(filename, R_OK) != 0) {
         sendErr(sock, recvAddr, socklen, ACCESS_VIOLATION);
         return false;
      }
   }
   return true;
}
bool checkExistance(int sock, const Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen, const char* filename) {
   bool fileExists = access(filename, F_OK) == 0;
   if (!fileExists && request->code == RRQ_CODE) { // ne postoji
      sendErr(sock, recvAddr, socklen, FILE_NOT_FOUND);
      return false;
   } else if (fileExists && request->code == WRQ_CODE) {
      sendErr(sock, recvAddr, socklen, FILE_ALREADY_EXISTS);
      return false;
   }
   return true;
}
bool fillFilename(int sock, const Rq* request, struct sockaddr_storage* recvAddr, socklen_t socklen, bool startsWith,
                  size_t next, char filename[MAX_FILENAME_L + 10 + 1]) {
   if (startsWith) {                          // startsWith /tftpboot/
      if (next > (MAX_FILENAME_L + 10 + 1)) { // u nextu je duljina prethodnog i pocetni znak sljedeceg str
         sendErr(sock, recvAddr, socklen, FILE_NOT_FOUND);
         return false;
      }

      Strncpy(filename, request->data, MAX_FILENAME_L + 10 + 1);
   } else {
      if (next > (MAX_FILENAME_L + 1)) { // u nextu je duljina prethodnog i pocetni znak sljedeceg str
         sendErr(sock, recvAddr, socklen, FILE_NOT_FOUND);
         return false;
      }

      Strncpy(filename, "/tftpboot/", 10);
      Strncpy(filename + 10, request->data, MAX_FILENAME_L + 1);
   }
   filename[MAX_FILENAME_L + 10] = '\0';
   return true;
}

void transfer(int sock, struct sockaddr_storage* recvAddr, socklen_t socklen, char filename[266], bool octet) {
   FILE* f = fopen(filename, "rb"); // file exists so should not return NULL !
   struct Data data;
   char buf_tmp[512];
   struct Ack ack;
   data.code = htons(DATA_CODE);

   debug_printf("isOctet: %d\n", octet);

   fd_set rset;
   FD_ZERO(&rset);
   FD_SET(sock, &rset);

   sigset_t sigset_alrm, sigset_empty;
   sigemptyset(&sigset_alrm);
   sigemptyset(&sigset_empty);
   sigaddset(&sigset_alrm, SIGALRM);

   signal(SIGALRM, recvfrom_alarm);
   sigprocmask(SIG_BLOCK, &sigset_alrm, NULL);

   int timeoutCNT         = 0;
   int lastPckNum         = 0;
   int nextPckNum         = 1;
   ssize_t r              = 1;
   bool retransmit        = false;
   size_t leftFromPrevBuf = 0;
   size_t numToSend       = 0;
   size_t prevSent        = 516;
   while (true) {
      if (r == 0)
         break;

      data.block_no = htons(nextPckNum);
      if (lastPckNum < nextPckNum) {
         r = freadn(buf_tmp + leftFromPrevBuf, sizeof(char), 512 - leftFromPrevBuf, f);
         if (!octet) {
            leftFromPrevBuf = convertCRLF(buf_tmp, data.data, leftFromPrevBuf + r, 512, &numToSend, nextPckNum == 1);
            strncpy(buf_tmp, (buf_tmp + 512 - leftFromPrevBuf), leftFromPrevBuf);
         } else {
            memcpy(data.data, buf_tmp, r);
         }
         if (r == -1) {
            debug_printf("read file error");
            break;
         }
      }

      numToSend = 4 + (octet ? r : numToSend);
      if (numToSend != 4 || prevSent == 516) {
         if (lastPckNum < nextPckNum || retransmit) {
            debug_printf("Send %d\n", numToSend);
            Sendto(sock, &data, numToSend, 0, (struct sockaddr*)recvAddr, socklen, 6, FATAL, NULL);
         }
      }

      if (!retransmit)
         lastPckNum = nextPckNum;

      alarm(1);
      int n = pselect(sock + 1, &rset, NULL, NULL, NULL, &sigset_empty);
      if (n < 0) {
         if (errno == EINTR) {
            debug_printf("timeout %d\n", timeoutCNT);
            if (timeoutCNT == 3) {
               sendErr(sock, recvAddr, socklen, NOT_DEFINED);
               return; // 3 retransmisije vec napravljene
            }
            retransmit = true;
            timeoutCNT++;
            continue;
         } else
            errx(5, "recvfrom error");
      } else if (n != 1)
         errx(5, "pselect error");

      Recvfrom(sock, &ack, sizeof ack, 0, NULL, NULL, 5, FATAL, NULL);

      debug_printf("ack code %d!\n", ntohs(ack.block_no));
      if(ntohs(ack.code) == ACK_CODE) {
         if (ntohs(ack.block_no) == lastPckNum) {
            nextPckNum++;
            timeoutCNT = 0;
            retransmit = false;
            prevSent   = numToSend;
         }
         else if (ntohs(ack.block_no) > lastPckNum) {
            sendErr(sock, recvAddr, socklen, NOT_DEFINED);
            return;
         }
      }else{
         sendErr(sock, recvAddr, socklen, ILLEGAL_TFTP_OPERATION);
         return;
      }
   }
   fclose(f);
}

// returns bytes not transfered to buf_tmp
size_t convertCRLF(const char bufFrom[512], char bufTo[512], size_t len1, size_t len2, size_t* numToSend, bool isFirst) {
   size_t pos2 = 0;
   size_t i    = 0;
   for (; i < len1 && pos2 < len2; i++, pos2++) {
      if (bufFrom[i] == '\n' && ((i == 0 && isFirst) || (i != 0 && bufFrom[i - 1] != '\r'))) { // na nultom mjestu ak je \n je sigurno zamijenjen vec ako se ne radi o prvom prvcatom...
         bufTo[pos2] = '\r';
         pos2++;
      }
      if (pos2 < len2)
         bufTo[pos2] = bufFrom[i];
   }

   if (pos2 <= len2) { // zadnji nije bio \n
      *numToSend = pos2;
      return len1 - i;
   } else {
      *numToSend = len2;
      return len1 - i + 1; // jer nismo skopirali taj \n
   }
}

void sendErr(int sock, struct sockaddr_storage* recvAddr, socklen_t socklen, uint16_t errCode) {
   struct Error error;
   error.code     = htons(ERROR_CODE);
   error.errCode  = htons(errCode);
   
   size_t sizeErr = 5;
   char buf[sizeErr];
   memcpy(buf, &error, 4);
   buf[4] = '\0';

   myLog(true, "TFTP ERROR %d\n", errCode);
   Sendto(sock, buf, sizeErr, 0, (struct sockaddr*)recvAddr, socklen, 6, FATAL, NULL);
}

size_t nextStringStartingPoint(const char* str, size_t currPos, size_t len) {
   size_t next = currPos;
   while (next < len && str[next])
      next++;

   return (next + 1); // ako == len onda gotovo inace je na poziciji...
}

bool validateFilename(char* filename, bool* startsWith) {
   int pos;
   for (pos = 0; filename[pos] != '/' && filename[pos] != '\0'; pos++)
      ;

   bool containsSlash = filename[pos] == '/';
   *startsWith        = false;
   if (containsSlash) {
      *startsWith = strncmp("/tftpboot/", filename, 10) == 0;
   }

   if (!*startsWith && containsSlash)
      return false; // ne pocinje s prefiksom i ima slash onda nevalja
   else if (!*startsWith)
      return true; // ne pocinje s prefiksom i nema slash OK

   // ako pocinje s prefiksom onda ne smije imat više slashova
   for (pos = 10; filename[pos] != '/' && filename[pos] != '\0'; pos++)
      ;
   if (filename[pos] == '/')
      return false;

   return true;
}

void myLog(bool error, char* fmt, ...) {
   va_list args;
   va_start(args, fmt);
   if (demon) {
      char format[1024];
      vsprintf(format, fmt, args);
      syslog(LOG_INFO, format);
   } else {
      if (error) {
         vfprintf(stderr, fmt, args);
         fflush(stderr);
      } else {
         vfprintf(stdout, fmt, args);
         fflush(stdout);
      }
      // fprintf(stdout, "");
   }
   va_end(args);
}

void usage() { fprintf(stderr, "tftpserver [-d]  port_name_or_number\n"); }
