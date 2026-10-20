/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @addtogroup fp
 * @{
 *
 * @file fp_disp_session_mgr.c
 * @brief Implementation of the Function Protocol Dispatcher generic session pool management.
 *
 * @}
 *
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdatomic.h>
#include "fp_defs.h"
#include "fp_disp.h"
#include "fp_disp_client.h"
#include "fp_disp_server.h"
#include "fp_internal.h"
#include "fp_disp_session_mgr.h"

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

/** @brief Session-specific information extracted from the system context at runtime */
typedef struct
{
    fp_disp_session_base_t    *p_session_pool;  /**< pointer to the session-specific pool storage */
    fp_disp_session_metrics_t *p_metrics;       /**< pointer to the session-speicifc metrics structure */
    fp_lock_section_t          section_lock_id; /**< ID of the section lock to use when accessing the session-specific data */
    uint16_t                   max_session_cnt; /**< number of allocated sessions of a given type */
} fp_disp_session_data_t;

/*
***************************************************************************************************
* EXTERNAL VARIABLES DEFINITION
***************************************************************************************************
*/

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DECLARATION
***************************************************************************************************
*/

/** @brief Provide session-type-specific data from the provided system context
 *
 *  @param[in] p_sys_ctx system-specific context containing the session pools and related locks
 *  @param[in] sess_type session type
 *  @param[in] p_sess_data a pointer to the user buffer where to return the session data
 *
 */
static inline void fp_disp_session_mgr_get_session_data(fp_dispatch_sys_context_t *const p_sys_ctx,
                                                        const fp_disp_session_type_t     sess_type,
                                                        fp_disp_session_data_t *const    p_sess_data);

/** @brief Provide a correctly aligned base pointer based on the session type given
 *
 *  Since server and client sessions have a different structure size, using the fp_disp_session_base_t type to
 *  dereference the session pool by index will lead to incorrect results and the actual size of the underlying
 *  structure must be accounted for. This function uses the correct specialized pointer depending on session type
 *  to calculate the address of the base field correctly.
 *
 *  @param[in] p_session_pool pointer to the beginning of the session pool which contains sessions of type @ref sess_type
 *  @param[in] sess_type types of sessions contained in the pool
 *  @param[in] sess_idx index of the corresponding session to be provided to the user
 *
 *  @return a correctly aligned base pointer depending on the session type
 */
static inline fp_disp_session_base_t *fp_disp_session_mgr_get_base_ptr(uint8_t *const               p_session_pool,
                                                                       const fp_disp_session_type_t sess_type,
                                                                       const uint16_t               sess_idx);

/** @brief Increment a scalar variable up to a given limit
 *  @param[out] p_var pointer to the variable to modify
 *  @param[in] max maximum number to increment to
 *
 *  @note The function will not modify @ref p_var if it already is equal to @ref max.
 */
static void inc_up_to(uint32_t *const p_var, const uint32_t max);

/** @brief Decrement a scalar variable if not zero
 *  @param[out] p_var pointer to the variable to modify
 *
 *  @note The function will not modify @ref p_var if it already zero.
 */
static void safe_dec(uint32_t *const p_var);

/** @brief Perform initialization of all base session parameters
 *
 *  @param[out] p_session pointer to the session structure
 */
static void base_init_session(fp_disp_session_base_t *const p_session);

/** @brief Call the session tracking function but also ensuring validity of all pointers
 *
 *  @param[in] p_sys_ctx system-specific context containing the session pools and related locks
 *  @param[in] p_session pointer to the session structure
 */
static void track_session(fp_dispatch_sys_context_t *const p_sys_ctx,
                          fp_disp_session_base_t *const    p_sess);

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
void fp_disp_session_mgr_init(fp_dispatch_sys_context_t *const p_sys_ctx)
{
    static const fp_disp_session_attributes_t *p_attr_map_per_type[FP_DISP_SESSION_TYPE_MAX] = {
        [FP_DISP_SESSION_TYPE_CLIENT] = &fp_disp_client_sess_attrib,
        [FP_DISP_SESSION_TYPE_SERVER] = &fp_disp_server_sess_attrib
    };

    if (NULL != p_sys_ctx)
    {
        // Initialize the session callbacks structure
        memset(&p_sys_ctx->session_callbacks, 0, sizeof(fp_user_session_callbacks_t));

        for (uint8_t sess_type_id = 0; sess_type_id < FP_DISP_SESSION_TYPE_MAX; sess_type_id++)
        {
            fp_disp_session_data_t sess_data;
            fp_disp_session_mgr_get_session_data(p_sys_ctx, (fp_disp_session_type_t)sess_type_id, &sess_data);

            FP_ARCH_TRACE_DEBUG("Session pool init: type=%u, count=%u", sess_type_id, sess_data.max_session_cnt);

            sess_data.p_metrics->open_session_cnt      = 0;
            sess_data.p_metrics->active_session_cnt    = 0;
            sess_data.p_metrics->closed_session_cnt    = 0;
            sess_data.p_metrics->timed_out_session_cnt = 0;

            fp_disp_session_base_t *p_sess;

            for (uint16_t sess_idx = 0; sess_idx < sess_data.max_session_cnt; sess_idx++)
            {
                p_sess = fp_disp_session_mgr_get_base_ptr((uint8_t *)sess_data.p_session_pool, (fp_disp_session_type_t)sess_type_id, sess_idx);

                base_init_session(p_sess);

                // initialize session-specific vtable interfaces
                p_sess->p_session_attr = (const void *)p_attr_map_per_type[sess_type_id];

                (*((const fp_disp_session_attributes_t *)p_sess->p_session_attr)->p_init_func)(p_sess);
            }
        }
    }
    else
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_sys_ctx=%p in fp_disp_session_mgr_init", p_sys_ctx);
    }
}

fp_disp_session_base_t *fp_disp_session_mgr_open_session(fp_dispatch_sys_context_t *const p_sys_ctx,
                                                         void                            *p_comm_params,
                                                         const fp_disp_session_type_t     sess_type)
{
    fp_disp_session_base_t *p_sess = NULL;

    if ((NULL == p_sys_ctx) || (sess_type >= FP_DISP_SESSION_TYPE_MAX))
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_sys_ctx=%p, sess_type=%u (max=%u) in fp_disp_session_mgr_open_session",
                            p_sys_ctx, sess_type, FP_DISP_SESSION_TYPE_MAX);

        return NULL;
    }

    fp_disp_session_data_t sess_data;
    fp_disp_session_mgr_get_session_data(p_sys_ctx, sess_type, &sess_data);

    FP_ARCH_TRACE_DEBUG("Session open request: type=%u", sess_type);

    void *p_section_lock = p_sys_ctx->p_section_locks[sess_data.section_lock_id];

    if (!fp_arch_lock_section(p_section_lock))
    {
        FP_ARCH_TRACE_ERROR("Lock acquire failed: type=session_mgr, handle=%p, context=session_open, sess_type=%u",
                            p_section_lock, sess_type);

        return NULL;
    }

    fp_disp_session_base_t *p_sess_entry;

    for (uint16_t sess_idx = 0; sess_idx < sess_data.max_session_cnt; sess_idx++)
    {
        p_sess_entry = fp_disp_session_mgr_get_base_ptr((uint8_t *)sess_data.p_session_pool, sess_type, sess_idx);

        if (FP_SESSION_STATE_INACTIVE == atomic_load(&p_sess_entry->state))
        {
            atomic_store(&p_sess_entry->state, FP_SESSION_STATE_OPENED);

            if (sess_data.p_metrics->open_session_cnt < UINT32_MAX)
            {
                sess_data.p_metrics->open_session_cnt++;
            }

            if (sess_data.p_metrics->active_session_cnt < UINT32_MAX)
            {
                sess_data.p_metrics->active_session_cnt++;
            }

            // pass communication parameters to the session
            p_sess_entry->p_comm_params = p_comm_params;
            p_sess                      = p_sess_entry;

            // Call the user-provided open session handler if it exists and a session was successfully opened
            if (NULL != p_sys_ctx->session_callbacks.p_open)
            {
                bool user_open_session_success = (*p_sys_ctx->session_callbacks.p_open)(p_sess);

                if (!user_open_session_success)
                {
                    FP_ARCH_TRACE_DEBUG("Session open failed: user_callback_failed, session_id=%p, type=%u",
                                        p_sess, sess_type);

                    fp_disp_session_mgr_close_session(p_sys_ctx, p_sess);

                    p_sess = NULL;
                }
            }

            break;
        }
    }

    fp_arch_unlock_section(p_section_lock);

    return p_sess;
}

fp_disp_session_base_t *fp_disp_session_mgr_find_session(fp_dispatch_sys_context_t *const p_sys_ctx,
                                                         const fp_disp_session_type_t     sess_type,
                                                         const fp_msg_hdr_t *const        p_msg_hdr)
{
    fp_disp_session_base_t *p_sess = NULL;

    if ((NULL == p_sys_ctx) || (NULL == p_msg_hdr) || (sess_type >= FP_DISP_SESSION_TYPE_MAX))
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_sys_ctx=%p, p_msg_hdr=%p, sess_type=%u (max=%u) in fp_disp_session_mgr_find_session",
                            p_sys_ctx, p_msg_hdr, sess_type, FP_DISP_SESSION_TYPE_MAX);

        return NULL;
    }

    fp_disp_session_data_t sess_data = { 0 };
    fp_disp_session_mgr_get_session_data(p_sys_ctx, sess_type, &sess_data);

    void *p_section_lock = p_sys_ctx->p_section_locks[sess_data.section_lock_id];

    if (!fp_arch_lock_section(p_section_lock))
    {
        FP_ARCH_TRACE_ERROR("Lock acquire failed: type=session_mgr, handle=%p, context=session_find, sess_type=%u",
                            p_section_lock, sess_type);

        return NULL;
    }

    fp_disp_session_base_t             *p_sess_iter;
    const fp_disp_session_attributes_t *p_sess_attr;

    for (uint16_t sess_idx = 0; sess_idx < sess_data.max_session_cnt; sess_idx++)
    {
        p_sess_iter = fp_disp_session_mgr_get_base_ptr((uint8_t *)sess_data.p_session_pool, sess_type, sess_idx);
        p_sess_attr = (const fp_disp_session_attributes_t *)p_sess_iter->p_session_attr;

        fp_arch_assert((NULL != p_sess_attr) && (NULL != p_sess_attr->p_is_matching));

        if ((FP_SESSION_STATE_INACTIVE != atomic_load(&p_sess_iter->state)) &&
            (NULL != p_sess_attr) &&
            (NULL != p_sess_attr->p_is_matching) &&
            ((*p_sess_attr->p_is_matching)(p_sess_iter, p_msg_hdr)))
        {
            p_sess = p_sess_iter;
            break;
        }
    }

    fp_arch_unlock_section(p_section_lock);

    return p_sess;
}

void fp_disp_session_mgr_close_session(fp_dispatch_sys_context_t *const p_sys_ctx,
                                       fp_disp_session_base_t *const    p_session)
{
    if ((NULL == p_sys_ctx) || (NULL == p_session) ||
        ((NULL != p_session) &&
         ((NULL == p_session->p_session_attr) ||
          (NULL == ((const fp_disp_session_attributes_t *)p_session->p_session_attr)->p_init_func))))
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_sys_ctx=%p, p_session=%p, p_session_attr=%p in fp_disp_session_mgr_close_session",
                            p_sys_ctx, p_session, (p_session ? p_session->p_session_attr : NULL));

        return;
    }

    fp_disp_session_data_t              sess_data     = { 0 };
    const fp_disp_session_attributes_t *p_sess_attrib = (const fp_disp_session_attributes_t *)p_session->p_session_attr;

    fp_disp_session_mgr_get_session_data(p_sys_ctx, p_sess_attrib->sess_type, &sess_data);

    FP_ARCH_TRACE_DEBUG("Session close: session_id=%p, request='%u:%lu:%u', type=%u",
                        p_session, p_session->req_fp_hdr.iface_id, p_session->req_fp_hdr.func_id, p_session->req_fp_hdr.cmd_id, p_sess_attrib->sess_type);

    void *p_section_lock = p_sys_ctx->p_section_locks[sess_data.section_lock_id];

    if (!fp_arch_lock_section(p_section_lock))
    {
        FP_ARCH_TRACE_ERROR("Lock acquire failed: type=session_mgr, handle=%p, context=session_close, session_id=%p",
                            p_section_lock, p_session);

        return;
    }

    // Call the user-provided close session handler if it exists
    if (NULL != p_sys_ctx->session_callbacks.p_close)
    {
        (*p_sys_ctx->session_callbacks.p_close)(p_session);
    }

    base_init_session(p_session);

    (*p_sess_attrib->p_init_func)(p_session);

    inc_up_to(&sess_data.p_metrics->closed_session_cnt, UINT32_MAX);
    safe_dec(&sess_data.p_metrics->active_session_cnt);

    fp_arch_unlock_section(p_section_lock);
}

void fp_disp_session_mgr_track_sessions(fp_dispatch_sys_context_t *const p_sys_ctx)
{
    fp_arch_break_assert(NULL != p_sys_ctx);

    if (NULL == p_sys_ctx)
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_sys_ctx=%p in fp_disp_session_mgr_track_sessions", p_sys_ctx);

        return;
    }

    for (uint8_t sess_type_id = 0; sess_type_id < FP_DISP_SESSION_TYPE_MAX; sess_type_id++)
    {
        fp_disp_session_data_t sess_data;
        fp_disp_session_mgr_get_session_data(p_sys_ctx, (fp_disp_session_type_t)sess_type_id, &sess_data);

        void *p_section_lock = p_sys_ctx->p_section_locks[sess_data.section_lock_id];

        if (fp_arch_lock_section(p_section_lock))
        {
            for (uint16_t sess_idx = 0; sess_idx < sess_data.max_session_cnt; sess_idx++)
            {
                fp_disp_session_base_t *const p_sess = fp_disp_session_mgr_get_base_ptr((uint8_t *)sess_data.p_session_pool, (fp_disp_session_type_t)sess_type_id, sess_idx);

                track_session(p_sys_ctx, p_sess);
            }

            fp_arch_unlock_section(p_section_lock);
        }
        else
        {
            FP_ARCH_TRACE_ERROR("Lock acquire failed: type=session_mgr, handle=%p, context=session_track, sess_type=%u",
                                p_section_lock, sess_type_id);
        }
    }
}

void fp_disp_session_mgr_register_callbacks(fp_dispatch_sys_context_t *const         p_sys_ctx,
                                            const fp_user_session_callbacks_t *const p_callbacks)
{
    if (NULL == p_sys_ctx)
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_sys_ctx=%p in fp_disp_session_mgr_register_callbacks", p_sys_ctx);
        return;
    }

    if (NULL == p_callbacks)
    {
        // Clear all callbacks
        memset(&p_sys_ctx->session_callbacks, 0, sizeof(fp_user_session_callbacks_t));
    }
    else
    {
        // Copy the callbacks
        p_sys_ctx->session_callbacks = *p_callbacks;
    }
}
/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/
static inline void fp_disp_session_mgr_get_session_data(fp_dispatch_sys_context_t *const p_sys_ctx,
                                                        const fp_disp_session_type_t     sess_type,
                                                        fp_disp_session_data_t *const    p_sess_data)
{
    fp_arch_assert((NULL != p_sys_ctx) && (NULL != p_sess_data) && (sess_type < FP_DISP_SESSION_TYPE_MAX));
    static_assert(FP_DISP_SESSION_TYPE_MAX == 2, "The code below may need to be changed to account for other types of sessions");

    if (FP_DISP_SESSION_TYPE_SERVER == sess_type)
    {
        p_sess_data->p_session_pool  = (fp_disp_session_base_t *)p_sys_ctx->server_session_pools;
        p_sess_data->p_metrics       = &p_sys_ctx->server_metrics;
        p_sess_data->section_lock_id = FP_LOCK_SECTION_SERVER_SESSION;
        p_sess_data->max_session_cnt = MAX_SERVER_SESSIONS_COUNT;
    }
    else
    { // FP_DISP_SESSION_TYPE_CLIENT
        p_sess_data->p_session_pool  = (fp_disp_session_base_t *)p_sys_ctx->client_session_pools;
        p_sess_data->p_metrics       = &p_sys_ctx->client_metrics;
        p_sess_data->section_lock_id = FP_LOCK_SECTION_CLIENT_SESSION;
        p_sess_data->max_session_cnt = MAX_CLIENT_SESSIONS_COUNT;
    }
}

static inline fp_disp_session_base_t *fp_disp_session_mgr_get_base_ptr(uint8_t *const               p_session_pool,
                                                                       const fp_disp_session_type_t sess_type,
                                                                       const uint16_t               sess_idx)
{
    fp_arch_assert(sess_type < FP_DISP_SESSION_TYPE_MAX);
    static_assert(FP_DISP_SESSION_TYPE_MAX == 2, "The code below may need to be changed to account for other types of sessions");

    if (FP_DISP_SESSION_TYPE_SERVER == sess_type)
    {
        return (fp_disp_session_base_t *)&((fp_disp_server_session_t *)p_session_pool)[sess_idx];
    }
    else
    {
        return (fp_disp_session_base_t *)&((fp_disp_client_session_t *)p_session_pool)[sess_idx];
    }
}

static void inc_up_to(uint32_t *const p_var, const uint32_t max)
{
    fp_arch_assert(NULL != p_var);

    if (*p_var < max)
    {
        ++(*p_var);
    }
}

static void safe_dec(uint32_t *const p_var)
{
    fp_arch_assert(NULL != p_var);

    if (*p_var > 0)
    {
        (*p_var)--;
    }
}

static void base_init_session(fp_disp_session_base_t *const p_session)
{
    fp_arch_assert(NULL != p_session);

    p_session->req_fp_hdr.cmd_id   = UINT16_MAX;
    p_session->req_fp_hdr.iface_id = UINT16_MAX;
    p_session->req_fp_hdr.func_id  = UINT32_MAX;
    p_session->req_fp_hdr.status   = FP_CMD_STATUS_MAX;
    atomic_store(&p_session->state, FP_SESSION_STATE_INACTIVE);
    p_session->timeout       = 0;
    p_session->p_comm_params = NULL;
    fp_arch_stop_timer(&p_session->timer_hnd);
}

static void track_session(fp_dispatch_sys_context_t *const p_sys_ctx,
                          fp_disp_session_base_t *const    p_sess)
{
    fp_arch_break_assert(NULL != p_sess);

    if ((NULL != p_sess) && (FP_SESSION_STATE_ACTIVE == atomic_load(&p_sess->state)))
    {
        // leave standard timeout session tracking active by default unless overridden by user-tracking handler
        fp_session_track_action_t track_action = FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE;

        // Call the user-provided tracking function first to handle the communication session (e.g. specific reception handling)
        if (NULL != p_sys_ctx->session_callbacks.p_track)
        {
            track_action = (*p_sys_ctx->session_callbacks.p_track)(p_sess);
        }

        if (FP_DISP_TRACK_ACTION_CLOSE_SESSION == track_action)
        {
            // user tracking confirmed that the session can be closed
            fp_disp_session_mgr_close_session(p_sys_ctx, p_sess);
        }
        else
        {
            // the session is still open, so track timeouts...
            p_track_session_t p_track_session = ((const fp_disp_session_attributes_t *)(p_sess->p_session_attr))->p_track_session;

            fp_arch_break_assert(NULL != p_track_session);

            if (NULL != p_track_session)
            {
                (*p_track_session)(p_sys_ctx, p_sess);
            }
        }
    }
}
