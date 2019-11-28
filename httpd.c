#include "httpd.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <semaphore.h>

#define CONNMAX 1000

static int listenfd;
static void startServer(const char *);

sem_t semaphore;

void respond(int clientfd, char* buf, unsigned int rcvd)
{
    char *saveptr;
    char    *method,    // "GET" or "POST"
            *uri,       // "/index.html" things before '?'
            *qs,        // "a=1&b=2"     things after  '?'
            *prot;      // "HTTP/1.x"

    method = strtok_r(buf,  " \t\r\n", &saveptr);
    uri    = strtok_r(NULL, " \t", &saveptr);
    prot   = strtok_r(NULL, " \t\r\n", &saveptr);

#ifdef DEBUG
    fprintf(stderr, "\x1b[32m + [%s] %s\x1b[0m\n", method, uri);
#endif
    qs = strchr(uri, '?');
    if (qs)
    {
        *qs++ = '\0'; //split URI
    } else {
        qs = uri - 1; //use an empty string
    }

    char *t;
    char *payload;
    int payload_size = 0;

    while (1) {
        char *header,*header_val;
        header = strtok_r(NULL, "\r\n: \t", &saveptr);
        if (!header) break;

        header_val = strtok_r(NULL, "\r\n", &saveptr);
        while(*header_val && *header_val == ' ') header_val++;
        if(!strcmp(header, "Content-Length")) {
            payload_size = (int)strtol(header_val, (char **)NULL, 10);
        }
#ifdef DEBUG
        fprintf(stderr, "[H] %s: %s\n", header, header_val);
#endif
        t = header_val + strlen(header_val) + 1;
        if (t[1] == '\r' && t[2] == '\n') { //TODO:
            t += 2; break;
        }
    }
    payload = t + 1;
    if(!payload_size) {
        payload_size = (int)(rcvd-(t-buf));
    }
    //TODO: if wrong http handle error

    // call router
    route(clientfd, uri, method, payload, payload_size);
}

void * respondThread(void *arg)
{
    int clientfd = (int)arg; //TODO: hack //    int clientfd = *((int *)arg);
    char buf[65535]; //    buf = malloc(65535); //TODO: may be bottle neck

    int rcvd=recv(clientfd, buf, 65535, 0);

    if (rcvd<0)    // receive error
        fprintf(stderr,"recv() error %s (%d) %d\n", strerror(errno), errno, clientfd);
    else if (rcvd==0)    // receive socket closed
        fprintf(stderr,"Client disconnected upexpectedly.\n");
    else    // message received, parse http
    {
        buf[rcvd] = '\0';
        respond(clientfd, buf, rcvd);
    }

    //Closing SOCKET
    shutdown(clientfd, SHUT_RDWR);         //All further send and recieve operations are DISABLED...
    close(clientfd);

#ifdef DEBUG
    printf("Exit socketThread %d\n", clientfd);
#endif

    sem_post(&semaphore);
    pthread_detach(pthread_self());
    pthread_exit(NULL);
}

//TODO: remove after debug
void print_thread_id(pthread_t id)
{
    size_t i;
    for (i = sizeof(i); i; --i)
        printf("%02x", *(((unsigned char*) &id) + i - 1));
}

void serve_forever(const char *PORT)
{
    int clientfd;
    struct sockaddr_in clientaddr;
    socklen_t addrlen;

    printf(
            "Server started %shttp://127.0.0.1:%s%s\n",
            "\033[92m",PORT,"\033[0m"
    );

    startServer(PORT);

    // Ignore SIGCHLD to avoid zombie threads
    signal(SIGCHLD,SIG_IGN);
    sem_init(&semaphore, 0, CONNMAX);
    while(1)
    {
        pthread_t tid;
        sem_wait(&semaphore);
        //Accept call creates a new socket for the incoming connection
        addrlen = sizeof(clientaddr);
        clientfd = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);
        //for each client request creates a thread and assign the client request to it to process
        //so the main thread can entertain next request
//        if( pthread_create(&tid, NULL, respondThread, &clientfd) != 0 )
        if( pthread_create(&tid, NULL, respondThread, (void*)clientfd) != 0 ) { //TODO: hack
            fprintf(stderr, "Failed to create thread\n");
            sem_post(&semaphore);
        }
#ifdef DEBUG
        else {
            printf("Thread created for sock: %d TID: ", clientfd);
            print_thread_id(tid);
            printf("\n");
        }
#endif
    }
}

//start server
void startServer(const char *port)
{
    struct addrinfo hints, *res, *p;

    // getaddrinfo for host
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo( NULL, port, &hints, &res) != 0)
    {
        perror ("getaddrinfo() error");
        exit(1);
    }
    // socket and bind
    for (p = res; p!=NULL; p=p->ai_next)
    {
        int option = 1;
        listenfd = socket (p->ai_family, p->ai_socktype, 0);
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        if (listenfd == -1) continue;
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
    }
    if (NULL == p)
    {
        perror ("socket() or bind()");
        exit(1);
    }

    freeaddrinfo(res);

    // listen for incoming connections
    if ( listen (listenfd, 1000000) != 0 )
    {
        perror("listen() error");
        exit(1);
    }
}