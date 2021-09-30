#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "debug.h"
#include "util.h"

#define HELLO_LEN 5

void usage()
{
    fprintf(stderr, "Usage:  ./UDP_server [-l port] [-p payload]\n");
}

// status 0 ok
// status 1 bad usage
// status 2 getAddrInfo failed
// status 3 socket creation failed
// status 4 bind failed
// status 5 recvFrom failed
// status 6 sendTo failed
int main(int argc, char *argv[])
{
    int ch;
    char *port = "1234";
    char payload[513];
    strcpy(payload, "PAYLOAD:");

    while ((ch = getopt(argc, argv, "l:p:")) != -1)
    {
        switch (ch)
        {
        case 'l':
            port = optarg;
            debug_printf("Port %s\n", port);
            break;
        case 'p':
            strcat(payload, optarg);
            debug_printf("Payload %s\n", payload);
            break;
        default:
            usage();
            return 1;
        }
    }

    strcat(payload, "\n");
    argc -= optind;
    argv += optind;

    if (argc > 0)
    {
        usage();
        return 1;
    }

    int sfd;
    struct sockaddr cli;
    struct addrinfo hints, *res;
    char buf[HELLO_LEN + 1];
    socklen_t clilen;
    int msglen;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_INET6, AF_UNSPEC
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // socket za pasivni open, *.port

    Getaddrinfo(NULL, port, &hints, &res, 2, "getAddrInfo for local address failed\n", true); // na svim adrr
    sfd = Socket(res->ai_family, res->ai_socktype, res->ai_protocol, 3, "socket creation failed\n");
    Bind(sfd, res->ai_addr, res->ai_addrlen, 4, "Unable to bind\n");

    debug_printf("Bound to %s\n", port);
    while (1)
    {
        clilen = sizeof(cli);
        msglen = Recvfrom(sfd, buf, HELLO_LEN, 0, &cli, &clilen, 5, "partial/failed read\n");
        buf[msglen] = '\0';

        if (!strcmp("HELLO", buf))
        {
            Sendto(sfd, payload, strlen(payload), 0, &cli, clilen, 6, "partial/failed sendTo bot");
            struct sockaddr_in * cli1 = (struct sockaddr_in *)&cli;
            char adr[INET_ADDRSTRLEN];
            inet_ntop(cli1->sin_family, &cli1->sin_addr, adr, INET_ADDRSTRLEN);
            debug_printf("Sending payload to %s %d\n", adr, ntohs(cli1->sin_port));
        }
    }

    close(sfd);
    freeaddrinfo(res);
    return 0;
}
