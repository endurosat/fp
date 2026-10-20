/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_DISP_H
#define FP_DISP_H

/**
 * @defgroup fp
 * @{
 *
 * Public interface of the Function Protocol Dispatcher module.
 *
 * @file fp_disp.h
 *
 * @}
 */

#include <stdint.h>
#include "fp_defs.h"
#include "fp_proto.h"
#include "fp_disp_session_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
***************************************************************************************************
* EXTERNAL DEFINES
***************************************************************************************************
*/

/*
***************************************************************************************************
* EXTERNAL TYPES DECLARATIONS
***************************************************************************************************
*/

/**
 * @brief Function Protocol (FP) dispatch context passed to each FP handler or
 * used when sending a reply to an FP node
 *
 * It contains FP node addressing and information about the data to be sent.
 * It also provides system-specific handles to properly send and synchronize
 * FP data access for a specific environment.
 */
typedef struct
{
    void                      *p_comm_params;   /**< pointer to a user-specific communication parameters which must be preserved along with the specific FP dispatch context (not used by FP-related functions but can be utilized by the p_on_fp_send_cbk function to know how to send the data to the physical bus) */
    uint8_t                   *p_data;          /**< pointer to the FP data (starting with the FP header) */
    uint32_t                   data_size;       /**< size of the data referred to by @ref p_data (in bytes) */
    void                      *p_ext_buf_ref;   /**< an untyped reference to an external buffer if such was allocated through p_sys_ctx->p_alloc_buf function (this depends on the deployment configuration for the generated code) */
    fp_dispatch_sys_context_t *p_sys_ctx;       /**< pointer to the system dispatch context used to handle the data for an FP transaction */
    fp_disp_session_base_t    *p_assigned_sess; /**< pointer to the session context assigned to this FP transaction */
} fp_dispatch_info_t;

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
/** @brief Find the corresponding FIDL interface and function, and execute any
 * registered user-callbacks for this command returning a reply with an error code automatically if
 * the requested command is not found
 *
 * @param[in] p_disp_ctx FP frame dispatch context
 * @retval true the FP frame was dispatched successfully
 * @retval false the FP frame dispatch failed
 *
 * @note Automatic responses will be returned only for invalid requests. Invalid requests will silently
 * be ignored by this function.
 */
FP_API bool fp_disp_execute(fp_dispatch_info_t *p_disp_ctx);

/**
 * @brief Send an FP frame to an FP node
 *
 * @param[in,out] p_disp_ctx FP frame send context
 * @retval true send was successful
 * @retval false send failed
 *
 * @note The caller must ensure the message header's func_id field is properly initialized
 *       with both the function ID and frame type bits before calling this function.
 *       Use fp_proto_set_function_type() to set both values in a single operation.
 */
FP_API bool fp_disp_send(fp_dispatch_info_t *p_disp_ctx);

/**
 * @brief Send an FP frame with a status response (without any additional data) to an FP node
 *
 * @param[in,out] p_disp_ctx FP frame send context
 * @param[in] status status to send
 * @retval true send was successful
 * @retval false send failed
 */
FP_API bool fp_disp_send_status_resp(fp_dispatch_info_t *p_disp_ctx, const fp_cmd_status_t status);

#ifdef __cplusplus
};
#endif

#endif
