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
#if defined(FP_STATIC)
/* Static build (CMake sets FP_STATIC for FP_LIBRARY_TYPE=STATIC): no import or export */
#define FP_API
#elif defined(_WIN32) || defined(_WIN64)
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

/* Mark an API as deprecated; the compiler warns at every call site that is still built */
#if defined(__GNUC__) || defined(__clang__)
#define FP_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#define FP_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define FP_DEPRECATED(msg)
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
