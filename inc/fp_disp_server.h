/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_DISP_SERVER_H
#define FP_DISP_SERVER_H

/**
 * @defgroup fp
 * @{
 *
 * Public interface of the Function Protocol Dispatcher server module.
 *
 * @file fp_disp_server.h
 *
 * @}
 */

#include "fp_disp.h"
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
/** @brief FP request types supported by this implementation */
typedef enum
{
    FP_DISP_SERVER_REQ_TYPE_REGULAR      = 0, /**< Standard FP request type which requires immediate execution on the server and doesn't need session tracking */
    FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE = 1, /**< ARM-and-FIRE request which is executed in two stages to avoid accidental execution with a single command */
    FP_DISP_SERVER_REQ_TYPE_MAX
} fp_disp_server_req_type_t;

/*
***************************************************************************************************
* EXTERNAL VARIABLES DECLARATIONS
***************************************************************************************************
*/
/** @brief Specific attribute information for server session instances
 */
FP_API extern const fp_disp_session_attributes_t fp_disp_server_sess_attrib;

/*
***************************************************************************************************
* EXTERNAL ROUTINES DECLARATIONS
***************************************************************************************************
*/
/** @brief Handles a client request with the given parameters.
 *
 *  The function is intended to be called by the FIDL-generated code as a generic handler of all
 *  requests. It performs handling of special request types such as arm-and-fire but also handles
 *  regular requests transparently by forwarding directly to the user handler.
 *
 *  @param[in] p_disp_ctx FP frame request context
 *  @param[in] req_type type of FP request as defined by the user configuration for the server FIDL commands (usually passed by generated code)
 *  @param[in] p_req_handler user-request handler (will be passed by the FIDL generated code)
 *  @param[in] req_type_size maximum size in bytes of the actual FP function request data type (used for filtering invalid requests early)
 *
 */
FP_API void fp_disp_server_handle_req(fp_dispatch_info_t             *p_disp_ctx,
                                      const fp_disp_server_req_type_t req_type,
                                      fp_generic_request_handler_t    p_req_handler,
                                      const uint32_t                  req_type_size);

/** @brief Performs a server response with the given parameters.
 *
 *  @param[in] p_disp_ctx FP frame response context
 *  @param[in] iface_id FP interface ID (as defined in the FIDL file annotated comment for the interface)
 *  @param[in] func_id FP function ID (as defined in the FIDL file annotated comment for a method)
 *  @param[in] p_resp pointer to the response data buffer
 *  @param[in] resp_size size in bytes of the data pointed to by the @ref p_resp parameter
 *
 *  @retval true send was successful
 *  @retval false send failed
 *
 *  @note The function ensures all FP header properties are filled according to the request before delegating the
 *  send operation to @ref fp_send
 */
FP_API bool fp_disp_server_send_resp(fp_dispatch_info_t *p_disp_ctx,
                                     const uint16_t      iface_id,
                                     const uint32_t      func_id,
                                     uint8_t *const      p_resp,
                                     const uint32_t      resp_size);

/** @brief Performs a server broadcast with the given parameters.
 *
 *  @param[in] p_disp_ctx FP frame broadcast context
 *  @param[in] iface_id FP interface ID (as defined in the FIDL file annotated comment for the interface)
 *  @param[in] func_id FP function ID (as defined in the FIDL file annotated comment for a method)
 *  @param[in] p_bcast pointer to the broadcast data buffer
 *  @param[in] bcast_size size in bytes of the data pointed to by the @ref p_bcast parameter
 *
 *  @retval true send was successful
 *  @retval false send failed
 *
 *  @note The function ensures all FP header properties are filled before delegating the
 *  send operation to @ref fp_disp_send. Unlike responses, broadcasts generate a new unique
 *  cmd_id using @ref fp_internal_gen_cmd_id, as broadcasts are one-way notifications that
 *  don't match to specific requests.
 */
FP_API bool fp_disp_server_send_bcast(fp_dispatch_info_t *p_disp_ctx,
                                      const uint16_t      iface_id,
                                      const uint32_t      func_id,
                                      uint8_t *const      p_bcast,
                                      const uint32_t      bcast_size);

#ifdef __cplusplus
};
#endif

#endif
