/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_PROTO_H
#define FP_PROTO_H

/**
 * @defgroup fp
 * @{
 *
 * Public interface of the Function Protocol Protocol routines.
 *
 * @file fp_proto.h
 *
 * @}
 */

#include "fp_defs.h"
#include <stdint.h>
#include "fp_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
***************************************************************************************************
* EXTERNAL TYPES DECLARATIONS
***************************************************************************************************
*/
/**
 * @brief Possible values for the @ref fp_msg_hdr_t::status field (used by FP responses; FP requests shall always set this to 0)
 *
 * 0x1X — Local error codes (e.g. operation failed on server or client side locally)
 * 0x2X — Success codes (can be returned either by client or server on the bus)
 * 0x3X — Client error codes
 * 0x4X — Protocol error codes
 * 0x5X — Server error codes
 *
 */
typedef enum
{
    FP_CMD_STATUS_INVARGS            = 0x10, /**< argument validity check failed */
    FP_CMD_STATUS_SENDERR            = 0x11, /**< the frame could not be sent due to fp_disp_send error */
    FP_CMD_STATUS_NOERROR            = 0x20, /**< the operation was executed successfully */
    FP_CMD_STATUS_TIMEOUT            = 0x30, /**< the server did not provide a response for the specified timeout period */
    FP_CMD_STATUS_NO_MEM             = 0x31, /**< not enough memory to track the request (configure more client slots) */
    FP_CMD_STATUS_UNEXP_FRM_TYPE     = 0x40, /**< protocol violation error indicating that an unexpected frame type has been received */
    FP_CMD_STATUS_UNEXP_FRM_SIZE     = 0x41, /**< protocol violation error indicating that the received frame's size is not matching the FIDL-declared size */
    FP_CMD_STATUS_FUNC_NOT_SUPPORTED = 0x50, /**< the server does not support the requested operation */
    FP_CMD_STATUS_ARMED              = 0x51, /**< indicates the request is only armed but not yet executed */
    FP_CMD_STATUS_DISARMED           = 0x52, /**< indicates the request is disarmed due to timeout or a reception of a DISARM command */
    FP_CMD_STATUS_REQ_ATTR_ERR       = 0x53, /**< the request is received with unexpected attribute bits in relation to server deployment configuration (e.g. ARM-and-FIRE request bits set for a regular request) */
    FP_CMD_STATUS_DATA_INTEGRITY_ERR = 0x54, /**< the relevant data fields of an ARMed request changed between stages but must be preserved for the request to operate */
    FP_CMD_STATUS_MAX                        /**< value used for range checking */
} fp_cmd_status_t;

PACK_STRUCT_BEGIN

// FunctionProtocol header
//
// Function ID format:
//         31            24      20                                       0
//          |             |       |                                       |
// MS bit [ x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x ] LS bit 0 (32 bits)
//          ----------------------- ---------------------------------------
//                    |                                     |
//         Function Attribute Bits (12 bits)            Function ID (20 bits)
//
typedef struct
{
    uint16_t iface_id; /**< Interface ID (matching the FIDL interface ID) */
    uint32_t func_id;  /**< Function  ID */
    uint16_t cmd_id;   /**< Command ID (unique ID used to match requests and responses) */
    // enum type is not used here on purpose to ensure the field size is always one byte (refer to @ref fp_cmd_status_t for details)
    uint8_t status;
} fp_msg_hdr_t;

PACK_STRUCT_END

/** @brief Possible values for the FP frame types */
typedef enum
{
    FP_FRAME_TYPE_REQ        = 0, /**< Standard Function Protocol request frame */
    FP_FRAME_TYPE_REQ_ARM    = 1, /**< ARM request frame */
    FP_FRAME_TYPE_REQ_FIRE   = 2, /**< FIRE request frame */
    FP_FRAME_TYPE_REQ_DISARM = 3, /**< DISARM request frame */
    FP_FRAME_TYPE_RESP       = 4, /**< Standard Function Protocol response frame */
    FP_FRAME_TYPE_BROADCAST  = 5, /**< Broadcast frame */
    FP_FRAME_TYPE_MAX             /**< value used for range checking */
} fp_frame_type_t;

/** @brief Supported attributes for a FIDL-specified function */
typedef struct
{
    uint32_t        bare_func_id; /**< function ID with attribute bits stripped off (can be used to dispatch FP functions directly) */
    fp_frame_type_t frame_type;   /**< indicates the frame type */
} fp_proto_func_attr_t;

/*
***************************************************************************************************
* EXTERNAL DEFINES
***************************************************************************************************
*/
#define FP_FRAME_TYPE_INVALID (FP_FRAME_TYPE_MAX) /**< define added for improved clarity when checking the FP frame type */

/*
***************************************************************************************************
* EXTERNAL VARIABLES DECLARATIONS
***************************************************************************************************
*/

/*
***************************************************************************************************
* EXTERNAL ROUTINES DECLARATIONS
***************************************************************************************************
*/

/** @brief Parse FP-specific attributes for the specified FP message.
 *
 *  @param[in] p_fp_hdr FP frame header pointer
 *  @param[out] p_attr pointer to the buffer where attribute information will be returned
 *
 *  @note The function assumes a valid FP msg is passed to it.
 */
FP_API void fp_proto_get_func_attributes(const fp_msg_hdr_t *const p_fp_hdr, fp_proto_func_attr_t *const p_attr);

/** @brief Applies a specific frame type to the given FP frame
 *
 *  @deprecated This function is deprecated. Use fp_proto_set_function_type() instead.
 *
 *  This function only modifies the frame type bits without fully initializing the func_id field.
 *  It assumes the buffer is already initialized and the function ID is already set, which can
 *  lead to incorrect bits remaining set if the buffer contains uninitialized data.
 *
 *  Use fp_proto_set_function_type() for complete initialization of the func_id field in a single
 *  atomic operation.
 *
 *  @param[in] p_fp_hdr FP frame header pointer
 *  @param[in] frame_type Frame type to set
 */
FP_API void fp_proto_set_frame_type(fp_msg_hdr_t *const p_fp_hdr, const fp_frame_type_t frame_type);

/** @brief Set both function ID and frame type in a single operation
 *
 *  This function fully initializes the func_id field by combining the raw function ID
 *  (lower 20 bits) with the appropriate frame type bitmask (upper 12 bits). This ensures
 *  complete initialization and prevents issues where uninitialized buffer data could leave
 *  incorrect bits set.
 *
 *  @param[in,out] p_fp_hdr FP frame header pointer
 *  @param[in] func_id FP function ID to set (only lower 20 bits are used)
 *  @param[in] frame_type Frame type to set
 *
 *  @note This is the preferred method for initializing the func_id field as it guarantees
 *        all bits are properly set in a single atomic operation.
 */
FP_API void fp_proto_set_function_type(fp_msg_hdr_t *const p_fp_hdr, const uint32_t func_id, const fp_frame_type_t frame_type);

/** @brief Modify only the function ID of the header without touching the attribute bits
 *
 *  @param[in,out] p_fp_hdr FP frame header pointer
 *  @param[in] func_id FP function ID to set in the FP frame header
 *
 */
FP_API void fp_proto_set_func_id(fp_msg_hdr_t *const p_fp_hdr, const uint32_t func_id);

/** @brief Performs a comparison between two function IDs by masking attribute bits
 *
 *  @param[in] func_id_1 first function ID parameter to compare
 *  @param[in] func_id_2 second function ID parameter to compare*
 *
 *  @retval true function IDs match
 *  @retval false function IDs do not match
 */
FP_API bool fp_proto_is_bare_func_id_matching(const uint32_t func_id_1, const uint32_t func_id_2);

#ifdef __cplusplus
};
#endif

#endif
