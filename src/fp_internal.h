/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_INTERNAL_H
#define FP_INTERNAL_H

/**
 * @defgroup fp
 * @{
 *
 * APIs shared for internal use withing the FP service implementation.
 *
 * @file fp_internal.h
 *
 * @}
 */

#include <stdint.h>

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
/** @brief Generate a new command ID
 *
 *  @return unique command ID
 *
 *  @note The function is thread-safe and uses atomic operations to ensure
 *  unique command IDs across all FP operations (client requests and server broadcasts).
 *  The counter wraps around at UINT16_MAX.
 */
uint16_t fp_internal_gen_cmd_id(void);

#ifdef __cplusplus
}
#endif

#endif /* FP_INTERNAL_H */
