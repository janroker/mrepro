#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include "debug.h"
#include "util.h"

#define BUF_L 4096

void usage();
void job(char *server, char *port, bool c, char *filename);

extern char *optarg;
extern int optind;

// status 0 OK
// status 4 arguments problem
// status 5 getaddrinfo failed
// status 6 socket creation failed
// status 7 connect failed
// status 8 Ako se program pozove bez opcije-c, a u direktoriju ve ́c postoji datotekafilename
// status 9 Insuficient rights
int main(int argc, char **argv)
{
    int ch;
    char *port = "1234";
    char *server = "127.0.0.1";
    bool c = false;
    while ((ch = getopt(argc, argv, "s:p:c")) != -1)
    {
        switch (ch)
        {
        case 's':
            server = optarg;
            break;

        case 'p':
            port = optarg;
            break;

        case 'c':
            c = true;
            break;

        default:
            usage();
            return 4;
        }
    }

    argc -= optind;
    argv += optind;
    if (argc != 1)
    {
        usage();
        return 4;
    }
    char *filename = argv[0];

    debug_printf("Filename: %s\n", filename);

    int exists = access(filename, F_OK);
    if (!c && exists == 0) // filename postoji a nije dan c
        errx(8, "File already exists\n");

    if (c && exists == 0 && access(filename, R_OK) != 0) // no rights
        errx(9, "Insuficient rights \n");

    job(server, port, c, filename);

    return 0;
}

void usage()
{
    fprintf(stderr, "Usage:  ./tcpklijent [-s server] [-p port] [-c] filename\n");
}

void closeEverything(struct addrinfo *res, int sockfd, FILE *f)
{
    freeaddrinfo(res);
    close(sockfd);
    fclose(f);
}

void job(char *server, char *port, bool c, char *filename)
{
    struct addrinfo *res, hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;
    Getaddrinfo(server, port, &hints, &res, 5, "Get addrinfo failed", true);

    char addrStr[100];
    uint16_t resPort = ntohs(((struct sockaddr_in *)res->ai_addr)->sin_port);
    inet_ntop(res->ai_family, &(((struct sockaddr_in *)res->ai_addr)->sin_addr), addrStr, 100);
    debug_printf("AddrInfo result: %s %d %s %d %d %d\n", addrStr, resPort, res->ai_canonname, res->ai_family, res->ai_socktype, res->ai_protocol);
    
    int sockfd = Socket(res->ai_family, res->ai_socktype, res->ai_protocol, 6, "Socket creation faiiled");
    Connect(sockfd, res->ai_addr, res->ai_addrlen, 7, "Connect failed");
    debug_printf("Connected\n");

    int exists = access(filename, F_OK);
    FILE *f = fopen(filename, "ab+");
    uint32_t offset;
    if ((c && exists != 0) || !c) // dan c a datoteka ne postoji ili nije dan c
    {
        offset = 0;
        debug_printf("File does not exist\n");
    }
    else // datoteka postoji i dan je c
    {
        fseek(f, 0, SEEK_END); // seek to end of file
        offset = ftell(f);     // get current file pointer
        debug_printf("File exists, offset = %d\n", offset);
    }

    int filenameSize = strlen(filename) + 1;
    int soffset = sizeof(offset);
    bool ofRes = Writn(sockfd, &offset, soffset) == soffset;
    bool fRes = Writn(sockfd, filename, filenameSize) == filenameSize;
    debug_printf("Sent req to the server\n", offset);
    if (!ofRes || !fRes)
    {
        closeEverything(res, sockfd, f);
        errx(10, "Failed to send message to server");
    }

    char code;
    int r = readn(sockfd, &code, 1, NULL);
    debug_printf("Recieved code: %d from the server\n", code);
    if (r != 1)
    {
        closeEverything(res, sockfd, f);
        errx(11, "Failed to recieve message from server");
    }
    if (code != ((char)0x00))
    {
        exit(code);
    }

    char buf[BUF_L];
    while (true) // pisi
    {
        r = readn(sockfd, buf, BUF_L, NULL);
        if (r == 0)
            break;
        if (r < 0)
        {
            closeEverything(res, sockfd, f);
            errx(11, "Failed to recieve message from server");
        }
        fwrite(buf, sizeof(char), r, f);
        debug_printf("Read %d bytes from server\n", r);
    }

    closeEverything(res, sockfd, f);
}
