/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @addtogroup fp
 * @{
 *
 * @file fp_disp_client.c
 * @brief Implementation of the Function Protocol Dispatcher client-specific routines.
 *
 * @}
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "fp_disp.h"
#include "fp_disp_client.h"
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

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DECLARATION
***************************************************************************************************
*/
/** @brief Initialize a client session context to default values
 *
 *  @param[in] p_sess pointer to the session context to initialize
 */
static inline void fp_disp_client_session_init(fp_disp_session_base_t *const p_sess);

/** @brief Checks if an FP header stored in a session matches the newly received one
 *
 *  @param[in] p_sess pointer to the client session data
 *  @param[in] p_new_hdr pointer to the received response FP header to check against
 *
 *  @retval true the session header matches the received header by interface and function ID as well as command ID
 *  @retval false the session header doesn't match the received header
 *
 *  @note The function is used to match an FP request command ID to the FP response command ID for client sessions.
 */
static inline bool fp_disp_client_session_is_matching(const fp_disp_session_base_t *const p_sess,
                                                      const fp_msg_hdr_t *const           p_new_hdr);

/** @brief Performs general client session tracking actions, e.g. handling response timeouts and potentially other
 *  necessary regular checks
 *
 *  @param[in] p_sys_ctx system-specific context containing the session pools and related locks
 *  @param[in] p_sess pointer to the server session data
 */
static void fp_disp_client_session_track(fp_dispatch_sys_context_t *const p_sys_ctx,
                                         fp_disp_session_base_t *const    p_sess);

/** @brief Disarms the release handler of an asynchronous request whose session is being closed
 *
 *  @param[in] p_sess pointer to the client session being closed
 *
 *  @return the release handler to call, or NULL; the session manager calls it after releasing the lock
 *
 *  @note Called by the session manager after the transport close callback, under the client-session lock.
 */
static fp_session_released_handler_t fp_disp_client_session_release(fp_disp_session_base_t *const p_sess);

/** @brief Tells the application that an asynchronous request timed out (response handler with
 *  FP_CMD_STATUS_TIMEOUT)
 *
 *  @param[in] p_sess pointer to the timed-out client session
 *
 *  @note Called by the session tracker without any fp lock held, before the session is closed.
 */
static void fp_disp_client_session_timeout(fp_disp_session_base_t *const p_sess);

/** @brief Marks an asynchronous request's session for a deferred close once its response handler has run
 *
 *  Stops the session timer and marks the session @ref FP_SESSION_STATE_DEFERRED with
 *  @ref FP_SESSION_DEFERRED_CLOSE, so that the next session tracking pass closes it and runs the transport
 *  close callback and the release handler without any fp lock held. The request then gets exactly one
 *  response handler call and no timeout.
 *
 *  Leaves the session alone when it is no longer @ref FP_SESSION_STATE_ACTIVE (a session deferred for
 *  delivery is closed by its p_deliver result, a timed-out one keeps its timeout), when it no longer
 *  holds the answered request (it timed out, was closed and was reused while the response handler ran),
 *  and when the response was dispatched from the transport's p_track callback inside the locked tracker
 *  phase (the tracker owns the session for that pass and closes it through the p_track result).
 *
 *  @param[in] p_sys_ctx  pointer to the system context
 *  @param[in] p_sess     pointer to the client session that took the response
 *  @param[in] p_req_hdr  the session's request header, as it was before the response handler ran
 */
static void fp_disp_client_defer_close(fp_dispatch_sys_context_t *const p_sys_ctx,
                                       fp_disp_session_base_t *const    p_sess,
                                       const fp_msg_hdr_t *const        p_req_hdr);

/*
***************************************************************************************************
* EXTERNAL VARIABLES DEFINITION
***************************************************************************************************
*/
const fp_disp_session_attributes_t fp_disp_client_sess_attrib = {
    // clang-format off
    .p_init_func = &fp_disp_client_session_init,
    .p_is_matching = &fp_disp_client_session_is_matching,
    .p_track_session = &fp_disp_client_session_track,
    .p_release_func = &fp_disp_client_session_release,
    .p_timeout_func = &fp_disp_client_session_timeout,
    .sess_type = FP_DISP_SESSION_TYPE_CLIENT
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

static fp_cmd_status_t fp_disp_client_send_req_impl(fp_dispatch_info_t           *p_disp_ctx,
                                                    const uint16_t                iface_id,
                                                    const uint32_t                func_id,
                                                    const fp_frame_type_t         frame_type,
                                                    uint8_t *const                p_req,
                                                    const uint32_t                req_size,
                                                    uint8_t *const                p_resp,
                                                    fp_generic_response_handler_t p_resp_handler,
                                                    const uint32_t                timeout,
                                                    const uint32_t                resp_buf_user_max)
{
    if ((NULL == p_disp_ctx) || (NULL == p_req) || (req_size < sizeof(fp_msg_hdr_t)) || (FP_FRAME_TYPE_BROADCAST == frame_type))
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_disp_ctx=%p, p_req=%p, req_size=%u (min=%u)",
                            p_disp_ctx, p_req, req_size, (uint32_t)sizeof(fp_msg_hdr_t));

        return FP_CMD_STATUS_INVARGS;
    }

    FP_ARCH_TRACE_DEBUG("Client request entry: iface_id=%u, func_id=%lu, frame_type=%u",
                        iface_id, func_id, frame_type);

    // initialize this pointer is initialized correctly in case the dispatch fails
    p_disp_ctx->p_ext_buf_ref = NULL;

    fp_disp_client_session_t *p_client_session = (fp_disp_client_session_t *)fp_disp_session_mgr_open_session(p_disp_ctx->p_sys_ctx, p_disp_ctx->p_comm_params, FP_DISP_SESSION_TYPE_CLIENT);
    // assign the opened session to the dispatch context
    p_disp_ctx->p_assigned_sess = (fp_disp_session_base_t *)p_client_session;

    // check if the maximum number of client sessions has been reached
    if (NULL == p_client_session)
    {
        FP_ARCH_TRACE_ERROR("Session allocation failed: no free client session contexts available for request='%u:%lu'",
                            iface_id, func_id);

        return FP_CMD_STATUS_NO_MEM;
    }

    // copy communication parameters from dispatch context to session
    p_client_session->base.p_comm_params = p_disp_ctx->p_comm_params;

    p_client_session->req_type = (NULL == p_resp_handler) ? FP_REQ_TYPE_SYNC : FP_REQ_TYPE_ASYNC;

    p_client_session->p_user_handler = p_resp_handler;

    // this is required only for synchronous requests to keep the pointer to the response buffer for
    // the response thread to be able to make a copy of the response before signaling the caller thread to
    // unblock
    p_client_session->p_resp_data = p_resp;

    // per-request response buffer capacity (0 == use the pool-wide resp_data_max_size)
    p_client_session->resp_buf_user_max = resp_buf_user_max;

    // reset per-request delivered size; set by handle_resp on a successful copy
    p_client_session->resp_data_size = 0U;

    // store timeout which will be tracked in a separate thread for asynchronous requests
    p_client_session->base.timeout = timeout;

    // initialize the request fields
    fp_msg_hdr_t *p_fp_hdr = (fp_msg_hdr_t *)p_req;
    p_fp_hdr->iface_id     = iface_id;
    p_fp_hdr->cmd_id       = fp_internal_gen_cmd_id();
    p_fp_hdr->status       = FP_CMD_STATUS_NOERROR;
    fp_proto_set_function_type(p_fp_hdr, func_id, frame_type);

    // point the dispatch context to the initialized request buffer
    p_disp_ctx->p_data    = p_req;
    p_disp_ctx->data_size = req_size;

    // store the request header for later identification
    p_client_session->base.req_fp_hdr = *(fp_msg_hdr_t *)p_req;

    // ensure the event object is properly associated with the current thread of
    // execution...
    fp_arch_init_event(&p_client_session->p_session_event);

    bool send_ok = fp_disp_send(p_disp_ctx);

    FP_ARCH_TRACE_DEBUG("Client request sent: iface_id=%u, func_id=%lu, cmd_id=%u, session_id=%p, timer_handle=%p",
                        iface_id, func_id, p_fp_hdr->cmd_id, p_client_session, &p_client_session->base.timer_hnd);

    if (true == send_ok)
    {
        if (FP_REQ_TYPE_SYNC == p_client_session->req_type)
        {
            // synchronous requests must block here until the reply arrives
            bool wait_res = fp_arch_wait_for_event(p_client_session->p_session_event, timeout);

            FP_ARCH_TRACE_DEBUG("Session close (sync): iface_id=%u, func_id=%lu, cmd_id=%u, session_id=%p",
                                p_client_session->base.req_fp_hdr.iface_id,
                                p_client_session->base.req_fp_hdr.func_id,
                                p_fp_hdr->cmd_id,
                                p_client_session);

            fp_cmd_status_t sync_status = (true == wait_res)
                                              ? (fp_cmd_status_t)p_client_session->base.req_fp_hdr.status
                                              : FP_CMD_STATUS_TIMEOUT;

            // surface the delivered response length to the caller (0 on error/timeout)
            p_disp_ctx->data_size = p_client_session->resp_data_size;

            fp_disp_session_mgr_close_session(p_disp_ctx->p_sys_ctx, (fp_disp_session_base_t *)p_client_session);

            return sync_status;
        }
        else
        {
            // start tracking the request timeout with session lock protection
            void *p_section_lock = p_disp_ctx->p_sys_ctx->p_section_locks[FP_LOCK_SECTION_CLIENT_SESSION];
            if (fp_arch_lock_section(p_section_lock))
            {
                // armed only for an accepted request: every error return leaves the
                // communication parameters with the caller
                p_client_session->p_released_handler = p_disp_ctx->p_on_released;

                fp_arch_start_timer(&p_client_session->base.timer_hnd);
                atomic_store(&p_client_session->base.state, FP_SESSION_STATE_ACTIVE);
                fp_arch_unlock_section(p_section_lock);

                // asynchronous request was sent -> start waiting for the reply which will come in the communications thread
                return FP_CMD_STATUS_NOERROR;
            }
            else
            {
                FP_ARCH_TRACE_ERROR("Lock acquire failed: type=client_session, handle=%p, context=timer_start, request='%u:%lu'",
                                    p_section_lock, iface_id, func_id);

                // If we can't acquire the lock, close the session to prevent inconsistent state
                fp_disp_session_mgr_close_session(p_disp_ctx->p_sys_ctx, (fp_disp_session_base_t *)p_client_session);
                return FP_CMD_STATUS_NO_MEM;
            }
        }
    }
    else
    {
        // send failed: close the reserved session, otherwise its slot leaks (no async
        // tracking was started for it)
        fp_disp_session_mgr_close_session(p_disp_ctx->p_sys_ctx, (fp_disp_session_base_t *)p_client_session);

        return FP_CMD_STATUS_SENDERR;
    }
}

fp_cmd_status_t fp_disp_client_send_req(fp_dispatch_info_t           *p_disp_ctx,
                                        const uint16_t                iface_id,
                                        const uint32_t                func_id,
                                        const fp_frame_type_t         frame_type,
                                        uint8_t *const                p_req,
                                        const uint32_t                req_size,
                                        uint8_t *const                p_resp,
                                        fp_generic_response_handler_t p_resp_handler,
                                        const uint32_t                timeout)
{
    // 0 == no caller bound; copy is bounded by the pool-wide resp_data_max_size
    return fp_disp_client_send_req_impl(p_disp_ctx, iface_id, func_id, frame_type, p_req, req_size,
                                        p_resp, p_resp_handler, timeout, 0U);
}

fp_cmd_status_t fp_disp_client_send_req_bounded(fp_dispatch_info_t           *p_disp_ctx,
                                                const uint16_t                iface_id,
                                                const uint32_t                func_id,
                                                const fp_frame_type_t         frame_type,
                                                uint8_t *const                p_req,
                                                const uint32_t                req_size,
                                                uint8_t *const                p_resp,
                                                const uint32_t                resp_max_size,
                                                fp_generic_response_handler_t p_resp_handler,
                                                const uint32_t                timeout)
{
    return fp_disp_client_send_req_impl(p_disp_ctx, iface_id, func_id, frame_type, p_req, req_size,
                                        p_resp, p_resp_handler, timeout, resp_max_size);
}

/** @brief Work out the status a client gets for a received response frame
 *
 *  @param[in] p_disp_ctx     dispatch context holding the received frame (p_data != NULL)
 *  @param[in] resp_type_size FIDL-declared size of the response, header included
 *
 *  @returns UNEXP_FRM_TYPE if the frame is not a response; UNEXP_FRM_SIZE if a NOERROR reply does not
 *           have the declared size; otherwise the status the server sent
 */
static fp_cmd_status_t fp_disp_client_resp_status(const fp_dispatch_info_t *p_disp_ctx,
                                                  const uint32_t            resp_type_size)
{
    const fp_msg_hdr_t  *p_fp_hdr = (const fp_msg_hdr_t *)p_disp_ctx->p_data;
    fp_proto_func_attr_t fattr;

    fp_proto_get_func_attributes(p_fp_hdr, &fattr);

    if (FP_FRAME_TYPE_RESP != fattr.frame_type)
    {
        FP_ARCH_TRACE_WARN("Protocol validation failed: expected=RESP_FRAME, received=%u, frame='%u:%lu:%u'",
                           fattr.frame_type, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

        return FP_CMD_STATUS_UNEXP_FRM_TYPE;
    }

    // Only a NOERROR reply carries the typed result, so only its length is checked. A server sends
    // every other status as the header alone (the function did not run), and the client must get
    // that status as sent, not a size violation.
    if ((FP_CMD_STATUS_NOERROR == p_fp_hdr->status) && (p_disp_ctx->data_size != resp_type_size))
    {
        FP_ARCH_TRACE_WARN("Protocol validation failed: expected_size=%u, received_size=%u, frame='%u:%lu:%u'",
                           resp_type_size, p_disp_ctx->data_size, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

        return FP_CMD_STATUS_UNEXP_FRM_SIZE;
    }

    return (fp_cmd_status_t)p_fp_hdr->status;
}

static bool fp_disp_client_handle_resp_impl(fp_dispatch_info_t *p_disp_ctx,
                                            const uint32_t      resp_type_size,
                                            const bool          size_validated)
{
    if ((NULL == p_disp_ctx) || (0 == resp_type_size) || (NULL == p_disp_ctx->p_data))
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_disp_ctx=%p, resp_type_size=%u, p_data=%p",
                            p_disp_ctx, resp_type_size, (p_disp_ctx ? p_disp_ctx->p_data : NULL));

        return false;
    }

    const fp_msg_hdr_t *p_fp_hdr        = (const fp_msg_hdr_t *)p_disp_ctx->p_data;
    fp_cmd_status_t     user_cbk_status = fp_disp_client_resp_status(p_disp_ctx, resp_type_size);

    fp_disp_client_session_t *p_client_session = (fp_disp_client_session_t *)fp_disp_session_mgr_find_session(p_disp_ctx->p_sys_ctx,
                                                                                                              FP_DISP_SESSION_TYPE_CLIENT,
                                                                                                              p_fp_hdr);
    p_disp_ctx->p_assigned_sess                = (fp_disp_session_base_t *)p_client_session;

    if (NULL == p_client_session)
    {
        FP_ARCH_TRACE_WARN("Session lookup failed: no active client session for response frame='%u:%lu:%u'",
                           p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

        return false;
    }

    if (p_client_session->req_type >= FP_REQ_TYPE_MAX)
    {
        FP_ARCH_TRACE_WARN("Protocol validation failed: invalid_req_type=%u for response frame='%u:%lu:%u'",
                           p_client_session->req_type, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

        // not delivered: fp_disp_execute must not report the response as dispatched
        p_disp_ctx->p_assigned_sess = NULL;

        return false;
    }

    FP_ARCH_TRACE_DEBUG("Client response entry: iface_id=%u, func_id=%lu, cmd_id=%u, session_id=%p",
                        p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id, p_client_session);

    if (FP_REQ_TYPE_ASYNC == p_client_session->req_type)
    {
        // a size-unvalidated frame must not reach a typed async handler (it could deserialize
        // from too few bytes); sync sessions are safe because their copy is length-bounded
        if (false == size_validated)
        {
            FP_ARCH_TRACE_WARN("Unvalidated RESP frame not delivered to async session: frame='%u:%lu:%u'",
                               p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

            // not delivered: fp_disp_execute must not report the response as dispatched
            p_disp_ctx->p_assigned_sess = NULL;

            return false;
        }

        fp_arch_assert(NULL != p_client_session->p_user_handler);

        // the session may be timed out, closed and reused while the handler runs without the lock:
        // keep the answered request's header to recognise it afterwards
        const fp_msg_hdr_t answered_req_hdr = p_client_session->base.req_fp_hdr;

        // invoke user-registered callback; the data is handed over only with NOERROR (a server
        // status other than NOERROR means no result, and a protocol violation means no valid one)
        (*p_client_session->p_user_handler)(
            p_disp_ctx,
            (FP_CMD_STATUS_NOERROR == user_cbk_status) ? p_disp_ctx->p_data : NULL,
            user_cbk_status);

        fp_disp_client_defer_close(p_disp_ctx->p_sys_ctx, &p_client_session->base, &answered_req_hdr);
    }
    else
    {
        // this is a reply to a synchronous request, hence we need to:
        // 1. perform a copy of the response data to the user response buffer to ensure data integrity when
        //    the user is unblocked and starts to process the data (simply because the response data pointer is only valid
        //    in the thread context where the response is received which is a different thread than the user one)
        // 2. signal the user thread to unblock and process the response

        fp_arch_assert(NULL != p_client_session->p_resp_data);
        uint8_t *const p_resp_data = p_client_session->p_resp_data;

        // bound writes to the tighter of the pool capacity and the per-request caller
        // buffer (resp_buf_user_max, 0 == not specified)
        uint32_t resp_bound = p_client_session->resp_data_max_size;
        if ((0U != p_client_session->resp_buf_user_max) &&
            (p_client_session->resp_buf_user_max < resp_bound))
        {
            resp_bound = p_client_session->resp_buf_user_max;
        }

        // The caller buffer holds one typed response. When the typed size is known, it is the
        // hard limit for every write into that buffer. The pool-wide capacity can be much larger
        // than the caller struct. A clear of the full capacity destroys the caller stack.
        if ((true == size_validated) && (resp_type_size < resp_bound))
        {
            resp_bound = resp_type_size;
        }

        if (NULL != p_resp_data)
        {
            if (FP_CMD_STATUS_NOERROR == user_cbk_status)
            {
                if (p_disp_ctx->data_size <= resp_bound)
                {
                    (void)memcpy(p_resp_data,
                                 p_disp_ctx->p_data,
                                 p_disp_ctx->data_size);

                    // record the delivered length for the caller
                    p_client_session->resp_data_size = p_disp_ctx->data_size;
                }
                else
                {
                    FP_ARCH_TRACE_WARN("Response too large for user buffer: received=%u, max=%u, frame='%u:%lu:%u'",
                                       p_disp_ctx->data_size, resp_bound,
                                       p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

                    (void)memset(p_resp_data, 0, resp_bound);
                    p_client_session->resp_data_size = 0U;
                    user_cbk_status = FP_CMD_STATUS_UNEXP_FRM_SIZE;
                }
            }
            else
            {
                (void)memset(p_resp_data, 0, resp_bound);
                p_client_session->resp_data_size = 0U;
            }
        }

        // propagate protocol violation status so send_req can return it to the caller
        p_client_session->base.req_fp_hdr.status = (uint8_t) user_cbk_status;

        fp_arch_assert(NULL != p_client_session->p_session_event);

        // unblock user thread to process the response
        fp_arch_signal_event(p_client_session->p_session_event);
    }

    // delivered to the matching client session
    return true;
}

bool fp_disp_client_handle_resp(fp_dispatch_info_t *p_disp_ctx,
                                const uint32_t      resp_type_size)
{
    // typed response size known, so the size check is meaningful and async delivery is safe
    return fp_disp_client_handle_resp_impl(p_disp_ctx, resp_type_size, true);
}

bool fp_disp_client_handle_resp_unmatched(fp_dispatch_info_t *p_disp_ctx)
{
    if (NULL == p_disp_ctx)
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_disp_ctx=NULL");

        return false;
    }

    // no typed response size: pass data_size as the expected size (size check becomes a no-op)
    // and flag the frame as size-unvalidated so it reaches sync sessions only
    return fp_disp_client_handle_resp_impl(p_disp_ctx, p_disp_ctx->data_size, false);
}

void fp_disp_client_handle_bcast(fp_dispatch_info_t            *p_disp_ctx,
                                 const uint32_t                 bcast_type_size,
                                 fp_generic_broadcast_handler_t p_bcast_handler)
{
    // Validate arguments
    if ((NULL == p_disp_ctx) || (0 == bcast_type_size) || (NULL == p_disp_ctx->p_data) || (NULL == p_bcast_handler))
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_disp_ctx=%p, bcast_type_size=%u, p_data=%p, p_bcast_handler=%p",
                            p_disp_ctx, bcast_type_size,
                            (p_disp_ctx ? p_disp_ctx->p_data : NULL),
                            p_bcast_handler);

        return;
    }

    const fp_msg_hdr_t  *p_fp_hdr = (const fp_msg_hdr_t *)p_disp_ctx->p_data;
    fp_proto_func_attr_t fattr;

    fp_proto_get_func_attributes(p_fp_hdr, &fattr);

    // Validate frame type is BROADCAST
    if (FP_FRAME_TYPE_BROADCAST != fattr.frame_type)
    {
        FP_ARCH_TRACE_WARN("Protocol validation failed: expected=BROADCAST_FRAME, received=%u, frame='%u:%lu:%u'",
                           fattr.frame_type, p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);
        return;
    }

    // Validate size matches expected broadcast type size
    if (p_disp_ctx->data_size != bcast_type_size)
    {
        FP_ARCH_TRACE_WARN("Protocol validation failed: expected_size=%u, received_size=%u, frame='%u:%lu:%u'",
                           bcast_type_size, p_disp_ctx->data_size,
                           p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);
        return;
    }

    FP_ARCH_TRACE_DEBUG("Client broadcast entry: iface_id=%u, func_id=%lu, cmd_id=%u",
                        p_fp_hdr->iface_id, p_fp_hdr->func_id, p_fp_hdr->cmd_id);

    // Invoke user-registered broadcast handler
    (*p_bcast_handler)(p_disp_ctx, p_disp_ctx->p_data);
}

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/
static inline void fp_disp_client_session_init(fp_disp_session_base_t *const p_sess)
{
    fp_arch_assert(NULL != p_sess);

    fp_disp_client_session_t *p_client_session = (fp_disp_client_session_t *)p_sess;

    p_client_session->p_resp_data        = NULL;
    p_client_session->p_user_handler     = NULL;
    p_client_session->p_released_handler = NULL;
    p_client_session->req_type           = FP_REQ_TYPE_MAX;
    // unlike resp_data_max_size (the once-set pool capacity), these are per-request and must be reset
    p_client_session->resp_buf_user_max = 0U;
    p_client_session->resp_data_size    = 0U;
    // p_client_session->p_session_event is used only externally by the project integration code
    // and will not be touched here to enable the option of initializing just once
    // p_client_session->resp_data_max_size must be initialized externally so we don't reset it here
    // as it is set only once
}

static inline bool fp_disp_client_session_is_matching(const fp_disp_session_base_t *const p_sess,
                                                      const fp_msg_hdr_t *const           p_new_hdr)
{
    fp_arch_assert(NULL != p_sess);
    fp_arch_assert(NULL != p_new_hdr);

    return ((p_sess->req_fp_hdr.iface_id == p_new_hdr->iface_id) &&
            (true == fp_proto_is_bare_func_id_matching(p_sess->req_fp_hdr.func_id, p_new_hdr->func_id)) &&
            (p_sess->req_fp_hdr.cmd_id == p_new_hdr->cmd_id));
}

static void fp_disp_client_session_track(fp_dispatch_sys_context_t *const p_sys_ctx,
                                         fp_disp_session_base_t *const    p_sess)
{
    fp_arch_assert(NULL != p_sys_ctx);
    fp_arch_assert(NULL != p_sess);

    fp_disp_client_session_t const *p_client_sess = (fp_disp_client_session_t const *)p_sess;

    if ((FP_REQ_TYPE_ASYNC == p_client_sess->req_type) &&
        (true == fp_arch_is_timer_elapsed(&p_sess->timer_hnd, p_sess->timeout)))
    {
        FP_ARCH_TRACE_DEBUG("Session timeout: session_id=%p, request='%u:%lu:%u', timeout=%ums",
                            p_sess, p_sess->req_fp_hdr.iface_id, p_sess->req_fp_hdr.func_id, p_sess->req_fp_hdr.cmd_id, p_sess->timeout);

        fp_arch_stop_timer(&p_sess->timer_hnd);

        // Increment the timed out session counter
        if (p_sys_ctx->client_metrics.timed_out_session_cnt < UINT32_MAX)
        {
            p_sys_ctx->client_metrics.timed_out_session_cnt++;
        }

        // called under the client-session lock: only mark the session. The session tracker calls the
        // response handler (fp_disp_client_session_timeout) and closes the session after releasing the lock.
        p_sess->deferred_action = (uint8_t)FP_SESSION_DEFERRED_TIMEOUT;
        atomic_store(&p_sess->state, FP_SESSION_STATE_DEFERRED);
    }
}

static void fp_disp_client_session_timeout(fp_disp_session_base_t *const p_sess)
{
    fp_arch_assert(NULL != p_sess);

    fp_disp_client_session_t const *p_client_sess  = (fp_disp_client_session_t const *)p_sess;
    fp_generic_response_handler_t   p_user_handler = p_client_sess->p_user_handler;

    // if we have a response handler assigned, inform user that the response timed-out
    if (NULL != p_user_handler)
    {
        fp_dispatch_info_t temp_ctx = { 0 };

        // transfer the comm params to a temporary context so that it can be accessed by the
        // response handler
        temp_ctx.p_comm_params = p_sess->p_comm_params;

        (*p_user_handler)(&temp_ctx, NULL, FP_CMD_STATUS_TIMEOUT);
    }
}

static fp_session_released_handler_t fp_disp_client_session_release(fp_disp_session_base_t *const p_sess)
{
    fp_arch_assert(NULL != p_sess);

    fp_disp_client_session_t     *p_client_session = (fp_disp_client_session_t *)p_sess;
    fp_session_released_handler_t p_handler        = p_client_session->p_released_handler;

    // disarm so that the handler runs exactly once; the session manager calls it after the unlock
    p_client_session->p_released_handler = NULL;

    return p_handler;
}

static void fp_disp_client_defer_close(fp_dispatch_sys_context_t *const p_sys_ctx,
                                       fp_disp_session_base_t *const    p_sess,
                                       const fp_msg_hdr_t *const        p_req_hdr)
{
    fp_arch_assert((NULL != p_sys_ctx) && (NULL != p_sess) && (NULL != p_req_hdr));

    // inside the locked tracker phase (dispatch from p_track): the tracker owns the session for this
    // pass and closes it through the p_track result; a mark here could be overwritten by its timeout check
    if (true == p_sys_ctx->client_track_locked)
    {
        return;
    }

    void *p_section_lock = p_sys_ctx->p_section_locks[FP_LOCK_SECTION_CLIENT_SESSION];

    if (!fp_arch_lock_section(p_section_lock))
    {
        // the session stays active and times out as before
        FP_ARCH_TRACE_ERROR("Lock acquire failed: type=client_session, handle=%p, context=resp_defer_close, session_id=%p",
                            p_section_lock, p_sess);

        return;
    }

    // a session already deferred (for delivery, or for its timeout while the response handler ran)
    // is finished by the session tracker as marked; a session reused for another request is not ours
    if ((FP_SESSION_STATE_ACTIVE == atomic_load(&p_sess->state)) &&
        (p_sess->req_fp_hdr.iface_id == p_req_hdr->iface_id) &&
        (p_sess->req_fp_hdr.func_id == p_req_hdr->func_id) &&
        (p_sess->req_fp_hdr.cmd_id == p_req_hdr->cmd_id))
    {
        fp_arch_stop_timer(&p_sess->timer_hnd);

        // the action is set before the state: the tracker reads the action once it sees DEFERRED
        p_sess->deferred_action = (uint8_t)FP_SESSION_DEFERRED_CLOSE;
        atomic_store(&p_sess->state, FP_SESSION_STATE_DEFERRED);

        FP_ARCH_TRACE_DEBUG("Session close deferred after response: session_id=%p, request='%u:%lu:%u'",
                            p_sess, p_sess->req_fp_hdr.iface_id, p_sess->req_fp_hdr.func_id, p_sess->req_fp_hdr.cmd_id);
    }

    fp_arch_unlock_section(p_section_lock);
}
