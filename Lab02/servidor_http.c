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
#include <poll.h>

// Optional: limits for OPEN_MAX (fallback if not defined elsewhere)
#include <limits.h>

// ---- Convenience / portability helpers ----

// If you’re following Stevens’ UNP style, SA is often used for sockaddr:
#ifndef SA
#define SA struct sockaddr
#endif

// Some systems don’t define INFTIM; it’s standard as -1 for poll()
#ifndef INFTIM
#define INFTIM (-1)
#endif

// If OPEN_MAX isn’t defined, provide a sane default (matches your code’s 256)
#ifndef OPEN_MAX
#define OPEN_MAX 256
#endif

// If you used max() as a function, define a macro or inline helper:
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define LISTENQ      10
#define MAXDATASIZE  256
#define MAXLINE      4096


int max(int a, int b) {
    if (a > b)
        return a;
    return b;
}
// Wrappers
int Socket(int domain, int type, int protocol) {
    int listenfd;
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    return listenfd;
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
    

    const char* response;
    if (strncmp(client_input, "GET / HTTP/1.0", 14) == 0 ||
        strncmp(client_input, "GET / HTTP/1.1", 14) == 0) {

        // Resposta em caso positivo
        response =
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 91\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><head><title>MC833</title></head><body><h1>MC833 TCP Concorrente </h1></body></html>\r\n";
    } else {
        response = "400 Bad Request\r\n";
    }

    (void)Write(connfd, response, strlen(response));
    sleep(tempo_sleep);
    Close(connfd);
}

int main(int argc, char *argv[]) {
    int listenfd, connfd;
    struct sockaddr_in servaddr;

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
        printf("[SERVIDOR] Escutando em 127.0.0.1:%u\n", p);
        FILE *f = fopen("server.info", "w");
        if (f) { fprintf(f, "IP=127.0.0.1\nPORT=%u\n", p); fclose(f); }
        fflush(stdout);
    }

    // listen
    
    printf("BACKLOG: %d", backlog);
    Listen(listenfd, backlog);

    // receber sinal dos filhos
    Signal (SIGCHLD, sig_chld);
    // laço: aceita clientes, envia banner e fecha a conexão do cliente

    struct pollfd client[OPEN_MAX];
    struct sockaddr_in cliaddr;
    socklen_t clilen;
    int i, sockfd;
    client[0].fd = listenfd;
    client[0].events = POLLRDNORM;
    for (i = 1; i < OPEN_MAX; i++)
        client[i].fd = -1; /* -1 indicates available entry */
    int maxi = 0; /* max index into client[] array */

    for (;;) {
        int nready = poll(client, maxi + 1, INFTIM);
        if (client[0].revents & POLLRDNORM) { /* new client conn */
            clilen = sizeof(cliaddr);
            connfd = Accept(listenfd, (SA *) &cliaddr, &clilen);
            for (i = 1; i < OPEN_MAX; i++)
                if (client[i].fd < 0) {
                    client[i].fd = connfd; /* save descriptor */
                    break;
                }
            if (i == OPEN_MAX){
                printf("too many clients\n");
            }
            client[i].events = POLLRDNORM;
            if (i > maxi)
                maxi = i; /* max index in client[] array */
            if (--nready <= 0)
                continue; /* no more readable descriptors */

            for (i = 1; i <= maxi; i++) { /* check all clients for data */
                if ( (sockfd = client[i].fd) < 0)
                    continue;

                if (client[i].revents & (POLLRDNORM | POLLERR)) {
                    doit(sockfd, tempo_sleep);
                    client[i].fd = -1;

                    if (--nready <= 0)
                        break; /* no more readable descriptors */
                }
            }
        }
    }

    return 0;
}
