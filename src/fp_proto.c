/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @addtogroup fp
 * @{
 *
 * @file fp_proto.c
 * @brief Implementation of FP-specific protocol handling routines.
 *
 * @}
 *
 */

#include <stdio.h>
#include "fp_proto.h"

/*
***************************************************************************************************
* INTERNAL DEFINES
***************************************************************************************************
*/

/** @brief Bitmask to specify an FP response */
#define RESP_FUNC_ID_MASK ((uint32_t)0x80000000)
/** @brief Bitmask to specify an ARM request type */
#define ARMREQ_FUNC_ID_MASK ((uint32_t)0x40000000)
/** @brief Bitmask to specify a FIRE request type */
#define FIREREQ_FUNC_ID_MASK ((uint32_t)0x20000000)
/** @brief Bitmask to specify a DISARM request type */
#define DISARMREQ_FUNC_ID_MASK ((uint32_t)0x10000000)
/** @brief Bitmask to specify a BROADCAST frame type */
#define BROADCAST_FUNC_ID_MASK ((uint32_t)0x08000000)

/** @brief Bitmask for filtering out special attribute bits */
#define ATTRIB_BITS_ONLY_MASK \
    (RESP_FUNC_ID_MASK |      \
     ARMREQ_FUNC_ID_MASK |    \
     FIREREQ_FUNC_ID_MASK |   \
     DISARMREQ_FUNC_ID_MASK | \
     BROADCAST_FUNC_ID_MASK)

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
/** @brief Mapping between FP frame type and a bitmask */
static const uint32_t ft_bitmask_map[FP_FRAME_TYPE_MAX] = {
    [FP_FRAME_TYPE_REQ]        = 0,
    [FP_FRAME_TYPE_REQ_ARM]    = ARMREQ_FUNC_ID_MASK,
    [FP_FRAME_TYPE_REQ_FIRE]   = FIREREQ_FUNC_ID_MASK,
    [FP_FRAME_TYPE_REQ_DISARM] = DISARMREQ_FUNC_ID_MASK,
    [FP_FRAME_TYPE_RESP]       = RESP_FUNC_ID_MASK,
    [FP_FRAME_TYPE_BROADCAST]  = BROADCAST_FUNC_ID_MASK,
};

/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DECLARATION
***************************************************************************************************
*/

/*
***************************************************************************************************
* EXTERNAL (NON STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/

void fp_proto_get_func_attributes(const fp_msg_hdr_t *const p_fp_hdr, fp_proto_func_attr_t *const p_attr)
{
    if ((NULL != p_fp_hdr) && (NULL != p_attr))
    {
        p_attr->bare_func_id = (p_fp_hdr->func_id) & ~ATTRIB_BITS_ONLY_MASK;
        p_attr->frame_type   = FP_FRAME_TYPE_INVALID;

        for (uint8_t ft_idx = 0; ft_idx < FP_FRAME_TYPE_MAX; ft_idx++)
        {
            if (ft_bitmask_map[ft_idx] == (p_fp_hdr->func_id & ATTRIB_BITS_ONLY_MASK))
            {
                p_attr->frame_type = (fp_frame_type_t)ft_idx;

                break;
            }
        }
    }
}

/** @deprecated This function is deprecated. Use fp_proto_set_function_type() instead.
 *
 *  This function only modifies the frame type bits without fully initializing the func_id field.
 *  It assumes the buffer is already initialized and the function ID is already set, which can
 *  lead to incorrect bits remaining set if the buffer contains uninitialized data.
 *
 *  Use fp_proto_set_function_type() for complete initialization of the func_id field in a single
 *  atomic operation.
 */
void fp_proto_set_frame_type(fp_msg_hdr_t *const p_fp_hdr, const fp_frame_type_t frame_type)
{
    if ((NULL == p_fp_hdr) || (frame_type >= FP_FRAME_TYPE_MAX))
    {
        return;
    }

    p_fp_hdr->func_id |= ft_bitmask_map[frame_type];
}

void fp_proto_set_function_type(fp_msg_hdr_t *const p_fp_hdr, const uint32_t func_id, const fp_frame_type_t frame_type)
{
    if ((NULL == p_fp_hdr) || (frame_type >= FP_FRAME_TYPE_MAX))
    {
        return;
    }

    // Fully initialize func_id field: clear all bits, then set function ID (lower 20 bits) and frame type bitmask (upper 12 bits)
    p_fp_hdr->func_id = (func_id & ~ATTRIB_BITS_ONLY_MASK) | ft_bitmask_map[frame_type];
}

void fp_proto_set_func_id(fp_msg_hdr_t *const p_fp_hdr, const uint32_t func_id)
{
    if (NULL == p_fp_hdr)
    {
        return;
    }

    p_fp_hdr->func_id |= func_id & ~ATTRIB_BITS_ONLY_MASK;
}

bool fp_proto_is_bare_func_id_matching(const uint32_t func_id_1, const uint32_t func_id_2)
{
    return (func_id_1 & ~ATTRIB_BITS_ONLY_MASK) == (func_id_2 & ~ATTRIB_BITS_ONLY_MASK);
}
/*
***************************************************************************************************
* INTERNAL (STATIC) ROUTINES DEFINITION
***************************************************************************************************
*/
