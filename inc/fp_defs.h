/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_DEFS_H
#define FP_DEFS_H

/**
 * @defgroup fp
 * @{
 *
 * Type definitions and headers used internally by the FP service implementation.
 *
 * @file fp_defs.h
 *
 * @}
 */

#include <stddef.h>
#include "fp_arch.h"
#include "fp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
***************************************************************************************************
* EXTERNAL DEFINES
***************************************************************************************************
*/

/* Define export macros for shared library builds */
#if defined(_WIN32) || defined(_WIN64)
/* Windows platform */
#ifdef FP_EXPORTS
/* Building the DLL */
#define FP_API __declspec(dllexport)
#else
/* Using the DLL */
#define FP_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && (defined(__linux__) || defined(__unix__))
/* GCC on Linux/Unix platforms */
#define FP_API __attribute__((visibility("default")))
#else
/* Other platforms or static build */
#define FP_API
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

#ifdef __cplusplus
};
#endif

#endif /* FP_DEFS_H */
