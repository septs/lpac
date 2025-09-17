#include "jsonrpc.h"

#include "proc.h"

int main() {
    struct jrpc_context ctx = {
        .procedure_count = 0,
        .procedures = NULL,
        .in = stdin,
        .out = stdout,
        .userdata = NULL,
    };
    setup_es10a(&ctx);
    return 0;
}
