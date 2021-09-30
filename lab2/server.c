#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include "debug.h"
#include "util.h"

#define HELLO_LEN 6
#define PAYLOAD_LEN 1025
#define BACKLOG 10

#define STDIN 0

#define PRINT "PRINT"
#define SET "SET"
#define QUIT "QUIT"

void usage();
void fillPayload(char *payload, size_t len);
int bindUDP(char *port);
int bindTCP(char *port);
bool handleStdin(char *payload, bool *payloadInit);
void handleTCPConnection(int tcpSock, fd_set *master, int *fdmax);
void handleUDPData(int udpSock, char *payload, bool payloadInit);
void handleTCPClient(int newfd, char *payload, bool payloadInit, fd_set *master);
void job(char *payload, char *udp_port, char *tcp_port, bool payloadInit);

// status 0 ok
// status 1 bad usage
// status 2 getAddrInfo failed
// status 3 socket creation failed
// status 4 bind failed
// status 5 recvFrom failed
// status 6 sendTo failed
// status 7 listen failed
// status 8 setsockopt failed
// status 9 select failed
int main(int argc, char *argv[]) {
    int ch;
    char *tcp_port = "1234";
    char *udp_port = "1234";
    char payload[PAYLOAD_LEN];
    bool payloadInit = false;

    while ((ch = getopt(argc, argv, "t:u:p:")) != -1) {
        switch (ch) {
            case 't':
                tcp_port = optarg;
                debug_printf("Tcp Port %s\n", tcp_port);
                break;
            case 'u':
                udp_port = optarg;
                debug_printf("Udp Port %s\n", udp_port);
                break;
            case 'p': {
                size_t len = Strncpy(payload, optarg, PAYLOAD_LEN);
                fillPayload(payload, len);
                payloadInit = true;
                debug_printf("Payload %s\n", payload);
                break;
            }
            default:
                usage();
                return 1;
        }
    }

    strcat(payload, "\n");
    argc -= optind;
    argv += optind;

    if (argc != 0) {
        usage();
        return 1;
    }

    job(payload, udp_port, tcp_port, payloadInit);

    return 0;
}

void usage() {
    fprintf(stderr, "./server [-t tcp_port] [-u udp_port] [-p popis]\n");
}

int bindUDP(char *port) {
    struct addrinfo *res;
    GetUDP_addrinfo(NULL, port, &res, AI_PASSIVE, 2, FATAL,
                    "getAddrInfo UDP for local address failed\n"); // na svim adrr
    int sfd = Socket(res->ai_family, res->ai_socktype, res->ai_protocol, 3, FATAL, "UDP socket creation failed\n");
    Bind(sfd, res->ai_addr, res->ai_addrlen, 4, FATAL, "Unable to UDP bind\n");

    freeaddrinfo(res);
    return sfd;
}

int bindTCP(char *port) {
    struct addrinfo *res, *p;
    GetTCP_addrinfo(NULL, port, &res, AI_PASSIVE, 2, FATAL,
                    "getAddrInfo TCP for local address failed\n"); // na svim adrr

    int on = 1; // SO_REUSEADDR
    int sfd = -1;
    for (p = res; p != NULL; p = p->ai_next) {
        sfd = Socket(res->ai_family, res->ai_socktype, res->ai_protocol, 3, DBG, "TCP socket creation failed\n");
        if (sfd < 0) {
            continue;
        }
        Setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int), 8, DBG, "set SO_REUSEADDR failed");

        if (Bind(sfd, res->ai_addr, res->ai_addrlen, 4, DBG, "Unable to TCP bind\n") < 0) {
            close(sfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        errx(4, "failed to bind TCP\n");
    }

    freeaddrinfo(res);
    return sfd;
}

void fillPayload(char *payload, size_t len) {
    switch (len) {
        case PAYLOAD_LEN: {
            payload[len - 1] = '\0';
            payload[len - 2] = '\n';
            payload[len - 3] = ':';
            break;
        } // nije uspio stavit \0
        case (PAYLOAD_LEN - 1): {
            payload[len - 1] = '\n'; // \0 je stavio na PAYLOAD_LEN-1 mjesto, a \n nije nigdje
            payload[len - 2] = ':';
            break;
        }
        case (PAYLOAD_LEN - 2): { // na predzadnjem i zadnjem mjestu je \0
            payload[len] = '\n'; // na predzadnje stavi \n
            payload[len - 1] = ':'; // ispred njega : prepisi ono sta je tamo
            break;
        }
        default: // \0 je stavio na len mjesto i na svako iza toga
        {
            payload[len] = ':';
            payload[len + 1] = '\n';
        }
    }
}

// returns true if exit
bool handleStdin(char *payload, bool *payloadInit) {
    int inLen = PAYLOAD_LEN + 4;
    char in[inLen]; // payload-len + space + SET + \0
    
    char delimiter='\n';
    ssize_t rd = readn(STDIN, in, inLen, &delimiter);
    
    if(rd == inLen) in[rd-1] = '\0';
    else in[rd] = '\0';
	
    if (rd == -1)
        debug_printf("STDIN err");
    else {
		debug_printf("inLen = %d; in = %s\n", rd, in);
        if (memcmp(in, QUIT, 4) == 0) return true; // break from while
        else if (memcmp(in, PRINT, 5) == 0) printf("%s", payload);
        else if (memcmp(in, SET, 3) == 0) {
            size_t len = Strncpy(payload, (in + 4), PAYLOAD_LEN);
            fillPayload(payload, len);
            *payloadInit = true;
            debug_printf("Payload %s\n", payload);
        } else
            warnx("Unknown command received\n");
    }
    return false;
}

void handleTCPConnection(int tcpSock, fd_set *master, int *fdmax) {
    int newfd;
    struct sockaddr_storage remoteaddr;
    char remoteIP[INET6_ADDRSTRLEN];

    socklen_t addrlen = sizeof remoteaddr;
    newfd = Accept(tcpSock, (struct sockaddr *) &remoteaddr, &addrlen, IGNORE, WARN, NULL);

    if (newfd != -1) {
        FD_SET(newfd, master); // add to master set
        if (newfd > *fdmax) *fdmax = newfd;
    }

    debug_printf("new connection from %s on socket %d\n",
                 inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr *) &remoteaddr),
                           remoteIP,
                           INET6_ADDRSTRLEN),
                 newfd);
}

void handleUDPData(int udpSock, char *payload, bool payloadInit) {
    struct sockaddr_storage remoteaddr;
    char remoteIP[INET6_ADDRSTRLEN];
    char buf[HELLO_LEN + 1];
    socklen_t clilen = sizeof(remoteaddr);

    ssize_t msglen = Recvfrom(udpSock, buf, HELLO_LEN + 1, 0, (struct sockaddr *) &remoteaddr, &clilen, IGNORE,
                              WARN, "partial/failed read\n");

    if (msglen != HELLO_LEN || memcmp(buf, "HELLO\n", HELLO_LEN) != 0)
        debug_printf("WRONG MESSAGE\n");
    else if (payloadInit) { // message ok && payload initialized
        Sendto(udpSock, payload, strlen(payload), 0, (struct sockaddr *) &remoteaddr, clilen, IGNORE, WARN,
               "partial/failed sendTo bot");
        debug_printf("sending payload to %s on UDP socket %d\n",
                     inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr *) &remoteaddr),
                               remoteIP,
                               INET6_ADDRSTRLEN),
                     udpSock);
    }
    else{
        debug_printf("Payload not initialized\n");
    }
}

void handleTCPClient(int newfd, char *payload, bool payloadInit, fd_set *master) {
    struct sockaddr_storage remoteaddr;
    char remoteIP[INET6_ADDRSTRLEN];
    char buf[HELLO_LEN + 1];

    ssize_t nbytes;
    if ((nbytes = Recv(newfd, buf, sizeof buf, 0, IGNORE, WARN, "Partial/failed read from newfd")) <= 0) {
        // got error or connection closed by client
        if (nbytes == 0) {
            // connection closed
            debug_printf("socket %d hung up\n", newfd);
        } else {
            perror("recv");
        }
        close(newfd); // bye!
        FD_CLR(newfd, master); // remove from master set
    } else {
        // we got some data from a client
        if (nbytes != HELLO_LEN || memcmp(buf, "HELLO\n", HELLO_LEN) != 0)
            debug_printf("WRONG MESSAGE\n");

        else if (payloadInit) { // message ok && payload initialized
            Send(newfd, payload, strlen(payload), 0, IGNORE, WARN, "partial/failed TCP send bot");
            debug_printf("sending payload to %s on TCP socket %d\n",
                         inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr *) &remoteaddr),
                                   remoteIP,
                                   INET6_ADDRSTRLEN),
                         newfd);
        }
        else
            debug_printf("Payload not initialized\n");
    }
}

void job(char *payload, char *udp_port, char *tcp_port, bool payloadInit) {
    int udpSock = bindUDP(udp_port);
    int tcpSock = bindTCP(tcp_port);
    Listen(tcpSock, BACKLOG, 7, FATAL, "listen failed");

    fd_set master, read_fds;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    FD_SET(STDIN, &master); // 0 == stdin
    FD_SET(udpSock, &master);
    FD_SET(tcpSock, &master);

    int fdmax = tcpSock; // do sada max??
    while (true) {
        read_fds = master; // copy
        Select(fdmax + 1, &read_fds, NULL, NULL, NULL, 9, FATAL,
               NULL); /////////// max + 1 jer gleda do dano isključivo...

        // run through the existing connections looking for data to read
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == STDIN) // stdin
                {
					debug_printf("stdin uspio\n");
                    if (handleStdin(payload, &payloadInit)) { goto DONE_WHILE; }
                } else if (i == tcpSock) // handle new connection
                {
                    handleTCPConnection(tcpSock, &master, &fdmax);
                } else if (i == udpSock) {
                    handleUDPData(udpSock, payload, payloadInit);
                } else // handle client...
                {
                    handleTCPClient(i, payload, payloadInit, &master);
                }
            } // END got new incoming connection
        } // END looping through file descriptors
    }

    DONE_WHILE:

    close(udpSock);
    close(tcpSock);
}
