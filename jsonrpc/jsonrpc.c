#include "jsonrpc.h"

#include "utils/lpac/utils.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int send_response(const struct jrpc_context *ctx, cJSON *payload) {
    _cleanup_cjson_free_ char *formatted = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    if (formatted == NULL)
        return -1;
    char *tmp = realloc(formatted, strlen(formatted) + 2);
    if (tmp == NULL)
        return -1;
    strcat(tmp, "\n");
    return (int)write(ctx->fd, tmp, strlen(tmp));
}

static int emit_error(const struct jrpc_context *ctx, const struct jrpc_error *error, cJSON *id) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "jsonrpc", cJSON_CreateString("2.0"));
    cJSON_AddItemToObject(root, "id", id);

    cJSON *error_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(error_obj, "code", error->code);
    cJSON_AddStringToObject(error_obj, "message", error->message);
    if (!cJSON_IsInvalid(error->data))
        cJSON_AddItemToObject(error_obj, "data", error->data);

    cJSON_AddItemToObject(root, "error", error_obj);
    return send_response(ctx, root);
}

static int emit_simple_error(const struct jrpc_context *ctx, const int code, char *message, cJSON *id) {
    return emit_error(ctx, &(struct jrpc_error){code, message, .data = NULL}, id);
}

static int emit_result(const struct jrpc_context *ctx, cJSON *result, cJSON *id) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "jsonrpc", cJSON_CreateString("2.0"));
    cJSON_AddItemToObject(root, "id", id);
    cJSON_AddNullToObject(root, "result");
    if (result != NULL)
        cJSON_AddItemToObject(root, "result", result);
    return send_response(ctx, root);
}

static void jrpc_procedure_free(struct jrpc_procedure *procedure) {
    if (procedure->name != NULL) {
        free(procedure->name);
        procedure->name = NULL;
    }
}

int jrpc_register_procedure(struct jrpc_context *ctx, const char *name, const jrpc_function fn_ptr) {
    ctx->procedure_count++;
    const int index = ctx->procedure_count;
    if (ctx->procedures == NULL) {
        ctx->procedures = calloc(index, sizeof(struct jrpc_procedure));
    } else {
        struct jrpc_procedure *ptr = realloc(ctx->procedures, sizeof(struct jrpc_procedure) * (index + 1));
        if (ptr == NULL) {
            return -1;
        }
        ctx->procedures = ptr;
    }
    if ((ctx->procedures[index].name = strdup(name)) == NULL)
        return -1;
    ctx->procedures[index].invoke = fn_ptr;
    return 0;
}

int jrpc_deregister_procedure(struct jrpc_context *ctx, const char *name) {
    if (ctx->procedures == NULL) {
        return -1;
    }
    bool found = false;
    for (int i = 0; i < ctx->procedure_count; i++) {
        if (found) {
            ctx->procedures[i - 1] = ctx->procedures[i];
        } else if (!strcmp(name, ctx->procedures[i].name)) {
            found = true;
            jrpc_procedure_free(&ctx->procedures[i]);
        }
    }
    if (!found) {
        return 0;
    }
    ctx->procedure_count--;
    if (ctx->procedure_count > 0) {
        struct jrpc_procedure *ptr = realloc(ctx->procedures, sizeof(struct jrpc_procedure) * ctx->procedure_count);
        if (ptr == NULL) {
            return -1;
        }
        ctx->procedures = ptr;
    } else {
        ctx->procedures = NULL;
    }
    return 0;
}

static int invoke_procedure(const struct jrpc_context *ctx, const char *name, cJSON *params, cJSON *id) {
    struct jrpc_error *error = malloc(sizeof(struct jrpc_error));
    memset(error, 0, sizeof(struct jrpc_error));
    const struct jrpc_request request = {
        .name = name,
        .params = params,
        .userdata = ctx->userdata,
    };
    for (int i = ctx->procedure_count; i > 0; i--) {
        const struct jrpc_procedure procedure = ctx->procedures[i];
        if (strcmp(procedure.name, name) != 0)
            continue;
        cJSON *result = procedure.invoke(&request, error);
        if (error->code != 0)
            return emit_error(ctx, error, id);
        return emit_result(ctx, result, id);
    }
    return emit_simple_error(ctx, JRPC_METHOD_NOT_FOUND, "Method not found.", id);
}

int invoke_request(const struct jrpc_context *ctx, const cJSON *request) {
    const char *version = cJSON_GetStringValue(cJSON_GetObjectItem(request, "jsonrpc"));
    if (version == NULL || strcmp(version, "2.0") != 0) {
        return emit_simple_error(ctx, JRPC_INVALID_REQUEST, "The JSON-RPC version is not supported.", NULL);
    }
    const cJSON *id = cJSON_GetObjectItem(request, "id");
    const cJSON *method = cJSON_GetObjectItem(request, "method");
    const cJSON *params = cJSON_GetObjectItem(request, "params");
    if (cJSON_IsInvalid(id) || !cJSON_IsString(method) || !cJSON_IsArray(params)) {
        return emit_simple_error(ctx, JRPC_INVALID_REQUEST, "The JSON sent is not a valid Request object.",
                                 cJSON_Duplicate(id, true));
    }
    return invoke_procedure(ctx,
                            strdup(cJSON_GetStringValue(method)), // method name
                            cJSON_Duplicate(params, true),        // parameters
                            cJSON_Duplicate(id, true));           // request id
}
