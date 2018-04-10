/**
 * @file    dcrcp.h
 * @brief   DeadCom Reader-Controller Protocol implementation
 *
 * This files defines interface of DeadCom Reader-Controller Protocol C implementation. This
 * protocol is used for communication between Reader and Controller. The C implementation handles
 * packing and unpacking CRPMs. Internally it is based on cn-cbor (Constrained Node CBOR)
 * https://github.com/cabo/cn-cbor
 *
 * This library is designed to be used by embedded components of Project Deadlock.
 */

#ifndef __DEADCOM_DCRCP_H
#define __DEADCOM_DCRCP_H

#include <stdint.h>
#include <stdlib.h>
#include "cn-cbor/cn-cbor.h"

#define DCRCP_AM0_MAX_UID_LEN   10

/**
 * @brief CRPM Type constants.
 *
 * These must match Frame IDs defined in `docs/crpm/protocol.cddl`.
 */
typedef enum {
    DCRCP_CRPM_SYS_QUERY_REQUEST      = 1,
    DCRCP_CRPM_SYS_QUERY_RESPONSE     = 2,
    DCRCP_CRPM_ACTIVATE_AUTH_METHOD   = 3,
    DCRCP_CRPM_RDR_FAILURE            = 4,
    DCRCP_CRPM_UI_UPDATE              = 5,
    DCRCP_CRPM_AM0_PICC_UID_OBTAINED  = 6
} DeadcomCRPMType;


/**
 * @brief CRPM Defined authentication method constants.
 */
typedef enum {
    DCRCP_CRPM_AM_PICC_UUID           = 0
} DeadcomCRPMAuthMethod;


/**
 * @brief CRPM User Interface states for Reader Class 0 UIs
 */
typedef enum {
    DCRCP_CRPM_UIC0_DOOR_CLOSED                 = 0,
    DCRCP_CRPM_UIC0_ID_ACCEPTED_DOOR_UNLOCKED   = 1,
    DCRCP_CRPM_UIC0_ID_REJECTED                 = 2,
    DCRCP_CRPM_UIC0_DOOR_PERMANENTLY_UNLOCKED   = 3,
    DCRCP_CRPM_UIC0_DOOR_PERMANENTLY_LOCKED     = 4,
    DCRCP_CRPM_UIC0_SYSTEM_FAILURE              = 5,
    DCRCP_CRPM_UIC0_DOOR_OPEN_TOO_LONG          = 6
} DeadcomCRPMUIClass0States;


/**
 * @brief A result of DCRCP operation
 */
typedef enum {
    DCRCP_STATUS_OK,            /**< Operation succeeded                                          */
    DCRCP_STATUS_INVALID_PARAM, /**< Invalid parameter passed to a function                       */
    DCRCP_STATUS_BUFFER_SMALL,  /**< Output buffer too small for encoded CRPM                     */
    DCRCP_STATUS_DECODE_FAIL,   /**< CRPM decoding has failed                                     */
    DCRCP_STATUS_SCHEMA_FAIL,   /**< CRPM decoded but did not contain expected data               */
    DCRCP_STATUS_OUT_OF_MEMORY  /**< Allocation context failed to allocate required memory        */
} DCRCPStatus;


/**
 * @brief CRPM Authentication Method 0 UID payload
 */
typedef struct {
    uint8_t uid[DCRCP_AM0_MAX_UID_LEN];
    size_t  uid_len;
} DeadcomCRPMAM0Uid;


/**
 * @brief Structure representing a single CRPM
 */
typedef struct DeadcomCRPM {
    DeadcomCRPMType type;
    union CRPMDataUnion {
        struct CRPMSysQueryResponsePayload {
            uint16_t rdrClass;
            uint16_t hwModel;
            uint16_t hwRev;
            char     serial[26];
            uint8_t  swVerMajor;
            uint8_t  swVerMinor;
        } sysQueryResponse;
        struct CRPMAuthMethodArray {
            DeadcomCRPMAuthMethod vals[10];
            size_t len;
        } authMethods;
        char rdrFailure[200];
        DeadcomCRPMUIClass0States ui_class0_state;
        struct CRPMAM0UuidPayload {
            size_t len;
            DeadcomCRPMAM0Uid vals[10];
        } authMethod0UuidObtained;
    } data;
} DeadcomCRPM;


/**
 * Encode CRPM in struct `crpm_in` to byte buffer.
 *
 * @param[in]    crpm_in  A structure representing a CRPM to be encoded to bytes
 * @param[out]   buf_out  Encoded bytes
 * @param[in]    buf_size  Maximum number of bytes that can be written to buf_out
 * @param[out]   out_size  Number of bytes actually written to `buf_out`
 * @param[inout] ctx  cn-cbor allocation context. See cn-cbor docs for explanation.
 *
 * @retval DCRCP_STATUS_OK  Operation succeded
 * @retval DCRCP_STATUS_INVALID_PARAM  Invalid parameters passed to the function
 * @retval DCRCP_STATUS_BUFFER_SMALL  Buffer too small for encoded byte
 * @retval DCRCP_STATUS_OUT_OF_MEMORY  Can't allocate memory required for CRPM encoding
 */
DCRCPStatus dcrcpEncode(DeadcomCRPM *crpm_in, uint8_t *buf_out, size_t buf_size, size_t *out_size,
                        cn_cbor_context *ctx);


/**
 * Decode bytes to CRPM struct `crpm_out`
 *
 * @param[out]   crpm_out  Output CRPM struct
 * @param[in]    buf_in  Input bytes
 * @param[in]    buf_size  Size of the input buffer
 * @param[inout] ctx  cn-cbor allocation context.
 *
 * @retval DCRCP_STATUS_OK  Operation succeeded
 * @retval DCRCP_STATUS_INVALID_PARAM  Invalid parameters passed to the function
 * @retval DCRCP_STATUS_DECODE_FAIL The buffer did not contain valid CBOR
 * @retval DCRCP_STATUS_SCHEMA_FAIL  CBOR decoded but data interpretation failed
 * @retval DCRCP_STATUS_OUT_OF_MEMORY  Can't allocate memory required for CRPM decoding
 *
 * @see dcrcpEncode
 */
DCRCPStatus dcrcpDecode(DeadcomCRPM *crpm_out, uint8_t *buf_in, size_t buf_size,
                        cn_cbor_context *ctx);


#endif
