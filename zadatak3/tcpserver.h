#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <sys/types.h>

struct MesIn {
    u_int32_t offset; // network byte order
    char *filename; // \0 terminated string
};

#endif