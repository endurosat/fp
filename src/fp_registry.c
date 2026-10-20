/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @addtogroup fp
 * @{
 *
 * @file fp_registry.c
 * @brief Implementation of the Function Protocol Registry module. It provides FP registry
 * management such as dynamic addition, removal and iteration on FP entries. The actual storage
 * for the registry comes from the user and is preallocated in advance. The module does not
 * use dynamic memory allocation but assumes the memory assigned to it can be accessed at all times and
 * is valid.
 *
 * @}
 *
 */

#include <stdio.h>
#include "fp_registry.h"
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
/** @brief FIDL house-keeping data structure */
typedef struct
{
    const fidl_if_t *fidl_handlers[MAX_FIDL_COUNT]; /**< a list of pointers to the FIDL handlers registered in the system */
    fidl_index_t     fidl_hnd_cnt;                  /**< number of registered FIDL handlers in the registry */
    fidl_index_t     current_idx;                   /**< index of the FIDL handler to be returned by the next call to @ref registry_iterator_get_next() */
    bool             is_locked;                     /**< flag indicating whether the registry is currently locked by the iterator */
} reg_internal_t;

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
static reg_internal_t reg_ctx; /**< FP registry internal context variable */

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DECLARATION
***************************************************************************************************
*/
/** @brief The function checks if a corresponding FIDL interface is already available in the registry and is
 *  mapped to the same server or client role.
 *
 * @param p_handler pointer to a FIDL interface structure describing the FIDL
 * operations
 *
 * @retval true The FIDL interface in the specified role is already in the registry
 * @retval false The FIDL interface in the specified role is not available in the registry
 */
static bool is_fidl_if_assigned(const fidl_if_t *const p_handler);

/*
***************************************************************************************************
* EXTERNAL (NON STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/
void fp_registry_init(void)
{
    for (fidl_index_t fidl_idx = 0; fidl_idx < MAX_FIDL_COUNT; fidl_idx++)
    {
        reg_ctx.fidl_handlers[fidl_idx] = NULL;
    }

    reg_ctx.fidl_hnd_cnt = 0;
    reg_ctx.current_idx  = 0;
    reg_ctx.is_locked    = false;
}

bool fp_registry_iterator_open(void)
{
    if (fp_arch_lock_registry())
    {
        reg_ctx.current_idx = 0;
        reg_ctx.is_locked   = true;
        return true;
    }
    else
    {
        reg_ctx.is_locked = false;
        FP_ARCH_TRACE_ERROR("Lock acquire failed: type=registry, context=iterator_open");
        return false;
    }
}

const fidl_if_t *fp_registry_iterator_get_next(void)
{
    const fidl_if_t *p_next_val = NULL;

    if (reg_ctx.current_idx < reg_ctx.fidl_hnd_cnt)
    {
        p_next_val = reg_ctx.fidl_handlers[reg_ctx.current_idx];
        ++reg_ctx.current_idx;
    }

    return p_next_val;
}

void fp_registry_iterator_close(void)
{
    if (reg_ctx.is_locked)
    {
        fp_arch_unlock_registry();
        reg_ctx.is_locked = false;
    }
}

const fidl_if_t *fp_registry_get_fidl_if_by_id(const uint16_t fidl_if_id, const fidl_gen_type_t gen_type)
{
    if (!fp_registry_iterator_open())
    {
        FP_ARCH_TRACE_ERROR("Registry search failed: iterator_open_failed, fidl_if_id=%u, gen_type=%u",
                            fidl_if_id, gen_type);
        return NULL;
    }

    const fidl_if_t *p_current_fidl_hnd = NULL;

    p_current_fidl_hnd = fp_registry_iterator_get_next();

    while (NULL != p_current_fidl_hnd)
    {
        if ((p_current_fidl_hnd->fidl_if_id == fidl_if_id) && (p_current_fidl_hnd->gen_type == gen_type))
        {
            break;
        }

        p_current_fidl_hnd = fp_registry_iterator_get_next();
    }

    fp_registry_iterator_close();

    return p_current_fidl_hnd;
}

bool fp_registry_add_fidl_if(const fidl_if_t *const p_handler)
{
    bool res = false;

    if (NULL == p_handler)
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_handler=%p in fp_registry_add_fidl_if", p_handler);

        return false;
    }

    if (!fp_arch_lock_registry())
    {
        FP_ARCH_TRACE_ERROR("Lock acquire failed: type=registry, context=add_fidl_if, fidl_if_id=%u",
                            p_handler->fidl_if_id);

        return false;
    }

    if ((p_handler->func_count > 0) &&
        (NULL != p_handler->p_func_list) &&
        (reg_ctx.fidl_hnd_cnt < MAX_FIDL_COUNT) &&
        (!is_fidl_if_assigned(p_handler)))
    {
        reg_ctx.fidl_handlers[reg_ctx.fidl_hnd_cnt++] = p_handler;

        res = true;
    }
    else
    {
        FP_ARCH_TRACE_WARN("Registry add failed: invalid_fidl_interface, fidl_if_id=%u, func_count=%u",
                           p_handler->fidl_if_id, p_handler->func_count);
    }

    fp_arch_unlock_registry();

    return res;
}

bool fp_registry_remove_fidl_if(const fidl_if_t *const p_handler)
{
    bool res = false;

    if (NULL == p_handler)
    {
        FP_ARCH_TRACE_ERROR("Invalid argument: p_handler=%p in fp_registry_remove_fidl_if", p_handler);

        return false;
    }

    if (!fp_arch_lock_registry())
    {
        FP_ARCH_TRACE_ERROR("Lock acquire failed: type=registry, context=remove_fidl_if, fidl_if_id=%u",
                            p_handler->fidl_if_id);

        return false;
    }

    if (reg_ctx.fidl_hnd_cnt == 0)
    {
        fp_arch_unlock_registry();

        return false;
    }

    for (fidl_index_t idx = 0; idx < reg_ctx.fidl_hnd_cnt; idx++)
    {
        if (reg_ctx.fidl_handlers[idx] == p_handler)
        {
            // move all items to the end of the list one slot ahead (overwriting the found entry which is to be removed)
            for (fidl_index_t copy_idx = idx + 1; copy_idx < reg_ctx.fidl_hnd_cnt; copy_idx++)
            {
                reg_ctx.fidl_handlers[idx++] = reg_ctx.fidl_handlers[copy_idx];
            }

            // since all elements moved ahead with 1, invalidate the last one which should now remain empty
            reg_ctx.fidl_handlers[--reg_ctx.fidl_hnd_cnt] = NULL;

            res = true;

            break;
        }
    }

    FP_ARCH_TRACE_WARN("Registry remove failed: fidl_interface_not_found, fidl_if_id=%u", p_handler->fidl_if_id);

    fp_arch_unlock_registry();

    return res;
}

const fidl_func_t *fp_registry_get_func_by_id(const fidl_if_t *p_fidl, const uint32_t target_function_id)
{
    const fidl_func_t *p_function = NULL;

    if ((NULL != p_fidl) && (p_fidl->func_count > 0))
    {
        int32_t low  = 0;
        int32_t high = p_fidl->func_count - 1;
        int32_t mid;

        while (low <= high)
        {
            mid        = low + (high - low) / 2;
            p_function = &p_fidl->p_func_list[mid];

            if (p_function->func_id == target_function_id)
            {
                break;
            }
            else if (p_function->func_id > target_function_id)
            {
                high = mid - 1;
            }
            else
            {
                low = mid + 1;
            }

            p_function = NULL;
        }
    }

    return p_function;
}

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/
static bool is_fidl_if_assigned(const fidl_if_t *const p_handler)
{
    fp_arch_assert(NULL != p_handler);

    for (fidl_index_t idx = 0; idx < reg_ctx.fidl_hnd_cnt; idx++)
    {
        if ((reg_ctx.fidl_handlers[idx]->gen_type == p_handler->gen_type) &&
            (reg_ctx.fidl_handlers[idx]->fidl_if_id == p_handler->fidl_if_id))
        {
            return true;
        }
    }

    return false;
}
