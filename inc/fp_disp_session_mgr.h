/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

// Force inclusion of C++ atomic header outside of any extern "C" block
// This is a workaround for when this header is included within extern "C" blocks
#if defined(__cplusplus) && !defined(FP_ATOMIC_HEADER_INCLUDED)
#define FP_ATOMIC_HEADER_INCLUDED
#include <atomic>
#endif

#ifndef FP_DISP_SESSION_MGR_H
#define FP_DISP_SESSION_MGR_H

/**
 * @defgroup fp
 * @{
 *
 * Public interface of the Function Protocol Dispatcher session management module.
 * It provides all session management routines used by generated code regardless of the type of session.
 *
 * @file fp_disp_session_mgr.h
 *
 * @}
 */

#include "fp_defs.h"
#include "fp_proto.h"

// Define atomic compatibility macros
#if defined(__cplusplus) && (__cplusplus <= 202002L)
// C++ compatibility: map C11 atomic operations to C++ std::atomic
// Since C++23, this mapping is done in stdatomic.h. The check is <= C++20 and not < C++23, because some compilers
// support the mapping in intermediate versions.
#define _Atomic(T) std::atomic<T>
#define atomic_load(ptr) (ptr)->load(std::memory_order_seq_cst)
#define atomic_store(ptr, val) (ptr)->store(val, std::memory_order_seq_cst)
#else
#include <stdatomic.h>
#endif

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

/** @brief Session state enumeration to replace the boolean is_active flag
 *  This provides atomic state management and eliminates race conditions
 *  between session allocation and timer start operations.
 */
typedef enum
{
    FP_SESSION_STATE_INACTIVE = 0, /**< Session is not in use (available for allocation) */
    FP_SESSION_STATE_OPENED   = 1, /**< Session is allocated but timer not yet started */
    FP_SESSION_STATE_ACTIVE   = 2, /**< Session is active with timer running */
    FP_SESSION_STATE_MAX           /**< Range checking value */
} fp_session_state_t;
/** @brief Generic typedef for a request handler which doesn't specify the actual request data type
 *  in order to support generic handling in the FP dispatcher instead of inside the generated code.
 *
 *  @param[in] p_disp_ctx FP dispatch context (of type @ref fp_dispatch_info_t) - void pointer used to avoid cyclic deps
 *  @param[in] p_untyped_req generic pointer to the request data bytes (proper casting to the specific response type structure will be performed by the user handler)
 */
typedef void (*fp_generic_request_handler_t)(void *p_disp_ctx, void *p_untyped_req);

/** @brief Session-management runtime metrics */
typedef struct
{
    uint32_t active_session_cnt;    /**< number of currently active sessions */
    uint32_t open_session_cnt;      /**< total number of acquired sessions since power up */
    uint32_t closed_session_cnt;    /**< total number of released sessions since power up */
    uint32_t timed_out_session_cnt; /**< number of released sessions due to response timeout */
} fp_disp_session_metrics_t;

#ifdef __cplusplus
}
#endif

/** @brief FP session base structure which must be included in every client/server-specific session structure as the first parameter so that the general session logic
 *  can use this to manage the standard session features. This is basically class inheritance in C.
 */
typedef struct
{
    _Atomic(fp_session_state_t) state;          /**< atomic session state */
    uint32_t                    timeout;        /**< response timeout in milliseconds */
    fp_arch_timer_handle_t      timer_hnd;      /**< timer handle to use for session timeout tracking */
    fp_msg_hdr_t                req_fp_hdr;     /**< FP header specifying the executed request (used to match to the same request if received again) */
    const void                 *p_session_attr; /**< field of type @ref fp_disp_session_attributes_t which contains attributes specific to the type of session (e.g. client or server) */
    void                       *p_comm_params;  /**< pointer to a user-specific communication parameters which must be preserved along with the specific FP dispatch context (not used by FP-related functions but can be utilized by the p_on_fp_send_cbk function to know how to send the data to the physical bus) */
} fp_disp_session_base_t;

/** @brief FP server session context used to track the reception of special types of requests (such as fire-and-arm) */
typedef struct
{
    fp_disp_session_base_t       base;           /**< base session context data (must always be the first field here to enable usage of fp_disp_session_base_t pointers to handle client/server-specific session structs) */
    fp_generic_request_handler_t p_user_handler; /**< pointer to a user-registered request handler */
    uint16_t                     req_data_hash;  /**< simple hash on the data of the initial request, e.g. a CRC (used to ensure the fire request contained the same data as the ARM request)  */
} fp_disp_server_session_t;

/** @brief Supported FP request types */
typedef enum
{
    FP_REQ_TYPE_ASYNC = 0, /**< Denotes an asynchronous request */
    FP_REQ_TYPE_SYNC  = 1, /**< Denotes a synchronous request */
    FP_REQ_TYPE_MAX        /**< value used for range checking */
} fp_req_type_t;

/** @brief Generic typedef for a response handler which doesn't specify the actual response data type
 *  in order to support generic handling in the FP dispatcher instead of inside the generated code.
 *
 *  @param[in] p_disp_ctx FP dispatch context (of type @ref fp_dispatch_info_t) - void pointer used to avoid cyclic deps
 *  @param[in] p_untyped_resp generic pointer to the response data bytes (proper casting to the specific response type structure will be performed by the user handler)
 *  @param[in] cmd_status dispatch error status code
 */
typedef void (*fp_generic_response_handler_t)(void                 *p_disp_ctx,
                                              uint8_t              *p_untyped_resp,
                                              const fp_cmd_status_t cmd_status);

/** @brief FP client session context used to track the reception of a response for an issued client request */
typedef struct
{
    fp_disp_session_base_t        base;               /**< base session context data (must always be the first field here to enable usage of fp_disp_session_base_t pointers to handle client/server-specific session structs) */
    fp_generic_response_handler_t p_user_handler;     /**< pointer to a user-registered response handler */
    fp_req_type_t                 req_type;           /**< indicate the type of request performed by the user to properly handle response data transfer */
    void                         *p_session_event;    /**< pointer to a session event variable used to implement blocking for synchronous FP requests */
    uint8_t                      *p_resp_data;        /**< in case of synchronous requests, this is a pointer to the buffer to copy the received response data to (used by fp_disp to deliver the data to the original user thread) */
    uint32_t                      resp_data_max_size; /**< pool-wide capacity of the response slot; set once at pool init, never per-request */
    uint32_t                      resp_buf_user_max;  /**< per-request caller buffer capacity; 0 = use resp_data_max_size (set via fp_disp_client_send_req_bounded) */
    uint32_t                      resp_data_size;     /**< number of response bytes actually delivered to the user buffer for the last sync reply (0 on error/timeout) */
} fp_disp_client_session_t;

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Defines the supported FP session types */
typedef enum
{
    FP_DISP_SESSION_TYPE_SERVER = 0, /**< server session */
    FP_DISP_SESSION_TYPE_CLIENT = 1, /**< client session */
    FP_DISP_SESSION_TYPE_MAX         /**< value used for range checking */
} fp_disp_session_type_t;

/** @brief Defines the possible return values for the session tracking callback */
typedef enum
{
    FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE = 0, /**< Keep the session active and continue tracking */
    FP_DISP_TRACK_ACTION_CLOSE_SESSION       = 1, /**< Close the session */
    FP_DISP_TRACK_ACTION_MAX                      /**< value used for range checking */
} fp_session_track_action_t;

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
/** @brief Enumerates the different logical sections of code which need race condition protection */
typedef enum
{
    FP_LOCK_SECTION_CLIENT_SESSION = 0, /**< used to protect client-session management structures */
    FP_LOCK_SECTION_SERVER_SESSION = 1, /**< used to protect server-session management structures */
    FP_LOCK_SECTION_MAX                 /**< used for range checking */
} fp_lock_section_t;

/** @brief Defines the prototype for a user-provided system level FP frame send function
 *  @param p_disp_ctx a parameter of type @ref fp_dispatch_info_t or an inherited type
 *
 *  @retval true The send operation succeeded
 *  @retval false The send operation failed
 */
typedef bool (*p_user_send_func_t)(void *p_disp_ctx);

/** @brief Defines the prototype of a user-specified system level function which can allocate message buffers upon request
 *
 *  @param[in] buf_size size of the buffer to allocated in bytes
 *  @param[out] pp_data user-provded pointer variable to initialize with the data field of the allocated buffer object
 *  (the return address from the function may be the same as the address provided here but the users shall use (*pp_data) to
 *  work with the actual buffer to ensure correct operations if the buffer object type is something different than just a plain
 *  array of bytes)
 *
 *  @return pointer to the allocated buffer with the requested size or NULL if the allocation was not possible
 */
typedef void *(*p_user_alloc_buffer_func_t)(uint32_t buf_size, uint8_t **pp_data);

/** @brief Structure containing all user-provided session lifecycle callbacks */
typedef struct
{
    /** @brief Called after a session is opened and fully initialized
     *
     *  @param[in] p_sess Pointer to the session data that was opened
     *
     *  @retval true The session initialization was successful and the session should remain open.
     *  @retval false The session initialization failed and the session should be closed.
     */
    bool (*p_open)(fp_disp_session_base_t *const p_sess);

    /** @brief Called during session tracking to check if session should be closed
     *
     *  @param[in] p_sess Pointer to the session data to perform tracking on
     *
     *  @return A value from the @ref fp_session_track_action_t enumeration:
     *          - @ref KEEP_SESSION_ACTIVE to keep the session active and continue tracking
     *          - @ref CLOSE_SESSION to close the session
     *
     *  @warning Do not close the session from this function! Leave that to the standard handlers. Use it only to poll the status
     *  of any active connections or read data from them each time before FP session monitoring is performed. The focus of this
     *  function is on processing the @ref p_comm_params context which will not be serviced by the standard handlers. One possible use
     *  case is to handle asynchronous request polling in combination with standard FP session monitoring to avoid introducing a separate task.
     */
    fp_session_track_action_t (*p_track)(fp_disp_session_base_t *const p_sess);

    /** @brief Called before a session is closed to enable users to free resources on the
     *  session which are part of the user-specific communication handling of the session, e.g. close active network connections or similar actions.
     *
     *  @param[in] p_sess Pointer to the session data that is being closed
     */
    void (*p_close)(fp_disp_session_base_t *const p_sess);
} fp_user_session_callbacks_t;

/** @brief System-specific dispatch context used to handle physical send operations and
 *  to provide locking for internal dispatch-specific data.
 */
typedef struct
{
    p_user_send_func_t          p_send;                                          /**< if not NULL, executed upon a send operation on this context, e.g. a server responding to a command (returns true if the send is successful); p_disp_ctx is a pointer to fp_dispatch_info_t but the type is not used directly to avoid cylic type dependencies */
    p_user_alloc_buffer_func_t  p_alloc_buf;                                     /**< called by the generated code if a FIDL method is configured to use an externally allocated buffer instead of one on the stack (e.g. to enable zero-copy transfers with the host system) */
    void                       *p_section_locks[FP_LOCK_SECTION_MAX];            /**< an array of system-specific section locks provided externally to synchronize access to independent data sections in FP service */
    fp_disp_server_session_t    server_session_pools[MAX_SERVER_SESSIONS_COUNT]; /**< allocated storage for FP server sessions */
    fp_disp_client_session_t    client_session_pools[MAX_CLIENT_SESSIONS_COUNT]; /**< allocated storage for the FP client sessions */
    fp_disp_session_metrics_t   server_metrics;                                  /**< standard runtime metrics information for servers */
    fp_disp_session_metrics_t   client_metrics;                                  /**< standard runtime metrics information for clients */
    fp_user_session_callbacks_t session_callbacks;                               /**< user-provided callbacks for session lifecycle events */
} fp_dispatch_sys_context_t;

/** @brief Session-specific initialization function pointer type
 *
 *  @param[in] p_sess pointer to the session base data used to perform initialization of base data fields
 */
typedef void (*p_init_func_t)(fp_disp_session_base_t *const p_sess);

/** @brief Match function pointer type. Used to provide condition matching on active sessions in the pool.
 *
 *  @param[in] p_sess pointer to the session base data
 *  @param[in] p_new_hdr pointer to an FP header to match against
 *
 *  @retval true Successful match
 *  @retval false No match
 *
 *  @note Matching is usually performed on specific fields in the frame FP header but can also be performed with
 *  other conditions in mind.
 */
typedef bool (*p_is_matching_t)(const fp_disp_session_base_t *const p_sess,
                                const fp_msg_hdr_t *const           p_new_hdr);

/** @brief Session tracking function pointer type. Used to implement functions which perform general FP session tracking such
 *  as timeouts and other protocol-specific matters. Usually called regularly to do its job.
 *
 *  @param[in] p_sys_ctx Pointer to the FP system context information
 *  @param[in] p_sess Pointer to the session data to perform tracking on
 */
typedef void (*p_track_session_t)(fp_dispatch_sys_context_t *const p_sys_ctx,
                                  fp_disp_session_base_t *const    p_sess);

/** @brief Session-specific constant attributes providing information about handling of sessions of type @ref fp_disp_session_attributes_t::sess_type */
typedef struct
{
    p_init_func_t                p_init_func;     /**< pointer to a session-specific initialization function */
    p_is_matching_t              p_is_matching;   /**< match function to use when searching for active sessions in the pool */
    p_track_session_t            p_track_session; /**< session-type-specific track function */
    const fp_disp_session_type_t sess_type;       /**< type of the session */
} fp_disp_session_attributes_t;

/** @brief Initializes the session pool memory which is statically allocated by the FP service based
 *  on compile-time configuration.
 *
 *  @param[in] p_sys_ctx pointer to a valid FP system context
 */
FP_API void fp_disp_session_mgr_init(fp_dispatch_sys_context_t *const p_sys_ctx);

/** @brief Open a session context
 *
 *  @param[in] p_sys_ctx pointer to a valid FP system context
 *  @param[in] p_comm_params pointer to the user-specific communication parameters with which the session is to be opened
 *  @param[in] sess_type type of the session to open
 *
 *  @returns A pointer to a valid client session context which will be reserved for use until it is
 *  released
 */
FP_API fp_disp_session_base_t *fp_disp_session_mgr_open_session(fp_dispatch_sys_context_t *const p_sys_ctx,
                                                                void                            *p_comm_params,
                                                                const fp_disp_session_type_t     sess_type);

/** @brief Look for an active session associated to the specified FP header
 *
 *  @param[in] p_sys_ctx pointer to a valid FP system context
 *  @param[in] sess_type type of the session to look for
 *  @param[in] p_msg_hdr pointer to an FP header which is used to find the session
 *
 *  @returns a pointer to the found session (returns NULL if the session was not found in the pool)
 */
FP_API fp_disp_session_base_t *fp_disp_session_mgr_find_session(fp_dispatch_sys_context_t *const p_sys_ctx,
                                                                const fp_disp_session_type_t     sess_type,
                                                                const fp_msg_hdr_t *const        p_msg_hdr);

/** @brief Release a context matching a specified session handle
 *
 *  @param[in] p_sys_ctx pointer to a valid FP system context
 *  @param[in] p_session pointer to a session from the pool to be released
 */
FP_API void fp_disp_session_mgr_close_session(fp_dispatch_sys_context_t *const p_sys_ctx,
                                              fp_disp_session_base_t *const    p_session);

/** @brief Handle session timeouts for all active sessions of any type
 *
 *  @param[in] p_sys_ctx pointer to a valid FP system context
 *
 *  @note The function shall be called regularly or be mapped to its own periodic OS task.
 */
FP_API void fp_disp_session_mgr_track_sessions(fp_dispatch_sys_context_t *const p_sys_ctx);

/** @brief Register user-provided session lifecycle callbacks
 *
 *  @param[in] p_sys_ctx pointer to a valid FP system context
 *  @param[in] p_callbacks pointer to a structure containing user-provided callbacks for session lifecycle events
 *
 *  @note The registered callbacks will be called at appropriate points in the session lifecycle:
 *        - p_open: after a session is opened and fully initialized
 *        - p_track: for each active session during session tracking, before the standard session-specific tracking function
 *        - p_close: for each session that is being closed, before the session is reinitialized
 */
FP_API void fp_disp_session_mgr_register_callbacks(fp_dispatch_sys_context_t *const         p_sys_ctx,
                                                   const fp_user_session_callbacks_t *const p_callbacks);

#ifdef __cplusplus
};
#endif

#endif
