/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @addtogroup fp
 * @{
 *
 * @file fp_disp.c
 * @brief Implementation of the Function Protocol Dispatcher module.
 *  - responsible for parsing FP frames and routing them to the corresponding registered command handler (if any)
 *  - enables direct server responses with an error code when an unregistered command is attempted for execution
 *  - performs general tracing of the FP communication flow in the system
 *  - performs synchronous waiting for a response for a given timeout
 *
 * @}
 *
 */

#include <stdio.h>
#include "fp_disp.h"
#include "fp_disp_client.h"
#include "fp_registry.h"
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
* EXTERNAL VARIABLES DEFINITION
***************************************************************************************************
*/

/*
***************************************************************************************************
* INTERNAL (STATIC) VARIABLES DEFINITION/DECLARATION
***************************************************************************************************
*/

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DECLARATION
***************************************************************************************************
*/
/** @brief Handle a frame that matched no registered FIDL function.
 *
 *  @retval true  an unregistered RESPONSE was delivered to a waiting client session (matched by cmd_id)
 *  @retval false an unsupported request/broadcast, or a response that matched no session
 */
static bool fp_disp_route_unmatched(fp_dispatch_info_t         *p_disp_ctx,
                                    const fp_proto_func_attr_t *p_fp_attr);

/** @brief Set the status a registered callback sees for a frame.
 *
 *  A response carries the server's result in its status, so the client handler must see it as
 *  received. A request carries no result, and a server broadcast has no request that could fail
 *  (the sender always sets NOERROR), so both get a defined NOERROR, whatever the frame carried.
 */
static void fp_disp_prepare_status(fp_msg_hdr_t               *p_fp_hdr,
                                   const fp_proto_func_attr_t *p_fp_attr);

/*
***************************************************************************************************
* EXTERNAL (NON STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/
bool fp_disp_execute(fp_dispatch_info_t *p_disp_ctx)
{
    bool fidl_func_found = false;
    bool fidl_func_called = false;

    fp_arch_break_assert(NULL != p_disp_ctx);

    if (NULL != p_disp_ctx)
    {
        fp_msg_hdr_t        *p_fp_hdr = (fp_msg_hdr_t *)p_disp_ctx->p_data;
        fp_proto_func_attr_t fp_attr;

        if (NULL == p_fp_hdr)
        {
            fp_arch_break_assert(NULL != p_fp_hdr);

            return false;
        }

        // initialize this pointer is initialized correctly in case the dispatch fails
        p_disp_ctx->p_ext_buf_ref = NULL;

        // the client response handler sets this only when a client session takes the response
        p_disp_ctx->p_assigned_sess = NULL;

        FP_ARCH_TRACE_DEBUG("Dispatch execute entry: iface_id=%u, func_id=%lu",
                            p_fp_hdr->iface_id, p_fp_hdr->func_id);

        fp_proto_get_func_attributes(p_fp_hdr, &fp_attr);

        const fidl_if_t *p_fidl = fp_registry_get_fidl_if_by_id(p_fp_hdr->iface_id,
                                                                ((FP_FRAME_TYPE_RESP == fp_attr.frame_type) || (FP_FRAME_TYPE_BROADCAST == fp_attr.frame_type))
                                                                    ? FIDL_TYPE_CLIENT
                                                                    : FIDL_TYPE_SERVER);

        if (NULL != p_fidl)
        {
            const fidl_func_t *p_func = fp_registry_get_func_by_id(p_fidl, fp_attr.bare_func_id);

            if ((NULL != p_func) && (NULL != p_func->p_on_func_cbk))
            {
                fp_disp_prepare_status(p_fp_hdr, &fp_attr);

                (*p_func->p_on_func_cbk)(p_disp_ctx);
                fidl_func_called = true;

                // a response counts as dispatched only if a client session took it; otherwise
                // (e.g. a stray frame with an unknown cmd_id) the transport keeps waiting
                fidl_func_found = (FP_FRAME_TYPE_RESP != fp_attr.frame_type) ||
                                  (NULL != p_disp_ctx->p_assigned_sess);
            }
        }

        // a frame whose registered callback ran is not routed again: a second session lookup could
        // hand a response the callback already took to the same session once more
        if (!fidl_func_called)
        {
            fidl_func_found = fp_disp_route_unmatched(p_disp_ctx, &fp_attr);
        }
    }

    return fidl_func_found;
}

bool fp_disp_send(fp_dispatch_info_t *p_disp_ctx)
{
    fp_arch_break_assert((NULL != p_disp_ctx) &&
                         (NULL != p_disp_ctx->p_sys_ctx) &&
                         (NULL != p_disp_ctx->p_sys_ctx->p_send) &&
                         (NULL != p_disp_ctx->p_data));

    if ((NULL != p_disp_ctx) &&
        (NULL != p_disp_ctx->p_sys_ctx) &&
        (NULL != p_disp_ctx->p_sys_ctx->p_send) &&
        (NULL != p_disp_ctx->p_data))
    {
        return (*p_disp_ctx->p_sys_ctx->p_send)(p_disp_ctx);
    }

    return false;
}

bool fp_disp_send_status_resp(fp_dispatch_info_t *p_disp_ctx, const fp_cmd_status_t status)
{
    fp_arch_break_assert((NULL != p_disp_ctx) &&
                         (NULL != p_disp_ctx->p_data) &&
                         (FP_CMD_STATUS_MAX > status));

    if ((NULL != p_disp_ctx) &&
        (NULL != p_disp_ctx->p_data) &&
        (FP_CMD_STATUS_MAX > status))
    {
        fp_msg_hdr_t *p_fp_hdr = (fp_msg_hdr_t *)p_disp_ctx->p_data;

        // Extract the bare function ID from the existing header
        fp_proto_func_attr_t fp_attr;
        fp_proto_get_func_attributes(p_fp_hdr, &fp_attr);

        // Set the status in the header
        p_fp_hdr->status = (uint8_t)status;

        // Set the data size to just the header size (no additional data)
        p_disp_ctx->data_size = sizeof(fp_msg_hdr_t);

        // Set function ID and response frame type
        fp_proto_set_function_type(p_fp_hdr, fp_attr.bare_func_id, FP_FRAME_TYPE_RESP);

        // Send the response
        return fp_disp_send(p_disp_ctx);
    }

    return false;
}

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/
static bool fp_disp_route_unmatched(fp_dispatch_info_t         *p_disp_ctx,
                                    const fp_proto_func_attr_t *p_fp_attr)
{
    // Self-addressed response: no client serdes exists to route it, so deliver it to the
    // waiting client session by cmd_id. Size-unvalidated, so sync sessions only.
    if (FP_FRAME_TYPE_RESP == p_fp_attr->frame_type)
    {
        return fp_disp_client_handle_resp_unmatched(p_disp_ctx);
    }

    // Unsupported request: reply with an error (broadcasts are one-way, so only traced).
    if (FP_FRAME_TYPE_BROADCAST != p_fp_attr->frame_type)
    {
        (void)fp_disp_send_status_resp(p_disp_ctx, FP_CMD_STATUS_FUNC_NOT_SUPPORTED);
    }

    FP_ARCH_TRACE_WARN("Unsupported function requested: iface_id=%u, func_id=%lu, frame_type=%u",
                       ((const fp_msg_hdr_t *)p_disp_ctx->p_data)->iface_id,
                       ((const fp_msg_hdr_t *)p_disp_ctx->p_data)->func_id,
                       p_fp_attr->frame_type);
    return false;
}

static void fp_disp_prepare_status(fp_msg_hdr_t               *p_fp_hdr,
                                   const fp_proto_func_attr_t *p_fp_attr)
{
    if (FP_FRAME_TYPE_RESP != p_fp_attr->frame_type)
    {
        p_fp_hdr->status = FP_CMD_STATUS_NOERROR;
    }
}
