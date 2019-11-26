#include "httpd.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include<pthread.h>
#include <semaphore.h>
#define CONNMAX 3

static int listenfd;
static void error(char *);
static void startServer(const char *);
static void respond(int);

//typedef struct { char *name, *value; } header_t;
//static header_t reqhdr[17] = { {"\0", "\0"} };
//static int clientfd;


sem_t semaphore;
#define TIMEOUT 5
void * socketThread(void *arg)
{
    int clientfd = *((int *)arg);
    int rcvd, fd, bytes_read;
    char *ptr;
    //     char *buf;
    char buf[65535];
//    buf = malloc(65535); //TODO: may be bottle neck
    rcvd=recv(clientfd, buf, 65535, 0);

    if (rcvd<0)    // receive error
        fprintf(stderr,("recv() error\n"));
    else if (rcvd==0)    // receive socket closed
        fprintf(stderr,"Client disconnected upexpectedly.\n");
    else    // message received
    {
        fprintf(stderr,("recv()\n"));
        // http magic go here
        buf[rcvd] = '\0';

        char    *method,    // "GET" or "POST"
        *uri,       // "/index.html" things before '?'
        *qs,        // "a=1&b=2"     things after  '?'
        *prot,      // "HTTP/1.1"
        *payload;

        method = strtok(buf,  " \t\r\n");
        uri    = strtok(NULL, " \t");
        prot   = strtok(NULL, " \t\r\n");

        fprintf(stderr, "\x1b[32m + [%s] %s\x1b[0m\n", method, uri);

        qs = strchr(uri, '?');
        if (qs)
        {
            *qs++ = '\0'; //split URI
        } else {
            qs = uri - 1; //use an empty string
        }

        char *t;
        int payload_size = 0;
        while (1) {
            char *header,*header_val;
            header = strtok(NULL, "\r\n: \t");
            if (!header) break;

            header_val = strtok(NULL, "\r\n");
            while(*header_val && *header_val == ' ') header_val++;
            if(!strcmp(header, "Content-Length")) {
                payload_size = atol(header_val);
            }

            fprintf(stderr, "[H] %s: %s\n", header, header_val);
            t = header_val + strlen(header_val) + 1;
            if (t[1] == '\r' && t[2] == '\n') { //TODO:
                t += 2; break;
            }
        }
        t++; // now the *t shall be the beginning of user payload
//        t2 = request_header("Content-Length"); // and the related header if there is
        payload = t;
        if(!payload_size) {
            payload_size = (rcvd-(t-buf));
        }
        // call router
//        dup2(clientfd, STDOUT_FILENO);
//        close(clientfd);

//        // call router
//        route();
//
        route(clientfd, uri, method, payload, payload_size);

//        // tidy up
//        fflush(stdout);
//        shutdown(STDOUT_FILENO, SHUT_WR);
//        close(STDOUT_FILENO);
    }
    //Closing SOCKET
    shutdown(clientfd, SHUT_RDWR);         //All further send and recieve operations are DISABLED...
    close(clientfd);

    printf("Exit socketThread %d\n", clientfd);
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
    char c;

    int slot=0;

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
        if( pthread_create(&tid, NULL, socketThread, &clientfd) != 0 )
            printf("Failed to create thread\n");
        else {
            printf("Thread created for sock: %d TID: ", clientfd);
            print_thread_id(tid);
            printf("\n");
        }
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
    if (p==NULL)
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

//
//// get request header
//char *request_header(const char* name)
//{
//    header_t *h = reqhdr;
//    while(h->name) {
//        if (strcmp(h->name, name) == 0) return h->value;
//        h++;
//    }
//    return NULL;
//}

//client connection
void respond(int n)
{
//    int rcvd, fd, bytes_read;
//    char *ptr;
//
//    buf = malloc(65535); //TODO: may be bottle neck
//    rcvd=recv(clients[n], buf, 65535, 0);
//
//    if (rcvd<0)    // receive error
//        fprintf(stderr,("recv() error\n"));
//    else if (rcvd==0)    // receive socket closed
//        fprintf(stderr,"Client disconnected upexpectedly.\n");
//    else    // message received
//    {
//        buf[rcvd] = '\0';
////
////        method = strtok(buf,  " \t\r\n");
////        uri    = strtok(NULL, " \t");
////        prot   = strtok(NULL, " \t\r\n");
////
////        fprintf(stderr, "\x1b[32m + [%s] %s\x1b[0m\n", method, uri);
////
////        if (qs = strchr(uri, '?'))
////        {
////            *qs++ = '\0'; //split URI
////        } else {
////            qs = uri - 1; //use an empty string
////        }
////
//////        header_t *h = reqhdr;
//////        char *t, *t2;
//////        while(h < reqhdr+16) {
////
////        char *t;
////        payload_size = 0;
////        while (1) {
////            char *header,*header_val;
////            header = strtok(NULL, "\r\n: \t"); if (!header) break;
////            header_val = strtok(NULL, "\r\n");     while(*header_val && *header_val == ' ') header_val++;
//////            h->name  = header;
//////            h->value = header_val;
//////            h++;
////            if(!strcmp(header, "Content-Length")) {
////                payload_size = atol(header_val);
////            }
////
////            fprintf(stderr, "[H] %s: %s\n", header, header_val);
////            t = header_val + strlen(header_val) + 1;
////            if (t[1] == '\r' && t[2] == '\n') { //TODO:
////                t += 2; break;
////            }
////        }
////        t++; // now the *t shall be the beginning of user payload
//////        t2 = request_header("Content-Length"); // and the related header if there is
////        payload = t;
////        if(!payload_size) {
////            payload_size = (rcvd-(t-buf));
////        }
////
////        // bind clientfd to stdout, making it easier to write
////        clientfd = clients[n];
////        dup2(clientfd, STDOUT_FILENO);
////        close(clientfd);
////
////        // call router
////        route();
////
////        // tidy up
////        fflush(stdout);
//        shutdown(STDOUT_FILENO, SHUT_WR);
//        close(STDOUT_FILENO);
//    }
//
//    //Closing SOCKET
//    shutdown(clientfd, SHUT_RDWR);         //All further send and recieve operations are DISABLED...
//    close(clientfd);
//    clients[n]=-1;
}