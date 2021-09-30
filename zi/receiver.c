#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

#include "debug.h"
#include "util.h"

void handle_alarm(int sig) {
   debug_printf("Sig recv\n"); // added
   if (sig == SIGALRM) exit(0);
}

void usage();

_Noreturn void job(bool t, bool u, char* ports[20], char* string, int numPorts);
void listenOnSocks(bool t, bool u, char** ports, int tcpSocks[20], int udpSocks[20], int numPorts);
bool checkFromUdp(bool u, int udpSocks[20], int numPorts, int fd);
void printMess(int i, struct sockaddr_storage* storage, socklen_t sockl, char* proto); // chaanged storage to *storage and added *proto
// return 1 bad args
// 2 bind udp fail
// 3 bind tcp fail
// 4 select fail
// 5 recvFrom fail
// 6 sendTo fail
// 7 accept failt
// 8 recv fail
// 9 send fail
int main(int argc, char* argv[]) {
   int ch;

   int timeout = 10;
   bool t      = false;
   bool u      = false;
   char* ports[20];
   char* string;
   int numPorts = 0;

   while ((ch = getopt(argc, argv, "i:tu")) != -1) {
      switch (ch) {
      case 'i':
         timeout = atoi(optarg);
         debug_printf("timeout %d\n", timeout);
         break;
      case 't':
         {
            t = true;
            debug_printf("Tcp is true\n");
            break;
         }
      case 'u':
         {
            u = true;
            debug_printf("Udp is true\n");
            break;
         }
      default:
         usage();
         return 1;
      }
   }

   if (!t && !u) {
      t = true;
      u = true;
   }

   argc -= optind;
   argv += optind;

   if (argc < 2) { // argc < 3 fixup
      usage();
      return 1;
   }
   string = argv[0];
   argv += 1; // argv += 2 fixup
   argc -= 1; // added, fixup

   if (argc < 1 || argc > 20) { // added fixup 2
      usage();
      return 1;
   }

   for (int i = 0; i < argc && i < 20; i++) {
      ports[i] = argv[i];
      numPorts++;
   }

   signal(SIGALRM, handle_alarm);
   siginterrupt(SIGALRM, 1); // fixup

   if (timeout > 0) {
      alarm(timeout);
   } else {
      alarm(10);
   }

   job(t, u, ports, string, numPorts);

   return (0);
}

_Noreturn void job(bool t, bool u, char* ports[20], char* string, int numPorts) {
   printf("%7s Local:%15s Remote:\n", " ", " ");

   int tcpSocks[20];
   int udpSocks[20];

   listenOnSocks(t, u, ports, tcpSocks, udpSocks, numPorts);

   fd_set master, read_fds;

   FD_ZERO(&master); // clear the master and temp sets
   FD_ZERO(&read_fds);

   for (int i = 0; i < numPorts; i++) {
      if (t) FD_SET(tcpSocks[i], &master);
      if (u) FD_SET(udpSocks[i], &master);
   }

   int fdmax = t ? tcpSocks[numPorts - 1] : udpSocks[numPorts - 1];

   int stringLen = strlen(string) + 1; // moved outside of while
   char buf[stringLen];
   struct sockaddr_storage sockAddr;
   socklen_t sockl      = sizeof(sockAddr);
   char* OKmessage      = "OK\n";            // dodano
   size_t OKmessage_len = strlen(OKmessage); // dodano

   while (true) {
      read_fds = master; // copy
      Select(fdmax + 1, &read_fds, NULL, NULL, NULL, 4, FATAL,
             NULL); /////////// max + 1 jer gleda do dano isključivo...

      debug_printf("Select done\n"); // added

      // run through the existing connections looking for data to read
      for (int i = 0; i <= fdmax; i++) {
         if (FD_ISSET(i, &read_fds)) { // we got one!!

            if (checkFromUdp(u, udpSocks, numPorts, i)) {
               Recvfrom(i, buf, stringLen, 0, (struct sockaddr*)&sockAddr, &sockl, 5, FATAL, NULL);

               if (strncmp(buf, string, stringLen) == 0) {
                  printMess(i, &sockAddr, sockl, "UDP"); // sve ostalo potrebno ignorirati fixup

                  debug_printf("Send ok udp\n"); // added

                  Sendto(i, OKmessage, OKmessage_len, 0, (struct sockaddr*)&sockAddr, sockl, 6, FATAL, NULL);
               }
            } else { // inace je tcp sock

               int acceptFd = Accept(i, (struct sockaddr*)&sockAddr, &sockl, 7, FATAL, NULL);

               Recv(acceptFd, buf, stringLen, 0, 8, FATAL, NULL);

               if (strncmp(buf, string, stringLen) == 0) {
                  printMess(i, &sockAddr, sockl, "TCP"); // sve ostalo potrebno ignorirati fixup

                  debug_printf("Send ok tcp\n"); // added

                  Send(acceptFd, OKmessage, OKmessage_len, 0, 9, FATAL, NULL); // send i --> send acceptFd fixup
               }

               close(acceptFd);
            }

            debug_printf("Handled new message\n"); // dodano
         }                                         // END got new incoming connection
      }                                            // END looping through file descriptors
   }

   for (int i = 0; i < numPorts; i++) {
      if (t) close(tcpSocks[i]);
      if (u) close(udpSocks[i]);
   }
}

void formatOut(char* dest, char* addr, char* port) { // added
   strcat(dest, addr);
   strcat(dest, ":");
   strcat(dest, port);
}

void printMess(int i, struct sockaddr_storage* storage, socklen_t sockl,
               char* proto) { // switched order of getnameinfos for local and remote and added proto arg
   char addr_str[NI_MAXHOST];
   char port[NI_MAXSERV];
   char printStr[NI_MAXHOST + 1 + NI_MAXSERV + 1];

   struct sockaddr_storage storage2;
   socklen_t sockl2 = sizeof(struct sockaddr_storage); // sockl2 initialization fixup

   getsockname(i, (struct sockaddr*)&(storage2), &sockl2); // storage to storage2 fixup
   Getnameinfo((struct sockaddr*)&(storage2), sockl2, addr_str, NI_MAXHOST, port, NI_MAXSERV,
               NI_NUMERICSERV | NI_NUMERICHOST, 10, FATAL, NULL);
   // printf("Local: %s:%s\n", addr_str, port);                commented out

   printStr[0] = '\0';
   formatOut(printStr, addr_str, port);
   printf("%-7s %-21s ", proto, printStr); // added

   Getnameinfo((struct sockaddr*)(storage), sockl, addr_str, NI_MAXHOST, port, NI_MAXSERV,
               NI_NUMERICSERV | NI_NUMERICHOST, 10, FATAL, NULL);
   // printf("Remote: %s:%s\n\n", addr_str, port);              commented out

   printStr[0] = '\0';
   formatOut(printStr, addr_str, port);
   printf("%-21s\n", printStr); // added
}

bool checkFromUdp(bool u, int udpSocks[20], int numPorts, int fd) {
   if (!u) return false;

   for (int i = 0; i < numPorts; i++) {
      if (udpSocks[i] == fd) return true;
   }

   return false;
}

void listenOnSocks(bool t, bool u, char** ports, int tcpSocks[20], int udpSocks[20], int numPorts) {
   for (int i = 0; i < numPorts; i++) {
      if (u) udpSocks[i] = bindUDP(ports[i], 2);
      if (t) {
         tcpSocks[i] = bindTCP(ports[i], 3);
         Listen(tcpSocks[i], 10, 7, FATAL, "listen fail");
      }
   }
}

void usage() { fprintf(stderr, "Usage: receiver [-i timeout] [-t |-u] \"string\" port ..."); }
