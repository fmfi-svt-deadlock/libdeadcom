#include <string.h>
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

#define CBOR_ARR_ADD_INT_CHECK(array, data, ctx, err)  { \
    cn_cbor *new_value_to_append = cn_cbor_int_create((data), (ctx), &(err)); \
    CHECK_ERROR_ENCODER(new_value_to_append, err); \
    if (!cn_cbor_array_append((array), new_value_to_append, &err)) { \
        CHECK_ERROR_ENCODER(NULL, err); \
    } \
}

#define CBOR_ARR_ADD_STR_CHECK(array, data, ctx, err)  { \
    cn_cbor *new_value_to_append = cn_cbor_string_create((data), (ctx), &(err)); \
    CHECK_ERROR_ENCODER(new_value_to_append, err); \
    if (!cn_cbor_array_append((array), new_value_to_append, &err)) { \
        CHECK_ERROR_ENCODER(NULL, err); \
    } \
}

#define FREE_AND_RETURN(to_free, alloc_context, to_return)  \
    cn_cbor_free((to_free), (alloc_context)); \
    return (to_return);

#define SCHEMA_FAIL_IF_NOT_UINT16(obj, parent_to_free, ctx)  \
    if ((obj) == NULL || (obj)->type != CN_CBOR_UINT || (obj)->v.uint > 0xFFFF) { \
        FREE_AND_RETURN((parent_to_free), (ctx), DCRCP_STATUS_SCHEMA_FAIL); \
    }

#define SCHEMA_FAIL_IF_NOT_UINT8(obj, parent_to_free, ctx)  \
    if ((obj) == NULL || (obj)->type != CN_CBOR_UINT || (obj)->v.uint > 0xFF) { \
        FREE_AND_RETURN((parent_to_free), (ctx), DCRCP_STATUS_SCHEMA_FAIL); \
    }

#define SCHEMA_FAIL_IF_NOT_STRING(max_len, obj, parent_to_free, ctx)  \
    if ((obj) == NULL || (obj)->type != CN_CBOR_TEXT || (obj)->length > (max_len)) { \
        FREE_AND_RETURN((parent_to_free), (ctx), DCRCP_STATUS_SCHEMA_FAIL); \
    }


DCRCPStatus dcrcpEncode(DeadcomCRPM *crpm_in, uint8_t *buf_out, size_t buf_size, size_t *out_size,
                        cn_cbor_context *ctx) {
    if (crpm_in == NULL || buf_out == NULL || buf_size == 0 || out_size == NULL || ctx == NULL) {
        return DCRCP_STATUS_INVALID_PARAM;
    }

    cn_cbor_errback err;
    cn_cbor *crpm = cn_cbor_map_create(ctx, &err);

    CHECK_ERROR_ENCODER(crpm, err);

    switch(crpm_in->type) {
        case DCRCP_CRPM_SYS_QUERY_REQUEST: {
            cn_cbor *nil = ctx->calloc_func(ctx->context);
            CHECK_ALLOC(nil);
            nil->type = CN_CBOR_NULL;
            if (!cn_cbor_mapput_int(crpm, DCRCP_CRPM_SYS_QUERY_REQUEST, nil, ctx, &err)) {
                CHECK_ERROR_ENCODER(NULL, err);
            }
            break;
        }
        case DCRCP_CRPM_SYS_QUERY_RESPONSE: {
            cn_cbor *sqr = cn_cbor_array_create(ctx, &err);
            CHECK_ERROR_ENCODER(sqr, err);

            CBOR_ARR_ADD_INT_CHECK(sqr, crpm_in->data.sysQueryResponse.rdrClass, ctx, err);
            CBOR_ARR_ADD_INT_CHECK(sqr, crpm_in->data.sysQueryResponse.hwModel, ctx, err);
            CBOR_ARR_ADD_INT_CHECK(sqr, crpm_in->data.sysQueryResponse.hwRev, ctx, err);
            CBOR_ARR_ADD_STR_CHECK(sqr, crpm_in->data.sysQueryResponse.serial, ctx, err);
            CBOR_ARR_ADD_INT_CHECK(sqr, crpm_in->data.sysQueryResponse.swVerMajor, ctx, err);
            CBOR_ARR_ADD_INT_CHECK(sqr, crpm_in->data.sysQueryResponse.swVerMinor, ctx, err);

            if (!cn_cbor_mapput_int(crpm, DCRCP_CRPM_SYS_QUERY_RESPONSE, sqr, ctx, &err)) {
                CHECK_ERROR_ENCODER(NULL, err);
            }
            break;
        }
        case DCRCP_CRPM_ACTIVATE_AUTH_METHOD: {
            cn_cbor *ams = cn_cbor_array_create(ctx, &err);
            CHECK_ERROR_ENCODER(ams, err);

            size_t i;
            for (i = 0; i < crpm_in->data.authMethods.len; i++) {
                CBOR_ARR_ADD_INT_CHECK(ams, crpm_in->data.authMethods.vals[i], ctx, err);
            }

            if (!cn_cbor_mapput_int(crpm, DCRCP_CRPM_ACTIVATE_AUTH_METHOD, ams, ctx, &err)) {
                CHECK_ERROR_ENCODER(NULL, err);
            }
            break;
        }
        case DCRCP_CRPM_RDR_FAILURE: {
            cn_cbor *err_str = cn_cbor_string_create(crpm_in->data.rdrFailure, ctx, &err);
            CHECK_ERROR_ENCODER(err_str, err);

            if (!cn_cbor_mapput_int(crpm, DCRCP_CRPM_RDR_FAILURE, err_str, ctx, &err)) {
                CHECK_ERROR_ENCODER(NULL, err);
            }
            break;
        }
        case DCRCP_CRPM_UI_UPDATE: {
            cn_cbor *ui_state = cn_cbor_int_create(crpm_in->data.ui_class0_state, ctx, &err);
            CHECK_ERROR_ENCODER(ui_state, err);

            if (!cn_cbor_mapput_int(crpm, DCRCP_CRPM_UI_UPDATE, ui_state, ctx, &err)) {
                CHECK_ERROR_ENCODER(NULL, err);
            }
            break;
        }
        case DCRCP_CRPM_AM0_PICC_UID_OBTAINED: {
            cn_cbor *uids = cn_cbor_array_create(ctx, &err);
            CHECK_ERROR_ENCODER(uids, err);

            size_t i;
            for (i = 0; i < crpm_in->data.authMethod0UuidObtained.len; i++) {
                cn_cbor *elem = cn_cbor_data_create(
                    crpm_in->data.authMethod0UuidObtained.vals[i].uid,
                    crpm_in->data.authMethod0UuidObtained.vals[i].uid_len,
                    ctx, &err
                );
                if (!cn_cbor_array_append(uids, elem, &err)) {
                    CHECK_ERROR_ENCODER(NULL, err);
                }
            }

            if (!cn_cbor_mapput_int(crpm, DCRCP_CRPM_AM0_PICC_UID_OBTAINED, uids, ctx, &err)) {
                CHECK_ERROR_ENCODER(NULL, err);
            }
            break;
        }
        default:
            FREE_AND_RETURN(crpm, ctx, DCRCP_STATUS_INVALID_PARAM);
    }

    ssize_t written = cn_cbor_encoder_write(buf_out, 0, buf_size, crpm);
    if (written != -1) {
        *out_size = written;
    } else {
        FREE_AND_RETURN(crpm, ctx, DCRCP_STATUS_BUFFER_SMALL);
    }

    FREE_AND_RETURN(crpm, ctx, DCRCP_STATUS_OK);
}


// TODO this decoder does not handle chunked text / bytes at all!
DCRCPStatus dcrcpDecode(DeadcomCRPM *crpm_out, uint8_t *buf_in, size_t buf_size,
                        cn_cbor_context *ctx) {
    if (crpm_out == NULL || buf_in == NULL || buf_size == 0 || ctx == NULL) {
        return DCRCP_STATUS_INVALID_PARAM;
    }

    cn_cbor_errback err;
    cn_cbor *crpm = cn_cbor_decode(buf_in, buf_size, ctx, &err);

    if (crpm == NULL) {
        switch(err.err) {
            case CN_CBOR_ERR_OUT_OF_MEMORY:
                return DCRCP_STATUS_OUT_OF_MEMORY;
            default:
                return DCRCP_STATUS_DECODE_FAIL;
        }
    }

    cn_cbor *payload;

    if (NULL != (payload = cn_cbor_mapget_int(crpm, DCRCP_CRPM_SYS_QUERY_REQUEST))) {

        // This CRPM has no payload
        crpm_out->type = DCRCP_CRPM_SYS_QUERY_REQUEST;

    } else if (NULL != (payload = cn_cbor_mapget_int(crpm, DCRCP_CRPM_SYS_QUERY_RESPONSE))) {

        crpm_out->type = DCRCP_CRPM_SYS_QUERY_RESPONSE;
        if (payload->type != CN_CBOR_ARRAY || payload->length < 6) {
            FREE_AND_RETURN(crpm, ctx, DCRCP_STATUS_SCHEMA_FAIL);
        }

        cn_cbor *child;

        child = cn_cbor_index(payload, 0);
        SCHEMA_FAIL_IF_NOT_UINT16(child, crpm, ctx);
        crpm_out->data.sysQueryResponse.rdrClass = (uint16_t) child->v.uint;

        child = cn_cbor_index(payload, 1);
        SCHEMA_FAIL_IF_NOT_UINT16(child, crpm, ctx);
        crpm_out->data.sysQueryResponse.hwModel = (uint16_t) child->v.uint;

        child = cn_cbor_index(payload, 2);
        SCHEMA_FAIL_IF_NOT_UINT16(child, crpm, ctx);
        crpm_out->data.sysQueryResponse.hwRev = (uint16_t) child->v.uint;

        child = cn_cbor_index(payload, 3);
        SCHEMA_FAIL_IF_NOT_STRING(25, child, crpm, ctx);
        strncpy(crpm_out->data.sysQueryResponse.serial, child->v.str, child->length);
        crpm_out->data.sysQueryResponse.serial[child->length] = '\0';

        child = cn_cbor_index(payload, 4);
        SCHEMA_FAIL_IF_NOT_UINT8(child, crpm, ctx);
        crpm_out->data.sysQueryResponse.swVerMajor = (uint8_t) child->v.uint;

        child = cn_cbor_index(payload, 5);
        SCHEMA_FAIL_IF_NOT_UINT8(child, crpm, ctx);
        crpm_out->data.sysQueryResponse.swVerMinor = (uint8_t) child->v.uint;

    } else if (NULL != (payload = cn_cbor_mapget_int(crpm, DCRCP_CRPM_ACTIVATE_AUTH_METHOD))) {

        crpm_out->type = DCRCP_CRPM_ACTIVATE_AUTH_METHOD;
        if (payload->type != CN_CBOR_ARRAY || payload->length < 1 || payload->length > 10) {
            FREE_AND_RETURN(crpm, ctx, DCRCP_STATUS_SCHEMA_FAIL);
        }

        cn_cbor *child = payload->first_child;
        while (child != NULL) {
            crpm_out->data.authMethods.vals[crpm_out->data.authMethods.len] = child->v.uint;
            crpm_out->data.authMethods.len += 1;
            child = child->next;
        }

    } else if (NULL != (payload = cn_cbor_mapget_int(crpm, DCRCP_CRPM_RDR_FAILURE))) {

        crpm_out->type = DCRCP_CRPM_RDR_FAILURE;
        SCHEMA_FAIL_IF_NOT_STRING(200, payload, crpm, ctx);

        strncpy(crpm_out->data.rdrFailure, payload->v.str, payload->length);
        crpm_out->data.rdrFailure[payload->length] = '\0';

    } else if (NULL != (payload = cn_cbor_mapget_int(crpm, DCRCP_CRPM_UI_UPDATE))) {

        crpm_out->type = DCRCP_CRPM_UI_UPDATE;
        SCHEMA_FAIL_IF_NOT_UINT8(payload, crpm, ctx);

        crpm_out->data.ui_class0_state = payload->v.uint;

    } else if (NULL != (payload = cn_cbor_mapget_int(crpm, DCRCP_CRPM_AM0_PICC_UID_OBTAINED))) {

        crpm_out->type = DCRCP_CRPM_AM0_PICC_UID_OBTAINED;
        if (payload->type != CN_CBOR_ARRAY || payload->length < 1 || payload->length > 10) {
            FREE_AND_RETURN(crpm, ctx, DCRCP_STATUS_SCHEMA_FAIL);
        }

        cn_cbor *child = payload->first_child;
        size_t *len = &(crpm_out->data.authMethod0UuidObtained.len);
        while (child != NULL) {
            // TODO use defined constants here
            if (child->length != 4 && child->length != 7 && child->length != 10) {
                FREE_AND_RETURN(crpm, ctx, DCRCP_STATUS_SCHEMA_FAIL);
            }
            crpm_out->data.authMethod0UuidObtained.vals[*len].uid_len = child->length;
            memcpy(crpm_out->data.authMethod0UuidObtained.vals[*len].uid, child->v.bytes,
                   child->length);
            (*len) += 1;
            child = child->next;
        }

    } else {
        // No valid CRPM ID as a key
        FREE_AND_RETURN(crpm, ctx, DCRCP_STATUS_SCHEMA_FAIL);
    }

    FREE_AND_RETURN(crpm, ctx, DCRCP_STATUS_OK);
}
