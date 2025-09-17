#include "proc.h"
#include "utils/lpac/utils.h"

#include <cjson/cJSON.h>

#include <euicc/es10a.c>

DEFINE_TRIVIAL_CLEANUP_FUNC(struct es10a_euicc_configured_addresses *, es10a_euicc_configured_addresses_free);

static cJSON *get_euicc_configured_addresses(const struct jrpc_request *req, struct jrpc_error *err) {
    struct euicc_ctx *ctx = req->userdata;
    if (cJSON_GetArraySize(req->params) != 0) {
        err->code = JRPC_INVALID_PARAMS;
        err->message = "No parameters expected";
        return NULL;
    }
    _cleanup_(es10a_euicc_configured_addresses_freep) struct es10a_euicc_configured_addresses *address =
        malloc(sizeof(struct es10a_euicc_configured_addresses));
    memset(address, 0, sizeof(struct es10a_euicc_configured_addresses));
    if (es10a_get_euicc_configured_addresses(ctx, address) != 0) {
        err->code = JRPC_INTERNAL_ERROR;
        err->message = "Failed to get eUICC configured addresses";
        return NULL;
    }
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "default_dp_address", address->defaultDpAddress);
    cJSON_AddStringToObject(result, "root_ds_address", address->rootDsAddress);
    return result;
}

static cJSON *set_default_dp_address(const struct jrpc_request *req, struct jrpc_error *err) {
    struct euicc_ctx *ctx = req->userdata;
    const char *address = cJSON_GetStringValue(cJSON_GetArrayItem(req->params, 0));
    if (es10a_set_default_dp_address(ctx, address) != 0) {
        err->code = JRPC_INTERNAL_ERROR;
        err->message = "Failed to set default SM-DP+ address";
    }
    return NULL;
}

int setup_es10a(struct jrpc_context *ctx) {
    jrpc_register_procedure(ctx, "get_euicc_configured_addresses", get_euicc_configured_addresses);
    jrpc_register_procedure(ctx, "set_default_dp_address", set_default_dp_address);
    return 0;
}
