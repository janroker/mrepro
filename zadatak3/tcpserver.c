#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include "tcpserver.h"
#include "util.h"
#include "debug.h"

#define BACKLOG 10
#define FSS 20
#define BUF_L 4096

void usage();
void job(char *port);
void processReq(struct MesIn *in, int fd);
// 0 if succ -1 if fail
int writeRes(struct MesIn *in, int fd);

extern char *optarg;
extern int optind;

// status 2 socket creation failed
// status 3 getaddrinfo failed
// status 4 bind failed
// status 5 listen failed
// status 6 accept failed -- non fatal ; not used
int main(int argc, char **argv)
{
    int ch;
    char *port = "1234";
    while ((ch = getopt(argc, argv, "p:")) != -1) // TODO provjeri ako je arg missing a dan je p
    {
        switch (ch)
        {
        case 'p':
            port = optarg;
            break;

        default:
            usage();
            return 1;
        }
    }

    argc -= optind;
    argv += optind;
    if (argc > 0)
    {
        usage();
        return 1;
    }

    job(port);

    return 0;
}

void usage()
{
    fprintf(stderr, "Usage:  ./tcpserver [-p port]\n");
}

bool checkFilename(char *filename)
{
    if (filename == NULL)
        return true;

    for (int i = 0; filename[i] != '\0'; i++)
        if (filename[i] == '/')
            return false;

    return true;
}

void job(char *port)
{
    struct addrinfo *res;
    GetTCP_addrinfo_passive(NULL, port, &res, 3, "Failed to obtain addrinfo for given port", true);

    char addrStr[100];
    inet_ntop(res->ai_family, &(((struct sockaddr_in *)res->ai_addr)->sin_addr), addrStr, 100);
    debug_printf("AddrInfo result: %s %s %d %d %d\n", addrStr, res->ai_canonname, res->ai_family, res->ai_socktype, res->ai_protocol);

    int sockfd = Socket(res->ai_family, res->ai_socktype, res->ai_protocol, 2, "failed to create socket");
    int on = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1)
        err(1, "setsockopt");

    Bind(sockfd, res->ai_addr, res->ai_addrlen, 4, "Bind failed");

    debug_printf("Socket bound\n");

    Listen(sockfd, BACKLOG, 5, "Listen failed");
    while (true)
    {
        debug_printf("Listen\n");
        struct sockaddr_in theirAddr;
        socklen_t sinsize = sizeof(theirAddr);
        int newfd = Accept(sockfd, (struct sockaddr *)&theirAddr, &sinsize, 6, "Accept failed");
        debug_printf("Accept\n");

        char addrStr2[100];
        inet_ntop(theirAddr.sin_family, &theirAddr.sin_addr, addrStr2, 100);
        debug_printf("AddrInfo result: %s %d\n", addrStr2, theirAddr.sin_family);

        struct MesIn in;
        readn(newfd, &in.offset, 4, NULL);
        debug_printf("Recieved offset %d\n", in.offset);

        int size = FSS;
        in.filename = (char *)malloc(size);
        int toRead = size;
        int r = 0;
        while (true)
        {
            char delimiter = '\0';
            r = readn(newfd, in.filename, toRead, &delimiter);
            if (r <= 0)
                break;   // EOF ili greska
            toRead -= r; // smanji kolko si procitao
            if (toRead <= 0)
            {                                             // moras resizeat
                size *= 2;                                // poduplaj
                in.filename = realloc(in.filename, size); // realociraj
                toRead = size / 2;                        // moras procitat jos za koliko si povecao
            }
        }
        debug_printf("Filename: %s\n", in.filename);

        if (r != 0) // greska
        {
            char code = (char)0x03;
            char *message = "Failed while reading request";
            debug_printf("%s\n", message);
            Writn(newfd, &code, 1);
            Writn(newfd, message, strlen(message));
        }
        else
            processReq(&in, newfd);

        free(in.filename);
        close(newfd);
    }

    freeaddrinfo(res);
    close(sockfd);
}

void processReq(struct MesIn *in, int fd)
{
    debug_printf("Processing req\n");
    char code;
    char *message = NULL;
    if (!checkFilename(in->filename) || access(in->filename, F_OK) != 0) // file ne postoji
    {
        code = (char)0x01;
        message = "File does not exist";
        debug_printf("Message %s\n", message);
    }
    else if (access(in->filename, R_OK) != 0) // missing read permission
    {
        code = (char)0x02;
        message = "Insufficient rights";
        debug_printf("Message %s\n", message);
    }

    if (message != NULL)
    {
        Writn(fd, &code, 1);

        int len = strlen(message);
        Writn(fd, message, len);
    }
    else
    {
        writeRes(in, fd);
    }
}

int writeRes(struct MesIn *in, int fd)
{
    debug_printf("WriteRes\n");
    FILE *f = fopen(in->filename, "rb"); // file exists so should not return NULL !

    char code;
    if (fseek(f, in->offset, SEEK_SET) != 0) // offseting failed
    {
        code = (char)0x03;
        Writn(fd, &code, 1);
        char *message = "Wrong offset given";
        int len = strlen(message);
        Writn(fd, message, len);
        return -1;
    }

    code = (char)0x00; // OK
    Writn(fd, &code, 1);

    debug_printf("Send code %c\n", code);
    char buf[BUF_L];
    while (true)
    {
        int r = fread(buf, sizeof(char), BUF_L, f);
        if (r < 0)
            debug_printf("Error while reading %s\n", in->filename);
        if (r <= 0)
            break;
        Writn(fd, buf, r);
        debug_printf("Write %d bytes\n", r);
    }

    debug_printf("Done writing\n");
    fclose(f);
    return 0;
}
