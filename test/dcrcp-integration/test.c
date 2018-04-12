#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include "unity.h"
#include "dcrcp.h"


/**************************************************************************************************/
/* Memory allocation context implementation with counting */

typedef struct test_alloc_block test_alloc_block;
struct test_alloc_block {
    void* loc;
    test_alloc_block *next;
};

typedef struct {
    size_t alloc_limit;
    size_t allocated;
} test_alloc_context;

cn_cbor* context_calloc(void *context) {
    test_alloc_context *tctx = (test_alloc_context*) context;

    if ((tctx->allocated + 1) > tctx->alloc_limit) {
        return NULL;
    }

    cn_cbor *n = calloc(1, sizeof(cn_cbor));

    if (n != NULL) {
        tctx->allocated += 1;
    }

    return n;
}

void context_free(cn_cbor *ptr, void *context) {
    test_alloc_context *tctx = (test_alloc_context*) context;
    free(ptr);
    tctx->allocated -= 1;
}

// This function creates a cn-cbor allocation context which uses `malloc` and `free`, but internally
// keeps a linked-list of allocated blocks and their sizes, and can limit number of bytes allocated.
cn_cbor_context* create_limited_context(size_t alloc_limit) {
    cn_cbor_context *r = calloc(1, sizeof(cn_cbor_context));
    test_alloc_context *ctx = calloc(1, sizeof(test_alloc_context));

    ctx->alloc_limit = alloc_limit;

    r->context = ctx;
    r->calloc_func = context_calloc;
    r->free_func = context_free;
    return r;
}


bool was_everything_deallocated(cn_cbor_context *ctx) {
    test_alloc_context *tctx = (test_alloc_context*) ctx->context;
    return tctx->allocated == 0;
}

void free_limited_context(cn_cbor_context *ctx) {
    test_alloc_context *tctx = (test_alloc_context*) ctx->context;
    free(tctx);
    free(ctx);
}

/* End Memory allocation context implementation with counting */
/**************************************************************************************************/


/**************************************************************************************************/
/* Encode-decode identity with memory constraint and schema validation tester */

#define CRPM_SIZE_LIMIT        245
#define PRINT_DEBUG_BYTES      false

// This define turns on verification against CDDL schema. The code that does that is extremely
// ugly and depends on ruby gem `cddl` being installed and in your path.
#define VERIFY_AGAINST_CDDL    true
const char dcrcp_cddl_spec_path[] = "docs/dcrcp/protocol.cddl";
const char cddl_cmdline[] = "cddl %s verify %s";


void run_test(DeadcomCRPM *crpm) {
    DeadcomCRPM out;
    memset(&out, 0, sizeof(DeadcomCRPM));

    cn_cbor_context *e_ctx = create_limited_context(DCRCP_REQUIRED_CNCBOR_BUFFERS);
    cn_cbor_context *d_ctx = create_limited_context(DCRCP_REQUIRED_CNCBOR_BUFFERS);

    uint8_t buf[CRPM_SIZE_LIMIT];
    size_t out_size;
    TEST_ASSERT_EQUAL(DCRCP_STATUS_OK, dcrcpEncode(crpm, buf, CRPM_SIZE_LIMIT, &out_size, e_ctx));
    TEST_ASSERT(was_everything_deallocated(e_ctx));

    if (PRINT_DEBUG_BYTES) {
        unsigned int cnt;
        for (cnt = 0; cnt < out_size; cnt++) {
            printf("%02x", buf[cnt]);
        }
        printf("\n");
    }

    if (VERIFY_AGAINST_CDDL) {
        char tmp_path[] = "/tmp/dcrcp_test_XXXXXX";
        int fd;
        TEST_ASSERT_NOT_EQUAL(-1, fd = mkstemp(tmp_path));
        size_t written = 0;
        while (written < out_size) {
            ssize_t w;
            TEST_ASSERT_NOT_EQUAL(-1,  w = write(fd, (buf+written), out_size-written));
            written += w;
        }
        TEST_ASSERT_EQUAL(0, close(fd));

        int cmd_len = snprintf(NULL, 0, cddl_cmdline, dcrcp_cddl_spec_path, tmp_path);
        char cddl_verify_cmd[cmd_len+2];
        snprintf(cddl_verify_cmd, cmd_len+2, cddl_cmdline, dcrcp_cddl_spec_path, tmp_path);
        TEST_ASSERT_EQUAL(0, WEXITSTATUS(system(cddl_verify_cmd)));

        unlink(tmp_path);
    }

    TEST_ASSERT_EQUAL(DCRCP_STATUS_OK, dcrcpDecode(&out, buf, out_size, d_ctx));
    TEST_ASSERT(was_everything_deallocated(d_ctx));

    TEST_ASSERT_EQUAL_MEMORY(crpm, &out, sizeof(DeadcomCRPM));

    free_limited_context(e_ctx);
    free_limited_context(d_ctx);
}

/* End Encode-decode identity with memory constraint and schema validation tester */
/**************************************************************************************************/


void test_EncodeDecodeIdempotence_SysQueryRequest() {
    DeadcomCRPM c; memset(&c, 0, sizeof(DeadcomCRPM));
    c.type = DCRCP_CRPM_SYS_QUERY_REQUEST;
    run_test(&c);
}

void test_EncodeDecodeIdempotence_SysQueryResponse() {
    DeadcomCRPM c; memset(&c, 0, sizeof(DeadcomCRPM));
    c.type = DCRCP_CRPM_SYS_QUERY_RESPONSE;
    c.data.sysQueryResponse.rdrClass = 0;
    c.data.sysQueryResponse.hwModel  = 1;
    c.data.sysQueryResponse.hwRev    = 1;
    memcpy(c.data.sysQueryResponse.serial, "DEADBEEFDEADBEEFBADF00D00", 26);
    c.data.sysQueryResponse.swVerMajor = 1;
    c.data.sysQueryResponse.swVerMinor = 1;
    run_test(&c);
}

void test_EncodeDecodeIdempotence_ActivateAuthMethod() {
    DeadcomCRPM c; memset(&c, 0, sizeof(DeadcomCRPM));
    c.type = DCRCP_CRPM_ACTIVATE_AUTH_METHOD;
    c.data.authMethods.vals[0] = DCRCP_CRPM_AM_PICC_UUID;
    c.data.authMethods.len = 1;
    run_test(&c);
}

void test_EncodeDecodeIdempotence_RdrFailure() {
    DeadcomCRPM c; memset(&c, 0, sizeof(DeadcomCRPM));
    c.type = DCRCP_CRPM_RDR_FAILURE;
    const char rdr_err_msg[] = "Something terrible has happened and the Reader is on fire";
    strncpy(c.data.rdrFailure, rdr_err_msg, 200);
    run_test(&c);
}

void test_EncodeDecodeIdempotence_UiUpdate() {
    DeadcomCRPM c; memset(&c, 0, sizeof(DeadcomCRPM));
    c.type = DCRCP_CRPM_UI_UPDATE;
    c.data.ui_class0_state = DCRCP_CRPM_UIC0_DOOR_PERMANENTLY_UNLOCKED;
    run_test(&c);
}

void test_EncodeDecodeIdempotence_AM0PiccUidObtained() {
    DeadcomCRPM c; memset(&c, 0, sizeof(DeadcomCRPM));
    c.type = DCRCP_CRPM_AM0_PICC_UID_OBTAINED;
    c.data.authMethod0UuidObtained.len = 3;

    uint8_t uid[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0 };

    c.data.authMethod0UuidObtained.vals[0].uid_len = 4;
    memcpy(c.data.authMethod0UuidObtained.vals[0].uid, uid, 4);

    c.data.authMethod0UuidObtained.vals[1].uid_len = 7;
    memcpy(c.data.authMethod0UuidObtained.vals[1].uid, uid, 7);

    c.data.authMethod0UuidObtained.vals[2].uid_len = 10;
    memcpy(c.data.authMethod0UuidObtained.vals[2].uid, uid, 10);

    run_test(&c);
}
