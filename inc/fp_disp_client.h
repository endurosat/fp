/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_DISP_CLIENT_H
#define FP_DISP_CLIENT_H

/**
 * @defgroup fp
 * @{
 *
 * Public interface of the Function Protocol Dispatcher client module.
 *
 * @file fp_disp_client.h
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

/** @brief Base type for client-received messages (responses and broadcasts)
 *  This type abstracts the common properties of all client-side received frames
 *  to enable ABI-compatible handling in the core dispatch logic.
 */
typedef struct
{
    fp_dispatch_info_t *p_disp_ctx; /**< Dispatch context */
    uint32_t            msg_size;   /**< Expected message size */
} fp_client_frame_base_t;

/** @brief Generic typedef for a broadcast handler which doesn't specify the actual broadcast data type
 *  in order to support generic handling in the FP dispatcher instead of inside the generated code.
 *
 *  @param[in] p_disp_ctx FP dispatch context (of type @ref fp_dispatch_info_t) - void pointer used to avoid cyclic deps
 *  @param[in] p_untyped_bcast generic pointer to the broadcast data bytes (proper casting to the specific broadcast type structure will be performed by the user handler)
 */
typedef void (*fp_generic_broadcast_handler_t)(void    *p_disp_ctx,
                                               uint8_t *p_untyped_bcast);

/*
***************************************************************************************************
* EXTERNAL VARIABLES DECLARATIONS
***************************************************************************************************
*/
/** @brief Specific attribute information for client session instances
 */
FP_API extern const fp_disp_session_attributes_t fp_disp_client_sess_attrib;

/*
***************************************************************************************************
* EXTERNAL ROUTINES DECLARATIONS
***************************************************************************************************
*/
/** @brief Perform a client request with the given parameters.
 *
 *  @param[in] p_disp_ctx FP frame request context
 *  @param[in] iface_id FP interface ID (as defined in the FIDL file annotated comment for the interface)
 *  @param[in] func_id FP function ID (as defined in the FIDL file annotated comment for a method)
 *  @param[in] frame_type Frame type to use for the request (FP_FRAME_TYPE_REQ, FP_FRAME_TYPE_REQ_ARM, FP_FRAME_TYPE_REQ_FIRE, or FP_FRAME_TYPE_REQ_DISARM)
 *  @param[in] p_req pointer to the request data buffer
 *  @param[in] req_size size in bytes of the data pointed to by the @ref p_req parameter
 *  @param[in] p_resp pointer to the user response data storage in case of synchronous requests (for async requests, specify as NULL)
 *  @param[in] p_resp_handler user-response handler callback (if NULL, the request is assumed to be synchronous and it will block the caller)
 *  @param[in] timeout milliseconds to wait for the reception of the response in case of blocking requests
 *
 *  @returns operation status code
 *
 *  @note The function performs size checking of the request and calls the user-registered
 *  handler (if assigned).
 *  @note On a successful synchronous request, @p p_disp_ctx->data_size is set to the number of
 *  response bytes copied into @p p_resp (0 on error/timeout).
 *  @note Writes into @p p_resp (the response copy and the error-path zeroing) are bounded by the
 *  FIDL-declared response size of the function when it is known, else by the pool-wide capacity.
 *  For a buffer smaller than that, use @ref fp_disp_client_send_req_bounded.
 */
FP_API fp_cmd_status_t fp_disp_client_send_req(fp_dispatch_info_t           *p_disp_ctx,
                                               const uint16_t                iface_id,
                                               const uint32_t                func_id,
                                               const fp_frame_type_t         frame_type,
                                               uint8_t *const                p_req,
                                               const uint32_t                req_size,
                                               uint8_t *const                p_resp,
                                               fp_generic_response_handler_t p_resp_handler,
                                               const uint32_t                timeout);

/** @brief Like @ref fp_disp_client_send_req, but bound the response copy to @p resp_max_size.
 *
 *  Use when @p p_resp is smaller than the pool-wide session capacity. @p resp_max_size caps the
 *  bytes written to @p p_resp on both the success copy and the error-path zeroing.
 *
 *  @param[in] p_disp_ctx FP frame request context
 *  @param[in] iface_id FP interface ID
 *  @param[in] func_id FP function ID
 *  @param[in] frame_type Frame type to use for the request
 *  @param[in] p_req pointer to the request data buffer
 *  @param[in] req_size size in bytes of the data pointed to by @p p_req
 *  @param[in] p_resp pointer to the user response data storage (synchronous requests)
 *  @param[in] resp_max_size capacity in bytes of @p p_resp (0 == fall back to the pool capacity)
 *  @param[in] p_resp_handler user-response handler callback (NULL => synchronous/blocking request)
 *  @param[in] timeout milliseconds to wait for the response in case of blocking requests
 *
 *  @returns operation status code
 */
FP_API fp_cmd_status_t fp_disp_client_send_req_bounded(fp_dispatch_info_t           *p_disp_ctx,
                                                       const uint16_t                iface_id,
                                                       const uint32_t                func_id,
                                                       const fp_frame_type_t         frame_type,
                                                       uint8_t *const                p_req,
                                                       const uint32_t                req_size,
                                                       uint8_t *const                p_resp,
                                                       const uint32_t                resp_max_size,
                                                       fp_generic_response_handler_t p_resp_handler,
                                                       const uint32_t                timeout);

/** @brief Perform general handling of a received FP function response.
 *
 *  @param[in] p_disp_ctx FP frame receive context
 *  @param[in] resp_type_size maximum size in bytes of the actual FP function response data type
 *
 *  @retval true  the response matched a waiting client session and was delivered/dispatched
 *  @retval false invalid args, or no client session matched (unexpected/stale reply)
 *
 *  @note The operation takes care of response slot management, sequence ID matching and
 *  invokation of the actual user response handler.
 */
FP_API bool fp_disp_client_handle_resp(fp_dispatch_info_t *p_disp_ctx,
                                       const uint32_t      resp_type_size);

/** @brief Deliver a RESP frame that matched no registered serdes client.
 *
 *  No typed response size exists, so the frame is size-unvalidated and delivered only to a
 *  matching synchronous session (length-bounded copy); a matching asynchronous session is refused.
 *
 *  @param[in] p_disp_ctx FP frame receive context
 *
 *  @retval true  the response matched a waiting synchronous client session and was delivered
 *  @retval false invalid args, no client session matched, or the matched session was asynchronous
 */
FP_API bool fp_disp_client_handle_resp_unmatched(fp_dispatch_info_t *p_disp_ctx);

/** @brief Perform general handling of a received FP broadcast frame.
 *
 *  @param[in] p_disp_ctx FP frame receive context
 *  @param[in] bcast_type_size expected size in bytes of the actual FP broadcast data type
 *  @param[in] p_bcast_handler user-registered broadcast handler callback
 *
 *  @note The operation performs frame type validation, size checking, and invokes
 *  the user-registered broadcast handler. Unlike responses, broadcasts do not require
 *  session management, cmd_id matching, or sync/async handling.
 */
FP_API void fp_disp_client_handle_bcast(fp_dispatch_info_t            *p_disp_ctx,
                                        const uint32_t                 bcast_type_size,
                                        fp_generic_broadcast_handler_t p_bcast_handler);

#ifdef __cplusplus
};
#endif

#endif
