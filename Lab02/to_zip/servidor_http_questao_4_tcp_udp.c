#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <errno.h>       // errno, EINTR
#include <sys/select.h>   // FD_ZERO, FD_SET, FD_ISSET, select()

#define LISTENQ      10
#define MAXDATASIZE  256
#define MAXLINE      4096

static const char HTTP_OK_RESPONSE[] =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 91\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<html><head><title>MC833</title></head><body><h1>MC833 TCP Concorrente </h1></body></html>\r\n";

static const char HTTP_BAD_REQUEST[] = "400 Bad Request\r\n";

static const char* http_response_for(const char *client_input) {
    if (!client_input) {
        return HTTP_BAD_REQUEST;
    }

    if (strncmp(client_input, "GET / HTTP/1.0", 14) == 0 ||
        strncmp(client_input, "GET / HTTP/1.1", 14) == 0) {
        return HTTP_OK_RESPONSE;
    }

    return HTTP_BAD_REQUEST;
}


int max(int a, int b) {
    if (a > b)
        return a;
    return b;
}
// Wrappers
int Socket(int domain, int type, int protocol) {
    int sockfd;
    if ((sockfd = socket(domain, type, protocol)) == -1) {
        perror("socket");
        exit(1);
    }
    return sockfd;
}
int Close(int fd) {
    return close(fd);
}
void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (bind(sockfd, addr, addrlen) == -1) {
        perror("bind");
        Close(sockfd);
        exit(1);
    }
}
void Listen(int sockfd, int backlog) {
    if (listen(sockfd, backlog) == -1) {
        perror("listen");
        Close(sockfd);
        exit(1);
    }
}
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return accept(sockfd, addr, addrlen);
}
int Getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return getsockname(sockfd, addr, addrlen);
}
int Getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return getpeername(sockfd, addr, addrlen);
}
ssize_t Read(int fd, void *buf, size_t count) {
    ssize_t n = read(fd, buf, count);

    if (buf && count > 0) {
        if (n >= 0 && (size_t)n < count) {
            ((char*)buf)[n] = '\0';
        } else {
            ((char*)buf)[count - 1] = '\0';
        }
    }
    // Imprime mensagem recebida
    printf("[CLI MSG] %s", (char*)buf);
    return n;
}
ssize_t Write(int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}

typedef void Sigfunc(int);
Sigfunc * Signal (int signo, Sigfunc *func)
{
    struct sigaction act, oact;
    act.sa_handler = func;
    sigemptyset (&act.sa_mask); /* Outros sinais não são bloqueados*/
    act.sa_flags = 0;
    if (signo == SIGALRM) { /* Para reiniciar chamadas interrompidas */
#ifdef SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT; /* SunOS 4.x */
#endif
    } else {
#ifdef SA_RESTART
        act.sa_flags |= SA_RESTART; /* SVR4, 4.4BSD */
#endif
    }
    if (sigaction (signo, &act, &oact) < 0)
        return (SIG_ERR);
    return (oact.sa_handler);
}

void sig_chld(int signo) {
    pid_t pid;
    int stat;
    while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("child %d terminated\n", pid);
    return;
}

void err_sys(const char *msg) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}


void doit(int connfd, int tempo_sleep) {
    struct sockaddr_in remoteaddr;
    socklen_t len = sizeof(remoteaddr);
    if (Getpeername(connfd, (struct sockaddr *)&remoteaddr, &len) == 0){
        char remoteip[INET_ADDRSTRLEN];
        unsigned short remoteport;

        inet_ntop(AF_INET, &(remoteaddr.sin_addr), remoteip, sizeof(remoteip));
        remoteport = ntohs(remoteaddr.sin_port);

        printf("local: %s:%hu\n",remoteip, remoteport);
    }

    // lê o input vindo do cliente
    char client_input[MAXLINE + 1];
    Read(connfd, client_input, MAXLINE);
    
    const char* response = http_response_for(client_input);
    (void)Write(connfd, response, strlen(response));
    sleep(tempo_sleep);
    Close(connfd);
}

void handle_udp_request(int udpfd, int tempo_sleep) {
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    char client_input[MAXLINE + 1];

    ssize_t n = recvfrom(udpfd, client_input, MAXLINE, 0, (struct sockaddr*)&cliaddr, &len);
    if (n < 0) {
        perror("recvfrom");
        return;
    }

    if (n >= MAXLINE) {
        n = MAXLINE;
    }
    client_input[n] = '\0';

    char remoteip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(cliaddr.sin_addr), remoteip, sizeof(remoteip));
    unsigned short remoteport = ntohs(cliaddr.sin_port);
    printf("udp remote: %s:%hu\n", remoteip, remoteport);
    fflush(stdout);

    const char* response = http_response_for(client_input);
    if (sendto(udpfd, response, strlen(response), 0, (struct sockaddr*)&cliaddr, len) < 0) {
        perror("sendto");
    }

    sleep(tempo_sleep);
}
int main(int argc, char *argv[]) {
    int listenfd, connfd, udpfd;
    struct sockaddr_in servaddr;
    unsigned short listen_port = 0;

    // socket
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int backlog = LISTENQ;
    int tempo_sleep = 0;

    if (argc > 1){
        int selected_port = atoi(argv[1]);

        if (argc > 2 ) {
            backlog = atoi(argv[2]);
        }

        if (argc > 3 ) {
            tempo_sleep = atoi(argv[3]);
        }

        servaddr.sin_port = htons(selected_port);
    }else{
        servaddr.sin_port = 0;              
    }

    Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    
    // Descobrir porta real e divulgar em arquivo server.info
    struct sockaddr_in bound; socklen_t blen = sizeof(bound);
    if (Getsockname(listenfd, (struct sockaddr*)&bound, &blen) == 0) {
        unsigned short p = ntohs(bound.sin_port);
        listen_port = p;
        printf("[SERVIDOR] Escutando em 127.0.0.1:%u\n", p);
        FILE *f = fopen("server.info", "w");
        if (f) { fprintf(f, "IP=127.0.0.1\nPORT=%u\n", p); fclose(f); }
        fflush(stdout);
    }

    // listen
    
    printf("BACKLOG: %d", backlog);
    Listen(listenfd, backlog);

    udpfd = Socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    if (setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
    }
    struct sockaddr_in udpaddr;
    memset(&udpaddr, 0, sizeof(udpaddr));
    udpaddr.sin_family = AF_INET;
    udpaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    udpaddr.sin_port = htons(listen_port);
    Bind(udpfd, (struct sockaddr *)&udpaddr, sizeof(udpaddr));
    printf("[SERVIDOR] UDP habilitado em 127.0.0.1:%u\n", listen_port);
    fflush(stdout);

    // receber sinal dos filhos
    Signal (SIGCHLD, sig_chld);
    // laço: aceita clientes, envia banner e fecha a conexão do cliente

    fd_set master_set, read_set;
    FD_ZERO(&master_set);
    FD_SET(listenfd, &master_set);
    FD_SET(udpfd, &master_set);
    int maxfd = max(listenfd, udpfd);

    for (;;) {
        read_set = master_set;
        int nready = select(maxfd + 1, &read_set, NULL, NULL, NULL);
        if (nready < 0) {
            if (errno == EINTR)
                continue; /* se for tratar o sinal,quando voltar dá erro em funções lentas */
            else
                err_sys("select error");
        }

        if (FD_ISSET(listenfd, &read_set)) {
            if ( (connfd = accept (listenfd, NULL, NULL)) < 0) {
                if (errno == EINTR)
                    continue; /* se for tratar o sinal,quando voltar dá erro em funções lentas */
                else
                    err_sys("accept error");
            }

            FD_SET(connfd, &master_set);
            if (connfd > maxfd) {
                maxfd = connfd;
            }
            if (--nready <= 0) {
                continue;
            }
        }

        if (FD_ISSET(udpfd, &read_set)) {
            handle_udp_request(udpfd, tempo_sleep);
            if (--nready <= 0) {
                continue;
            }
        }

        for (int fd = 0; fd <= maxfd && nready > 0; ++fd) {
            if (fd == listenfd || fd == udpfd) {
                continue;
            }
            if (FD_ISSET(fd, &read_set)) {
                doit(fd, tempo_sleep);
                FD_CLR(fd, &master_set);
                nready--;
            }
        }
    }

    return 0;
}
