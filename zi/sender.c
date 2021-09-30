#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "util.h"

void usage();

void job(int sec, int delay, bool t, bool u, char* string, char* ip, char* ports[20], int numPorts);
int getUdpSock(char* node, char* port);
int connectTcpSock(char* node, char* port);
void sendOneTcp(int sock, char* string, int sec, char* port);
void sendOneUdp(int sock, char* string, int sec, struct sockaddr* pSockaddr, socklen_t addrlen);
// return 1 invalid args
// 2 getAddrInfo fail
// 3 socket create fail
int main(int argc, char* argv[]) {
   int ch;

   int sec      = 0;
   int delay    = 0;
   bool t       = false;
   bool u       = false;
   char* string = "";
   char* ip     = "";
   char* ports[20];
   int numPorts = 0;

   while ((ch = getopt(argc, argv, "r:d:tu")) != -1) {
      switch (ch) {
      case 'r':
         sec = atoi(optarg);
         debug_printf("Sec %d\n", sec);
         break;
      case 'd':
         delay = atoi(optarg);
         debug_printf("Delay %d\n", delay);
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

   if (argc < 3) {
      usage();
      return 1;
   }
   string = argv[0];
   ip     = argv[1];
   argv += 2;
   argc -= 2; // argc fixup

   if (argc < 1 || argc > 20) { // added fixup 2
      usage();
      return 1;
   }

   for (int i = 0; i < argc && i < 20; i++) {
      ports[i] = argv[i];
      numPorts++;
   }

   debug_printf("Num ports: %d\n", numPorts); // dodano

   job(sec, delay, t, u, string, ip, ports, numPorts);

   return (0);
}

void handle_alarm(int sig) {
   (void)sig;
   debug_printf("Signal recv\n"); // dodano
}

void job(int sec, int delay, bool t, bool u, char* string, char* ip, char* ports[20], int numPorts) {
   int udpSock = -1;
   int tcpSock = -1;

   signal(SIGALRM, handle_alarm);
   siginterrupt(SIGALRM, 1);                       // SIGINTERRUPT FIXUP za recvfrom

   if (u) { udpSock = getUdpSock(NULL, "0"); }

   for (int i = 0; i < numPorts; i++) {
      if (t) {
         debug_printf("Sending tcp port %s\n", ports[i]); // dodano

         tcpSock = connectTcpSock(ip, ports[i]);

         // CONTINUE fixup if(tcpSock == -1){continue;}
         if (tcpSock != -1) { // dodan if
            sendOneTcp(tcpSock, string, sec, ports[i]);
            close(tcpSock);
         }
      }

      if (u) {
         debug_printf("Sending udp port %s\n", ports[i]); // dodano

         struct addrinfo* res;
         GetUDP_addrinfo(ip, ports[i], &res, 0, IGNORE, DBG, NULL);
         sendOneUdp(udpSock, string, sec, res->ai_addr, res->ai_addrlen);
         freeaddrinfo(res);
      }

      if (delay > 0) { sleep(delay); }
      debug_printf("\n"); // dodano
   }

   if (u) close(udpSock);
}
void sendOneUdp(int sock, char* string, int sec, struct sockaddr* pSockaddr, socklen_t addrlen) {
   Sendto(sock, string, strlen(string), 0, pSockaddr, addrlen, IGNORE, DBG, NULL);

   if (sec > 0) {
      alarm(sec);
   } else {
      return;
   }

   struct sockaddr pSockaddr2;
   socklen_t addrlen2 = addrlen; // dodano

   char buf[1];

   ssize_t res = Recvfrom(sock, buf, 1, 0, &pSockaddr2, &addrlen2, IGNORE, DBG, NULL); // sockaddr2 i addlen2 za usporedbu s prethodnim

   if (res >= 0 && memcmp(pSockaddr, &pSockaddr2, sizeof(struct sockaddr)) == 0 &&
       addrlen == addrlen2) { // res != 0 --> res >= 0 fixup i dodano memcmp te addrlen == addrlen2

      struct sockaddr_in* inAddr = (struct sockaddr_in*)pSockaddr;
      printf("UDP %d open\n", ntohs(inAddr->sin_port));
   }

   if (sec > 0) { alarm(0); } // fixup alarm sec to alarm 0 da se otkaže alarm za svaki slučaj
}

void sendOneTcp(int sock, char* string, int sec, char* port) {
   Send(sock, string, strlen(string), 0, IGNORE, DBG, NULL);

   if (sec > 0) {
      alarm(sec); // cekaj sec sekundi na odgovor inace return bez cekanja
   } else {
      return;
   }

   char buf[1];
   ssize_t res = Recv(sock, buf, 1, 0, IGNORE, DBG, NULL);
   if (res >= 0) { printf("TCP %s open\n", port); } // res != 0 fixup

   if (sec > 0) { alarm(0); } // fixup alarm sec to alarm 0 da se otkaže alarm za svaki slučaj
}

int connectTcpSock(char* node, char* port) {
   int tcpSock = -1;
   struct addrinfo* res;
   struct addrinfo* rp = NULL;

   GetTCP_addrinfo(node, port, &res, 0, 2, WARN, NULL);
   for (rp = res; rp != NULL; rp = rp->ai_next) {
      tcpSock = Socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol, IGNORE, DBG, NULL);
      if (tcpSock == -1) continue;
      if (Connect(tcpSock, rp->ai_addr, rp->ai_addrlen, IGNORE, DBG, "connect failed") != -1) break; /* Success */
      close(tcpSock);
   }
   freeaddrinfo(res);

   if (rp == NULL) {
      debug_printf("Unable to connect\n");
      return -1; // dodan return -1 ako unable to connect
   }

   return tcpSock;
}

int getUdpSock(char* node, char* port) {
   struct addrinfo* res;
   GetUDP_addrinfo(node, port, &res, 0, 2, FATAL, NULL);
   struct addrinfo* res1 = res;
   int sock =
       Socket(res1->ai_family, res1->ai_socktype, res1->ai_protocol, 3, FATAL, "Could not complete socket creation");
   freeaddrinfo(res);

   return sock;
}

void usage() { fprintf(stderr, "Usage: sender [-r sec] [-d delay] [-t |-u] \"string\" ip_adresa port ..."); }
