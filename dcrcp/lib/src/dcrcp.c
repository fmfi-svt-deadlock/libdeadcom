#include "dcrcp.h"
#include "cn-cbor/cn-cbor.h"


#define CHECK_ERROR_ENCODER(obj, err)  if ((obj) == NULL) { \
    switch ((err).err) { \
        case CN_CBOR_ERR_OUT_OF_MEMORY: \
            return DCRCP_STATUS_OUT_OF_MEMORY; \
        default: \
            return DCRCP_STATUS_INVALID_PARAM; \
    } \
}

#define CHECK_ALLOC(obj)  if ((obj) == NULL) {return DCRCP_STATUS_OUT_OF_MEMORY;}


DCRCPStatus dcrcpEncode(DeadcomCRPM *crpm_in, uint8_t *buf_out, size_t buf_size, size_t *out_size,
                        cn_cbor_context *ctx) {
    if (crpm_in == NULL || buf_out == NULL || buf_size == 0 || out_size == NULL) {
        return DCRCP_STATUS_INVALID_PARAM;
    }

    cn_cbor_errback err;
    cn_cbor *crpm = cn_cbor_map_create(ctx, &err);

    CHECK_ERROR_ENCODER(crpm, err);

    switch(crpm_in->type) {
        case DCRCP_CRPM_SYS_QUERY_REQUEST: {
            cn_cbor *nil = ctx->calloc_func(1, sizeof(cn_cbor), ctx->context);
            CHECK_ALLOC(nil);
            nil->type = CN_CBOR_NULL;
            if (!cn_cbor_mapput_int(crpm, DCRCP_CRPM_SYS_QUERY_REQUEST, nil, ctx, &err)) {
                CHECK_ERROR_ENCODER(NULL, err);
            }
            break;
        }
    }

    ssize_t written = cn_cbor_encoder_write(buf_out, 0, buf_size, crpm);
    if (written != -1) {
        *out_size = written;
    } else {
        return DCRCP_STATUS_BUFFER_SMALL;
    }

    return DCRCP_STATUS_OK;
}


DCRCPStatus dcrcpDecode(DeadcomCRPM *crpm_out, uint8_t *buf_in, size_t buf_size,
                        cn_cbor_context *ctx) {
    return DCRCP_STATUS_OK;
}
