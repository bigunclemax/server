#include "httpd.h"
#include "json-parser/json.h"
#include "engine/gosthash.h"
#include <openssl/sha.h>
#include<sys/socket.h>

int main(int c, char** v)
{
    serve_forever("2001");
    return 0;
}

int parse_json(char *payload, int payload_size, char * data_val_ptr, size_t * data_sz);
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

void route(int clientfd, char* uri, char* method, char* payload, int payload_size)
{
    ROUTE_START()

    ROUTE_GET("/")
    {
            const char* const fmt_header =
                    "HTTP/1.0 200 OK\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: %ld\r\n"
                    "\r\n"
                    "%s";
            char msg[] = "Hi there!";
            char buf [200];
            snprintf(buf, 200, fmt_header, strlen(msg), msg);

            if (send(clientfd, buf, strlen(buf), 0) == -1) {
                perror("send");
            }
    }

    ROUTE_POST("/")
    {
        char data[1024];
        size_t data_size;
        if(parse_json(payload, payload_size, data, &data_size)) {

            const char* const fmt_header = "HTTP/1.0 400 Bad Request\r\n";

            if (send(clientfd, fmt_header, strlen(fmt_header), 0) == -1) {
                perror("send");
            }

        } else {

            const char* const fmt_header =
                    "HTTP/1.0 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "\r\n"
                    "{\"gost\" : \"%s\", \"sha512\" : \"%s\"}";

            //get gost hash
            unsigned char gost_hash[32];
            get_gost_hash((unsigned char*)data, data_size, gost_hash);

            //get sha hash
            unsigned char sha_hash[64];
            get_sha_hash((unsigned char*)data, data_size, sha_hash);

            //get resp
            int i;
            char gost_str[65];
            for (i = 0; i < 32; i++) {
                sprintf(gost_str + 2 * i, "%02x", gost_hash[i]);
            }

            char sha_str[129];
            for (i = 0; i < 64; i++) {
                sprintf(sha_str + 2 * i, "%02x", sha_hash[i]);
            }

            char buf [500];
            snprintf(buf, 500, fmt_header, gost_str, sha_str);
            if (send(clientfd, buf, strlen(buf), 0) == -1) {
                perror("send");
            }
        }
    }

    ROUTE_END()
}

int parse_json(char *payload, int payload_size, char * data_val_ptr, size_t * data_sz)
{
    json_value* value;
    json_char* json = (json_char*)payload;

    value = json_parse(json, payload_size);

    if (value == NULL) {
        fprintf(stderr, "Unable to parse data\n");
        return -1;
    }

    if (value->type != json_object) {
        fprintf(stderr, "Wrong json content\n");
        return -1;
    }

    if(value->u.object.length !=1 ) {
        fprintf(stderr, "Wrong json content\n");
        return -1;
    }

    if (strcmp(value->u.object.values[0].name, "data") != 0) {
        fprintf(stderr, "Wrong json content\n");
        return -1;
    }

    if(value->u.object.values[0].value->type != json_string) {
        fprintf(stderr, "Wrong json content\n");
        return -1;
    }

    strcpy(data_val_ptr, value->u.object.values[0].value->u.string.ptr);
    *data_sz = value->u.object.values[0].value->u.string.length;

    json_value_free(value);

    return 0;
}