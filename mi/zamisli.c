#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include "util.h"
#include "debug.h"

#define BACKLOG 10

static uint32_t cntID = 0;

struct PAIR* pairs;
int pairsSize;
int timeout = 5;

struct PAIR {
    uint32_t clid;
    uint16_t random; // host ord
    uint8_t cnt;
};

struct INIT_A {
    char command[2];
    uint16_t nula;
    uint32_t clid;
};

struct INIT {
    char command[4];
    uint16_t max;
    uint16_t nula;
};

struct BROJ {
    char command[4];
    uint32_t clid;
    uint16_t nn;
    uint16_t xx;
};

struct RESP {
    char command[2];
    uint16_t nula;
    uint32_t clid;
    uint16_t nn;
    uint16_t port;
};

struct ODG {
    uint32_t clid;
    char str[39];
};

void handle_signal(int sig) {

}

void usage();

/*U prvoj poruci, klijent šalje broj max te od poslužitelja očekuje da generira („zamisli”) slučajan
        broj između 1 i max.
        Klijent zatim pokušava pogoditi taj zamišljeni broj.
*/
uint16_t generate1Max(uint16_t max);

// status 0 OK
// status 1 bad args
// 2 addrinfo failed
// 3 sock failed
// 4 bind failed
int main(int argc, char *argv[]) {
    srand(time(NULL));
    char *port = "1234";

    int ch;
    while ((ch = getopt(argc, argv, "t:")) != -1) {
        switch (ch) {
            case 't':
                timeout = atoi(optarg);
                debug_printf("Timeout %s\n", timeout);
                break;
            default:
                usage();
                return 1;
        }
    }

    argc -= optind;
    argv += optind;
    if (argc > 1 || argc < 0) {
        usage();
        return 1;
    }else if(argc == 1){
        port = argv[0];
    }

    struct addrinfo *res;
    GetUDP_addrinfo(NULL, port, &res, AI_PASSIVE, 2, "UDP server getAddrInfo failed", FATAL);
    int sock = Socket(res->ai_family, res->ai_socktype, res->ai_protocol, 3, "UDP server sock failed", FATAL);
    Bind(sock, res->ai_addr, res->ai_addrlen, 4, "UDP server unable to bind", FATAL);

    debug_printf("Bound to %s\n", port);
    struct BROJ req;
    pairsSize = 2;
    pairs = malloc(pairsSize * sizeof(struct PAIR));
    while(1) {
        struct sockaddr rec;
        socklen_t recLen;
        Recvfrom(sock, &req, sizeof(struct BROJ), 0, &rec, &recLen, IGNORE, "Recv from failed", DBG);

        struct INIT *init = (struct INIT *) &req;
        if(strcmp(init->command, "INIT") == 0){
            uint32_t clid = cntID;
            cntID++;
            uint16_t random = generate1Max(ntohs(init->max)); // xx je uint16_t zapis broja max u „network byte order”

            if(pairsSize < cntID){
                pairs = realloc(pairs, pairsSize * 2);
                pairsSize *= 2;
            }

            pairs[clid].clid = clid;
            pairs[clid].random = random;

            struct INIT_A resp;
            resp.clid = htonl(clid);
            resp.command[0] = 'I';
            resp.command[1] = 'D';

            Sendto(sock, &resp, sizeof(struct INIT_A), 0, &rec, recLen, IGNORE, "Send to failed", DBG);
        }else if(strcmp(req.command, "BROJ") == 0){
            uint32_t clid = ntohl(req.clid);
            if(clid >= cntID) {
                debug_printf("clid >= cntID\n");
                continue;
            }

            pairs[clid].cnt++;
            uint16_t random = ntohs(req.xx);
            if(pairs[clid].random == random) {
                struct RESP resp;
                resp.command[0] = 'O';
                resp.command[1] = 'K';
                resp.clid= htonl(clid);
                resp.nn = req.nn;

                struct addrinfo *res2;
                GetTCP_addrinfo(NULL, "0", &res2, AI_PASSIVE, 5, "getTcpAddrInfo failed", WARN);
                int sockfd = Socket(res2->ai_family, res2->ai_socktype, res2->ai_protocol, 2, "failed to create socket", WARN);
                int on = 1;
                Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int), IGNORE, "setsockopt failed", WARN);

                Bind(sockfd, res->ai_addr, res->ai_addrlen, 4, "Bind failed", WARN);
                debug_printf("Socket bound\n");

                resp.port = ((struct sockaddr_in *) &res2->ai_addr)->sin_port;
                Sendto(sock, &resp, sizeof(struct RESP), 0, &rec, recLen, IGNORE, "Send to failed", DBG);

                Listen(sockfd, BACKLOG, IGNORE, "Listen failed", WARN);

                signal(SIGALRM, &handle_signal);
                siginterrupt(SIGALRM, 1);
                alarm(timeout);

                int newfd = Accept(sockfd, NULL, NULL, IGNORE, "Accept failed", DBG);

                struct ODG odg;
                odg.clid = resp.clid;
                strcpy(odg.str, ":<–FLAG–MrePro–2020-2021-MI–>\n");

                Writn(newfd, &odg, sizeof(struct ODG), IGNORE, "Writn failed", DBG);

                alarm(0);

                close(newfd);
                close(sockfd);
                freeaddrinfo(res2);
            }
            else if(pairs[clid].random > random) {
                struct RESP resp;
                resp.command[0] = 'H';
                resp.command[1] = 'I';
                resp.clid = htonl(clid);
                resp.nn = req.nn;

                Sendto(sock, &resp, sizeof(struct RESP), 0, &rec, recLen, IGNORE, "Send to failed", DBG);
            }
            else if(pairs[clid].random < random) {
                struct RESP resp;
                resp.command[0] = 'L';
                resp.command[1] = 'O';
                resp.clid = htonl(clid);
                resp.nn = req.nn;

                Sendto(sock, &resp, sizeof(struct RESP), 0, &rec, recLen, IGNORE, "Send to failed", DBG);
            }
        }
    }


    freeaddrinfo(res);
    free(pairs);
    close(sock);
}

uint16_t generate1Max(uint16_t max){
    double r = ((double) rand()) / (RAND_MAX + 1.); // 0 ... 0.999...
    r = r * (max) + 1;
    return (r);
}

void usage(){
    fprintf(stderr, "Usage:  ./zamisli [-t timeout] [port]\n");
}
