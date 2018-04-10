#include "unity.h"

#include "dcrcp.h"


void* context_calloc(size_t count, size_t size, void *context) {
    // TODO verify amount of mem allocated!
    return calloc(count, size);
}

void context_free(void *ptr, void *context) {
    free(ptr);
}

cn_cbor_context ctx = {
    .calloc_func = context_calloc,
    .free_func   = context_free,
    .context     = NULL
};


void test_Dummy() {
    DeadcomCRPM c;
    c.type = DCRCP_CRPM_SYS_QUERY_REQUEST;

    uint8_t buf[254];
    size_t out_size;
    dcrcpEncode(&c, buf, 254, &out_size, &ctx);
    unsigned int cnt;
    for (cnt = 0; cnt < out_size; cnt++) {
        printf("%02x", buf[cnt]);
    }
    printf("\n");
}
