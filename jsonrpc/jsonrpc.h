#pragma once

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdio.h>

#define JRPC_PARSE_ERROR (-32700)
#define JRPC_INVALID_REQUEST (-32600)
#define JRPC_METHOD_NOT_FOUND (-32601)
#define JRPC_INVALID_PARAMS (-32603)
#define JRPC_INTERNAL_ERROR (-32693)

struct jrpc_request {
    const char *name;
    cJSON *params;
    void *userdata;
};

struct jrpc_error {
    int code;
    char *message;
    cJSON *data;
};

typedef cJSON *(*jrpc_function)(const struct jrpc_request *, struct jrpc_error *);

struct jrpc_procedure {
    char *name;

    jrpc_function invoke;

    bool notification;
};

struct jrpc_context {
    int procedure_count;
    struct jrpc_procedure *procedures;
    int fd;

    void *userdata;
};

int jrpc_register_procedure(struct jrpc_context *, const char *, jrpc_function);

int jrpc_deregister_procedure(struct jrpc_context *, const char *);

int invoke_request(const struct jrpc_context *, const cJSON *);
