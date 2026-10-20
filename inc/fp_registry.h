/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_REGISTRY_H
#define FP_REGISTRY_H

/**
 * @defgroup fp
 * @{
 *
 * Public interface of the Function Protocol Registry module.
 *
 * @file fp_registry.h
 *
 * @}
 */

#include <stdint.h>
#include <limits.h>
#include "fp_disp.h"

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
/** @brief Identifies the type of code generation for the respective FIDL */
typedef enum
{
    FIDL_TYPE_SERVER,
    FIDL_TYPE_CLIENT,
    FIDL_TYPE_MAX
} fidl_gen_type_t;

/**
 * @brief FIDL function registry entry to enable proper function routing on reception of an FP frame
 */
typedef struct
{
    uint32_t func_id;                                      /**< FIDL function ID */
    void (*p_on_func_cbk)(fp_dispatch_info_t *p_disp_ctx); /**< function pointer to the function which handles @ref func_id */
} fidl_func_t;

/** @brief FIDL type used to index the registry */
typedef uint16_t fidl_index_t;

/**
 * @brief FIDL handler configuration
 */
typedef struct
{
    uint16_t           version;     /**< FIDL version */
    fidl_gen_type_t    gen_type;    /**< Type of code generation - server or client (used by general dispatch functionality to reject invalid requests early) */
    uint16_t           fidl_if_id;  /**< ID of the FIDL interface as specified in the FIDL file */
    uint32_t           func_count;  /**< number of functions in the list */
    const fidl_func_t *p_func_list; /**< pointer to the beginning of the functions list */
} fidl_if_t;

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
/**
 * @brief Initialize the FP registry storage for use.
 *
 * @note The storage is statically allocated by the FP service based on a compile time configuration.
 */
FP_API void fp_registry_init(void);

/**
 * @brief Reset the FIDL registry iterator and acquire the registry lock
 *
 * @retval true The iterator was successfully opened and the registry lock acquired
 * @retval false The registry lock could not be acquired
 *
 * @note If this function returns false, do not call fp_registry_iterator_close()
 */
FP_API bool fp_registry_iterator_open(void);

/** @brief Get the next FIDL entry in the registry
 *  @return next FIDL entry
 */
FP_API const fidl_if_t *fp_registry_iterator_get_next(void);

/** @brief Releases the global iterator lock so that other threads can use it for FIDL
 *  iteration.
 *
 *  @note Must always be called after a call to @ref fp_registry_iterator_open.
 */
FP_API void fp_registry_iterator_close(void);

/** @brief Search for a FIDL interface entry by FIDL interface ID
 *  @param[in] fidl_if_id  ID of the FIDL interface
 *  @param[in] gen_type Type of FIDL interface being searched (e.g. in case of client and server FIDLs available in the same registry)
 */
FP_API const fidl_if_t *fp_registry_get_fidl_if_by_id(const uint16_t fidl_if_id, const fidl_gen_type_t gen_type);

/**
 * @brief Adds a new FIDL interface to the registry and makes it available for dispatch
 *
 * @param p_handler pointer to a FIDL interface descriptor
 * operations
 * @retval true the FIDL interface is registered for use
 * @retval false the FIDL interface registration failed most probably due to lack of space
 * in the registry
 * @note If the FIDL is already registered, the function will do nothing and just return true.
 */
FP_API bool fp_registry_add_fidl_if(const fidl_if_t *const p_handler);

/** @brief Removes a FIDL interface from the registry
 *
 * @param[in] p_handler pointer to a FIDL handler structure to remove
 * @retval true the FIDL registration is removed from the registry
 * @retval false the FIDL was not found in the registry
 */
FP_API bool fp_registry_remove_fidl_if(const fidl_if_t *const p_handler);

/** @brief Searches for a function with a specified function ID inside a FIDL interface descriptor
 *
 *  @param[in] p_fidl pointer to the FIDL interface descriptor
 *  @param[in] target_function_id function ID to search for
 *
 *  @returns FIDL function pointer or NULL if the function ID is not found
 *
 *  @note The function relies on the binary search algorithm to locate the FIDL handler ID and it
 *  requires that the FIDL functions list pointed to by the p_fidl argument is sorted in ascending order.
 *  This step is usually guaranteed by the FIDL code generator but it must be accounted for when binding to
 *  manually written code.
 */
FP_API const fidl_func_t *fp_registry_get_func_by_id(const fidl_if_t *p_fidl, const uint32_t target_function_id);

#ifdef __cplusplus
};
#endif

#endif
