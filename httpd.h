#ifndef _HTTPD_H___
#define _HTTPD_H___

#include <string.h>
#include <stdio.h>
#include <errno.h>

void serve_forever(const char *PORT);
void route(int clientfd, char* uri, char* method, char* payload, int payload_size);

// some interesting macro for `route()`
#define ROUTE_START()       if (0) {
#define ROUTE(METHOD,URI)   } else if (strcmp(URI,uri)==0&&strcmp(METHOD,method)==0) {
#define ROUTE_GET(URI)      ROUTE("GET", URI) 
#define ROUTE_POST(URI)     ROUTE("POST", URI) 
#define ROUTE_END()         } else { \
                            const char* const fmt_header =  \
                                    "HTTP/1.0 404 Not Found\r\n" \
                                    "Server: cs\r\n" \
                                    "\r\n"; \
                            if (send(clientfd, fmt_header, strlen(fmt_header), 0) == -1) { \
                                fprintf(stderr, "send() error %s (%d)", strerror(errno), errno); } \
                            }

#endif