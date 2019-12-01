#ifndef _HTTPD_H___
#define _HTTPD_H___

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

void serve_forever(const char *PORT);
void route(int clientfd, char* uri, char* method, char* payload, int payload_size, char* content_type);

// some interesting macro for `route()`
#define ROUTE_START()       if (0) {
#define ROUTE(METHOD,URI)   } else if (strcmp(URI,uri)==0&&strcmp(METHOD,method)==0) {
#define ROUTE_GET(URI)      ROUTE("GET", URI) 
#define ROUTE_POST(URI)     ROUTE("POST", URI)
#define ROUTE_HEAD(URI)     ROUTE("HEAD", URI)
#define ROUTE_ERR()         } else {
#define ROUTE_END()         }

#endif