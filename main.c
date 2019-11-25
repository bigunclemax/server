#include "httpd.h"
#include "json-parser/json.h"
#include "engine/gosthash.h"

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

void route()
{
    ROUTE_START()

    ROUTE_GET("/")
    {
        printf("HTTP/1.0 200 OK\r\n\r\n");
    }

    ROUTE_POST("/")
    {
        char data[1024];
        size_t data_size;
        if(parse_json(payload, payload_size, data, &data_size)) {
            printf("HTTP/1.0 400 Bad Request\r\n");
        } else {

            //get gost hash
            unsigned char gost_hash[32];
            get_gost_hash((unsigned char*)data, data_size, gost_hash);

            //get sha hash
            char sha_hash[64];
            get_gost_hash((unsigned char*)data, data_size, gost_hash);

            //get resp
            printf("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n{\n");

            printf("\"gost\":\"");
            int i;
            for (i = 0; i < 32; i++) {
                printf("%02x", gost_hash[i]);
            }
            printf("\",\n");
            printf("}\r\n");

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

