#ifndef LAB3_STRUCTURES_H
#define LAB3_STRUCTURES_H

#include <netinet/in.h>

struct addr_pair {
    char ip[INET_ADDRSTRLEN]; // TODO pazi na braodcast addr
    char port[22];
};

/*Struktura MSG sadrži 1 oktet za naredbu, te od 0 do najviše 20 parova IP adresa i portova koji mogu
        biti zapisani kao naziv ili brojčano. Među IP adresama se može nalaziti i broadcast adresa.*/
struct MSG {
    char command;
    struct addr_pair pairs[20];
};

#endif //LAB3_STRUCTURES_H
