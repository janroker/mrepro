#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <err.h>
#include <string.h>
#include <ctype.h>

extern char *optarg;
extern int optind;

void usage(void);

void process(int aiFamily, int socktype, int protocol, char *host, char *service, int r, int n, int x);

int isNum(char *str){
    if(!str) return 0;

    for(int i = 0; str[i] != '\0' && i < 1000; i++){
        if(isdigit(str[i]) == 0)
            return 0;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    int ch;
    int t = 0, u = 0, x = 0, h = 0, n = 0, four = 0, six = 0, r = 0;
    int af = AF_INET, protocol = IPPROTO_TCP, sockType = SOCK_STREAM;

    /////////////////////////////////////////
    ////////////input parsing
    ////////////////////////////////////////
    while ((ch = getopt(argc, argv, "tuxhn46r")) != -1)
    {
        switch (ch)
        {
        case 't':
            if (u == 1)
            {
                fprintf(stderr, "Only one of [-t | -u] options can be provided!\n");
                usage();
                return 3;
            }

            t = 1;
            break;

        case 'u':
            if (t == 1)
            {
                fprintf(stderr, "Only one of [-t | -u] options can be provided!\n");
                usage();
                return 3;
            }

            u = 1;
            protocol = IPPROTO_UDP;
            sockType = SOCK_DGRAM;
            break;

        case 'x':
            x = 1;
            break;

        case 'h':
            if (n == 1)
            {
                fprintf(stderr, "Only one of [-h | -n] options can be provided!\n");
                usage();
                return 3;
            }

            h = 1;
            break;

        case 'n':
            if (h == 1)
            {
                fprintf(stderr, "Only one of [-h | -n] options can be provided!\n");
                usage();
                return 3;
            }

            n = 1;
            break;

        case '4':
            if (six == 1)
            {
                fprintf(stderr, "Only one of [-4 | -6] options can be provided!\n");
                usage();
                return 3;
            }

            four = 1;
            break;

        case '6':

            if (four == 1)
            {
                fprintf(stderr, "Only one of [-4 | -6] options can be provided!\n");
                usage();
                return 3;
            }

            six = 1;
            af = AF_INET6;
            break;

        case 'r':
            r = 1;
            break;

        default:
            usage();
            return 1;
        }
    }

    argc -= optind;
    argv += optind;
    if (argc != 2)
    {
        usage();
        return 1;
    }

    /////////////////////////////////////////
    ////////////input parsing end
    ////////////////////////////////////////

    process(af, sockType, protocol, argv[0], argv[1], r, n, x);

    return 0;
}

void usage(void)
{
    printf("prog [-r] [-t|-u] [-x] [-h|-n] [-46] {hostname | IP_address} {servicename | port}\n");
}

void process(int aiFamily, int socktype, int protocol, char *host, char *service, int r, int n, int x)
{
    int error;

    if (!r)
    {
        struct addrinfo hints, *res, *pamti;
        char addrstr[100];

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = aiFamily;
        hints.ai_flags |= AI_CANONNAME;
        hints.ai_protocol = protocol;
        hints.ai_socktype = socktype;

        error = getaddrinfo(host, service, &hints, &res);

        if (error)
            errx(2, "%s", gai_strerror(error));

        pamti = res;
        while (res)
        {
            char port[NI_MAXSERV];
            int flags = NI_NUMERICSERV | (res->ai_protocol == IPPROTO_UDP ? NI_DGRAM : 0);
            getnameinfo(res->ai_addr, res->ai_addrlen, NULL, 0, port, sizeof(port), flags);

            if (aiFamily == AF_INET)
                inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, addrstr, 100);
            else
                inet_ntop(res->ai_family, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, addrstr, 100);

            uint16_t intPort = atoi(port);
            if (n) // Broj porta prikazan je u host byte redoslijedu a network byte redoslijed se dobiva korištenjem opcije -n
                intPort = htons(intPort);

            // Broj porta ispisuje se u dekadskom obliku a heksadekadski oblik (tocno 4 znamenke) se aktivira korištenjem opcije -x
            x ? printf("%s (%s) %04X\n", addrstr, res->ai_canonname, intPort) : printf("->  %s (%s) %d\n", addrstr, res->ai_canonname, intPort);

            res = res->ai_next;
        }
        freeaddrinfo(pamti); // oslobodi alocirani prostor
    }
    else
    {
        if(isNum(service) != 1) errx(2, "%s is not a port number", service);

        char host2[NI_MAXHOST];
        char servName[100];

        if (aiFamily == AF_INET6)
        {
            struct sockaddr_in6 saIn;
            saIn.sin6_family = aiFamily;
            saIn.sin6_port = htons(atoi(service));

            if (inet_pton(AF_INET6, host, &(saIn.sin6_addr)) != 1)
            {
                errx(2, "%s is not a valid IP address", host);
            }

            error = getnameinfo((struct sockaddr *)&saIn, sizeof(saIn), host2, sizeof(host2), servName, sizeof(servName), NI_NAMEREQD | (protocol == IPPROTO_UDP ? NI_DGRAM : 0));
        }
        else
        {
            struct sockaddr_in saIn;
            saIn.sin_family = aiFamily;
            saIn.sin_port = htons(atoi(service));

            if (inet_pton(AF_INET, host, &(saIn.sin_addr)) != 1)
            {
                errx(2, "%s is not a valid IP address", host);
            }

            error = getnameinfo((struct sockaddr *)&saIn, sizeof(saIn), host2, sizeof(host2), servName, sizeof(servName), NI_NAMEREQD | (protocol == IPPROTO_UDP ? NI_DGRAM : 0));
        }

        if (error)
            errx(2, "getnameinfo: %s", gai_strerror(error));

        printf("%s (%s) %s\n", host, host2, servName);
    }
}
