#include "httpd.h"
#include "engine/gosthash.h"
#include <openssl/sha.h>
#include <sys/socket.h>
#include <jsmn.h>

int main(int c, char** v)
{
    serve_forever("2001");
    pthread_exit(NULL);
    return 0;
}

void get_gost_hash(const unsigned char* buffer, size_t bytes, unsigned char* out_buffer)
{
    gost_hash_ctx ctx;
    gost_subst_block *b = &GostR3411_94_CryptoProParamSet;

    init_gost_hash_ctx(&ctx, b);
    start_hash(&ctx);
    hash_block(&ctx, buffer, bytes);
    finish_hash(&ctx, out_buffer);
    done_gost_hash_ctx(&ctx);
}


void get_sha_hash(const unsigned char* buffer, size_t bytes, unsigned char* out_buffer)
{
    SHA512_CTX ctx;
    SHA512_Init(&ctx);
    SHA512_Update(&ctx, buffer, bytes);
    SHA512_Final(out_buffer, &ctx);
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, (size_t) (tok->end - tok->start)) == 0) {
        return 0;
    }
    return -1;
}

int parse_json(const char * const payload, int payload_size, const char** ptr)
{
    //json parse
    jsmn_parser p;
    jsmntok_t t[10];
    jsmn_init(&p);
    int r;
    r = jsmn_parse(&p, payload, (const size_t) payload_size, t, sizeof(t) / sizeof(t[0]));
    if(r < 0) {
        fprintf(stderr,"Failed to parse JSON: %d\n", r);
        return -1;
    }

    /* Assume the top-level element is an object */
    if (r < 1 || t[0].type != JSMN_OBJECT) {
        fprintf(stderr,"Object expected\n");
        return -1;
    }

    int i;
    for (i = 1; i < r; i++) {
        if (jsoneq(payload, &t[i], "data") == 0) {
            *ptr = payload + t[i + 1].start;
            return t[i + 1].end - t[i + 1].start;
        }
    }

    return -1;
}

struct hash_args {
    const unsigned char* data_ptr;
    int data_sz;
    unsigned char* hash;
};

void *get_gost_hash_tread(void *arguments)
{
    struct hash_args *args = arguments;
    get_gost_hash(args->data_ptr, (size_t) args->data_sz, args->hash);
    pthread_exit(NULL);
}

void *get_sha_hash_tread(void *arguments)
{
    struct hash_args *args = arguments;
    get_sha_hash(args->data_ptr, (size_t) args->data_sz, args->hash);
    pthread_exit(NULL);
}

int calc_hashes(const char * const data_ptr, int data_sz, unsigned char* gost_hash, unsigned char* sha_hash)
{
#ifdef HASH_MULTITHREAD

    struct hash_args gost_args = {
            .data_ptr = (unsigned char*)data_ptr,
            .data_sz = data_sz,
            .hash = gost_hash
    };

    struct hash_args sha_args = {
            .data_ptr = (unsigned char*)data_ptr,
            .data_sz = data_sz,
            .hash = sha_hash
    };

    pthread_t gost_thread;
    pthread_t sha_thread;

    if (pthread_create(&gost_thread, NULL, &get_gost_hash_tread, (void *)&gost_args) != 0) {
        fprintf(stderr, "Failed to create thread\n");
        return -1;
    }

    if (pthread_create(&sha_thread, NULL, &get_sha_hash_tread, (void *)&sha_args) != 0) {
        fprintf(stderr, "Failed to create thread\n");
        return -1;
    }

    pthread_join(gost_thread, NULL);
    pthread_join(sha_thread, NULL);
#else
    get_gost_hash((unsigned char*)data_ptr, (size_t) data_sz, gost_hash);
    get_sha_hash((unsigned char*)data_ptr, (size_t) data_sz, sha_hash);
#endif
    return 0;
}

int process_request(const char * const payload, int payload_size, unsigned char* gost_hash, unsigned char* sha_hash)
{
    const char* data_ptr;
    int data_sz;
    data_sz = parse_json(payload, payload_size, &data_ptr);
    if(data_sz < 0 || data_sz > 1024) {
        return -1;
    }

    if(calc_hashes(data_ptr, data_sz, gost_hash, sha_hash)) {
        return -1;
    }

    return 0;
}

void route(int clientfd, char* uri, char* method, char* payload, int payload_size, char* content_type)
{
    ROUTE_START()

    ROUTE_GET("/")
    {
        const char* const fmt_header =
                "HTTP/1.0 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: 9\r\n"
                "\r\n"
                "Hi there!";

        if (send(clientfd, fmt_header, strlen(fmt_header), 0) == -1) {
            fprintf(stderr, "send() error %s (%d)", strerror(errno), errno);
        }
    }

    ROUTE_POST("/")
    {
        const char* const fmt_header =
                "HTTP/1.0 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: 9\r\n"
                "\r\n"
                "Hi there!";

        if (send(clientfd, fmt_header, strlen(fmt_header), 0) == -1) {
            fprintf(stderr, "send() error %s (%d)", strerror(errno), errno);
        }
    }

    ROUTE_POST("/hash")
    {
        // if application type json
        const char* const err_header =
                "HTTP/1.0 400 Bad Request\r\n\r\n";

        const char* resp_str;
        char resp_msg[500] = "HTTP/1.0 200 OK\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: 215\r\n"
                             "\r\n"
                             "{\"gost\":\"";

        if(payload_size != 0
            && payload != NULL
            && content_type !=NULL
            && !strcmp(content_type, "application/json"))
        {

            unsigned char gost_hash[32];
            unsigned char sha_hash[64];

            if(!process_request(payload, payload_size, gost_hash, sha_hash))
            {
                int i;
                char * _str = resp_msg + strlen(resp_msg);
                for (i = 0; i < 32; i++) {
                    sprintf(_str + 2 * i, "%02x", gost_hash[i]);
                }

                _str += 64;
                int res  = sprintf(_str, "\",\"sha512\":\"");
                _str += res;
                for (i = 0; i < 64; i++) {
                    sprintf(_str + 2 * i, "%02x", sha_hash[i]);
                }
                _str += 128;
                sprintf(_str, "\"}");

                resp_str = resp_msg;

            } else {
                resp_str = err_header;
            }
        } else {
            resp_str = err_header;
        }

        if (send(clientfd, resp_str, strlen(resp_str), 0) == -1) {
            fprintf(stderr, "send() error %s (%d)", strerror(errno), errno);
        }
    }

    ROUTE_ERR()
    {
        char * resp_str;
        if(strcmp("GET",method) != 0 && strcmp("HEAD",method) != 0 && strcmp("POST",method) != 0) {
            resp_str = "HTTP/1.0 501 Not Implemented\r\n\r\n";
        } else {
            resp_str = "HTTP/1.0 404 Not Found\r\n\r\n";
        }

        if (send(clientfd, resp_str, strlen(resp_str), 0) == -1) {
            fprintf(stderr, "send() error %s (%d)", strerror(errno), errno);
        }
    }

    ROUTE_END()
}