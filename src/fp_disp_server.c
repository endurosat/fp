/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @addtogroup fp
 * @{
 *
 * @file fp_disp_server.c
 * @brief Implementation of the Function Protocol Dispatcher server-specific routines.
 *
 * @}
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "fp_proto.h"
#include "fp_disp.h"
#include "fp_disp_server.h"
#include "fp_disp_session_mgr.h"
#include "fp_internal.h"

/*
***************************************************************************************************
* INTERNAL DEFINES
***************************************************************************************************
*/

/*
***************************************************************************************************
* INTERNAL TYPES DEFINITION
***************************************************************************************************
*/
/** @brief Generic special request handler type
 *
 *  @param[in] p_disp_ctx FP frame request context
 *  @param[in] p_req_handler user-request handler (will be passed by the FIDL generated code)
 */
typedef void (*p_special_req_handler)(fp_dispatch_info_t                 *p_disp_ctx,
                                      fp_generic_request_handler_t *const p_req_handler);

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DECLARATION
***************************************************************************************************
*/
/** @brief Handle an ARM-and-FIRE request
 *
 *  @param[in] p_disp_ctx FP frame request context
 *  @param[in] p_req_handler user-request handler (will be passed by the FIDL generated code)
 */
static void fp_disp_server_handle_arm_and_fire_req(fp_dispatch_info_t          *p_disp_ctx,
                                                   fp_generic_request_handler_t p_req_handler);

/** @brief Initialize a server session context to default values
 *
 *  @param[in] p_sess pointer to the session context to initialize
 */
static inline void fp_disp_server_session_init(fp_disp_session_base_t *const p_sess);

/** @brief Checks if an FP request header stored in a server session matches the newly received one
 *
 *  @param[in] p_sess pointer to the server session data
 *  @param[in] p_new_hdr pointer to the received request FP header to check against
 *
 *  @retval true the session header matches the received header by interface and function ID
 *  @retval false the session header doesn't match the received header
 *
 *  @note The function is used to match arm-and-fire requests during the different stages of handling.
 */
static inline bool fp_disp_server_session_is_matching(const fp_disp_session_base_t *const p_sess,
                                                      const fp_msg_hdr_t *const           p_new_hdr);

/** @brief Performs general server session tracking actions, e.g. handling arm-and-fire request timeouts and potentially other
 *  necessary regular checks
 *
 *  @param[in] p_sys_ctx system-specific context containing the session pools and related locks
 *  @param[in] p_sess pointer to the server session data
 */
static void fp_disp_server_session_track(fp_dispatch_sys_context_t *const p_sys_ctx,
                                         fp_disp_session_base_t *const    p_sess);

/*
***************************************************************************************************
* EXTERNAL VARIABLES DEFINITION
***************************************************************************************************
*/
const fp_disp_session_attributes_t fp_disp_server_sess_attrib = {
    // clang-format off
    .p_init_func = &fp_disp_server_session_init,
    .p_is_matching = &fp_disp_server_session_is_matching,
    .p_track_session = &fp_disp_server_session_track,
    .sess_type = FP_DISP_SESSION_TYPE_SERVER
    // clang-format on
};

/*
***************************************************************************************************
* INTERNAL (STATIC) VARIABLES DEFINITION/DECLARATION
***************************************************************************************************
*/

/*
***************************************************************************************************
* EXTERNAL (NON STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/

void fp_disp_server_handle_req(fp_dispatch_info_t             *p_disp_ctx,
                               const fp_disp_server_req_type_t req_type_cfg,
                               fp_generic_request_handler_t    p_req_handler,
                               const uint32_t                  req_type_size)
{
    if ((NULL == p_disp_ctx) ||
        (NULL == p_disp_ctx->p_data) ||
        (NULL == p_req_handler) ||
        (p_disp_ctx->data_size != req_type_size))
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_disp_ctx=%p, p_data=%p, p_req_handler=%p, data_size=%u, expected_size=%u in fp_disp_server_handle_req",
                            p_disp_ctx, (p_disp_ctx ? p_disp_ctx->p_data : NULL), p_req_handler,
                            (p_disp_ctx ? p_disp_ctx->data_size : 0), req_type_size);

        return;
    }

    // ensure there is no assigned session by default... subsequent calls will take care to
    // assign a session to this context if necessary but in case a session is not opened to
    // handle the request, this will ensure the context is not left with a dangling pointer
    p_disp_ctx->p_assigned_sess = (fp_disp_session_base_t *)NULL;

    // Extract FP header from data
    fp_msg_hdr_t *p_fp_hdr = (fp_msg_hdr_t *)p_disp_ctx->p_data;
    (void)p_fp_hdr; // suppress 'unused variable' warning in case the trace macros below are disabled

    FP_ARCH_TRACE_DEBUG("Server request entry: iface_id=%u, func_id=%lu, cmd_id=%u",
                        p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

    switch (req_type_cfg)
    {
        case FP_DISP_SERVER_REQ_TYPE_REGULAR:
            {
                // directly call user-registered callback in case of a regular request
                (*p_req_handler)(p_disp_ctx, p_disp_ctx->p_data);

                break;
            }

        case FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE:
            {
                fp_disp_server_handle_arm_and_fire_req(p_disp_ctx, p_req_handler);
                break;
            }

        default:
            {
                FP_ARCH_TRACE_WARN("Protocol validation failed: invalid_req_type=%u for request='%u:%lu:%u', ignoring request",
                                   (uint8_t)req_type_cfg, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

                (void)fp_disp_send_status_resp(p_disp_ctx, FP_CMD_STATUS_REQ_ATTR_ERR);

                break;
            }
    }
}

/** @brief Internal helper to send a server frame (response or broadcast)
 *
 *  @param[in] p_disp_ctx FP frame context
 *  @param[in] iface_id FP interface ID
 *  @param[in] func_id FP function ID (bare function ID without attribute bits)
 *  @param[in] p_frame pointer to the frame data buffer
 *  @param[in] frame_size size in bytes of the data pointed to by p_frame
 *  @param[in] frame_type type of frame to send (FP_FRAME_TYPE_RESP or FP_FRAME_TYPE_BROADCAST)
 *  @param[in] copy_cmd_id if true, copy cmd_id from request context; if false, generate new cmd_id
 *
 *  @retval true send was successful
 *  @retval false send failed
 *
 *  @note This is an internal helper function used by both fp_disp_server_send_resp and fp_disp_server_send_bcast
 */
static bool fp_disp_server_send_frame(fp_dispatch_info_t   *p_disp_ctx,
                                      const uint16_t        iface_id,
                                      const uint32_t        func_id,
                                      uint8_t *const        p_frame,
                                      const uint32_t        frame_size,
                                      const fp_frame_type_t frame_type,
                                      const bool            copy_cmd_id)
{
    if ((NULL == p_disp_ctx) ||
        (NULL == p_frame))
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_disp_ctx=%p, p_frame=%p in fp_disp_server_send_frame",
                            p_disp_ctx, p_frame);

        return false;
    }

    fp_msg_hdr_t *p_fp_hdr = (fp_msg_hdr_t *)p_frame;

    p_fp_hdr->iface_id = iface_id;
    p_fp_hdr->status   = FP_CMD_STATUS_NOERROR;

    // Set function ID and frame type
    fp_proto_set_function_type(p_fp_hdr, func_id, frame_type);

    // Handle cmd_id based on frame type
    if (copy_cmd_id)
    {
        // Response: copy command ID from request context to match client request
        p_fp_hdr->cmd_id = ((fp_msg_hdr_t *)p_disp_ctx->p_data)->cmd_id;
    }
    else
    {
        // Broadcast: generate new unique command ID
        p_fp_hdr->cmd_id = fp_internal_gen_cmd_id();
    }

    p_disp_ctx->p_data    = p_frame;
    p_disp_ctx->data_size = frame_size;

    return fp_disp_send(p_disp_ctx);
}

bool fp_disp_server_send_resp(fp_dispatch_info_t *p_disp_ctx,
                              const uint16_t      iface_id,
                              const uint32_t      func_id,
                              uint8_t *const      p_resp,
                              const uint32_t      resp_size)
{
    return fp_disp_server_send_frame(p_disp_ctx, iface_id, func_id, p_resp, resp_size,
                                     FP_FRAME_TYPE_RESP, true);
}

bool fp_disp_server_send_bcast(fp_dispatch_info_t *p_disp_ctx,
                               const uint16_t      iface_id,
                               const uint32_t      func_id,
                               uint8_t *const      p_bcast,
                               const uint32_t      bcast_size)
{
    return fp_disp_server_send_frame(p_disp_ctx, iface_id, func_id, p_bcast, bcast_size,
                                     FP_FRAME_TYPE_BROADCAST, false);
}

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/
static void
fp_disp_server_handle_arm_and_fire_req(fp_dispatch_info_t          *p_disp_ctx,
                                       fp_generic_request_handler_t p_req_handler)
{
    fp_arch_assert((NULL != p_disp_ctx) && (NULL != p_disp_ctx->p_data));
    fp_arch_assert(NULL != p_req_handler);

    fp_msg_hdr_t *p_fp_hdr = (fp_msg_hdr_t *)p_disp_ctx->p_data;

    fp_proto_func_attr_t fattr;
    uint16_t             original_cmd_id = p_fp_hdr->cmd_id; // cache the original command ID to restore it after the masking performed before CRC calculation

    fp_proto_get_func_attributes(p_fp_hdr, &fattr);

    p_fp_hdr->cmd_id  = 0;                  // mask the command ID which changes with each client request to avoid differences in CRC calculation even if no other data changed
    p_fp_hdr->func_id = fattr.bare_func_id; // leave the bare function ID here before CRC check because attribute bits are actually changing during multi-stage requests but the rest of the data shouldn't change

    // check if there is an active server session for this request (which would indicate the request has been already ARMed)
    fp_disp_server_session_t *p_server_sess = (fp_disp_server_session_t *)fp_disp_session_mgr_find_session(p_disp_ctx->p_sys_ctx, FP_DISP_SESSION_TYPE_SERVER, p_fp_hdr);
    p_disp_ctx->p_assigned_sess             = (fp_disp_session_base_t *)p_server_sess;

    // this request has already been ARMed because there is an associated active server session
    if (NULL != p_server_sess)
    {
        uint16_t new_req_data_hash = fp_arch_calc_hash(p_disp_ctx->p_data, p_disp_ctx->data_size);
        p_fp_hdr->cmd_id           = original_cmd_id; // restore original command ID before emitting response to enable proper matching on client side

        // check if the original request data matches the new request data before actually processing the frame,
        // otherwise just continue tracking the DISARM timeout
        if (new_req_data_hash != p_server_sess->req_data_hash)
        {
            FP_ARCH_TRACE_WARN("ARM-and-FIRE validation failed: data_hash_mismatch, session_id=%p, request='%u:%lu:%u'",
                               p_server_sess, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

            (void)fp_disp_send_status_resp(p_disp_ctx, FP_CMD_STATUS_DATA_INTEGRITY_ERR);

            return;
        }

        switch (fattr.frame_type)
        {
            case FP_FRAME_TYPE_REQ_DISARM:
                {
                    FP_ARCH_TRACE_DEBUG("ARM-and-FIRE disarm: session_id=%p, request='%u:%lu:%u', action=user_disarm",
                                        p_server_sess, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

                    // inform client that the request was accepted as ARMed
                    (void)fp_disp_send_status_resp(p_disp_ctx, FP_CMD_STATUS_DISARMED);

                    //...and close the session
                    fp_disp_session_mgr_close_session(p_disp_ctx->p_sys_ctx, (fp_disp_session_base_t *)p_server_sess);

                    break;
                }

            case FP_FRAME_TYPE_REQ_FIRE:
                {
                    FP_ARCH_TRACE_DEBUG("ARM-and-FIRE execute: session_id=%p, request='%u:%lu:%u', action=fire",
                                        p_server_sess, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

                    // call user-registered handler which will be responsible to send the client response...
                    (*p_req_handler)(p_disp_ctx, p_disp_ctx->p_data);

                    //...and close the session
                    fp_disp_session_mgr_close_session(p_disp_ctx->p_sys_ctx, (fp_disp_session_base_t *)p_server_sess);

                    break;
                }

            default:
                {
                    FP_ARCH_TRACE_WARN("Protocol validation failed: unexpected_frame_type=%u, session_id=%p, request='%u:%lu:%u'",
                                       fattr.frame_type, p_server_sess, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

                    (void)fp_disp_send_status_resp(p_disp_ctx, FP_CMD_STATUS_REQ_ATTR_ERR);

                    break;
                }
        }
    }
    else
    {
        // there is no active server session on this request...
        if (FP_FRAME_TYPE_REQ_ARM == fattr.frame_type)
        {
            // this is the first time we get this request and it is an ARM request, hence open a tracking session
            p_server_sess = (fp_disp_server_session_t *)fp_disp_session_mgr_open_session(p_disp_ctx->p_sys_ctx, p_disp_ctx->p_comm_params, FP_DISP_SESSION_TYPE_SERVER);
            // assign the opened session to the dispatch context
            p_disp_ctx->p_assigned_sess = (fp_disp_session_base_t *)p_server_sess;

            if (NULL == p_server_sess)
            {
                FP_ARCH_TRACE_WARN("Session allocation failed: no free server session contexts for ARM request='%u:%lu'",
                                   p_fp_hdr->iface_id, p_fp_hdr->func_id);

                // inform client that the request cannot be served due to lack of free contexts
                (void)fp_disp_send_status_resp(p_disp_ctx, FP_CMD_STATUS_NO_MEM);

                return;
            }

            // copy communication parameters from dispatch context to session
            p_server_sess->base.p_comm_params = p_disp_ctx->p_comm_params;

            // hash the request data because we will need this when we receive the FIRE request again to check
            // if the request data changed (for FIRE to execute, the data must be exactly the same)
            // the hash avoids the need to keep a copy of the whole message in the server session
            p_server_sess->req_data_hash = fp_arch_calc_hash(p_disp_ctx->p_data, p_disp_ctx->data_size);
            p_fp_hdr->cmd_id             = original_cmd_id; // restore original command ID before emitting response to enable proper matching on client side

            // inform client that the request was accepted as ARMed
            (void)fp_disp_send_status_resp(p_disp_ctx, FP_CMD_STATUS_ARMED);

            FP_ARCH_TRACE_DEBUG("ARM-and-FIRE armed: session_id=%p, request='%u:%lu:%u', action=arm_success",
                                p_server_sess, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

            // session was successfully opened, start disarm timer with session lock protection
            void *p_section_lock = p_disp_ctx->p_sys_ctx->p_section_locks[FP_LOCK_SECTION_SERVER_SESSION];
            if (fp_arch_lock_section(p_section_lock))
            {
                fp_arch_start_timer(&p_server_sess->base.timer_hnd);
                atomic_store(&p_server_sess->base.state, FP_SESSION_STATE_ACTIVE);

                // preserve the fields of the request ID which will be used for later matching
                p_server_sess->base.req_fp_hdr.iface_id = p_fp_hdr->iface_id;
                p_server_sess->base.req_fp_hdr.func_id  = fattr.bare_func_id; // use bare function ID here for future matching
                p_server_sess->base.req_fp_hdr.cmd_id   = 0;
                p_server_sess->base.req_fp_hdr.status   = 0;

                fp_arch_unlock_section(p_section_lock);
            }
            else
            {
                FP_ARCH_TRACE_ERROR("Lock acquire failed: type=server_session, handle=%p, context=timer_start, request='%u:%lu'",
                                    p_section_lock, p_fp_hdr->iface_id, p_fp_hdr->func_id);

                // If we can't acquire the lock, close the session to prevent inconsistent state
                fp_disp_session_mgr_close_session(p_disp_ctx->p_sys_ctx, (fp_disp_session_base_t *)p_server_sess);
                (void)fp_disp_send_status_resp(p_disp_ctx, FP_CMD_STATUS_NO_MEM);
                return;
            }
        }
        else
        {
            FP_ARCH_TRACE_WARN("Protocol validation failed: unexpected_frame_type=%u for ARM request='%u:%lu:%u'",
                               fattr.frame_type, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

            (void)fp_disp_send_status_resp(p_disp_ctx, FP_CMD_STATUS_REQ_ATTR_ERR);
        }
    }
}

static inline void fp_disp_server_session_init(fp_disp_session_base_t *const p_sess)
{
    fp_arch_assert(NULL != p_sess);

    fp_disp_server_session_t *p_server_session = (fp_disp_server_session_t *)p_sess;

    p_server_session->p_user_handler = NULL;
    p_server_session->req_data_hash  = 0;
}

static inline bool fp_disp_server_session_is_matching(const fp_disp_session_base_t *const p_sess,
                                                      const fp_msg_hdr_t *const           p_new_hdr)
{
    fp_arch_assert(NULL != p_sess);
    fp_arch_assert(NULL != p_new_hdr);

    return ((p_sess->req_fp_hdr.iface_id == p_new_hdr->iface_id) &&
            (true == fp_proto_is_bare_func_id_matching(p_sess->req_fp_hdr.func_id, p_new_hdr->func_id)));
}

static void fp_disp_server_session_track(fp_dispatch_sys_context_t *const p_sys_ctx,
                                         fp_disp_session_base_t *const    p_sess)
{
    fp_arch_assert(NULL != p_sys_ctx);
    fp_arch_assert(NULL != p_sess);

    if (true == fp_arch_is_timer_elapsed(&((fp_disp_server_session_t *)p_sess)->base.timer_hnd, ARMED_REQ_TOUT_S * 1000))
    {
        FP_ARCH_TRACE_DEBUG("Session timeout: session_id=%p, request='%u:%lu:%u', timeout=%ds, action=auto_disarm",
                            p_sess, p_sess->req_fp_hdr.iface_id, p_sess->req_fp_hdr.func_id, p_sess->req_fp_hdr.cmd_id, ARMED_REQ_TOUT_S);

        // Increment the timed out session counter
        if (p_sys_ctx->server_metrics.timed_out_session_cnt < UINT32_MAX)
        {
            p_sys_ctx->server_metrics.timed_out_session_cnt++;
        }

        // just release the session
        fp_disp_session_mgr_close_session(p_sys_ctx, p_sess);
    }
}
