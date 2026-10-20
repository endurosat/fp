/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @addtogroup fp
 * @{
 *
 * @file fp_config.h
 * @brief System-specific configuraion definitions related to the FP implementation
 *
 * @}
 *
 */

#ifndef FP_CONFIG_H
#define FP_CONFIG_H

#include <stdint.h>

/*
***************************************************************************************************
* EXTERNAL DEFINES
***************************************************************************************************
*/
/** @brief Maximum number of supported simultaneous client sessions (limited by the range of uint16_t which is the type used internally to manage this) */
#ifndef MAX_CLIENT_SESSIONS_COUNT
#define MAX_CLIENT_SESSIONS_COUNT ((uint16_t)5)
#endif

/** @brief Maximum number of supported simultaneous server sessions which require special handling (e.g. arm-and-fire requests) (limited by the range of uint16_t which is the type used internally to manage this) */
#ifndef MAX_SERVER_SESSIONS_COUNT
#define MAX_SERVER_SESSIONS_COUNT ((uint16_t)5)
#endif

/** @brief Maximum number FIDLs registered in the system */
#ifndef MAX_FIDL_COUNT
#define MAX_FIDL_COUNT ((uint16_t)5)
#endif

/** @brief Number of seconds to wait before disarming an armed fire-and-arm type of request from a client */
#ifndef ARMED_REQ_TOUT_S
#define ARMED_REQ_TOUT_S ((uint16_t)60)
#endif

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

#endif /* FP_CONFIG_H */
