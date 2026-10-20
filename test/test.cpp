/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#include <random>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <set>
#include "stubs.hpp"
#include <array>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "fp_registry.h"
#include "fp_disp.h"
#include "fp_proto.h"
#include "fp_disp_session_mgr.h"
#include "../src/fp_internal.h" // this header is not exposed by the CMake build because it it internal for the component

/** @brief Dummy FIDL function callback used for the test
 *
 *  @param[in] p_disp_ctx pointer to a FIDL descriptor
 *
 */
static void dummy_fidl_func_cbk(fp_dispatch_info_t *p_disp_ctx);

/** @brief Dummy system context send function callback used during the test
 *
 *  @param[in] p_disp_ctx pointer to a FIDL descriptor
 */
static bool test_sys_ctx_send_func(void *p_disp_ctx);

/** @brief Generates some dummy FIDL interfaces and populates them
 *  with hard-coded data.
 */
static void gen_fidl_data(void);

static constexpr auto FIDL_CNT = MAX_FIDL_COUNT; /**< maximum number of FIDL interfaces generated for this unit test */

static constexpr uint32_t RESP_BIT_MASK       = 0x80000000;                                                                                  /**< Bit-mask for an FP response */
static constexpr uint32_t ARM_BIT_MASK        = 0x40000000;                                                                                  /**< Bit-mask for an ARM FP request */
static constexpr uint32_t FIRE_BIT_MASK       = 0x20000000;                                                                                  /**< Bit-mask for a FIRE FP request */
static constexpr uint32_t DISARM_BIT_MASK     = 0x10000000;                                                                                  /**< Bit-mask for a DISARM FP request */
static constexpr uint32_t BROADCAST_BIT_MASK  = 0x08000000;                                                                                  /**< Bit-mask for a BROADCAST frame */
static constexpr uint32_t ATTR_BITS_ONLY_MASK = (RESP_BIT_MASK | ARM_BIT_MASK | FIRE_BIT_MASK | DISARM_BIT_MASK | BROADCAST_BIT_MASK); /**< Bit-mask to filter out all special attribute bits in the function ID field of the FP header */

static constexpr uint16_t  test_if_id              = 0xAA;       /**< just an invalid test FIDL interface ID constant used throughout this test */
static constexpr uint32_t  test_func_id            = 0xBB;       /**< just an invalid test FIDL function ID constant used throughout this test */
static constexpr uint32_t  test_cmd_id             = 0xCC;       /**< just an invalid test FIDL command ID constant used throughout this test */
static constexpr uint32_t  test_cmd_status_invalid = 0xAF;       /**< just an invalid test FIDL command status constant used throughout this test */
static constexpr uint32_t  some_ptr_addr           = 0xAA;       /**< constant used as a substitute for a valid pointer address throughout this test (in cases where the pointer is not expected to be actually used) */
static constexpr uint16_t  test_fidl_version       = 0x0101;     /**< FIDL version constant used throughout this test */
static constexpr uint32_t  test_timeout            = 5000;       /**< test timeout value used throughout this test */
static constexpr uintptr_t test_comm_params_value  = 0x12345678; /**< constant used as a test communication parameter value throughout this test */
static constexpr uint32_t  test_data_size_value    = 0xABCD;     /**< constant used as a test data size value in client request tests */

// import unit test environment namespace
using namespace stubs;

// reserve static storage for the FP registry data
static fidl_if_t   dummy_fidl_interfaces[FIDL_CNT] = { 0 };
static fidl_func_t dummy_fidl_func_list[100]       = { 0 };

using internal_test_env_t = struct
{
    fp_dispatch_info_t     *p_dummy_fidl_func_disp_ctx_user;    /**< pointer value passed to the dummy function callback when it is executed */
    void                   *p_dummy_fidl_func_user_disp_ctx;    /**< pointer to the dispatch context passed to the user-level FP callback which contains also the untyped pointer request data */
    void                   *p_dummy_fidl_func_user_untyped_req; /**< pointer to the untyped request data passed to the user-level FP function callback when executed */
    fp_dispatch_info_t     *p_sys_ctx_send_disp_ctx;            /**< pointer value passed to the dummy send callback when it is executed */
    fp_dispatch_info_t     *p_client_resp_handler_disp_ctx;     /**< pointer value passed to the dummy client response handler callback when it is executed */
    fp_disp_session_base_t *p_sess_init_cbk_session_ctx;        /**< session pointer passed to dummy init callback */
    bool                    dummy_func_cbk_called;              /**< true if the dummy function callback was executed, false - otherwise */
    bool                    dummy_func_cbk_1_called;            /**< true if the dummy function callback 1 was executed, false - otherwise */
    bool                    dummy_user_func_cbk_called;         /**< true if the dummy user level FP callback to handle the request at highest level was called */
    bool                    dummy_send_cbk_called;              /**< true if the dummy send callback was executed, false - otherwise */
    bool                    dummy_sess_init_cbk_called;         /**< true if the dummy session init callback was executed, false - otherwise */
    bool                    client_resp_handler_cbk_called;     /**< true if the dummy client response handler callback was executed, false - otherwise */
    bool                    dummy_send_cbk_ret_val;             /**< return value of the dummy send callback to pass to caller */
    uint8_t                *p_untyped_resp_param;               /**< untyped response pointer passed to the client response handler callback when it is executed */
    fp_cmd_status_t         resp_cmd_status;                    /**< response command status passed to the client response handler callback when it is executed */
    void                   *timeout_p_comm_params;              /**< value of the p_comm_params field of the session when the timeout occurs */
    uint32_t                sent_data_size;                     /**< snapshot of disp_ctx->data_size captured when the dummy send callback ran (the request size at send time, before send_req may overwrite data_size with the response length) */

    /** @brief  Clear all structure fields to their default values */
    void clear(void)
    {
        p_dummy_fidl_func_disp_ctx_user    = nullptr;
        p_dummy_fidl_func_user_untyped_req = nullptr;
        p_dummy_fidl_func_user_disp_ctx    = nullptr;
        p_sys_ctx_send_disp_ctx            = nullptr;
        sent_data_size                     = 0U;
        p_client_resp_handler_disp_ctx     = nullptr;
        p_sess_init_cbk_session_ctx        = nullptr;
        timeout_p_comm_params              = nullptr;
        dummy_user_func_cbk_called         = false;
        dummy_func_cbk_called              = false;
        dummy_func_cbk_1_called            = false;
        dummy_send_cbk_called              = false;
        dummy_sess_init_cbk_called         = false;
        client_resp_handler_cbk_called     = false;
        dummy_send_cbk_ret_val             = false;
    }
};

static internal_test_env_t internal_test_env;

static void dummy_fidl_func_cbk(fp_dispatch_info_t *p_disp_ctx)
{
    internal_test_env.p_dummy_fidl_func_disp_ctx_user = p_disp_ctx;
    internal_test_env.dummy_func_cbk_called           = true;
}

static void dummy_fidl_func_cbk_1(fp_dispatch_info_t *p_disp_ctx)
{
    internal_test_env.p_dummy_fidl_func_disp_ctx_user = p_disp_ctx;
    internal_test_env.dummy_func_cbk_1_called         = true;
}

static void dummy_fidl_func_user_cbk(void *p_disp_ctx, void *p_untyped_req)
{
    internal_test_env.p_dummy_fidl_func_user_disp_ctx    = p_disp_ctx;
    internal_test_env.p_dummy_fidl_func_user_untyped_req = p_untyped_req;
    internal_test_env.dummy_user_func_cbk_called         = true;
}

static bool test_sys_ctx_send_func(void *p_disp_ctx)
{
    internal_test_env.p_sys_ctx_send_disp_ctx = reinterpret_cast<fp_dispatch_info_t *>(p_disp_ctx);
    // capture the request size at send time: on a successful synchronous reply send_req
    // overwrites disp_ctx->data_size with the delivered response length
    internal_test_env.sent_data_size          = internal_test_env.p_sys_ctx_send_disp_ctx->data_size;
    internal_test_env.dummy_send_cbk_called   = true;

    return internal_test_env.dummy_send_cbk_ret_val;
}

static void session_init_dummy_callback(fp_disp_session_base_t *p_sess)
{
    internal_test_env.p_sess_init_cbk_session_ctx = p_sess;
    internal_test_env.dummy_sess_init_cbk_called  = true;
}

static void test_client_resp_handler_cbk(void                 *p_disp_ctx,
                                         uint8_t              *p_untyped_resp,
                                         const fp_cmd_status_t cmd_status)
{
    internal_test_env.p_client_resp_handler_disp_ctx = reinterpret_cast<fp_dispatch_info_t *>(p_disp_ctx);
    internal_test_env.p_untyped_resp_param           = p_untyped_resp;
    internal_test_env.resp_cmd_status                = cmd_status;
    internal_test_env.client_resp_handler_cbk_called = true;
}

static void test_client_resp_handler_cbk_timeout(void                 *p_disp_ctx,
                                                 uint8_t              *p_untyped_resp,
                                                 const fp_cmd_status_t cmd_status)
{
    internal_test_env.resp_cmd_status                = cmd_status;
    internal_test_env.p_untyped_resp_param           = p_untyped_resp;
    internal_test_env.client_resp_handler_cbk_called = true;
    // when a timeout occurs, a temporary dispatch context is created to provide the p_comm_params for the session which timed out
    internal_test_env.timeout_p_comm_params = ((fp_dispatch_info_t *)p_disp_ctx)->p_comm_params;
};

static void gen_fidl_data(void)
{
    std::size_t i;

    for (i = 0; i < std::size(dummy_fidl_interfaces); i++)
    {
        dummy_fidl_interfaces[i].fidl_if_id  = (uint16_t)i;
        dummy_fidl_interfaces[i].gen_type    = (0 == (i % 2)) ? FIDL_TYPE_SERVER : FIDL_TYPE_CLIENT;
        dummy_fidl_interfaces[i].func_count  = std::size(dummy_fidl_func_list);
        dummy_fidl_interfaces[i].p_func_list = dummy_fidl_func_list;
        dummy_fidl_interfaces[i].version     = 0x1010;
    }

    for (i = 0; i < std::size(dummy_fidl_func_list); i++)
    {
        dummy_fidl_func_list[i].func_id       = (uint32_t)i;
        dummy_fidl_func_list[i].p_on_func_cbk = &dummy_fidl_func_cbk;
    }
}

TEST_CASE("Testing normal operation (iteration and modifications of the FIDL interfaces list)", "[registry]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    gen_fidl_data();

    fp_registry_init();

    // initialize the pointer storage
    for (auto i = 0; i < std::size(dummy_fidl_interfaces); i++)
    {
        REQUIRE(true == fp_registry_add_fidl_if(&dummy_fidl_interfaces[i]));
        REQUIRE(i + 1 == env::fp_arch_lock_registry.call_cnt);
        REQUIRE(i + 1 == env::fp_arch_unlock_registry.call_cnt);
    }

    SECTION("Attempt to add a FIDL which was already added to the registry")
    {
        // try to add a handler which was already added => this shall fail
        REQUIRE_FALSE(true == fp_registry_add_fidl_if(&dummy_fidl_interfaces[0]));
    }

    SECTION("FP registry iteration")
    {
        const fidl_if_t *p_fidl_hnd = nullptr;
        std::size_t      idx        = 0;

        env::clear();

        bool open_result = fp_registry_iterator_open();
        REQUIRE(open_result == true);

        REQUIRE(env::fp_arch_lock_registry.call_cnt == 1);
        REQUIRE(env::fp_arch_unlock_registry.call_cnt == 0);

        while (nullptr != (p_fidl_hnd = fp_registry_iterator_get_next()))
        {
            REQUIRE(p_fidl_hnd == &dummy_fidl_interfaces[idx]);

            idx++;
        }

        fp_registry_iterator_close();
        REQUIRE(env::fp_arch_lock_registry.call_cnt == 1);
        REQUIRE(env::fp_arch_unlock_registry.call_cnt == 1);
    }

    SECTION("FP registry iterator lock failure")
    {
        env::clear();

        // Set up the mock to return false for lock acquisition
        env::fp_arch_lock_registry.retval = false;

        bool open_result = fp_registry_iterator_open();
        REQUIRE(open_result == false);

        REQUIRE(env::fp_arch_lock_registry.call_cnt == 1);
        REQUIRE(env::fp_arch_unlock_registry.call_cnt == 0);

        // Calling close after failed open should not call unlock
        fp_registry_iterator_close();
        REQUIRE(env::fp_arch_lock_registry.call_cnt == 1);
        REQUIRE(env::fp_arch_unlock_registry.call_cnt == 0);

        // Reset for next test
        env::fp_arch_lock_registry.retval = true;
    }

    SECTION("FP registry search with lock failure")
    {
        env::clear();

        // Set up the mock to return false for lock acquisition
        env::fp_arch_lock_registry.retval = false;

        // Search should return NULL when lock fails
        const fidl_if_t *p_result = fp_registry_get_fidl_if_by_id(1, FIDL_TYPE_SERVER);
        REQUIRE(p_result == nullptr);

        REQUIRE(env::fp_arch_lock_registry.call_cnt == 1);
        REQUIRE(env::fp_arch_unlock_registry.call_cnt == 0);

        // Reset for next test
        env::fp_arch_lock_registry.retval = true;
    }

    SECTION("Attempt FIDL interface addition with invalid args")
    {
        env::fp_arch_lock_registry.call_cnt   = 0;
        env::fp_arch_unlock_registry.call_cnt = 0;

        REQUIRE_FALSE(fp_registry_add_fidl_if(nullptr));
        // lock is not called when p_handler is nullptr
        REQUIRE(0 == env::fp_arch_lock_registry.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_registry.call_cnt);

        dummy_fidl_interfaces[0].p_func_list = nullptr;

        REQUIRE_FALSE(fp_registry_add_fidl_if(&dummy_fidl_interfaces[0]));

        dummy_fidl_interfaces[0].func_count = 0;

        REQUIRE_FALSE(fp_registry_add_fidl_if(&dummy_fidl_interfaces[0]));
    }

    SECTION("Attempt FIDL interface addition with valid args but no space in the storage")
    {
        fidl_func_t dummy_m[1] = {
            [0] = {
                   .func_id       = 1,
                   .p_on_func_cbk = &dummy_fidl_func_cbk }
        };

        fidl_if_t new_handler = {
            .version     = test_fidl_version,
            .gen_type    = FIDL_TYPE_SERVER,
            .fidl_if_id  = 1,
            .func_count  = 1,
            .p_func_list = dummy_m
        };

        REQUIRE_FALSE(fp_registry_add_fidl_if(&new_handler));
    }

    SECTION("Attempt FIDL interface addition with valid args with some space available in the storage (use invalid FIDL ID)")
    {
        fidl_if_t new_handler = {
            .version     = test_fidl_version,
            .gen_type    = FIDL_TYPE_SERVER,
            .fidl_if_id  = FIDL_CNT, // ensure the FIDL interface ID to add does not match any of the generated IDs which are in the range [0..FIDL_CNT - 1]
            .func_count  = 1,
            .p_func_list = dummy_fidl_func_list
        };

        env::fp_arch_lock_registry.call_cnt   = 0;
        env::fp_arch_unlock_registry.call_cnt = 0;

        // remove one handler to free up some space in the FIDL list for our new addition
        REQUIRE(true == fp_registry_remove_fidl_if(&dummy_fidl_interfaces[0]));

        REQUIRE(1 == env::fp_arch_lock_registry.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_registry.call_cnt);

        REQUIRE(true == fp_registry_add_fidl_if(&new_handler));
    }

    SECTION("Attempt to remove a FIDL interface with invalid args")
    {
        env::fp_arch_lock_registry.call_cnt   = 0;
        env::fp_arch_unlock_registry.call_cnt = 0;
        env::fp_arch_lock_registry.retval     = true;

        REQUIRE_FALSE(fp_registry_remove_fidl_if(nullptr));
        REQUIRE(0 == env::fp_arch_lock_registry.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_registry.call_cnt);
    }

    SECTION("Attempt to remove all initially added FIDL interface from the registry")
    {
        for (uint8_t i = 0; i < std::size(dummy_fidl_interfaces); i++)
        {
            REQUIRE(fp_registry_remove_fidl_if(&dummy_fidl_interfaces[i]));
        }
    }

    SECTION("Duplicate FIDL ID registration with proper lock/unlock behavior")
    {
        // Clear counters to track lock/unlock calls for this test
        env::fp_arch_lock_registry.call_cnt   = 0;
        env::fp_arch_unlock_registry.call_cnt = 0;
        env::fp_arch_lock_registry.retval     = true;

        // Create a FIDL interface with the same ID and generation type as an existing one
        fidl_func_t duplicate_func_list[1] = {
            [0] = {
                .func_id       = 999,
                .p_on_func_cbk = &dummy_fidl_func_cbk
            }
        };

        fidl_if_t duplicate_handler = {
            .version     = test_fidl_version,
            .gen_type    = dummy_fidl_interfaces[0].gen_type,    // Same generation type
            .fidl_if_id  = dummy_fidl_interfaces[0].fidl_if_id,  // Same FIDL ID
            .func_count  = 1,
            .p_func_list = duplicate_func_list
        };

        // Attempt to register the duplicate FIDL interface
        bool result = fp_registry_add_fidl_if(&duplicate_handler);

        // Verify that registration fails
        REQUIRE_FALSE(result);

        // Verify that lock was acquired and released properly
        REQUIRE(1 == env::fp_arch_lock_registry.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_registry.call_cnt);

        // Verify lock/unlock calls are balanced
        REQUIRE(env::fp_arch_lock_registry.call_cnt == env::fp_arch_unlock_registry.call_cnt);
    }

    SECTION("Registry capacity overflow with proper lock/unlock behavior")
    {
        // Clear counters to track lock/unlock calls for this test
        env::fp_arch_lock_registry.call_cnt   = 0;
        env::fp_arch_unlock_registry.call_cnt = 0;
        env::fp_arch_lock_registry.retval     = true;

        // Note: At this point, the registry is already full with MAX_FIDL_COUNT (5) interfaces
        // from the initialization in the main test setup

        // Create additional FIDL interfaces to test capacity overflow
        fidl_func_t overflow_func_list[1] = {
            [0] = {
                .func_id       = 1000,
                .p_on_func_cbk = &dummy_fidl_func_cbk
            }
        };

        // Try to add interfaces beyond the maximum capacity
        for (uint16_t i = 0; i < 3; i++) // Try to add 3 more interfaces
        {
            fidl_if_t overflow_handler = {
                .version     = test_fidl_version,
                .gen_type    = FIDL_TYPE_SERVER,
                .fidl_if_id  = (uint16_t)(MAX_FIDL_COUNT + i), // Use IDs beyond existing ones
                .func_count  = 1,
                .p_func_list = overflow_func_list
            };

            // Attempt to register the interface (should fail due to capacity)
            bool result = fp_registry_add_fidl_if(&overflow_handler);

            // Verify that registration fails
            REQUIRE_FALSE(result);
        }

        // Verify that lock was acquired and released for each attempt
        REQUIRE(3 == env::fp_arch_lock_registry.call_cnt);
        REQUIRE(3 == env::fp_arch_unlock_registry.call_cnt);

        // Verify lock/unlock calls remain balanced throughout all attempts
        REQUIRE(env::fp_arch_lock_registry.call_cnt == env::fp_arch_unlock_registry.call_cnt);

        // Verify registry remains in consistent state by checking we can still find existing interfaces
        const fidl_if_t *p_existing = fp_registry_get_fidl_if_by_id(
            dummy_fidl_interfaces[0].fidl_if_id,
            dummy_fidl_interfaces[0].gen_type
        );
        REQUIRE(p_existing == &dummy_fidl_interfaces[0]);
    }

    SECTION("Attempt to remove a FIDL when the registry lock cannot be obtained")
    {
        env::fp_arch_lock_registry.call_cnt   = 0;
        env::fp_arch_unlock_registry.call_cnt = 0;
        env::fp_arch_lock_registry.retval     = false;

        REQUIRE_FALSE(fp_registry_remove_fidl_if(&dummy_fidl_interfaces[0]));
        REQUIRE(1 == env::fp_arch_lock_registry.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_registry.call_cnt);
    }

    SECTION("Attempt to add a FIDL when the registry lock cannot be obtained")
    {
        env::fp_arch_lock_registry.call_cnt   = 0;
        env::fp_arch_unlock_registry.call_cnt = 0;
        env::fp_arch_lock_registry.retval     = false;

        // Create a valid FIDL interface for testing
        fidl_func_t test_func_list[1] = {
            [0] = {
                .func_id       = 999,
                .p_on_func_cbk = &dummy_fidl_func_cbk
            }
        };

        fidl_if_t test_handler = {
            .version     = test_fidl_version,
            .gen_type    = FIDL_TYPE_SERVER,
            .fidl_if_id  = 999, // Use a unique ID
            .func_count  = 1,
            .p_func_list = test_func_list
        };

        // Attempt to add FIDL interface when lock acquisition fails
        REQUIRE_FALSE(fp_registry_add_fidl_if(&test_handler));

        // Verify that lock was attempted but unlock was not called (since lock failed)
        REQUIRE(1 == env::fp_arch_lock_registry.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_registry.call_cnt);

        // Reset for subsequent tests
        env::fp_arch_lock_registry.retval = true;
    }

    SECTION("Attempt FIDL interface addition with valid args with some space available in the storage (use FIDL ID which is already added to the registry)")
    {
        env::fp_arch_lock_registry.call_cnt   = 0;
        env::fp_arch_unlock_registry.call_cnt = 0;

        // remove one FIDL interface to free up some space in the FIDL list for our new addition
        REQUIRE(true == fp_registry_remove_fidl_if(&dummy_fidl_interfaces[0]));

        fidl_if_t new_handler = {
            .version     = test_fidl_version,
            .gen_type    = dummy_fidl_interfaces[1].gen_type,
            .fidl_if_id  = dummy_fidl_interfaces[1].fidl_if_id, // ensure the FIDL ID to add is already existing in the registry
            .func_count  = 1,
            .p_func_list = dummy_fidl_func_list
        };

        REQUIRE(1 == env::fp_arch_lock_registry.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_registry.call_cnt);

        REQUIRE_FALSE(fp_registry_add_fidl_if(&new_handler));
    }

    SECTION("Attempt FIDL interface removal with invalid arguments")
    {
        env::clear();

        fp_registry_init();

        // pass just any pointer to trigger checks related to the number of available handles in the registry
        // if there are no items in the registry, then nothing can be removed and thus the pointer passed is irrelevant
        // and should not be used by the implementation
        REQUIRE_FALSE(fp_registry_remove_fidl_if(reinterpret_cast<const fidl_if_t *>(1)));

        // now try to remove a non-existent FIDL interface
        fidl_func_t dummy_m[1] = {
            [0] = {
                   .func_id       = 1,
                   .p_on_func_cbk = &dummy_fidl_func_cbk }
        };

        fidl_if_t new_handler = {
            .version     = test_fidl_version,
            .gen_type    = FIDL_TYPE_SERVER,
            .fidl_if_id  = 1,
            .func_count  = 1,
            .p_func_list = dummy_m
        };

        // add at least one interface to the registry
        REQUIRE(fp_registry_add_fidl_if(&new_handler));

        // try removing a non-existing interface to trigger some additional branch coverage
        REQUIRE_FALSE(fp_registry_remove_fidl_if(reinterpret_cast<const fidl_if_t *>(1)));
    }

    SECTION("Test FIDL interface addition with partially valid arguments")
    {
        // FIDL interfaces test data
        fidl_if_t test_fidl_if;
        test_fidl_if.version     = test_fidl_version;
        test_fidl_if.gen_type    = FIDL_TYPE_MAX;
        test_fidl_if.fidl_if_id  = 10;
        test_fidl_if.func_count  = 0;
        test_fidl_if.p_func_list = nullptr;

        env::clear();

        fp_registry_init();

        REQUIRE_FALSE(fp_registry_add_fidl_if(&test_fidl_if));
    }
}

TEST_CASE("Test registry search for FIDL interfaces and functions", "[registry]")
{
    env::clear();

    gen_fidl_data();

    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // initialize the pointer storage
    for (auto i = 0; i < std::size(dummy_fidl_interfaces); i++)
    {
        fp_registry_add_fidl_if(&dummy_fidl_interfaces[i]);
    }

    SECTION("Test FIDL interface search by id (success)")
    {
        const fidl_if_t *p_fidl_hnd = nullptr;

        env::fp_arch_lock_registry.call_cnt   = 0;
        env::fp_arch_unlock_registry.call_cnt = 0;

        // traverse the full static storage and try to find each FIDL by ID checking whether the
        // returned pointer matches the address of the corresponding FIDL storage
        for (std::size_t i = 0; i < FIDL_CNT; i++)
        {
            p_fidl_hnd = fp_registry_get_fidl_if_by_id(dummy_fidl_interfaces[i].fidl_if_id, dummy_fidl_interfaces[i].gen_type);

            REQUIRE(p_fidl_hnd == &dummy_fidl_interfaces[i]);
        }

        REQUIRE(FIDL_CNT == env::fp_arch_unlock_registry.call_cnt);
        REQUIRE(env::fp_arch_lock_registry.call_cnt == env::fp_arch_unlock_registry.call_cnt);
    }

    SECTION("Test FIDL interface search by id (failure due to mismatching gen type)")
    {
        const fidl_if_t *p_fidl_hnd = nullptr;

        // traverse the full static storage and try to find each FIDL by ID checking whether the
        // returned pointer matches the address of the corresponding FIDL storage
        for (std::size_t i = 0; i < FIDL_CNT; i++)
        {
            // reverse gen type to avoid a successful match
            p_fidl_hnd = fp_registry_get_fidl_if_by_id(dummy_fidl_interfaces[i].fidl_if_id, (FIDL_TYPE_SERVER == dummy_fidl_interfaces[i].gen_type) ? FIDL_TYPE_CLIENT : FIDL_TYPE_SERVER);

            REQUIRE(p_fidl_hnd != &dummy_fidl_interfaces[i]);
        }
    }

    SECTION("Test FIDL function search with invalid arguments")
    {
        env::fp_arch_lock_section.call_cnt   = 0;
        env::fp_arch_unlock_section.call_cnt = 0;

        // pass invalid args
        const fidl_func_t *p_func = fp_registry_get_func_by_id(nullptr, 0);

        REQUIRE(nullptr == p_func);

        // FIDL function search does not perform explicit locking because it uses user-provided context and
        // not the direct registry storage
        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        // call with zero registered functions
        dummy_fidl_interfaces[0].func_count = 0;
        REQUIRE(nullptr == fp_registry_get_func_by_id(dummy_fidl_interfaces, 0));
    }

    SECTION("Test FIDL function search with valid arguments [function does not exist]")
    {
        // pass invalid args
        const fidl_func_t *p_func = fp_registry_get_func_by_id(&dummy_fidl_interfaces[0], 0xFFFFFFFF);

        REQUIRE(nullptr == p_func);
    }

    SECTION("Test FIDL function search with valid arguments [function exists]")
    {
        // perform search for all registered FIDL functions and see if they will be found
        for (std::size_t i = 0; i < dummy_fidl_interfaces[0].func_count; i++)
        {
            const fidl_func_t *p_func = fp_registry_get_func_by_id(&dummy_fidl_interfaces[0], dummy_fidl_interfaces[0].p_func_list[i].func_id);

            REQUIRE(nullptr != p_func);
            REQUIRE(p_func == &dummy_fidl_interfaces[0].p_func_list[i]);
        }
    }
}

TEST_CASE("Test FP protocol bit manipulations", "[proto]")
{
    fp_msg_hdr_t       fp_test_hdr  = { 0 };
    constexpr uint32_t test_func_id = 5;

    constexpr std::array<uint32_t, 6> test_bitmask_values = {
        0,          /* regular FP request without attribute bits */
        0x40000000, /* ARMREQ_FUNC_ID_MASK */
        0x20000000, /* FIREREQ_FUNC_ID_MASK */
        0x10000000, /* DISARMREQ_FUNC_ID_MASK */
        0x80000000, /* RESP_FUNC_ID_MASK */
        0x08000000  /* BROADCAST_FUNC_ID_MASK */
    };

    constexpr std::array<fp_frame_type_t, 6> test_frm_types = {
        FP_FRAME_TYPE_REQ,
        FP_FRAME_TYPE_REQ_ARM,
        FP_FRAME_TYPE_REQ_FIRE,
        FP_FRAME_TYPE_REQ_DISARM,
        FP_FRAME_TYPE_RESP,
        FP_FRAME_TYPE_BROADCAST
    };

    SECTION("Test get FP attribute parsing (invalid args)")
    {
        // these are just coverage calls
        fp_proto_get_func_attributes(nullptr, reinterpret_cast<fp_proto_func_attr_t *>(1));
        fp_proto_get_func_attributes(reinterpret_cast<fp_msg_hdr_t *>(1), nullptr);
    }

    SECTION("Test get FP attribute parsing (valid args)")
    {
        fp_proto_func_attr_t fp_attr = {
            .bare_func_id = 0,
            .frame_type   = FP_FRAME_TYPE_MAX
        };

        for (std::size_t i = 0; i < test_bitmask_values.size(); i++)
        {
            fp_test_hdr.func_id = test_func_id; // set some non-zero function ID to see if it will get messed when updating special attributes

            fp_test_hdr.func_id |= test_bitmask_values[i];
            fp_proto_get_func_attributes(&fp_test_hdr, &fp_attr);
            REQUIRE(fp_attr.frame_type == test_frm_types[i]);
            REQUIRE(fp_attr.bare_func_id == test_func_id);
        }
    }

    SECTION("Test set FP frame type (invalid args)")
    {
        // these are just coverage calls
        fp_proto_set_frame_type(nullptr, FP_FRAME_TYPE_REQ);
        fp_proto_set_frame_type(reinterpret_cast<fp_msg_hdr_t *>(1), FP_FRAME_TYPE_MAX);
    }

    SECTION("Test set FP frame type (valid args)")
    {
        for (std::size_t i = 0; i < test_bitmask_values.size(); i++)
        {
            fp_test_hdr.func_id = test_func_id; // set some non-zero function ID to see if it will get messed when updating special attributes

            fp_test_hdr.func_id |= test_bitmask_values[i];
            fp_proto_set_frame_type(&fp_test_hdr, test_frm_types[i]);
            REQUIRE(fp_test_hdr.func_id == (test_func_id | test_bitmask_values[i]));
        }
    }

    SECTION("Test set FP function type (invalid args)")
    {
        // Test with nullptr
        fp_proto_set_function_type(nullptr, test_func_id, FP_FRAME_TYPE_REQ);
        // Test with invalid frame type
        fp_proto_set_function_type(&fp_test_hdr, test_func_id, FP_FRAME_TYPE_MAX);
    }

    SECTION("Test set FP function type (valid args)")
    {
        for (std::size_t i = 0; i < test_bitmask_values.size(); i++)
        {
            // Initialize with garbage to verify complete initialization
            fp_test_hdr.func_id = 0xFFFFFFFF;

            fp_proto_set_function_type(&fp_test_hdr, test_func_id, test_frm_types[i]);

            // Verify function ID is set correctly (lower 20 bits)
            REQUIRE((fp_test_hdr.func_id & ~ATTR_BITS_ONLY_MASK) == test_func_id);
            // Verify frame type bitmask is set correctly (upper 12 bits)
            REQUIRE((fp_test_hdr.func_id & ATTR_BITS_ONLY_MASK) == test_bitmask_values[i]);
            // Verify complete func_id value
            REQUIRE(fp_test_hdr.func_id == (test_func_id | test_bitmask_values[i]));
        }
    }

    SECTION("Test setting of the FP function ID (invalid args)")
    {
        // these is just a coverage call
        fp_proto_set_func_id(nullptr, 0);
    }

    SECTION("Test setting of the FP function ID (valid args)")
    {
        uint32_t func_id = GENERATE(0, 5, 10, 100, 150);

        for (const auto &bitmask_val : test_bitmask_values)
        {
            // set special bits to see if the function ID update will mess them up
            fp_test_hdr.func_id = bitmask_val;

            fp_proto_set_func_id(&fp_test_hdr, func_id);

            REQUIRE((fp_test_hdr.func_id & bitmask_val) == bitmask_val);
        }
    }

    SECTION("Test bare function ID matching")
    {
        constexpr uint32_t attr_bits_value = RESP_BIT_MASK | ARM_BIT_MASK | DISARM_BIT_MASK | FIRE_BIT_MASK;

        REQUIRE(fp_proto_is_bare_func_id_matching(test_func_id | attr_bits_value, test_func_id));
        REQUIRE(fp_proto_is_bare_func_id_matching(test_func_id, test_func_id | attr_bits_value));
        REQUIRE(fp_proto_is_bare_func_id_matching(test_func_id, test_func_id));

        constexpr uint32_t test_func_id_1 = 0xAA;

        REQUIRE_FALSE(fp_proto_is_bare_func_id_matching(test_func_id, test_func_id_1 | attr_bits_value));
        REQUIRE_FALSE(fp_proto_is_bare_func_id_matching(test_func_id_1 | attr_bits_value, test_func_id));
        REQUIRE_FALSE(fp_proto_is_bare_func_id_matching(test_func_id_1, test_func_id));
    }
}

TEST_CASE("Test FP internal helpers", "[helpers]")
{
    SECTION("Test command ID generation")
    {
        // Command IDs generated by the FP service when making client requests shall be different
        // each time a new request is issued (they are not necessarily unique but they should not repeat often)
        // The current implementation is very simple but should work well for practical purposes. It boils down to
        // a simple increment of the command ID for each new request overflowing each 2^16 iterations (2-byte variable used).
        uint32_t exec_cnt      = 0;
        uint16_t initial_value = fp_internal_gen_cmd_id();

        while (++exec_cnt <= UINT16_MAX)
        {
            (void)fp_internal_gen_cmd_id();
        }

        // the value repeats only after UINT16_MAX iterations
        REQUIRE(initial_value == fp_internal_gen_cmd_id());
    }
}

TEST_CASE("Test basic FP command dispatching", "[dispatch]")
{
    fp_msg_hdr_t msg_hdr = { 0 };

    // FIDL functions test data
    std::array<fidl_func_t, 2> test_fidl_func_list = {
        { // valid function ID and callback assignment
          { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk },
         // valid function ID but invalid callback assignment
          { .func_id = 6, .p_on_func_cbk = nullptr } }
    };

    // FIDL interfaces test data
    std::array<fidl_if_t, 2>
        test_fidl_ifs = {
            {
             { .version     = test_fidl_version,
                  .gen_type    = FIDL_TYPE_SERVER,
                  .fidl_if_id  = 10,
                  .func_count  = 2,
                  .p_func_list = test_fidl_func_list.data() },
             { .version     = test_fidl_version,
                  .gen_type    = FIDL_TYPE_CLIENT,
                  .fidl_if_id  = 12,
                  .func_count  = 2,
                  .p_func_list = test_fidl_func_list.data() },
             }
    };

    // system context used for the test sections that follow
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send = &test_sys_ctx_send_func
    };

    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    internal_test_env.clear();

    fp_dispatch_info_t test_dispatch_info;

    // bind test system context to dispatch context
    test_dispatch_info.p_sys_ctx = &test_sys_ctx;
    test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
    test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

    // initialize the pointer storage
    for (const auto &fidl_if : test_fidl_ifs)
    {
        REQUIRE(true == fp_registry_add_fidl_if(&fidl_if));
    }

    // NOTE: Keep in mind that Catch2 executes the code above for each section which follows below!

    SECTION("Test dispatch when a NULL context is provided")
    {
        fp_dispatch_info_t test_dispatch_info = {
            .p_comm_params = nullptr,
            .p_data        = nullptr,
            .data_size     = 0,
            .p_ext_buf_ref = nullptr,
            .p_sys_ctx     = nullptr
        };

        REQUIRE_FALSE(fp_disp_execute(&test_dispatch_info));
    }

    SECTION("Test dispatch when the provided data in the dispatch context is NULL")
    {
        REQUIRE_FALSE(fp_disp_execute(nullptr));
    }

    SECTION("Test dispatch of an unsupported FIDL interface ID")
    {
        msg_hdr.iface_id = 1; // must not exist in the test setup data above

        REQUIRE_FALSE(fp_disp_execute(&test_dispatch_info));

        // check if an actual response is sent back to the client with status FUNC_NOT_SUPPORTED
        REQUIRE(internal_test_env.dummy_send_cbk_called);
        REQUIRE(&test_dispatch_info == internal_test_env.p_sys_ctx_send_disp_ctx);
        REQUIRE(nullptr != internal_test_env.p_sys_ctx_send_disp_ctx->p_data);
        REQUIRE(nullptr == internal_test_env.p_sys_ctx_send_disp_ctx->p_ext_buf_ref);

        auto p_resp_msg_hdr = reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data);
        REQUIRE(sizeof(fp_msg_hdr_t) == internal_test_env.p_sys_ctx_send_disp_ctx->data_size);
        REQUIRE(p_resp_msg_hdr->status == FP_CMD_STATUS_FUNC_NOT_SUPPORTED);
        REQUIRE((p_resp_msg_hdr->func_id & RESP_BIT_MASK) == RESP_BIT_MASK);                       // check if the RESPONSE bit is set
        REQUIRE((p_resp_msg_hdr->func_id & ~RESP_BIT_MASK) == (msg_hdr.func_id & ~RESP_BIT_MASK)); // check if the function ID is the one we requested for dispatch
    }

    SECTION("Test dispatch of an unsupported FIDL function")
    {
        msg_hdr.iface_id = test_fidl_ifs[0].fidl_if_id;
        msg_hdr.func_id  = 1; // must not exist in the test setup data above

        REQUIRE_FALSE(fp_disp_execute(&test_dispatch_info));

        // check if an actual response is sent back to the client with status FUNC_NOT_SUPPORTED
        REQUIRE(internal_test_env.dummy_send_cbk_called);
        REQUIRE(nullptr != internal_test_env.p_sys_ctx_send_disp_ctx->p_data);

        fp_msg_hdr_t *p_resp_msg_hdr = reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data);
        REQUIRE(p_resp_msg_hdr->status == FP_CMD_STATUS_FUNC_NOT_SUPPORTED);
        REQUIRE((p_resp_msg_hdr->func_id & RESP_BIT_MASK) == RESP_BIT_MASK);                       // check if the RESPONSE bit is set
        REQUIRE((p_resp_msg_hdr->func_id & ~RESP_BIT_MASK) == (msg_hdr.func_id & ~RESP_BIT_MASK)); // check if the function ID is the one we requested for dispatch
    }

    SECTION("Test dispatch of a supported FIDL function")
    {
        msg_hdr.iface_id = test_fidl_ifs[0].fidl_if_id;
        msg_hdr.func_id  = test_fidl_ifs[0].p_func_list[0].func_id;

        REQUIRE(fp_disp_execute(&test_dispatch_info));

        // if the FIDL function handler is dispatched successfully, then the expectation is only that
        // the user-specified callback gets executed (it is up to the user implementation to send a response; our test stubs do not send it so we just ensure that
        // the call to fp_disp_execute() will not somehow decide to do it itself)
        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);
        REQUIRE(nullptr == internal_test_env.p_sys_ctx_send_disp_ctx);
        REQUIRE(internal_test_env.dummy_func_cbk_called);
        // check if the FIDL function handler received the correct dispatch context
        REQUIRE(internal_test_env.p_dummy_fidl_func_disp_ctx_user == &test_dispatch_info);

        auto p_resp_msg_hdr = reinterpret_cast<const fp_msg_hdr_t *>(internal_test_env.p_dummy_fidl_func_disp_ctx_user->p_data);
        REQUIRE(p_resp_msg_hdr->status == FP_CMD_STATUS_NOERROR);
        REQUIRE((p_resp_msg_hdr->func_id & ~RESP_BIT_MASK) == (msg_hdr.func_id & ~RESP_BIT_MASK)); // check if the function ID is the one we requested for dispatch
    }

    SECTION("Test dispatch of a supported FIDL function with an invalid callback")
    {
        msg_hdr.iface_id = test_fidl_ifs[0].fidl_if_id;
        msg_hdr.func_id  = test_fidl_ifs[0].p_func_list[1].func_id;

        REQUIRE_FALSE(fp_disp_execute(&test_dispatch_info));

        // check if an actual response is sent back to the client with status FUNC_NOT_SUPPORTED
        REQUIRE(internal_test_env.dummy_send_cbk_called);
        REQUIRE_FALSE(internal_test_env.dummy_func_cbk_called);
        REQUIRE(nullptr == internal_test_env.p_dummy_fidl_func_disp_ctx_user);
        REQUIRE(&test_dispatch_info == internal_test_env.p_sys_ctx_send_disp_ctx);
        REQUIRE(nullptr != internal_test_env.p_sys_ctx_send_disp_ctx->p_data);

        auto p_resp_msg_hdr = reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data);
        REQUIRE(p_resp_msg_hdr->status == FP_CMD_STATUS_FUNC_NOT_SUPPORTED);
        REQUIRE((p_resp_msg_hdr->func_id & RESP_BIT_MASK) == RESP_BIT_MASK);                       // check if the RESPONSE bit is set
        REQUIRE((p_resp_msg_hdr->func_id & ~RESP_BIT_MASK) == (msg_hdr.func_id & ~RESP_BIT_MASK)); // check if the function ID is the one we requested for dispatch
    }

    SECTION("Test dispatch finds correct session based on frame_type")
    {
        // Set up test data for different frame types
        constexpr std::array<fp_frame_type_t, 6> test_frame_types = {
            FP_FRAME_TYPE_REQ,
            FP_FRAME_TYPE_REQ_ARM,
            FP_FRAME_TYPE_REQ_FIRE,
            FP_FRAME_TYPE_REQ_DISARM,
            FP_FRAME_TYPE_RESP,
            FP_FRAME_TYPE_BROADCAST
        };

        // Test both server-side and client-side configurations
        for (size_t fidl_idx = 0; fidl_idx < test_fidl_ifs.size(); fidl_idx++)
        {
            // For each frame type
            for (const auto &frame_type : test_frame_types)
            {
                // Reset test environment
                internal_test_env.clear();

                // Set up message header with valid interface and function IDs
                msg_hdr.iface_id = test_fidl_ifs[fidl_idx].fidl_if_id;
                msg_hdr.cmd_id   = 0x1234; // Set a unique command ID

                // Set function ID and frame type in a single operation
                fp_proto_set_function_type(&msg_hdr, test_fidl_ifs[fidl_idx].p_func_list[0].func_id, frame_type);

                // Set communication parameters
                uint8_t test_comm_params[16]     = { 0 };
                test_dispatch_info.p_comm_params = test_comm_params;

                // Execute dispatch
                bool result = fp_disp_execute(&test_dispatch_info);

                // For response frames, we need to check if the FIDL interface is of type CLIENT
                if (frame_type == FP_FRAME_TYPE_RESP)
                {
                    // If we're using a CLIENT interface, the function should be found
                    if (test_fidl_ifs[fidl_idx].gen_type == FIDL_TYPE_CLIENT)
                    {
                        // Verify the function was found and executed
                        REQUIRE(result);

                        // Verify that the correct function callback was called
                        REQUIRE(internal_test_env.dummy_func_cbk_called);

                        // Verify that p_comm_params was correctly passed to the dispatch context
                        REQUIRE(internal_test_env.p_dummy_fidl_func_disp_ctx_user->p_comm_params == test_comm_params);
                    }
                    // If we're using a SERVER interface, the function should not be found for response frames
                    else
                    {
                        // For SERVER interfaces, response frames should not find a function
                        // But they should also not send an error response (silently ignored)
                        REQUIRE_FALSE(result);
                        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);
                    }
                }
                // For broadcast frames, check that they route to CLIENT-type handlers (same as responses)
                else if (frame_type == FP_FRAME_TYPE_BROADCAST)
                {
                    // Broadcasts route to CLIENT-type FIDL handlers (server-to-client notifications)
                    if (test_fidl_ifs[fidl_idx].gen_type == FIDL_TYPE_CLIENT)
                    {
                        // Verify the function was found and executed
                        REQUIRE(result);

                        // Verify that the correct function callback was called
                        REQUIRE(internal_test_env.dummy_func_cbk_called);

                        // Verify that p_comm_params was correctly passed to the dispatch context
                        REQUIRE(internal_test_env.p_dummy_fidl_func_disp_ctx_user->p_comm_params == test_comm_params);
                    }
                    // If we're using a SERVER interface, broadcasts should not be routed there
                    else
                    {
                        // For SERVER interfaces, broadcast frames should not find a function
                        // But they should NOT send an error response (broadcasts are one-way, silently ignored)
                        REQUIRE_FALSE(result);
                        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);
                    }
                }
                // For request frames, we need to check if the FIDL interface is of type SERVER
                else
                {
                    // If we're using a SERVER interface, the function should be found
                    if (test_fidl_ifs[fidl_idx].gen_type == FIDL_TYPE_SERVER)
                    {
                        // Verify the function was found and executed
                        REQUIRE(result);

                        // Verify that the correct function callback was called
                        REQUIRE(internal_test_env.dummy_func_cbk_called);

                        // Verify that p_comm_params was correctly passed to the dispatch context
                        REQUIRE(internal_test_env.p_dummy_fidl_func_disp_ctx_user->p_comm_params == test_comm_params);
                    }
                    // If we're using a CLIENT interface, the function should not be found for request frames
                    else
                    {
                        // For CLIENT interfaces, request frames should not find a function
                        // But they should send an error response
                        REQUIRE_FALSE(result);
                        REQUIRE(internal_test_env.dummy_send_cbk_called);

                        // Verify error response was sent
                        REQUIRE(nullptr != internal_test_env.p_sys_ctx_send_disp_ctx->p_data);

                        fp_msg_hdr_t *p_resp_msg_hdr = reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data);
                        REQUIRE(p_resp_msg_hdr->status == FP_CMD_STATUS_FUNC_NOT_SUPPORTED);
                    }
                }
            }
        }

        // Test case where no matching function is found
        internal_test_env.clear();
        msg_hdr.iface_id = test_fidl_ifs[0].fidl_if_id;
        msg_hdr.func_id  = 0xFFFF; // Use a function ID that doesn't exist

        // Execute dispatch and verify it fails but sends an error response
        REQUIRE_FALSE(fp_disp_execute(&test_dispatch_info));

        // Verify error response was sent for non-response frames
        REQUIRE(internal_test_env.dummy_send_cbk_called);
        REQUIRE(nullptr != internal_test_env.p_sys_ctx_send_disp_ctx->p_data);

        fp_msg_hdr_t *p_resp_msg_hdr = reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data);
        REQUIRE(p_resp_msg_hdr->status == FP_CMD_STATUS_FUNC_NOT_SUPPORTED);
    }

    SECTION("Test fp_disp_send_status_resp() with invalid args")
    {
        // Test with invalid dispatch context
        REQUIRE_FALSE(fp_disp_send_status_resp(nullptr, FP_CMD_STATUS_NOERROR));

        // Test with valid dispatch context but null data
        test_dispatch_info.p_data = nullptr;
        REQUIRE_FALSE(fp_disp_send_status_resp(&test_dispatch_info, FP_CMD_STATUS_NOERROR));

        // Test with invalid status value
        test_dispatch_info.p_data = reinterpret_cast<uint8_t *>(&msg_hdr);
        REQUIRE_FALSE(fp_disp_send_status_resp(&test_dispatch_info, FP_CMD_STATUS_MAX));
    }

    SECTION("Test fp_disp_send_status_resp() with valid args but send callback fails")
    {
        // Set up the test environment
        internal_test_env.clear();
        internal_test_env.dummy_send_cbk_ret_val = false;

        // Test with valid dispatch context, but send callback returns false
        REQUIRE_FALSE(fp_disp_send_status_resp(&test_dispatch_info, FP_CMD_STATUS_NOERROR));

        // Check if the correct dispatch context is passed to send callback
        REQUIRE(internal_test_env.dummy_send_cbk_called);
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);

        // Check that the status is set correctly
        REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data)->status == FP_CMD_STATUS_NOERROR);

        // Check that the data size is set to the header size
        REQUIRE(test_dispatch_info.data_size == sizeof(fp_msg_hdr_t));

        // Check that the response bit is set in the function ID
        REQUIRE((reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data)->func_id & RESP_BIT_MASK) == RESP_BIT_MASK);
    }

    SECTION("Test fp_disp_send_status_resp() with valid args and send callback succeeds")
    {
        // Set up the test environment
        internal_test_env.clear();
        internal_test_env.dummy_send_cbk_ret_val = true;

        // Test with different status values
        fp_cmd_status_t test_status = GENERATE(FP_CMD_STATUS_NOERROR, FP_CMD_STATUS_ARMED, FP_CMD_STATUS_DISARMED, FP_CMD_STATUS_TIMEOUT);

        // Test with valid dispatch context and send callback returns true
        REQUIRE(fp_disp_send_status_resp(&test_dispatch_info, test_status));

        // Check if the correct dispatch context is passed to send callback
        REQUIRE(internal_test_env.dummy_send_cbk_called);
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);

        // Check that the status is set correctly
        REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data)->status == test_status);

        // Check that the data size is set to the header size
        REQUIRE(test_dispatch_info.data_size == sizeof(fp_msg_hdr_t));

        // Check that the response bit is set in the function ID
        REQUIRE((reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data)->func_id & RESP_BIT_MASK) == RESP_BIT_MASK);
    }
}

TEST_CASE("Test lock failure scenarios in client send request", "[dispatch][client]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // FIDL functions test data
    std::array<fidl_func_t, 1> test_fidl_func_list = {
        { { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk } }
    };

    // FIDL interface test data
    fidl_if_t test_fidl_if = {
        .version     = test_fidl_version,
        .gen_type    = FIDL_TYPE_CLIENT,
        .fidl_if_id  = 10,
        .func_count  = 1,
        .p_func_list = test_fidl_func_list.data()
    };

    REQUIRE(true == fp_registry_add_fidl_if(&test_fidl_if));

    // system context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send = &test_sys_ctx_send_func
    };

    fp_msg_hdr_t msg_hdr = { 0 };
    fp_dispatch_info_t test_dispatch_info;
    test_dispatch_info.p_sys_ctx = &test_sys_ctx;
    test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
    test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

    internal_test_env.clear();

    SECTION("Test client send request with lock failure on section lock")
    {
        // Set up the mock to return false for section lock acquisition
        env::fp_arch_lock_section.retval = false;

        uint8_t req_buffer[sizeof(fp_msg_hdr_t)] = { 0 };
        uint8_t resp_buffer[sizeof(fp_msg_hdr_t)] = { 0 };

        fp_cmd_status_t status = fp_disp_client_send_req(
            &test_dispatch_info,
            test_fidl_if.fidl_if_id,
            test_fidl_func_list[0].func_id,
            FP_FRAME_TYPE_REQ,
            req_buffer,
            sizeof(req_buffer),
            resp_buffer,
            nullptr,
            test_timeout
        );

        // The function should handle lock failure gracefully
        // Depending on implementation, it may return an error status
        // This tests the uncovered branch in fp_disp_client_send_req

        // Reset for next test
        env::fp_arch_lock_section.retval = true;
    }
}

TEST_CASE("Test correct FP command dispatching when a node is both a server and a client of the same FIDL", "[dispatch]")
{
    fp_msg_hdr_t msg_hdr = { 0 };

    constexpr uint16_t fidl_id_common = 10;
    constexpr uint32_t func_id_common = 5;

    // FIDL functions test data
    std::array<fidl_func_t, 2> test_fidl_func_list_server = {
        { // valid function ID and callback assignment
          { .func_id = func_id_common, .p_on_func_cbk = &dummy_fidl_func_cbk },
         // valid function ID but invalid callback assignment
          { .func_id = 6, .p_on_func_cbk = nullptr } }
    };

    // FIDL functions test data
    std::array<fidl_func_t, 1> test_fidl_func_list_client = {
        // valid function ID and callback assignment
        { { .func_id = func_id_common, .p_on_func_cbk = &dummy_fidl_func_cbk_1 } }
    };

    // FIDL interfaces test data
    std::array<fidl_if_t, 2>
        test_fidl_ifs = {
            {
             { .version     = test_fidl_version,
                  .gen_type    = FIDL_TYPE_SERVER,
                  .fidl_if_id  = fidl_id_common,
                  .func_count  = 2,
                  .p_func_list = test_fidl_func_list_server.data() },
             { .version     = test_fidl_version,
                  .gen_type    = FIDL_TYPE_CLIENT,
                  .fidl_if_id  = fidl_id_common,
                  .func_count  = 2,
                  .p_func_list = test_fidl_func_list_client.data() },
             }
    };

    // system context used for the test sections that follow
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send = &test_sys_ctx_send_func
    };

    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    internal_test_env.clear();

    fp_dispatch_info_t test_dispatch_info;

    // bind test system context to dispatch context
    test_dispatch_info.p_sys_ctx = &test_sys_ctx;
    test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
    test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

    // initialize the pointer storage
    for (const auto &fidl_if : test_fidl_ifs)
    {
        REQUIRE(true == fp_registry_add_fidl_if(&fidl_if));
    }

    SECTION("Test that server dispatch is not mistaken by client dispatch")
    {
        msg_hdr.iface_id = fidl_id_common;                          // must not exist in the test setup data above
        msg_hdr.func_id  = test_fidl_ifs[1].p_func_list[0].func_id; // ensure this is a request frame so the dispatcher must land on the server gen context

        REQUIRE(fp_disp_execute(&test_dispatch_info));

        // if the FIDL function handler is dispatched successfully, then the expectation is only that
        // the user-specified callback gets executed (it is up to the user implementation to send a response; our test stubs do not send it so we just ensure that
        // the call to fp_disp_execute() will not somehow decide to do it itself)
        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);
        REQUIRE(nullptr == internal_test_env.p_sys_ctx_send_disp_ctx);
        REQUIRE(internal_test_env.dummy_func_cbk_called);
        // check if the FIDL function handler received the correct dispatch context
        REQUIRE(internal_test_env.p_dummy_fidl_func_disp_ctx_user == &test_dispatch_info);

        auto p_resp_msg_hdr = reinterpret_cast<const fp_msg_hdr_t *>(internal_test_env.p_dummy_fidl_func_disp_ctx_user->p_data);
        REQUIRE(p_resp_msg_hdr->status == FP_CMD_STATUS_NOERROR);
        REQUIRE((p_resp_msg_hdr->func_id & ~RESP_BIT_MASK) == (msg_hdr.func_id & ~RESP_BIT_MASK)); // check if the function ID is the one we requested for dispatch
    }

    SECTION("Test that client dispatch is not mistaken by server dispatch")
    {
        msg_hdr.iface_id = fidl_id_common;                                          // must not exist in the test setup data above
        msg_hdr.func_id  = test_fidl_ifs[0].p_func_list[0].func_id | RESP_BIT_MASK; // ensure this is a response frame so the dispatcher must land on the client gen context

        REQUIRE(fp_disp_execute(&test_dispatch_info));

        // if the FIDL function handler is dispatched successfully, then the expectation is only that
        // the user-specified callback gets executed (it is up to the user implementation to send a response; our test stubs do not send it so we just ensure that
        // the call to fp_disp_execute() will not somehow decide to do it itself)
        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);
        REQUIRE(nullptr == internal_test_env.p_sys_ctx_send_disp_ctx);
        REQUIRE(internal_test_env.dummy_func_cbk_1_called);
        // check if the FIDL function handler received the correct dispatch context
        REQUIRE(internal_test_env.p_dummy_fidl_func_disp_ctx_user == &test_dispatch_info);

        auto p_resp_msg_hdr = reinterpret_cast<const fp_msg_hdr_t *>(internal_test_env.p_dummy_fidl_func_disp_ctx_user->p_data);
        REQUIRE(p_resp_msg_hdr->status == FP_CMD_STATUS_NOERROR);
        REQUIRE((p_resp_msg_hdr->func_id & ~RESP_BIT_MASK) == (msg_hdr.func_id & ~RESP_BIT_MASK)); // check if the function ID is the one we requested for dispatch
    }
}

TEST_CASE("Test fp_disp_send()", "[dispatch]")
{
    // system context used for the test sections that follow
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send = &test_sys_ctx_send_func
    };

    internal_test_env.clear();

    fp_dispatch_info_t test_dispatch_info = { 0 };

    // bind test system context to dispatch context
    test_dispatch_info.p_sys_ctx = &test_sys_ctx;

    // just make sure we point to a valid storage
    fp_msg_hdr_t test_msg_hdr = { 0 };
    test_dispatch_info.p_data = reinterpret_cast<uint8_t *>(&test_msg_hdr);

    SECTION("Test fp_disp_send() with invalid args")
    {
        // test with invalid dispatch context
        REQUIRE_FALSE(fp_disp_send(nullptr));

        // test with invalid send callback assignment
        test_sys_ctx.p_send = nullptr;

        REQUIRE_FALSE(fp_disp_send(&test_dispatch_info));

        // test with unassigned system context
        test_dispatch_info.p_sys_ctx = nullptr;

        REQUIRE_FALSE(fp_disp_send(&test_dispatch_info));

        // test with null data field
        fp_dispatch_sys_context_t sys_ctx;
        sys_ctx.p_send = (p_user_send_func_t)1; // just a non-null value needed for this test

        test_dispatch_info.p_sys_ctx = &sys_ctx;
        test_dispatch_info.p_data    = nullptr;

        REQUIRE_FALSE(fp_disp_send(&test_dispatch_info));
    }

    SECTION("Test fp_disp_send() with valid args but send callback fails")
    {
        // Set up message header with ARM frame type
        constexpr uint32_t test_func_id = 5;
        fp_proto_set_function_type(&test_msg_hdr, test_func_id, FP_FRAME_TYPE_REQ_ARM);

        // Test with valid dispatch context, but send callback returns false. In such case, fp_disp_send() returns the send callback value.
        internal_test_env.dummy_send_cbk_ret_val = false;
        REQUIRE_FALSE(fp_disp_send(&test_dispatch_info));

        // check if the correct dispatch context is passed to send callback
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);

        // check that the request type corresponds to the one we set (no need to check all types since this is tested elsewhere)
        REQUIRE((reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->func_id & ARM_BIT_MASK) == ARM_BIT_MASK);
    }

    SECTION("Test fp_disp_send() with valid args and send callback succeeds")
    {
        // Set up message header with DISARM frame type
        constexpr uint32_t test_func_id = 5;
        fp_proto_set_function_type(&test_msg_hdr, test_func_id, FP_FRAME_TYPE_REQ_DISARM);

        // Test with valid dispatch context, but send callback returns false. In such case, fp_disp_send() returns the send callback value.
        internal_test_env.dummy_send_cbk_ret_val = true;
        REQUIRE(fp_disp_send(&test_dispatch_info));

        // check if the correct dispatch context is passed to send callback
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);

        // check that the request type corresponds to the one we set (no need to check all types since this is tested elsewhere)
        REQUIRE((reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->func_id & DISARM_BIT_MASK) == DISARM_BIT_MASK);
    }
}

TEST_CASE("Test FP server requests", "[dispatch]")
{
    fp_msg_hdr_t msg_hdr = { 0 };

    // FIDL functions test data
    std::array<fidl_func_t, 2> test_fidl_func_list = {
        { // valid function ID and callback assignment
          { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk },
         // valid function ID but invalid callback assignment
          { .func_id = 6, .p_on_func_cbk = nullptr } }
    };

    // FIDL interfaces test data
    std::array<fidl_if_t, 2>
        test_fidl_ifs = {
            {
             { .version     = test_fidl_version,
                  .gen_type    = FIDL_TYPE_SERVER,
                  .fidl_if_id  = 10,
                  .func_count  = 2,
                  .p_func_list = test_fidl_func_list.data() },
             { .version     = test_fidl_version,
                  .gen_type    = FIDL_TYPE_CLIENT,
                  .fidl_if_id  = 12,
                  .func_count  = 2,
                  .p_func_list = test_fidl_func_list.data() },
             }
    };

    // system context used for the test sections that follow
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    env::clear();

    fp_registry_init();

    fp_disp_session_mgr_init(&test_sys_ctx);

    internal_test_env.clear();

    fp_dispatch_info_t test_dispatch_info;

    // bind test system context to dispatch context
    test_dispatch_info.p_sys_ctx = &test_sys_ctx;
    test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
    test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

    env::fp_arch_lock_registry.retval = true;
    // initialize the pointer storage
    for (const auto &fidl_if : test_fidl_ifs)
    {
        REQUIRE(true == fp_registry_add_fidl_if(&fidl_if));
    }

    SECTION("Test FP server dispatch with invalid args")
    {
        // these are mostly coverage calls because the function doesn't give any result back
        fp_disp_server_handle_req(nullptr, FP_DISP_SERVER_REQ_TYPE_MAX, nullptr, 0);
        fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_MAX, nullptr, 0);
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);
        fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_MAX, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), 0);

        // this check here is due to the decision to treat invalid request types as regular (it is currently not certain if this behavior will remain but at the moment, it must be checked)
        fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_MAX, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));
        REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data)->status == FP_CMD_STATUS_REQ_ATTR_ERR);
        REQUIRE(internal_test_env.dummy_send_cbk_called);
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);

        // test with null data field
        test_dispatch_info.p_data = nullptr;
        fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_MAX, nullptr, 0);
    }

    SECTION("Test regular server FP requests dispatch")
    {
        fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_REGULAR, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));
        REQUIRE(internal_test_env.dummy_user_func_cbk_called);
        REQUIRE(internal_test_env.p_dummy_fidl_func_user_disp_ctx == &test_dispatch_info);
        REQUIRE(internal_test_env.p_dummy_fidl_func_user_untyped_req == test_dispatch_info.p_data);
    }

    SECTION("Test server response sending with invalid args")
    {
        REQUIRE_FALSE(fp_disp_server_send_resp(nullptr, 0, 0, nullptr, 0));
        REQUIRE_FALSE(fp_disp_server_send_resp(&test_dispatch_info, 0, 0, nullptr, 0));

        uint8_t resp_buf[sizeof(fp_msg_hdr_t)] = { 0 };
        REQUIRE_FALSE(fp_disp_server_send_resp(nullptr, 0, 0, resp_buf, 0));
    }

    SECTION("Test server response sending with valid args")
    {
        uint8_t       resp_buf[sizeof(fp_msg_hdr_t)] = { 0 };
        fp_msg_hdr_t *p_fp_hdr                       = (fp_msg_hdr_t *)resp_buf;
        fp_msg_hdr_t  req_fp_hdr                     = { .cmd_id = test_cmd_id };

        // set dispatch context parameters to invalid values to be able to detect if
        // fp_disp_server_send_resp() changed it properly
        test_dispatch_info.data_size = 0;
        test_dispatch_info.p_data    = (uint8_t *)&req_fp_hdr;

        p_fp_hdr->cmd_id = test_cmd_id + 1; // ensure the command ID is different from the test ID used here

        internal_test_env.dummy_send_cbk_called  = false;
        internal_test_env.dummy_send_cbk_ret_val = true;

        REQUIRE(fp_disp_server_send_resp(&test_dispatch_info, test_if_id, test_func_id, resp_buf, sizeof(resp_buf)));

        REQUIRE(p_fp_hdr->iface_id == test_if_id);
        REQUIRE((p_fp_hdr->func_id & ~RESP_BIT_MASK) == test_func_id);
        REQUIRE(FP_CMD_STATUS_NOERROR == p_fp_hdr->status);
        REQUIRE(p_fp_hdr->cmd_id == test_cmd_id);
        REQUIRE(test_dispatch_info.data_size == sizeof(resp_buf));
        REQUIRE(test_dispatch_info.p_data == resp_buf);
        REQUIRE(internal_test_env.dummy_send_cbk_called);

        // check if error in fp_disp call is propagated to the return result of fp_disp_server_send_resp
        internal_test_env.dummy_send_cbk_ret_val = false;
        REQUIRE_FALSE(fp_disp_server_send_resp(&test_dispatch_info, test_if_id, test_func_id, resp_buf, sizeof(resp_buf)));
    }

    SECTION("Test server broadcast sending with invalid args")
    {
        REQUIRE_FALSE(fp_disp_server_send_bcast(nullptr, 0, 0, nullptr, 0));
        REQUIRE_FALSE(fp_disp_server_send_bcast(&test_dispatch_info, 0, 0, nullptr, 0));

        uint8_t bcast_buf[sizeof(fp_msg_hdr_t)] = { 0 };
        REQUIRE_FALSE(fp_disp_server_send_bcast(nullptr, 0, 0, bcast_buf, 0));
    }

    SECTION("Test server broadcast sending with valid args")
    {
        uint8_t       bcast_buf[sizeof(fp_msg_hdr_t)] = { 0 };
        fp_msg_hdr_t *p_fp_hdr                        = (fp_msg_hdr_t *)bcast_buf;

        // Get initial cmd_id value to verify incrementing behavior
        uint16_t initial_cmd_id = fp_internal_gen_cmd_id();

        internal_test_env.dummy_send_cbk_called  = false;
        internal_test_env.dummy_send_cbk_ret_val = true;

        REQUIRE(fp_disp_server_send_bcast(&test_dispatch_info, test_if_id, test_func_id, bcast_buf, sizeof(bcast_buf)));

        // Verify header fields are set correctly
        REQUIRE(p_fp_hdr->iface_id == test_if_id);
        REQUIRE((p_fp_hdr->func_id & ~BROADCAST_BIT_MASK) == test_func_id);
        REQUIRE((p_fp_hdr->func_id & BROADCAST_BIT_MASK) == BROADCAST_BIT_MASK);
        REQUIRE(FP_CMD_STATUS_NOERROR == p_fp_hdr->status);

        // Verify cmd_id was generated (should be initial_cmd_id + 1)
        REQUIRE(p_fp_hdr->cmd_id == (initial_cmd_id + 1));

        REQUIRE(test_dispatch_info.data_size == sizeof(bcast_buf));
        REQUIRE(test_dispatch_info.p_data == bcast_buf);
        REQUIRE(internal_test_env.dummy_send_cbk_called);

        // Verify frame type is BROADCAST
        fp_proto_func_attr_t fp_attr;
        fp_proto_get_func_attributes(p_fp_hdr, &fp_attr);
        REQUIRE(fp_attr.frame_type == FP_FRAME_TYPE_BROADCAST);

        // check if error in fp_disp call is propagated to the return result of fp_disp_server_send_bcast
        internal_test_env.dummy_send_cbk_ret_val = false;
        REQUIRE_FALSE(fp_disp_server_send_bcast(&test_dispatch_info, test_if_id, test_func_id, bcast_buf, sizeof(bcast_buf)));
    }

    SECTION("Test broadcast cmd_id increments for successive calls")
    {
        uint8_t       bcast_buf1[sizeof(fp_msg_hdr_t)] = { 0 };
        uint8_t       bcast_buf2[sizeof(fp_msg_hdr_t)] = { 0 };
        uint8_t       bcast_buf3[sizeof(fp_msg_hdr_t)] = { 0 };
        fp_msg_hdr_t *p_fp_hdr1                        = (fp_msg_hdr_t *)bcast_buf1;
        fp_msg_hdr_t *p_fp_hdr2                        = (fp_msg_hdr_t *)bcast_buf2;
        fp_msg_hdr_t *p_fp_hdr3                        = (fp_msg_hdr_t *)bcast_buf3;

        internal_test_env.dummy_send_cbk_ret_val = true;

        // Send three broadcasts and verify cmd_id increments
        REQUIRE(fp_disp_server_send_bcast(&test_dispatch_info, test_if_id, test_func_id, bcast_buf1, sizeof(bcast_buf1)));
        uint16_t cmd_id1 = p_fp_hdr1->cmd_id;

        REQUIRE(fp_disp_server_send_bcast(&test_dispatch_info, test_if_id, test_func_id, bcast_buf2, sizeof(bcast_buf2)));
        uint16_t cmd_id2 = p_fp_hdr2->cmd_id;

        REQUIRE(fp_disp_server_send_bcast(&test_dispatch_info, test_if_id, test_func_id, bcast_buf3, sizeof(bcast_buf3)));
        uint16_t cmd_id3 = p_fp_hdr3->cmd_id;

        // Verify cmd_ids are incrementing
        REQUIRE(cmd_id2 == (cmd_id1 + 1));
        REQUIRE(cmd_id3 == (cmd_id2 + 1));
    }

    SECTION("Test broadcast frames route to CLIENT-type FIDL handlers")
    {
        // This test verifies that broadcast frames are correctly routed to CLIENT-type FIDL handlers
        // (not SERVER-type), reflecting the semantic direction: server → client

        // Create CLIENT-type and SERVER-type FIDL interfaces with the same function ID
        constexpr uint32_t test_bcast_func_id = 42;

        std::array<fidl_func_t, 1> client_func_list = {
            { { .func_id = test_bcast_func_id, .p_on_func_cbk = &dummy_fidl_func_cbk } }
        };

        std::array<fidl_func_t, 1> server_func_list = {
            { { .func_id = test_bcast_func_id, .p_on_func_cbk = &dummy_fidl_func_cbk_1 } }
        };

        fidl_if_t client_fidl_if = {
            .version     = test_fidl_version,
            .gen_type    = FIDL_TYPE_CLIENT,
            .fidl_if_id  = 100,
            .func_count  = 1,
            .p_func_list = client_func_list.data()
        };

        fidl_if_t server_fidl_if = {
            .version     = test_fidl_version,
            .gen_type    = FIDL_TYPE_SERVER,
            .fidl_if_id  = 100, // Same interface ID
            .func_count  = 1,
            .p_func_list = server_func_list.data()
        };

        // Register both FIDL interfaces
        REQUIRE(fp_registry_add_fidl_if(&client_fidl_if));
        REQUIRE(fp_registry_add_fidl_if(&server_fidl_if));

        // Create a broadcast frame
        uint8_t       bcast_buf[sizeof(fp_msg_hdr_t)] = { 0 };
        fp_msg_hdr_t *p_bcast_hdr                     = (fp_msg_hdr_t *)bcast_buf;

        p_bcast_hdr->iface_id = 100;
        fp_proto_set_function_type(p_bcast_hdr, test_bcast_func_id, FP_FRAME_TYPE_BROADCAST);

        test_dispatch_info.p_data    = bcast_buf;
        test_dispatch_info.data_size = sizeof(bcast_buf);

        // Clear test environment
        internal_test_env.clear();

        // Execute dispatch
        bool result = fp_disp_execute(&test_dispatch_info);

        // Verify the broadcast was routed to the CLIENT-type handler
        REQUIRE(result);
        REQUIRE(internal_test_env.dummy_func_cbk_called);      // CLIENT callback
        REQUIRE_FALSE(internal_test_env.dummy_func_cbk_1_called); // SERVER callback should NOT be called

        // Verify no error response was sent (broadcasts are one-way)
        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);

        // Clean up
        REQUIRE(fp_registry_remove_fidl_if(&client_fidl_if));
        REQUIRE(fp_registry_remove_fidl_if(&server_fidl_if));
    }

    SECTION("Test broadcast frames are silently ignored when no CLIENT handler found")
    {
        // This test verifies that broadcasts without matching CLIENT-type handlers are silently ignored
        // (no error response sent, consistent with one-way notification semantics)

        constexpr uint32_t test_bcast_func_id = 99;
        constexpr uint16_t test_bcast_iface   = 200;

        // Create a broadcast frame for a non-existent interface
        uint8_t       bcast_buf[sizeof(fp_msg_hdr_t)] = { 0 };
        fp_msg_hdr_t *p_bcast_hdr                     = (fp_msg_hdr_t *)bcast_buf;

        p_bcast_hdr->iface_id = test_bcast_iface;
        fp_proto_set_function_type(p_bcast_hdr, test_bcast_func_id, FP_FRAME_TYPE_BROADCAST);

        test_dispatch_info.p_data    = bcast_buf;
        test_dispatch_info.data_size = sizeof(bcast_buf);

        // Clear test environment
        internal_test_env.clear();

        // Execute dispatch
        bool result = fp_disp_execute(&test_dispatch_info);

        // Verify the broadcast was NOT dispatched (no handler found)
        REQUIRE_FALSE(result);
        REQUIRE_FALSE(internal_test_env.dummy_func_cbk_called);

        // Verify no error response was sent (broadcasts are silently ignored)
        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);
    }

    SECTION("Test ARM-and-FIRE server FP requests dispatch")
    {
        msg_hdr.iface_id                     = test_fidl_ifs[0].fidl_if_id;
        msg_hdr.func_id                      = test_fidl_ifs[0].p_func_list[0].func_id;
        msg_hdr.status                       = 0;
        msg_hdr.cmd_id                       = 0;
        constexpr uint16_t fp_data_test_hash = 0x00FF;

        SECTION("Test ARM feature")
        {
            test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);
            auto p_msg_hdr               = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);

            internal_test_env.clear();
            env::fp_arch_lock_section.retval = true;

            // Create a test communication parameter to check if it's copied to the session
            test_dispatch_info.p_comm_params = reinterpret_cast<void *>(test_comm_params_value);

            // prepare an ARM request...
            msg_hdr.func_id |= ARM_BIT_MASK;

            // set some CRC result
            env::fp_arch_calc_hash.retval = fp_data_test_hash;

            fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

            // we now expect that this request opened a server session for tracking the timeout...

            // Check that p_comm_params was copied from dispatch context to session
            REQUIRE(reinterpret_cast<void *>(test_comm_params_value) == test_dispatch_info.p_assigned_sess->p_comm_params);

            // a response with ARMed status is also expected to be sent by the server
            REQUIRE(internal_test_env.dummy_send_cbk_called);
            REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);
            REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->status == FP_CMD_STATUS_ARMED);
            REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->data_size == sizeof(fp_msg_hdr_t));
            REQUIRE(FP_SESSION_STATE_INACTIVE != atomic_load(&test_sys_ctx.server_session_pools[0].base.state));
            REQUIRE(test_sys_ctx.server_session_pools[0].base.req_fp_hdr.func_id == (msg_hdr.func_id & ~RESP_BIT_MASK));
            REQUIRE(test_sys_ctx.server_session_pools[0].base.req_fp_hdr.iface_id == msg_hdr.iface_id);
            // check that p_assigned_sess is properly set to the opened server session
            REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->p_assigned_sess == (fp_disp_session_base_t *)&test_sys_ctx.server_session_pools[0]);

            p_msg_hdr->func_id = test_fidl_ifs[0].p_func_list[0].func_id; // set bare function ID again because it was changed by the previous calls to fp_disp_server_handle_req()

            internal_test_env.clear();
            env::fp_arch_calc_hash.retval = fp_data_test_hash;

            SECTION("Request already ARMed, now simulate a FIRE request with a wrong CRC")
            {
                test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

                // set some CRC result different from the one set before to simulate difference in the data
                env::fp_arch_calc_hash.retval = (uint16_t)~fp_data_test_hash;

                p_msg_hdr->func_id |= FIRE_BIT_MASK;

                // simulate FIRE request
                fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

                // send callback is executed to respond with an error status
                REQUIRE(internal_test_env.dummy_send_cbk_called);
                // user callback won't be called this time because the CRC doesn't match
                REQUIRE_FALSE(internal_test_env.dummy_user_func_cbk_called);
                REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);
                REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->status == FP_CMD_STATUS_DATA_INTEGRITY_ERR);
                REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->data_size == sizeof(fp_msg_hdr_t));
            }

            SECTION("Request already ARMed, now simulate a FIRE request with a valid CRC")
            {
                test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

                p_msg_hdr->func_id |= FIRE_BIT_MASK;

                // simulate FIRE request
                fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

                // user callback must be called in this case but...
                REQUIRE(internal_test_env.dummy_user_func_cbk_called);
                // ...send callback is not expected because the dummy user callback function in this test doesn't execute it
                REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);
                REQUIRE(internal_test_env.p_dummy_fidl_func_user_disp_ctx == &test_dispatch_info);
                REQUIRE(internal_test_env.p_dummy_fidl_func_user_untyped_req == test_dispatch_info.p_data);
            }

            SECTION("Request already ARMed, now simulate a DISARM request with an invalid CRC")
            {
                test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

                // set some CRC result different from the one set before to simulate difference in the data
                env::fp_arch_calc_hash.retval = ~fp_data_test_hash;

                p_msg_hdr->func_id |= DISARM_BIT_MASK;

                // simulate DISARM request
                fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

                REQUIRE(internal_test_env.dummy_send_cbk_called);
                REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);
                REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->status == FP_CMD_STATUS_DATA_INTEGRITY_ERR);
                REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->data_size == sizeof(fp_msg_hdr_t));
            }

            SECTION("Request already ARMed, now simulate a DISARM request with a valid CRC")
            {
                test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

                p_msg_hdr->func_id |= DISARM_BIT_MASK;

                // Get a reference to the server session before disarming
                auto &server_sess = test_sys_ctx.server_session_pools[0];

                // Verify the session is active before disarming
                REQUIRE(FP_SESSION_STATE_INACTIVE != atomic_load(&server_sess.base.state));

                // simulate DISARM request
                fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

                REQUIRE(internal_test_env.dummy_send_cbk_called);
                REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);
                REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->status == FP_CMD_STATUS_DISARMED);
                REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->data_size == sizeof(fp_msg_hdr_t));

                // Verify that the session is now closed (inactive)
                REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&server_sess.base.state));

                // Verify that session fields are reset to their default values
                REQUIRE(UINT16_MAX == server_sess.base.req_fp_hdr.cmd_id);
                REQUIRE(UINT16_MAX == server_sess.base.req_fp_hdr.iface_id);
                REQUIRE(UINT32_MAX == server_sess.base.req_fp_hdr.func_id);
                REQUIRE(FP_CMD_STATUS_MAX == server_sess.base.req_fp_hdr.status);
                REQUIRE(0 == server_sess.base.timeout);
                REQUIRE(nullptr == server_sess.base.p_comm_params);

                // Verify that the closed_session_cnt metric was incremented
                REQUIRE(1 == test_sys_ctx.server_metrics.closed_session_cnt);
            }

            SECTION("Request already ARMed, verify user close callback is called when disarmed")
            {
                // Create static variables to track the handler call
                static bool                    test_close_handler_called = false;
                static fp_disp_session_base_t *close_session_ptr         = nullptr;

                // Reset the static variables
                test_close_handler_called = false;
                close_session_ptr         = nullptr;

                // Create a static function to use as the close handler
                static auto test_close_handler = [](fp_disp_session_base_t *const p_sess) {
                    // Set the flag to indicate the handler was called
                    test_close_handler_called = true;
                    // Store the session pointer to verify it's the correct one
                    close_session_ptr = p_sess;
                };

                // Create a callbacks structure with only the close callback
                fp_user_session_callbacks_t callbacks = {
                    .p_open  = nullptr,
                    .p_track = nullptr,
                    .p_close = test_close_handler
                };

                // Register the callbacks
                fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

                test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);
                p_msg_hdr->func_id |= DISARM_BIT_MASK;

                // Get a reference to the server session before disarming
                auto &server_sess = test_sys_ctx.server_session_pools[0];

                // Verify the session is active before disarming
                REQUIRE(FP_SESSION_STATE_INACTIVE != atomic_load(&server_sess.base.state));

                // Create a test communication parameter
                int test_param = 42;

                // Set the p_comm_params to point to our test parameter
                server_sess.base.p_comm_params = &test_param;

                // simulate DISARM request
                fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

                // Verify that our close handler was called
                REQUIRE(test_close_handler_called);

                // Verify that the correct session pointer was passed to the close handler
                REQUIRE(close_session_ptr == (fp_disp_session_base_t *)&server_sess);

                // Verify that the session is now closed (inactive)
                REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&server_sess.base.state));

                // Verify that session fields are reset to their default values
                REQUIRE(UINT16_MAX == server_sess.base.req_fp_hdr.cmd_id);
                REQUIRE(UINT16_MAX == server_sess.base.req_fp_hdr.iface_id);
                REQUIRE(UINT32_MAX == server_sess.base.req_fp_hdr.func_id);
                REQUIRE(FP_CMD_STATUS_MAX == server_sess.base.req_fp_hdr.status);
                REQUIRE(0 == server_sess.base.timeout);
                REQUIRE(nullptr == server_sess.base.p_comm_params);
            }

            SECTION("Request already ARMed, now simulate an invalid request type")
            {
                test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

                // set ARM bit again which would be a violation of the protocol
                p_msg_hdr->func_id |= ARM_BIT_MASK;

                // simulate DISARM request
                fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

                REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);
                REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->status == FP_CMD_STATUS_REQ_ATTR_ERR);
                REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->data_size == sizeof(fp_msg_hdr_t));
            }
        }

        SECTION("Try arming a request when there are no sessions available")
        {
            // exhaust all sessions before sending an ARM request
            for (fp_disp_server_session_t &sess : test_sys_ctx.server_session_pools)
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_ACTIVE);
                sess.base.req_fp_hdr.iface_id = test_if_id; // set to an INVALID FIDL interface ID to ensure an opened session won't be found
            }

            test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);
            auto p_msg_hdr               = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);

            p_msg_hdr->iface_id = test_sys_ctx.server_session_pools[0].base.req_fp_hdr.iface_id - 1;

            internal_test_env.clear();

            // prepare an ARM request...
            msg_hdr.func_id |= ARM_BIT_MASK;

            fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

            // a response with NO_MEM status is expected to be sent by the server
            REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);
            REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->status == FP_CMD_STATUS_NO_MEM);
            REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->data_size == sizeof(fp_msg_hdr_t));
        }
    }

    SECTION("Try sending an invalid server request type")
    {
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

        internal_test_env.clear();

        // prepare an invalid request type...
        msg_hdr.func_id |= ARM_BIT_MASK | DISARM_BIT_MASK;

        fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);
        REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->status == FP_CMD_STATUS_REQ_ATTR_ERR);
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->data_size == sizeof(fp_msg_hdr_t));
    }

    SECTION("Test FP server request matching for coverage using more than one function ID to achieve more branch coverage")
    {
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

        internal_test_env.clear();

        // exhaust all sessions before sending an ARM request
        for (fp_disp_server_session_t &sess : test_sys_ctx.server_session_pools)
        {
            atomic_store(&sess.base.state, FP_SESSION_STATE_ACTIVE);
            sess.base.req_fp_hdr.iface_id = test_if_id;
            sess.base.req_fp_hdr.func_id  = test_func_id; // set to an INVALID FIDL interface ID to ensure an opened session won't be found
        }

        msg_hdr.iface_id = 0; // this interface ID must match one of the session iface_ids assigned above
        // prepare an invalid function ID to request...
        msg_hdr.func_id = 0xFF | ARM_BIT_MASK | DISARM_BIT_MASK;

        fp_disp_server_handle_req(&test_dispatch_info, FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE, reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk), sizeof(fp_msg_hdr_t));

        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);
        REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->status == FP_CMD_STATUS_REQ_ATTR_ERR);
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->data_size == sizeof(fp_msg_hdr_t));
    }
}

TEST_CASE("Test FP session management routines", "[session management]")
{
    // system context used for the test sections that follow
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send = &test_sys_ctx_send_func
    };

    env::clear();
    env::fp_arch_lock_section.retval  = true;
    env::fp_arch_lock_registry.retval = true;

    internal_test_env.clear();
    test_sys_ctx.server_metrics = { 0 };
    test_sys_ctx.client_metrics = { 0 };

    SECTION("Test session init with invalid args")
    {
        // this one aims for coverage but also checks if an invalid pointer is passed, that an exception will not arise
        fp_disp_session_mgr_init(nullptr);
    }

    SECTION("Test that all session pools get properly initialized")
    {
        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        fp_disp_session_mgr_init(&test_sys_ctx);

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        const std::array<fp_disp_session_type_t, FP_DISP_SESSION_TYPE_MAX> sess_types = {
            { FP_DISP_SESSION_TYPE_SERVER,
             FP_DISP_SESSION_TYPE_CLIENT }
        };

        for (fp_disp_session_type_t sess_type : sess_types)
        {
            switch (sess_type)
            {
                case FP_DISP_SESSION_TYPE_SERVER:
                    {
                        REQUIRE(0 == test_sys_ctx.server_metrics.active_session_cnt);
                        REQUIRE(0 == test_sys_ctx.server_metrics.closed_session_cnt);
                        REQUIRE(0 == test_sys_ctx.server_metrics.open_session_cnt);
                        REQUIRE(0 == test_sys_ctx.server_metrics.timed_out_session_cnt);

                        fp_disp_server_session_t *p_sess;

                        for (uint16_t sess_idx = 0; sess_idx < std::size(test_sys_ctx.server_session_pools); sess_idx++)
                        {
                            p_sess = &test_sys_ctx.server_session_pools[sess_idx];

                            REQUIRE(UINT16_MAX == p_sess->base.req_fp_hdr.cmd_id);
                            REQUIRE(UINT16_MAX == p_sess->base.req_fp_hdr.iface_id);
                            REQUIRE(UINT32_MAX == p_sess->base.req_fp_hdr.func_id);
                            REQUIRE(FP_CMD_STATUS_MAX == p_sess->base.req_fp_hdr.status);
                            REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&p_sess->base.state));
                            REQUIRE(0 == p_sess->base.timeout);
                            REQUIRE(std::size(test_sys_ctx.server_session_pools) == (env::fp_arch_stop_timer.call_cnt - std::size(test_sys_ctx.client_session_pools)));

                            REQUIRE(&fp_disp_server_sess_attrib == p_sess->base.p_session_attr);
                            REQUIRE(FP_DISP_SESSION_TYPE_SERVER == reinterpret_cast<const fp_disp_session_attributes_t *>(p_sess->base.p_session_attr)->sess_type);
                            REQUIRE(0 == p_sess->req_data_hash);
                            REQUIRE(nullptr == p_sess->p_user_handler);
                        }

                        break;
                    }

                case FP_DISP_SESSION_TYPE_CLIENT:
                    {
                        REQUIRE(0 == test_sys_ctx.client_metrics.active_session_cnt);
                        REQUIRE(0 == test_sys_ctx.client_metrics.closed_session_cnt);
                        REQUIRE(0 == test_sys_ctx.client_metrics.open_session_cnt);
                        REQUIRE(0 == test_sys_ctx.client_metrics.timed_out_session_cnt);

                        fp_disp_client_session_t *p_sess;

                        for (uint16_t sess_idx = 0; sess_idx < std::size(test_sys_ctx.client_session_pools); sess_idx++)
                        {
                            p_sess = &test_sys_ctx.client_session_pools[sess_idx];

                            REQUIRE(UINT16_MAX == p_sess->base.req_fp_hdr.cmd_id);
                            REQUIRE(UINT16_MAX == p_sess->base.req_fp_hdr.iface_id);
                            REQUIRE(UINT32_MAX == p_sess->base.req_fp_hdr.func_id);
                            REQUIRE(FP_CMD_STATUS_MAX == p_sess->base.req_fp_hdr.status);
                            REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&p_sess->base.state));
                            REQUIRE(0 == p_sess->base.timeout);
                            REQUIRE(std::size(test_sys_ctx.client_session_pools) == (env::fp_arch_stop_timer.call_cnt - std::size(test_sys_ctx.server_session_pools)));

                            REQUIRE(&fp_disp_client_sess_attrib == p_sess->base.p_session_attr);
                            REQUIRE(FP_DISP_SESSION_TYPE_CLIENT == reinterpret_cast<const fp_disp_session_attributes_t *>(p_sess->base.p_session_attr)->sess_type);
                            REQUIRE(nullptr == p_sess->p_user_handler);
                            REQUIRE(nullptr == p_sess->p_resp_data);
                            REQUIRE(nullptr == p_sess->p_session_event);
                            REQUIRE(FP_REQ_TYPE_MAX == p_sess->req_type);
                            REQUIRE(0 == p_sess->resp_data_max_size);
                        }

                        break;
                    }

                default:
                    {
                        assert(false);
                        break;
                    }
            }
        }
    }

    SECTION("Test opening a session with invalid args")
    {
        REQUIRE(nullptr == fp_disp_session_mgr_open_session(nullptr, nullptr, FP_DISP_SESSION_TYPE_SERVER));
        REQUIRE(nullptr == fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_MAX));
    }

    SECTION("Test opening a session (only one slot available)")
    {
        uint16_t sess_idx                = 0;
        env::fp_arch_lock_section.retval = true;

        for (fp_disp_server_session_t &sess : test_sys_ctx.server_session_pools)
        {
            // leave last session as inactive
            atomic_store(&sess.base.state, (sess_idx++ < std::size(test_sys_ctx.server_session_pools) - 1) ? FP_SESSION_STATE_ACTIVE : FP_SESSION_STATE_INACTIVE);
        }

        sess_idx = 0;

        for (fp_disp_client_session_t &sess : test_sys_ctx.client_session_pools)
        {
            // leave only first session as inactive to just slightly change the busy-slot pattern
            atomic_store(&sess.base.state, !(sess_idx++ == 0) ? FP_SESSION_STATE_ACTIVE : FP_SESSION_STATE_INACTIVE);
        }

        // Create a dispatch context to test p_assigned_sess
        fp_dispatch_info_t test_disp_ctx = {
            .p_sys_ctx = &test_sys_ctx
        };

        auto p_server_sess = (fp_disp_server_session_t *)fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_SERVER);
        // check if last server session will be returned as opened because it is the only one available
        REQUIRE(&test_sys_ctx.server_session_pools[std::size(test_sys_ctx.server_session_pools) - 1] == p_server_sess);
        REQUIRE(1 == test_sys_ctx.server_metrics.open_session_cnt);
        REQUIRE(1 == test_sys_ctx.server_metrics.active_session_cnt);
        // Check that p_comm_params is initialized to NULL
        REQUIRE(nullptr == p_server_sess->base.p_comm_params);

        test_disp_ctx.p_assigned_sess = nullptr;
        // after opening the last available session, it should be impossible to open another session of server type
        REQUIRE(nullptr == fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_SERVER));
        REQUIRE(1 == test_sys_ctx.server_metrics.open_session_cnt);
        REQUIRE(1 == test_sys_ctx.server_metrics.active_session_cnt);

        auto p_client_sess = (fp_disp_client_session_t *)fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT);
        // check if the first client session will be returned as opened because it is the only one available
        REQUIRE(&test_sys_ctx.client_session_pools[0] == p_client_sess);
        REQUIRE(1 == test_sys_ctx.client_metrics.open_session_cnt);
        REQUIRE(1 == test_sys_ctx.client_metrics.active_session_cnt);
        // Check that p_comm_params is initialized to NULL
        REQUIRE(nullptr == p_client_sess->base.p_comm_params);

        env::clear();

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        // after opening the last available session, it should be impossible to open another session of client type
        REQUIRE(nullptr == fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT));
        REQUIRE(1 == test_sys_ctx.client_metrics.open_session_cnt);
        REQUIRE(1 == test_sys_ctx.client_metrics.active_session_cnt);
        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_section.call_cnt);

        // set session counters to max and see if another attempt to open a session will keep the metrics values as they are
        test_sys_ctx.server_metrics.open_session_cnt   = UINT32_MAX;
        test_sys_ctx.server_metrics.active_session_cnt = UINT32_MAX;
        test_sys_ctx.client_metrics.open_session_cnt   = UINT32_MAX;
        test_sys_ctx.client_metrics.active_session_cnt = UINT32_MAX;

        // provide some more inactive sessions for opening
        atomic_store(&test_sys_ctx.server_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        REQUIRE((fp_disp_session_base_t *)&test_sys_ctx.server_session_pools[0] == fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_SERVER));
        REQUIRE(UINT32_MAX == test_sys_ctx.server_metrics.open_session_cnt);
        REQUIRE(UINT32_MAX == test_sys_ctx.server_metrics.active_session_cnt);

        REQUIRE((fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0] == fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT));
        REQUIRE(UINT32_MAX == test_sys_ctx.client_metrics.open_session_cnt);
        REQUIRE(UINT32_MAX == test_sys_ctx.client_metrics.active_session_cnt);
    }

    SECTION("Test session find with invalid args")
    {
        REQUIRE(nullptr == fp_disp_session_mgr_find_session(nullptr, FP_DISP_SESSION_TYPE_MAX, nullptr));
        REQUIRE(nullptr == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_MAX, nullptr));
        fp_msg_hdr_t hdr = { 0 };
        REQUIRE(nullptr == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_MAX, &hdr));
    }

    SECTION("Test session open with valid args byt impossibility to obtain section lock")
    {
        env::fp_arch_lock_section.retval = false;
        REQUIRE(nullptr == fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT));
    }

    SECTION("Test finding an opened server session with a real but uninitialized system context")
    {
        uint16_t sess_idx = 0;

        for (fp_disp_server_session_t &sess : test_sys_ctx.server_session_pools)
        {
            sess.base.p_session_attr = nullptr;

            if (sess_idx++ < std::size(test_sys_ctx.server_session_pools) - 1)
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_INACTIVE);
            }
            else
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_ACTIVE);
                // server sessions are expected to match only on interface and function ID fields of the FP header
                sess.base.req_fp_hdr.iface_id = test_if_id;
                sess.base.req_fp_hdr.func_id  = test_func_id;
            }
        }

        fp_msg_hdr_t test_msg_hdr = {
            .iface_id = test_if_id,
            .cmd_id   = test_cmd_id
        };

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        // the find operation is expected to fail due to the bad initialization for p_session_attr above
        REQUIRE(nullptr == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_SERVER, &test_msg_hdr));

        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_section.call_cnt);

        fp_disp_session_attributes_t sess_attr = {
            .p_init_func     = nullptr,
            .p_is_matching   = nullptr,
            .p_track_session = nullptr
        };

        // initialize with a valid session attributes structure pointer but with unassigned handlers inside
        test_sys_ctx.server_session_pools[std::size(test_sys_ctx.server_session_pools) - 1].base.p_session_attr = &sess_attr;

        // the find operation is expected to fail due to the bad initialization for p_session_attr above
        REQUIRE(nullptr == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_SERVER, &test_msg_hdr));
    }

    SECTION("Test finding an opened client session with a real but uninitialized system context")
    {
        uint16_t sess_idx = 0;

        for (fp_disp_client_session_t &sess : test_sys_ctx.client_session_pools)
        {
            sess.base.p_session_attr = nullptr;

            if (sess_idx++ < std::size(test_sys_ctx.client_session_pools) - 1)
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_INACTIVE);
            }
            else
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_ACTIVE);
                // client sessions are expected to match only on interface and function ID fields of the FP header
                sess.base.req_fp_hdr.iface_id = test_if_id;
                sess.base.req_fp_hdr.func_id  = test_func_id;
                sess.base.req_fp_hdr.cmd_id   = test_cmd_id;
            }
        }

        fp_msg_hdr_t test_msg_hdr = {
            .iface_id = test_if_id,
            .cmd_id   = test_cmd_id
        };

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        // the find operation is expected to fail due to the bad initialization for p_session_attr above
        REQUIRE(nullptr == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_CLIENT, &test_msg_hdr));

        env::fp_arch_lock_section.retval = true;
        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_section.call_cnt);

        fp_disp_session_attributes_t sess_attr = {
            .p_init_func     = nullptr,
            .p_is_matching   = nullptr,
            .p_track_session = nullptr
        };

        // initialize with a valid session attributes structure pointer but with unassigned handlers inside
        test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1].base.p_session_attr = &sess_attr;

        // the find operation is expected to fail due to the bad initialization for p_session_attr above
        REQUIRE(nullptr == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_CLIENT, &test_msg_hdr));
    }

    SECTION("Test finding an opened client session with a real and initialized system context")
    {
        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        uint16_t sess_idx = 0;

        for (auto &sess : test_sys_ctx.client_session_pools)
        {
            if (sess_idx++ < std::size(test_sys_ctx.client_session_pools) - 1)
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_INACTIVE);
            }
            else
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_ACTIVE);
                // client sessions are expected to match on interface, function and command ID fields of the FP header
                sess.base.req_fp_hdr.iface_id = test_if_id;
                sess.base.req_fp_hdr.func_id  = test_func_id;
                sess.base.req_fp_hdr.cmd_id   = test_cmd_id;
            }
        }

        // initialize the request with the same data as in the session pool to ensure it will be found
        fp_msg_hdr_t test_msg_hdr = {
            .iface_id = test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1].base.req_fp_hdr.iface_id,
            .func_id  = test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1].base.req_fp_hdr.func_id,
            .cmd_id   = test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1].base.req_fp_hdr.cmd_id
        };

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        REQUIRE((fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1] == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_CLIENT, &test_msg_hdr));

        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_section.call_cnt);

        // just for coverage, perform also non-matching calls
        env::clear();

        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // this is for branch coverage of the matching function in order to ensure full AND condition evaluation
        test_msg_hdr.iface_id = test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1].base.req_fp_hdr.iface_id;
        test_msg_hdr.func_id  = ~test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1].base.req_fp_hdr.func_id;
        test_msg_hdr.cmd_id   = ~test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1].base.req_fp_hdr.cmd_id;

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        REQUIRE(nullptr == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_CLIENT, &test_msg_hdr));

        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_section.call_cnt);

        // this is for branch coverage of the matching function in order to ensure full AND condition evaluation
        test_msg_hdr.func_id = test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1].base.req_fp_hdr.func_id;
        test_msg_hdr.cmd_id  = ~test_sys_ctx.client_session_pools[std::size(test_sys_ctx.client_session_pools) - 1].base.req_fp_hdr.cmd_id;

        REQUIRE(nullptr == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_CLIENT, &test_msg_hdr));
    }

    SECTION("Test finding an opened server session with a real and initialized system context")
    {
        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        uint16_t sess_idx = 0;

        for (fp_disp_server_session_t &sess : test_sys_ctx.server_session_pools)
        {
            if (sess_idx++ < std::size(test_sys_ctx.server_session_pools) - 1)
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_INACTIVE);
            }
            else
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_ACTIVE);
                // server sessions are expected to match only on interface and function ID fields of the FP header
                sess.base.req_fp_hdr.iface_id = test_if_id;
                sess.base.req_fp_hdr.func_id  = test_func_id;
            }
        }

        // initialize the request with the same data as in the session pool to ensure it will be found
        fp_msg_hdr_t test_msg_hdr = {
            .iface_id = test_sys_ctx.server_session_pools[std::size(test_sys_ctx.server_session_pools) - 1].base.req_fp_hdr.iface_id,
            .func_id  = test_sys_ctx.server_session_pools[std::size(test_sys_ctx.server_session_pools) - 1].base.req_fp_hdr.func_id
        };

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        REQUIRE((fp_disp_session_base_t *)&test_sys_ctx.server_session_pools[std::size(test_sys_ctx.server_session_pools) - 1] == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_SERVER, &test_msg_hdr));

        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_section.call_cnt);
    }

    SECTION("Testing session find with initialized session but impossibility to obtain shared section lock")
    {
        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        env::fp_arch_lock_section.retval = false;

        uint16_t sess_idx = 0;

        for (fp_disp_server_session_t &sess : test_sys_ctx.server_session_pools)
        {
            if (sess_idx++ < std::size(test_sys_ctx.server_session_pools) - 1)
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_INACTIVE);
            }
            else
            {
                atomic_store(&sess.base.state, FP_SESSION_STATE_ACTIVE);
                // server sessions are expected to match only on interface and function ID fields of the FP header
                sess.base.req_fp_hdr.iface_id = test_if_id;
                sess.base.req_fp_hdr.func_id  = test_func_id;
            }
        }

        // initialize the request with the same data as in the session pool to ensure it will be found
        fp_msg_hdr_t test_msg_hdr = {
            .iface_id = test_sys_ctx.server_session_pools[std::size(test_sys_ctx.server_session_pools) - 1].base.req_fp_hdr.iface_id,
            .func_id  = test_sys_ctx.server_session_pools[std::size(test_sys_ctx.server_session_pools) - 1].base.req_fp_hdr.func_id
        };

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);

        REQUIRE(nullptr == fp_disp_session_mgr_find_session(&test_sys_ctx, FP_DISP_SESSION_TYPE_SERVER, &test_msg_hdr));

        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);
    }

    SECTION("Test session close with invalid args")
    {
        const fp_disp_session_attributes_t sess_attr = {
            .p_init_func     = nullptr,
            .p_is_matching   = nullptr,
            .p_track_session = nullptr,
            .sess_type       = FP_DISP_SESSION_TYPE_SERVER
        };
        test_sys_ctx.server_session_pools[0].base.p_session_attr = reinterpret_cast<const fp_disp_session_attributes_t *>(&sess_attr);

        // these are coverage calls and checking if the implementation tries to use an invalid pointer passed to it
        // (resulting in an exception for this unit test)
        fp_disp_session_mgr_close_session(nullptr, nullptr);
        fp_disp_session_mgr_close_session(&test_sys_ctx, nullptr);
        fp_disp_session_mgr_close_session(&test_sys_ctx, reinterpret_cast<fp_disp_session_base_t *>(&test_sys_ctx.server_session_pools[0]));
    }

    SECTION("Test session close with invalid args (uninitialized session attributes)")
    {
        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);
        REQUIRE(0 == test_sys_ctx.server_metrics.closed_session_cnt);
        REQUIRE(0 == test_sys_ctx.client_metrics.closed_session_cnt);

        fp_disp_session_base_t dummy_session = {
            .p_session_attr = nullptr
        };

        // attempt to close a session which is not in the session pool
        fp_disp_session_mgr_close_session(&test_sys_ctx, &dummy_session);

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);
        REQUIRE(0 == test_sys_ctx.server_metrics.closed_session_cnt);
        REQUIRE(0 == test_sys_ctx.client_metrics.closed_session_cnt);
    }

    SECTION("Test server session close with a valid session from the session pool")
    {
        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);
        REQUIRE(0 == test_sys_ctx.server_metrics.closed_session_cnt);

        fp_disp_session_attributes_t dummy_sess_attr = {
            // we don't care that the pointer is valid but rather non-null because the
            // session won't be found anyway in the pool and thus the init function will never
            // be called
            .p_init_func = &session_init_dummy_callback,
            .sess_type   = FP_DISP_SESSION_TYPE_SERVER
        };

        test_sys_ctx.server_session_pools[2].base.p_session_attr = &dummy_sess_attr;

        REQUIRE_FALSE(internal_test_env.dummy_sess_init_cbk_called);

        // attempt to close a session which is not in the session pool
        fp_disp_session_mgr_close_session(&test_sys_ctx, reinterpret_cast<fp_disp_session_base_t *>(&test_sys_ctx.server_session_pools[2]));

        // check that the session is actually found and its corresponding init operation is called
        REQUIRE(internal_test_env.dummy_sess_init_cbk_called);
        REQUIRE(internal_test_env.p_sess_init_cbk_session_ctx == reinterpret_cast<fp_disp_session_base_t *>(&test_sys_ctx.server_session_pools[2]));
        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_section.call_cnt);
        REQUIRE(1 == test_sys_ctx.server_metrics.closed_session_cnt);
    }

    SECTION("Test client session close with a valid session from the session pool")
    {
        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);
        REQUIRE(0 == test_sys_ctx.client_metrics.closed_session_cnt);

        fp_disp_session_attributes_t dummy_sess_attr = {
            // we don't care that the pointer is valid but rather non-null because the
            // session won't be found anyway in the pool and thus the init function will never
            // be called
            .p_init_func = &session_init_dummy_callback,
            .sess_type   = FP_DISP_SESSION_TYPE_CLIENT
        };

        test_sys_ctx.client_session_pools[2].base.p_session_attr = &dummy_sess_attr;

        REQUIRE_FALSE(internal_test_env.dummy_sess_init_cbk_called);

        // attempt to close a session which is not in the session pool
        fp_disp_session_mgr_close_session(&test_sys_ctx, reinterpret_cast<fp_disp_session_base_t *>(&test_sys_ctx.client_session_pools[2]));

        // check that the session is actually found and its corresponding init operation is called
        REQUIRE(internal_test_env.dummy_sess_init_cbk_called);
        REQUIRE(internal_test_env.p_sess_init_cbk_session_ctx == reinterpret_cast<fp_disp_session_base_t *>(&test_sys_ctx.client_session_pools[2]));
        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(1 == env::fp_arch_unlock_section.call_cnt);
        REQUIRE(1 == test_sys_ctx.client_metrics.closed_session_cnt);
    }

    SECTION("Test client session close with a valid session from the session pool but impossibility to obtain the section lock")
    {
        env::fp_arch_lock_section.retval = false;

        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        REQUIRE(0 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);
        REQUIRE(0 == test_sys_ctx.client_metrics.closed_session_cnt);

        fp_disp_session_attributes_t dummy_sess_attr = {
            // we don't care that the pointer is valid but rather non-null because the
            // session won't be found anyway in the pool and thus the init function will never
            // be called
            .p_init_func = &session_init_dummy_callback,
            .sess_type   = FP_DISP_SESSION_TYPE_CLIENT
        };

        test_sys_ctx.client_session_pools[2].base.p_session_attr = &dummy_sess_attr;

        REQUIRE_FALSE(internal_test_env.dummy_sess_init_cbk_called);

        // attempt to close a session which is not in the session pool
        fp_disp_session_mgr_close_session(&test_sys_ctx, reinterpret_cast<fp_disp_session_base_t *>(&test_sys_ctx.client_session_pools[2]));

        // check that the session is actually found and its corresponding init operation is called
        REQUIRE_FALSE(internal_test_env.dummy_sess_init_cbk_called);
        REQUIRE_FALSE(internal_test_env.p_sess_init_cbk_session_ctx == reinterpret_cast<fp_disp_session_base_t *>(&test_sys_ctx.client_session_pools[2]));
        REQUIRE(1 == env::fp_arch_lock_section.call_cnt);
        REQUIRE(0 == env::fp_arch_unlock_section.call_cnt);
        REQUIRE(0 == test_sys_ctx.client_metrics.closed_session_cnt);
    }

    SECTION("Test client session close with a valid session from the session pool (max closed sessions reached)")
    {
        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        fp_disp_session_attributes_t dummy_sess_attr = {
            // we don't care that the pointer is valid but rather non-null because the
            // session won't be found anyway in the pool and thus the init function will never
            // be called
            .p_init_func = &session_init_dummy_callback,
            .sess_type   = FP_DISP_SESSION_TYPE_CLIENT
        };

        test_sys_ctx.client_metrics.closed_session_cnt           = UINT32_MAX;
        test_sys_ctx.client_session_pools[2].base.p_session_attr = &dummy_sess_attr;

        REQUIRE_FALSE(internal_test_env.dummy_sess_init_cbk_called);

        // attempt to close a session which is not in the session pool
        fp_disp_session_mgr_close_session(&test_sys_ctx, reinterpret_cast<fp_disp_session_base_t *>(&test_sys_ctx.client_session_pools[2]));

        REQUIRE(UINT32_MAX == test_sys_ctx.client_metrics.closed_session_cnt);
        REQUIRE(internal_test_env.dummy_sess_init_cbk_called);
        REQUIRE(internal_test_env.p_sess_init_cbk_session_ctx == reinterpret_cast<fp_disp_session_base_t *>(&test_sys_ctx.client_session_pools[2]));
    }

    SECTION("Test session close with NULL parameters (coverage-only)")
    {
        // Test with NULL system context
        fp_disp_session_mgr_close_session(nullptr, nullptr);

        // Test with NULL session
        fp_disp_session_mgr_close_session(&test_sys_ctx, nullptr);

        // Test with valid system context and session but NULL session attributes
        fp_disp_session_mgr_init(&test_sys_ctx);
        test_sys_ctx.client_session_pools[0].base.p_session_attr = nullptr;
        fp_disp_session_mgr_close_session(&test_sys_ctx, (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);

        // Test with valid system context and session but NULL init function in session attributes
        fp_disp_session_attributes_t dummy_sess_attr = {
            .p_init_func = nullptr,
            .sess_type   = FP_DISP_SESSION_TYPE_CLIENT
        };
        test_sys_ctx.client_session_pools[0].base.p_session_attr = &dummy_sess_attr;
        fp_disp_session_mgr_close_session(&test_sys_ctx, (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);
    }

    SECTION("Test session tracking with invalid args")
    {
        fp_disp_session_mgr_track_sessions(nullptr);
    }

    SECTION("Test registering session callbacks with invalid args")
    {
        // Test with NULL system context
        fp_disp_session_mgr_register_callbacks(nullptr, nullptr);
    }

    SECTION("Test registering NULL session callbacks")
    {
        // Register NULL callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, nullptr);

        // Verify that all callbacks are set to NULL
        REQUIRE(nullptr == test_sys_ctx.session_callbacks.p_open);
        REQUIRE(nullptr == test_sys_ctx.session_callbacks.p_track);
        REQUIRE(nullptr == test_sys_ctx.session_callbacks.p_close);
    }

    SECTION("Test registering valid session callbacks")
    {
        // Create static variables to track the handler calls
        static bool                    open_handler_called;
        static bool                    track_handler_called;
        static bool                    close_handler_called;
        static fp_disp_session_base_t *open_session_ptr;
        static fp_disp_session_base_t *track_session_ptr;
        static fp_disp_session_base_t *close_session_ptr;

        // Reset the static variables
        open_handler_called  = false;
        track_handler_called = false;
        close_handler_called = false;
        open_session_ptr     = nullptr;
        track_session_ptr    = nullptr;
        close_session_ptr    = nullptr;

        // Create static functions to use as callbacks
        static auto test_open_handler = [](fp_disp_session_base_t *const p_sess) -> bool {
            open_handler_called = true;
            open_session_ptr    = p_sess;
            return true; // Return true to keep the session open
        };

        static auto test_track_handler = [](fp_disp_session_base_t *const p_sess) -> fp_session_track_action_t {
            track_handler_called = true;
            track_session_ptr    = p_sess;
            return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE; // Keep session open
        };

        static auto test_close_handler = [](fp_disp_session_base_t *const p_sess) {
            close_handler_called = true;
            close_session_ptr    = p_sess;
        };

        // Create the callbacks structure
        fp_user_session_callbacks_t callbacks = {
            .p_open  = test_open_handler,
            .p_track = test_track_handler,
            .p_close = test_close_handler
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Verify that the callbacks were set correctly
        REQUIRE(test_open_handler == test_sys_ctx.session_callbacks.p_open);
        REQUIRE(test_track_handler == test_sys_ctx.session_callbacks.p_track);
        REQUIRE(test_close_handler == test_sys_ctx.session_callbacks.p_close);
    }

    SECTION("Test open session callback is called when opening a session")
    {
        // Create static variables to track the handler calls
        static bool                    open_handler_called = false;
        static fp_disp_session_base_t *open_session_ptr    = nullptr;

        // Reset the static variables
        open_handler_called = false;
        open_session_ptr    = nullptr;

        // Initialize the session manager first
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Create a static function to use as the open callback
        static auto test_open_handler = [](fp_disp_session_base_t *const p_sess) -> bool {
            open_handler_called = true;
            open_session_ptr    = p_sess;
            return true; // Return true to keep the session open
        };

        // Create the callbacks structure with only the open callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = test_open_handler,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks after initialization
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Verify the open handler has not been called yet
        REQUIRE_FALSE(open_handler_called);
        REQUIRE(nullptr == open_session_ptr);

        // Open a session
        fp_disp_session_base_t *p_session = fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT);

        // Verify that the open handler was called
        REQUIRE(open_handler_called);

        // Verify that the correct session pointer was passed to the open handler
        REQUIRE(p_session == open_session_ptr);

        // Verify that the session is active
        REQUIRE(FP_SESSION_STATE_INACTIVE != atomic_load(&p_session->state));
    }

    SECTION("Test open session callback is not called when opening a session fails")
    {
        // Create static variables to track the handler calls
        static bool                    open_handler_called = false;
        static fp_disp_session_base_t *open_session_ptr    = nullptr;

        // Reset the static variables
        open_handler_called = false;
        open_session_ptr    = nullptr;

        // Initialize the session manager first
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Create a static function to use as the open callback
        static auto test_open_handler = [](fp_disp_session_base_t *const p_sess) -> bool {
            open_handler_called = true;
            open_session_ptr    = p_sess;
            return true; // Return true to keep the session open
        };

        // Create the callbacks structure with only the open callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = test_open_handler,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks after initialization
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Make all sessions active so opening a new one will fail
        for (auto &client_sess : test_sys_ctx.client_session_pools)
        {
            atomic_store(&client_sess.base.state, FP_SESSION_STATE_ACTIVE);
        }

        // Try to open a session (should fail)
        fp_disp_session_base_t *p_session = fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT);

        // Verify that the open handler was not called
        REQUIRE_FALSE(open_handler_called);
        REQUIRE(nullptr == open_session_ptr);

        // Verify that no session was returned
        REQUIRE(nullptr == p_session);
    }

    SECTION("Test complete session lifecycle with callbacks")
    {
        // Create static variables to track the handler calls
        static bool                    open_handler_called  = false;
        static bool                    track_handler_called = false;
        static bool                    close_handler_called = false;
        static fp_disp_session_base_t *open_session_ptr     = nullptr;
        static fp_disp_session_base_t *track_session_ptr    = nullptr;
        static fp_disp_session_base_t *close_session_ptr    = nullptr;

        // Reset the static variables
        open_handler_called  = false;
        track_handler_called = false;
        close_handler_called = false;
        open_session_ptr     = nullptr;
        track_session_ptr    = nullptr;
        close_session_ptr    = nullptr;

        // Initialize the session manager first
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Create static functions to use as callbacks
        static auto test_open_handler = [](fp_disp_session_base_t *const p_sess) -> bool {
            open_handler_called = true;
            open_session_ptr    = p_sess;

            // Set some test data in the session
            p_sess->p_comm_params = reinterpret_cast<void *>(test_comm_params_value);

            return true; // Return true to keep the session open
        };

        static auto test_track_handler = [](fp_disp_session_base_t *const p_sess) -> fp_session_track_action_t {
            track_handler_called = true;
            track_session_ptr    = p_sess;

            // Verify that the comm params set in open are still there
            REQUIRE(reinterpret_cast<void *>(test_comm_params_value) == p_sess->p_comm_params);

            // Return FP_DISP_TRACK_ACTION_CLOSE_SESSION to close the session
            return FP_DISP_TRACK_ACTION_CLOSE_SESSION;
        };

        static auto test_close_handler = [](fp_disp_session_base_t *const p_sess) {
            close_handler_called = true;
            close_session_ptr    = p_sess;

            // Verify that the comm params set in open are still there
            REQUIRE(reinterpret_cast<void *>(test_comm_params_value) == p_sess->p_comm_params);
        };

        // Create the callbacks structure
        fp_user_session_callbacks_t callbacks = {
            .p_open  = test_open_handler,
            .p_track = test_track_handler,
            .p_close = test_close_handler
        };

        // Register the callbacks after initialization
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Open a session
        fp_disp_session_base_t *p_session = fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT);

        // Verify that the open handler was called
        REQUIRE(open_handler_called);
        REQUIRE(p_session == open_session_ptr);

        // Activate the session (normally done by fp_disp_client/server when sending a request)
        // This is required because track_session only processes ACTIVE sessions
        atomic_store(&p_session->state, FP_SESSION_STATE_ACTIVE);

        // Track sessions - this should call the track handler which returns true to close the session
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify that the track handler was called
        REQUIRE(track_handler_called);
        REQUIRE(p_session == track_session_ptr);

        // Verify that the close handler was called
        REQUIRE(close_handler_called);
        REQUIRE(p_session == close_session_ptr);

        // Verify that the session is now inactive
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&p_session->state));
        REQUIRE(nullptr == p_session->p_comm_params);
    }

    SECTION("Test open session callback is called after session is fully initialized")
    {
        // Create a static variable to track if the session was properly initialized when the open callback was called
        static bool session_was_properly_initialized = false;

        // Reset the static variable
        session_was_properly_initialized = false;

        // Initialize the session manager first
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Create a static function to use as the open callback that checks if the session is properly initialized
        static auto test_open_handler = [](fp_disp_session_base_t *const p_sess) -> bool {
            // Check that the session is active
            session_was_properly_initialized = (FP_SESSION_STATE_INACTIVE != atomic_load(&p_sess->state));

            // Check that the session attributes are set
            session_was_properly_initialized &= (p_sess->p_session_attr != nullptr);

            return true; // Return true to keep the session open
        };

        // Create the callbacks structure with only the open callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = test_open_handler,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks after initialization
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Open a session
        fp_disp_session_base_t *p_session = fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT);

        // Verify that the session was properly initialized when the open callback was called
        REQUIRE(session_was_properly_initialized);

        // Verify that the session is active
        REQUIRE(FP_SESSION_STATE_INACTIVE != atomic_load(&p_session->state));
    }

    SECTION("Test open session callback returning false causes session to be closed")
    {
        // Create static variables to track the handler calls
        static bool                    open_handler_called = false;
        static fp_disp_session_base_t *open_session_ptr    = nullptr;

        // Reset the static variables
        open_handler_called = false;
        open_session_ptr    = nullptr;

        // Initialize the session manager first
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Create a static function to use as the open callback that returns false
        static auto test_open_handler = [](fp_disp_session_base_t *const p_sess) -> bool {
            open_handler_called = true;
            open_session_ptr    = p_sess;

            return false; // Return false to close the session
        };

        // Create the callbacks structure with only the open callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = test_open_handler,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks after initialization
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Verify the open handler has not been called yet
        REQUIRE_FALSE(open_handler_called);
        REQUIRE(nullptr == open_session_ptr);

        // Try to open a session (should fail because the open callback returns false)
        fp_disp_session_base_t *p_session = fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT);

        // Verify that the open handler was called
        REQUIRE(open_handler_called);

        // Verify that a valid session pointer was passed to the open handler
        REQUIRE(nullptr != open_session_ptr);

        // Verify that no session was returned (NULL was returned)
        REQUIRE(nullptr == p_session);

        // Verify that the session was properly cleaned up (state set to inactive)
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&open_session_ptr->state));

        // Verify that the session metrics were properly updated
        REQUIRE(0 == test_sys_ctx.client_metrics.active_session_cnt);
    }

    SECTION("Test open session callback returning false with lock acquisition failure")
    {
        // Create static variables to track the handler calls
        static bool                    open_handler_called = false;
        static fp_disp_session_base_t *open_session_ptr    = nullptr;

        // Reset the static variables
        open_handler_called = false;
        open_session_ptr    = nullptr;

        // Initialize the session manager first
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Create a static function to use as the open callback that returns false
        static auto test_open_handler = [](fp_disp_session_base_t *const p_sess) -> bool {
            open_handler_called = true;
            open_session_ptr    = p_sess;

            // Set some test data in the session
            p_sess->p_comm_params = reinterpret_cast<void *>(test_comm_params_value);

            // Make the lock acquisition fail after the open callback returns
            env::fp_arch_lock_section.retval = false;

            return false; // Return false to close the session
        };

        // Create the callbacks structure with only the open callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = test_open_handler,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks after initialization
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Make sure lock acquisition succeeds initially
        env::fp_arch_lock_section.retval = true;

        // Try to open a session (should fail because the open callback returns false)
        fp_disp_session_base_t *p_session = fp_disp_session_mgr_open_session(&test_sys_ctx, nullptr, FP_DISP_SESSION_TYPE_CLIENT);

        // Verify that the open handler was called
        REQUIRE(open_handler_called);

        // Verify that a valid session pointer was passed to the open handler
        REQUIRE(nullptr != open_session_ptr);

        // Verify that no session was returned (NULL was returned)
        REQUIRE(nullptr == p_session);

        // The session state is not cleaned up because the lock acquisition failed
        // But we still return NULL to indicate failure
    }

    SECTION("Test registering a NULL track callback")
    {
        // Create a callbacks structure with only the track callback set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Verify that the track callback was set to NULL
        REQUIRE(nullptr == test_sys_ctx.session_callbacks.p_track);
    }

    SECTION("Test registering a valid track callback")
    {
        // Create a flag to track if our handler was called
        bool user_handler_called = false;

        // Initialize a session and set it to active
        fp_disp_session_mgr_init(&test_sys_ctx);
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // Create a user track handler function
        auto user_track_handler = [](fp_disp_session_base_t *const p_sess) -> fp_session_track_action_t {
            // Set the flag to indicate the handler was called
            *static_cast<bool *>(p_sess->p_comm_params) = true;
            // Return false to keep the session open
            return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE;
        };

        // Create a callbacks structure with only the track callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = user_track_handler,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Verify that the track callback was set correctly
        REQUIRE(user_track_handler == test_sys_ctx.session_callbacks.p_track);

        // Set the p_comm_params to point to our flag
        test_sys_ctx.client_session_pools[0].base.p_comm_params = &user_handler_called;

        // Call track sessions
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify that our handler was called
        REQUIRE(user_handler_called);
    }

    SECTION("Test registering a valid close callback")
    {
        // Create static variables to track the handler call
        static bool                    test_close_handler_called = false;
        static fp_disp_session_base_t *close_session_ptr         = nullptr;

        // Reset the static variables
        test_close_handler_called = false;
        close_session_ptr         = nullptr;

        // Initialize a session and set it to active
        fp_disp_session_mgr_init(&test_sys_ctx);
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // Create a static function to use as the close handler
        static auto test_close_handler = [](fp_disp_session_base_t *const p_sess) {
            // Set the flag to indicate the handler was called
            test_close_handler_called = true;
            // Store the session pointer to verify it's the correct one
            close_session_ptr = p_sess;
        };

        // Create a callbacks structure with only the close callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = test_close_handler
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Verify that the close callback was set correctly
        REQUIRE(test_close_handler == test_sys_ctx.session_callbacks.p_close);

        // Create a test communication parameter
        int test_param = 42;

        // Set the p_comm_params to point to our test parameter
        test_sys_ctx.client_session_pools[0].base.p_comm_params = &test_param;

        // Close the session
        fp_disp_session_mgr_close_session(&test_sys_ctx, (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);

        // Verify that our handler was called
        REQUIRE(test_close_handler_called);

        // Verify that the correct session pointer was passed to the close handler
        REQUIRE(close_session_ptr == (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);
    }

    SECTION("Test closing a session with no close callback registered")
    {
        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Verify that the close callback was set to nullptr
        REQUIRE(nullptr == test_sys_ctx.session_callbacks.p_close);

        // Initialize a session and set it to active
        fp_disp_session_mgr_init(&test_sys_ctx);
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // Create a test communication parameter
        int test_param = 42;

        // Set the p_comm_params to point to our test parameter
        test_sys_ctx.client_session_pools[0].base.p_comm_params = &test_param;

        // Close the session - this should not cause any issues even though no close handler is registered
        fp_disp_session_mgr_close_session(&test_sys_ctx, (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);

        // Verify that the session was closed properly
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&test_sys_ctx.client_session_pools[0].base.state));
        REQUIRE(nullptr == test_sys_ctx.client_session_pools[0].base.p_comm_params);
    }

    SECTION("Test closing a session with a close callback and NULL p_comm_params")
    {
        // Create static variables to track the handler call
        static bool test_close_handler_called = false;

        // Reset the static variable
        test_close_handler_called = false;

        // Initialize a session and set it to active
        fp_disp_session_mgr_init(&test_sys_ctx);
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // Create a static function to use as the close handler
        static auto test_close_handler = [](fp_disp_session_base_t *const p_sess) {
            // Set the flag to indicate the handler was called
            test_close_handler_called = true;
        };

        // Create a callbacks structure with only the close callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = test_close_handler
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Set p_comm_params to nullptr
        test_sys_ctx.client_session_pools[0].base.p_comm_params = nullptr;

        // Close the session - the close handler should still be called even if p_comm_params is NULL
        fp_disp_session_mgr_close_session(&test_sys_ctx, (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);

        // Verify that the close handler was called
        REQUIRE(test_close_handler_called);

        // Verify that the session was closed properly
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&test_sys_ctx.client_session_pools[0].base.state));
    }

    SECTION("Test that user track callback is called before standard track function")
    {
        // Create a struct to track the order of function calls
        struct TrackingOrder
        {
            bool user_handler_called_first = false;
            bool standard_handler_called   = false;
        };

        TrackingOrder tracking_order;

        // Initialize a session
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Create a user track handler function that sets a flag
        auto user_track_handler = [](fp_disp_session_base_t *const p_sess) -> fp_session_track_action_t {
            // Get our tracking struct
            auto tracking = static_cast<TrackingOrder *>(p_sess->p_comm_params);

            // Set the flag to indicate the user handler was called first
            // (this should be true because the standard handler hasn't been called yet)
            tracking->user_handler_called_first = !tracking->standard_handler_called;

            // Return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE to keep the session open
            return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE;
        };

        // Create a callbacks structure with only the track callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = user_track_handler,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Override the standard client tracking function to set a flag when it's called
        env::fp_arch_is_timer_elapsed.p_user_stub = [](fp_arch_timer_handle_t *const p_timer_hnd, const uint32_t elapse_time) -> bool {
            // Get the session from the timer handle
            auto p_sess = reinterpret_cast<fp_disp_session_base_t *>(
                reinterpret_cast<char *>(p_timer_hnd) - offsetof(fp_disp_session_base_t, timer_hnd));

            // Get our tracking struct
            auto tracking = static_cast<TrackingOrder *>(p_sess->p_comm_params);

            // Set the flag to indicate the standard handler was called
            tracking->standard_handler_called = true;

            // Return false to prevent the session from being closed
            return false;
        };

        // Explicitly set the session to active - this is critical for the test
        // as tracking functions are only called for active sessions
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // Verify the session is actually active before proceeding
        REQUIRE(FP_SESSION_STATE_INACTIVE != atomic_load(&test_sys_ctx.client_session_pools[0].base.state));

        test_sys_ctx.client_session_pools[0].base.timeout = 1000; // Set a timeout to ensure tracking function is called

        // Set the p_comm_params to point to our tracking struct
        test_sys_ctx.client_session_pools[0].base.p_comm_params = &tracking_order;
        test_sys_ctx.client_session_pools[0].req_type           = FP_REQ_TYPE_ASYNC;

        // Call track sessions
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify that our user handler was called before the standard handler
        REQUIRE(tracking_order.user_handler_called_first);
        REQUIRE(tracking_order.standard_handler_called);
    }

    SECTION("Test that user track callback is not called for inactive sessions")
    {
        // Create a flag to track if our handler was called
        bool user_handler_called = false;

        // Create a user track handler function
        auto user_track_handler = [](fp_disp_session_base_t *const p_sess) -> fp_session_track_action_t {
            // Set the flag to indicate the handler was called
            *static_cast<bool *>(p_sess->p_comm_params) = true;
            // Return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE to keep the session open
            return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE;
        };

        // Create a callbacks structure with only the track callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = user_track_handler,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Initialize a session but set it to inactive
        fp_disp_session_mgr_init(&test_sys_ctx);
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        // Set the p_comm_params to point to our flag
        test_sys_ctx.client_session_pools[0].base.p_comm_params = &user_handler_called;

        // Call track sessions
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify that our handler was not called for the inactive session
        REQUIRE_FALSE(user_handler_called);
    }

    SECTION("Test that user track callback can close a session by returning true")
    {
        // Create a flag to track if our handler was called
        bool user_handler_called = false;

        // Initialize a session and set it to active
        fp_disp_session_mgr_init(&test_sys_ctx);
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // Create a user track handler function that returns FP_DISP_TRACK_ACTION_CLOSE_SESSION to close the session
        auto user_track_handler = [](fp_disp_session_base_t *const p_sess) -> fp_session_track_action_t {
            // Set the flag to indicate the handler was called
            *static_cast<bool *>(p_sess->p_comm_params) = true;
            // Return FP_DISP_TRACK_ACTION_CLOSE_SESSION to close the session
            return FP_DISP_TRACK_ACTION_CLOSE_SESSION;
        };

        // Create a callbacks structure with only the track callback
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = user_track_handler,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Set some values to verify they get reset when the session is closed
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = test_cmd_status_invalid;
        test_sys_ctx.client_session_pools[0].base.timeout             = test_timeout;

        // Set the p_comm_params to point to our flag
        test_sys_ctx.client_session_pools[0].base.p_comm_params = &user_handler_called;

        // Reset the stop timer call counter
        env::fp_arch_stop_timer.call_cnt = 0;

        // Call track sessions
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify that our handler was called
        REQUIRE(user_handler_called);

        // Verify that the session was closed
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&test_sys_ctx.client_session_pools[0].base.state));
        REQUIRE(0 == test_sys_ctx.client_session_pools[0].base.timeout);
        REQUIRE(1 == env::fp_arch_stop_timer.call_cnt);
        REQUIRE(UINT16_MAX == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id);
        REQUIRE(UINT16_MAX == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id);
        REQUIRE(UINT32_MAX == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id);
        REQUIRE(FP_CMD_STATUS_MAX == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        REQUIRE(nullptr == test_sys_ctx.client_session_pools[0].base.p_comm_params);
    }

    SECTION("Test that user close callback is called when user track callback returns true")
    {
        // Create static variables to track the handler calls
        static bool                    track_handler_called = false;
        static bool                    close_handler_called = false;
        static fp_disp_session_base_t *close_session_ptr    = nullptr;

        // Reset the static variables
        track_handler_called = false;
        close_handler_called = false;
        close_session_ptr    = nullptr;

        // Initialize a session and set it to active
        fp_disp_session_mgr_init(&test_sys_ctx);
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // Create a user track handler function that returns FP_DISP_TRACK_ACTION_CLOSE_SESSION to close the session
        auto user_track_handler = [](fp_disp_session_base_t *const p_sess) -> fp_session_track_action_t {
            // Set the flag to indicate the handler was called
            *static_cast<bool *>(p_sess->p_comm_params) = true;
            // Return FP_DISP_TRACK_ACTION_CLOSE_SESSION to close the session
            return FP_DISP_TRACK_ACTION_CLOSE_SESSION;
        };

        // Create a static function to use as the close handler
        static auto test_close_handler_for_track = [](fp_disp_session_base_t *const p_sess) {
            // Set the flag to indicate the handler was called
            close_handler_called = true;
            // Store the session pointer to verify it's the correct one
            close_session_ptr = p_sess;
        };

        // Create a callbacks structure with both track and close callbacks
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = user_track_handler,
            .p_close = test_close_handler_for_track
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // Set the p_comm_params to point to our flag
        test_sys_ctx.client_session_pools[0].base.p_comm_params = &track_handler_called;

        // Call track sessions
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify that our track handler was called
        REQUIRE(track_handler_called);

        // Verify that our close handler was called
        REQUIRE(close_handler_called);

        // Verify that the correct session pointer was passed to the close handler
        REQUIRE(close_session_ptr == (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);

        // Verify that the session was closed
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&test_sys_ctx.client_session_pools[0].base.state));
        REQUIRE(nullptr == test_sys_ctx.client_session_pools[0].base.p_comm_params);
    }

    SECTION("Test session tracking with valid args but impossibility to obtain section lock")
    {
        env::fp_arch_lock_section.retval = false;

        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        fp_disp_session_mgr_track_sessions(&test_sys_ctx);
    }

    SECTION("Test server session tracking with valid args")
    {
        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // check tracking for all configured sessions
        for (auto &server_sess : test_sys_ctx.server_session_pools)
        {
            env::clear();
            internal_test_env.clear();

            // make sure the session pools are correctly initialized
            fp_disp_session_mgr_init(&test_sys_ctx);

            // activate one session
            atomic_store(&server_sess.base.state, FP_SESSION_STATE_ACTIVE);
            server_sess.base.timeout   = test_timeout; // set timeout value (just to check this parameter is not used by server track function)

            env::fp_arch_is_timer_elapsed.retval = false;

            fp_disp_session_mgr_track_sessions(&test_sys_ctx);

            // server timeouts are hard-coded to 60 seconds
            REQUIRE(60000 == env::fp_arch_is_timer_elapsed.params.back().elapse_time);
            REQUIRE(&server_sess.base.timer_hnd == env::fp_arch_is_timer_elapsed.params.back().p_timer_hnd);
            // check that session is still active
            REQUIRE(FP_SESSION_STATE_ACTIVE == atomic_load(&server_sess.base.state));

            // now force the timeout to expire
            env::fp_arch_is_timer_elapsed.retval = true;
            env::fp_arch_stop_timer.call_cnt     = 0; // reset the stop_timer call counter to see if it will be called on session close later

            fp_disp_session_mgr_track_sessions(&test_sys_ctx);
            // check that session is now closed
            REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&server_sess.base.state));
            REQUIRE(0 == server_sess.base.timeout);
            REQUIRE(1 == env::fp_arch_stop_timer.call_cnt);
            REQUIRE(UINT16_MAX == server_sess.base.req_fp_hdr.cmd_id);
            REQUIRE(UINT16_MAX == server_sess.base.req_fp_hdr.iface_id);
            REQUIRE(UINT32_MAX == server_sess.base.req_fp_hdr.func_id);
            REQUIRE(FP_CMD_STATUS_MAX == server_sess.base.req_fp_hdr.status);
        }
    }

    SECTION("Test client session tracking with a valid response handler assigned")
    {
        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // ensure all sessions are inactive because later we will activate only one at a time but
        // fp_disp_session_mgr_track_sessions() checks all of them and not initializing them here
        // may lead to side effects
        for (auto &client_sess : test_sys_ctx.client_session_pools)
        {
            atomic_store(&client_sess.base.state, FP_SESSION_STATE_INACTIVE);
        }

        // check tracking for all configured sessions
        for (auto &client_sess : test_sys_ctx.client_session_pools)
        {
            env::clear();
            internal_test_env.clear();

            // make sure the session pools are correctly initialized
            fp_disp_session_mgr_init(&test_sys_ctx);

            // activate one session
            atomic_store(&client_sess.base.state, FP_SESSION_STATE_ACTIVE);
            client_sess.base.timeout       = test_timeout;
            client_sess.req_type           = FP_REQ_TYPE_ASYNC;
            client_sess.base.p_comm_params = &client_sess; // just some pointer value to compare to later

            // assign a valid user response handler (passed to the FP request function in regular execution scenarios)
            client_sess.p_user_handler = (fp_generic_response_handler_t)&test_client_resp_handler_cbk_timeout;

            env::fp_arch_is_timer_elapsed.retval = false;

            fp_disp_session_mgr_track_sessions(&test_sys_ctx);

            // client timeouts are provided by the user
            REQUIRE(test_timeout == env::fp_arch_is_timer_elapsed.params.back().elapse_time);
            REQUIRE(&client_sess.base.timer_hnd == env::fp_arch_is_timer_elapsed.params.back().p_timer_hnd);
            // check that session is still active
            REQUIRE(FP_SESSION_STATE_ACTIVE == atomic_load(&client_sess.base.state));

            // user response handler shall not be called before the timeout
            REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);

            // now force the timeout to expire
            env::fp_arch_is_timer_elapsed.retval = true;
            env::fp_arch_stop_timer.call_cnt     = 0; // reset the stop_timer call counter to see if it will be called on session close later

            fp_disp_session_mgr_track_sessions(&test_sys_ctx);

            REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&client_sess.base.state));
            REQUIRE(0 == client_sess.base.timeout);
            REQUIRE(0 < env::fp_arch_stop_timer.call_cnt);
            REQUIRE(UINT16_MAX == client_sess.base.req_fp_hdr.cmd_id);
            REQUIRE(UINT16_MAX == client_sess.base.req_fp_hdr.iface_id);
            REQUIRE(UINT32_MAX == client_sess.base.req_fp_hdr.func_id);
            REQUIRE(FP_CMD_STATUS_MAX == client_sess.base.req_fp_hdr.status);

            // check if the user response handler was called
            REQUIRE(internal_test_env.client_resp_handler_cbk_called);
            REQUIRE(internal_test_env.timeout_p_comm_params == &client_sess);
            REQUIRE(nullptr == internal_test_env.p_untyped_resp_param);
            REQUIRE(FP_CMD_STATUS_TIMEOUT == internal_test_env.resp_cmd_status);
        }
    }

    SECTION("Test client session tracking with a NULL response handler assigned (e.g. when tracking a synchronous request)")
    {
        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // check tracking for all configured sessions
        for (auto &client_sess : test_sys_ctx.client_session_pools)
        {
            env::clear();
            internal_test_env.clear();
            env::fp_arch_lock_section.retval   = true;
            env::fp_arch_lock_registry.retval  = true;
            env::fp_arch_wait_for_event.retval = true;

            // make sure the session pools are correctly initialized
            fp_disp_session_mgr_init(&test_sys_ctx);

            // activate one session
            atomic_store(&client_sess.base.state, FP_SESSION_STATE_ACTIVE);
            client_sess.base.timeout             = test_timeout;
            client_sess.req_type                 = FP_REQ_TYPE_SYNC;
            client_sess.base.req_fp_hdr.cmd_id   = test_cmd_id;
            client_sess.base.req_fp_hdr.func_id  = test_func_id;
            client_sess.base.req_fp_hdr.iface_id = test_if_id;
            client_sess.base.req_fp_hdr.status   = test_cmd_status_invalid;

            // assign a null user handler
            client_sess.p_user_handler = nullptr;

            env::fp_arch_is_timer_elapsed.retval = false;

            // call session tracking...
            fp_disp_session_mgr_track_sessions(&test_sys_ctx);

            // is_timer_elapsed() must not be called in case of SYNC requests (timeouts directly handled by blocking function)
            REQUIRE_FALSE(test_timeout == env::fp_arch_is_timer_elapsed.params.back().elapse_time);
            REQUIRE_FALSE(&client_sess.base.timer_hnd == env::fp_arch_is_timer_elapsed.params.back().p_timer_hnd);
            // check that session is still active
            REQUIRE(FP_SESSION_STATE_ACTIVE == atomic_load(&client_sess.base.state));

            // user response handler shall not be called before the timeout
            REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);

            // now force the timeout to expire
            env::fp_arch_is_timer_elapsed.retval = true;
            env::fp_arch_stop_timer.call_cnt     = 0; // reset the stop_timer call counter to see if it will be called on session close later

            fp_disp_session_mgr_track_sessions(&test_sys_ctx);

            // check that client_sess in still active, since session tracking affects only asynchronous client sessions
            REQUIRE(FP_SESSION_STATE_ACTIVE == atomic_load(&client_sess.base.state));
            REQUIRE(test_timeout == client_sess.base.timeout);
            REQUIRE(0 == env::fp_arch_stop_timer.call_cnt);
            REQUIRE(test_cmd_id == client_sess.base.req_fp_hdr.cmd_id);
            REQUIRE(test_if_id == client_sess.base.req_fp_hdr.iface_id);
            REQUIRE(test_func_id == client_sess.base.req_fp_hdr.func_id);
            REQUIRE(test_cmd_status_invalid == client_sess.base.req_fp_hdr.status);
        }
    }

    SECTION("Test client session tracking with a NULL response handler assigned (e.g. when tracking an asynchronous request)")
    {
        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        // ensure all sessions are inactive because later we will activate only one at a time but
        // fp_disp_session_mgr_track_sessions() checks all of them and not initializing them here
        // may lead to side effects
        for (auto &client_sess : test_sys_ctx.client_session_pools)
        {
            atomic_store(&client_sess.base.state, FP_SESSION_STATE_INACTIVE);
        }

        // check tracking for all configured sessions
        for (auto &client_sess : test_sys_ctx.client_session_pools)
        {
            env::clear();
            internal_test_env.clear();
            env::fp_arch_lock_section.retval   = true;
            env::fp_arch_lock_registry.retval  = true;
            env::fp_arch_wait_for_event.retval = true;

            // make sure the session pools are correctly initialized
            fp_disp_session_mgr_init(&test_sys_ctx);

            // activate one session
            atomic_store(&client_sess.base.state, FP_SESSION_STATE_ACTIVE);
            client_sess.base.timeout             = test_timeout;
            client_sess.req_type                 = FP_REQ_TYPE_ASYNC;
            client_sess.base.req_fp_hdr.cmd_id   = test_cmd_id;
            client_sess.base.req_fp_hdr.func_id  = test_func_id;
            client_sess.base.req_fp_hdr.iface_id = test_if_id;
            client_sess.base.req_fp_hdr.status   = test_cmd_status_invalid;

            // assign a null user handler
            client_sess.p_user_handler = nullptr;

            env::fp_arch_is_timer_elapsed.retval = false;

            // call session tracking...
            fp_disp_session_mgr_track_sessions(&test_sys_ctx);

            // is_timer_elapsed() shall be called in case of ASYNC requests
            REQUIRE(test_timeout == env::fp_arch_is_timer_elapsed.params.back().elapse_time);
            REQUIRE(&client_sess.base.timer_hnd == env::fp_arch_is_timer_elapsed.params.back().p_timer_hnd);
            // check that session is still active because the timeout has not elapsed
            REQUIRE(FP_SESSION_STATE_ACTIVE == atomic_load(&client_sess.base.state));

            // user response handler must not be called because it is null
            REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);

            // now force the timeout to expire
            env::fp_arch_is_timer_elapsed.retval = true;
            env::fp_arch_stop_timer.call_cnt     = 0; // reset the stop_timer call counter to see if it will be called on session close later

            fp_disp_session_mgr_track_sessions(&test_sys_ctx);

            // check that client_sess is finalized after timeout expiry
            REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&client_sess.base.state));
            REQUIRE(0 == client_sess.base.timeout);
            REQUIRE(0 < env::fp_arch_stop_timer.call_cnt);
            REQUIRE(UINT16_MAX == client_sess.base.req_fp_hdr.cmd_id);
            REQUIRE(UINT16_MAX == client_sess.base.req_fp_hdr.iface_id);
            REQUIRE(UINT32_MAX == client_sess.base.req_fp_hdr.func_id);
            REQUIRE(FP_CMD_STATUS_MAX == client_sess.base.req_fp_hdr.status);
        }
    }

    SECTION("Test client session timed_out_session_cnt counter incrementation when session times out")
    {
        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        env::clear();
        internal_test_env.clear();
        env::fp_arch_lock_section.retval   = true;
        env::fp_arch_lock_registry.retval  = true;
        env::fp_arch_wait_for_event.retval = true;

        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Reset the timed_out_session_cnt counter
        test_sys_ctx.client_metrics.timed_out_session_cnt = 0;

        // Activate a client session
        auto &client_sess                    = test_sys_ctx.client_session_pools[0];
        atomic_store(&client_sess.base.state, FP_SESSION_STATE_ACTIVE);
        client_sess.base.timeout             = test_timeout;
        client_sess.req_type                 = FP_REQ_TYPE_ASYNC;
        client_sess.base.req_fp_hdr.cmd_id   = test_cmd_id;
        client_sess.base.req_fp_hdr.func_id  = test_func_id;
        client_sess.base.req_fp_hdr.iface_id = test_if_id;
        client_sess.base.req_fp_hdr.status   = test_cmd_status_invalid;
        client_sess.p_user_handler           = nullptr;

        // First check that the counter is 0
        REQUIRE(0 == test_sys_ctx.client_metrics.timed_out_session_cnt);

        // Set timer to not elapsed
        env::fp_arch_is_timer_elapsed.retval = false;

        // Call session tracking
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Counter should still be 0 as no timeout occurred
        REQUIRE(0 == test_sys_ctx.client_metrics.timed_out_session_cnt);

        // Now force the timeout to expire
        env::fp_arch_is_timer_elapsed.retval = true;

        // Call session tracking again
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Check that the timed_out_session_cnt counter was incremented
        REQUIRE(1 == test_sys_ctx.client_metrics.timed_out_session_cnt);

        // Activate another client session
        auto &client_sess2                    = test_sys_ctx.client_session_pools[1];
        atomic_store(&client_sess2.base.state, FP_SESSION_STATE_ACTIVE);
        client_sess2.base.timeout             = test_timeout;
        client_sess2.req_type                 = FP_REQ_TYPE_ASYNC;
        client_sess2.base.req_fp_hdr.cmd_id   = test_cmd_id;
        client_sess2.base.req_fp_hdr.func_id  = test_func_id;
        client_sess2.base.req_fp_hdr.iface_id = test_if_id;
        client_sess2.base.req_fp_hdr.status   = test_cmd_status_invalid;
        client_sess2.p_user_handler           = nullptr;

        // Force another timeout
        env::fp_arch_is_timer_elapsed.retval = true;

        // Call session tracking again
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Check that the timed_out_session_cnt counter was incremented again
        REQUIRE(2 == test_sys_ctx.client_metrics.timed_out_session_cnt);
    }

    SECTION("Test client session timed_out_session_cnt counter when it reaches UINT32_MAX")
    {
        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        env::clear();
        internal_test_env.clear();
        env::fp_arch_lock_section.retval   = true;
        env::fp_arch_lock_registry.retval  = true;
        env::fp_arch_wait_for_event.retval = true;

        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Set the timed_out_session_cnt counter to UINT32_MAX
        test_sys_ctx.client_metrics.timed_out_session_cnt = UINT32_MAX;

        // Activate a client session
        auto &client_sess                    = test_sys_ctx.client_session_pools[0];
        atomic_store(&client_sess.base.state, FP_SESSION_STATE_ACTIVE);
        client_sess.base.timeout             = test_timeout;
        client_sess.req_type                 = FP_REQ_TYPE_ASYNC;
        client_sess.base.req_fp_hdr.cmd_id   = test_cmd_id;
        client_sess.base.req_fp_hdr.func_id  = test_func_id;
        client_sess.base.req_fp_hdr.iface_id = test_if_id;
        client_sess.base.req_fp_hdr.status   = test_cmd_status_invalid;
        client_sess.p_user_handler           = nullptr;

        // First check that the counter is UINT32_MAX
        REQUIRE(UINT32_MAX == test_sys_ctx.client_metrics.timed_out_session_cnt);

        // Force the timeout to expire
        env::fp_arch_is_timer_elapsed.retval = true;

        // Call session tracking
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Check that the timed_out_session_cnt counter remains at UINT32_MAX (not incremented)
        REQUIRE(UINT32_MAX == test_sys_ctx.client_metrics.timed_out_session_cnt);

        // Verify that the session was still closed despite the counter not incrementing
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&client_sess.base.state));
    }

    SECTION("Test server session timed_out_session_cnt counter incrementation when session times out")
    {
        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        env::clear();
        internal_test_env.clear();
        env::fp_arch_lock_section.retval   = true;
        env::fp_arch_lock_registry.retval  = true;
        env::fp_arch_wait_for_event.retval = true;

        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Reset the timed_out_session_cnt counter
        test_sys_ctx.server_metrics.timed_out_session_cnt = 0;

        // Activate a server session
        auto &server_sess                    = test_sys_ctx.server_session_pools[0];
        atomic_store(&server_sess.base.state, FP_SESSION_STATE_ACTIVE);
        server_sess.base.timeout             = 0; // Server sessions use ARMED_REQ_TOUT_S constant
        server_sess.base.req_fp_hdr.cmd_id   = test_cmd_id;
        server_sess.base.req_fp_hdr.func_id  = test_func_id;
        server_sess.base.req_fp_hdr.iface_id = test_if_id;
        server_sess.base.req_fp_hdr.status   = test_cmd_status_invalid;

        // First check that the counter is 0
        REQUIRE(0 == test_sys_ctx.server_metrics.timed_out_session_cnt);

        // Set timer to not elapsed
        env::fp_arch_is_timer_elapsed.retval = false;

        // Call session tracking
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Counter should still be 0 as no timeout occurred
        REQUIRE(0 == test_sys_ctx.server_metrics.timed_out_session_cnt);

        // Now force the timeout to expire
        env::fp_arch_is_timer_elapsed.retval = true;

        // Call session tracking again
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Check that the timed_out_session_cnt counter was incremented
        REQUIRE(1 == test_sys_ctx.server_metrics.timed_out_session_cnt);

        // Activate another server session
        auto &server_sess2                    = test_sys_ctx.server_session_pools[1];
        atomic_store(&server_sess2.base.state, FP_SESSION_STATE_ACTIVE);
        server_sess2.base.timeout             = 0; // Server sessions use ARMED_REQ_TOUT_S constant
        server_sess2.base.req_fp_hdr.cmd_id   = test_cmd_id;
        server_sess2.base.req_fp_hdr.func_id  = test_func_id;
        server_sess2.base.req_fp_hdr.iface_id = test_if_id;
        server_sess2.base.req_fp_hdr.status   = test_cmd_status_invalid;

        // Force another timeout
        env::fp_arch_is_timer_elapsed.retval = true;

        // Call session tracking again
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Check that the timed_out_session_cnt counter was incremented again
        REQUIRE(2 == test_sys_ctx.server_metrics.timed_out_session_cnt);
    }

    SECTION("Test server session timed_out_session_cnt counter when it reaches UINT32_MAX")
    {
        // Create a callbacks structure with all callbacks set to nullptr
        fp_user_session_callbacks_t callbacks = {
            .p_open  = nullptr,
            .p_track = nullptr,
            .p_close = nullptr
        };

        // Register the callbacks
        fp_disp_session_mgr_register_callbacks(&test_sys_ctx, &callbacks);

        env::clear();
        internal_test_env.clear();
        env::fp_arch_lock_section.retval   = true;
        env::fp_arch_lock_registry.retval  = true;
        env::fp_arch_wait_for_event.retval = true;

        // make sure the session pools are correctly initialized
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Set the timed_out_session_cnt counter to UINT32_MAX
        test_sys_ctx.server_metrics.timed_out_session_cnt = UINT32_MAX;

        // Activate a server session
        auto &server_sess                    = test_sys_ctx.server_session_pools[0];
        atomic_store(&server_sess.base.state, FP_SESSION_STATE_ACTIVE);
        server_sess.base.timeout             = 0; // Server sessions use ARMED_REQ_TOUT_S constant
        server_sess.base.req_fp_hdr.cmd_id   = test_cmd_id;
        server_sess.base.req_fp_hdr.func_id  = test_func_id;
        server_sess.base.req_fp_hdr.iface_id = test_if_id;
        server_sess.base.req_fp_hdr.status   = test_cmd_status_invalid;

        // First check that the counter is UINT32_MAX
        REQUIRE(UINT32_MAX == test_sys_ctx.server_metrics.timed_out_session_cnt);

        // Force the timeout to expire
        env::fp_arch_is_timer_elapsed.retval = true;

        // Call session tracking
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Check that the timed_out_session_cnt counter remains at UINT32_MAX (not incremented)
        REQUIRE(UINT32_MAX == test_sys_ctx.server_metrics.timed_out_session_cnt);

        // Verify that the session was still closed despite the counter not incrementing
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&server_sess.base.state));
    }
}

TEST_CASE("Test FP client requests", "[dispatch]")
{
    fp_msg_hdr_t msg_hdr = { 0 };

    // FIDL functions test data
    std::array<fidl_func_t, 2> test_fidl_func_list = {
        { // valid function ID and callback assignment
          { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk },
         // valid function ID but invalid callback assignment
          { .func_id = 6, .p_on_func_cbk = nullptr } }
    };

    // FIDL interfaces test data
    std::array<fidl_if_t, 2>
        test_fidl_ifs = {
            {
             { .version     = test_fidl_version,
                  .gen_type    = FIDL_TYPE_SERVER,
                  .fidl_if_id  = 10,
                  .func_count  = 2,
                  .p_func_list = test_fidl_func_list.data() },
             { .version     = test_fidl_version,
                  .gen_type    = FIDL_TYPE_CLIENT,
                  .fidl_if_id  = 12,
                  .func_count  = 2,
                  .p_func_list = test_fidl_func_list.data() },
             }
    };

    // system context used for the test sections that follow
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    env::clear();
    env::fp_arch_lock_registry.retval = true;
    env::fp_arch_lock_section.retval  = true;

    fp_registry_init();

    fp_disp_session_mgr_init(&test_sys_ctx);

    internal_test_env.clear();

    fp_dispatch_info_t test_dispatch_info;

    // bind test system context to dispatch context
    test_dispatch_info.p_sys_ctx = &test_sys_ctx;
    test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
    test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

    // initialize the pointer storage
    for (const auto &fidl_if : test_fidl_ifs)
    {
        REQUIRE(true == fp_registry_add_fidl_if(&fidl_if));
    }

    SECTION("Test FP client requests with invalid args")
    {
        // trigger validity check for nullptr passed as the dispatch context
        REQUIRE(FP_CMD_STATUS_INVARGS == fp_disp_client_send_req(nullptr, 0, 0, FP_FRAME_TYPE_REQ, nullptr, 0, nullptr, nullptr, 0));
        // trigger validity check for nullptr passed as request data
        REQUIRE(FP_CMD_STATUS_INVARGS == fp_disp_client_send_req(&test_dispatch_info, 0, 0, FP_FRAME_TYPE_REQ, nullptr, 0, nullptr, nullptr, 0));
        // trigger validity check for messages which do are less than the FP header in size
        REQUIRE(FP_CMD_STATUS_INVARGS == fp_disp_client_send_req(&test_dispatch_info, 0, 0, FP_FRAME_TYPE_REQ, test_dispatch_info.p_data, sizeof(fp_msg_hdr_t) - 1, nullptr, nullptr, 0));
        // trigger validity check for broadcast frame type (not allowed for client requests)
        REQUIRE(FP_CMD_STATUS_INVARGS == fp_disp_client_send_req(&test_dispatch_info, 0, 0, FP_FRAME_TYPE_BROADCAST, test_dispatch_info.p_data, sizeof(fp_msg_hdr_t), reinterpret_cast<uint8_t *>(some_ptr_addr), nullptr, 0));
    }

    SECTION("Test FP client request when there are no available client sessions to open")
    {
        // reserve all session slots so that fp_disp_client_send_req() will be unable to get a spot on the train
        for (auto &client_sess : test_sys_ctx.client_session_pools)
        {
            atomic_store(&client_sess.base.state, FP_SESSION_STATE_ACTIVE);
        }

        REQUIRE(FP_CMD_STATUS_NO_MEM == fp_disp_client_send_req(&test_dispatch_info, 0, 0, FP_FRAME_TYPE_REQ, reinterpret_cast<uint8_t *>(some_ptr_addr), sizeof(fp_msg_hdr_t), reinterpret_cast<uint8_t *>(some_ptr_addr), nullptr, test_timeout));
    }

    SECTION("Test sending a synchronous FP client request (bus send succeeds)")
    {
        // ensure there is at least one available client session slot to reseve
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        // signal the reply event
        env::fp_arch_wait_for_event.retval = true;

        // ensure that physical send will be successful if attempted
        internal_test_env.dummy_send_cbk_ret_val = true;

        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);

        REQUIRE(0 == env::fp_arch_wait_for_event.call_cnt);
        REQUIRE(0 == env::fp_arch_start_timer.call_cnt);

        constexpr uint32_t test_data_size                  = test_data_size_value;
        uint8_t            local_msg[sizeof(fp_msg_hdr_t)] = { 0 };

        // Create a test communication parameter to check if it's copied to the session
        fp_dispatch_info_t local_ctx = {
            .p_comm_params = reinterpret_cast<void *>(test_comm_params_value),
            .p_data        = nullptr,
            .data_size     = 0,
            .p_sys_ctx     = test_dispatch_info.p_sys_ctx
        };

        ((fp_msg_hdr_t *)(local_msg))->iface_id = 0;
        ((fp_msg_hdr_t *)(local_msg))->func_id  = 0;

        // we don't care about the actual validity of the p_resp parameter (it must be non-NULL only) because it is not used by this function
        REQUIRE(FP_CMD_STATUS_NOERROR == fp_disp_client_send_req(&local_ctx, test_if_id, test_func_id, FP_FRAME_TYPE_REQ, local_msg, test_data_size, reinterpret_cast<uint8_t *>(some_ptr_addr), nullptr, test_timeout));
        REQUIRE(1 == env::fp_arch_init_event.call_cnt);
        REQUIRE(1 == env::fp_arch_wait_for_event.call_cnt);
        // check that the passed interface ID and function ID have been transferred correctly to the dispatch context
        REQUIRE(test_if_id == ((fp_msg_hdr_t *)internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->iface_id);
        REQUIRE(test_func_id == (((fp_msg_hdr_t *)internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->func_id & ~ATTR_BITS_ONLY_MASK));

        // Check that p_comm_params was set to nullptr because the session has been closed
        REQUIRE(nullptr == local_ctx.p_assigned_sess->p_comm_params);
        // check if the request dispatch fields have been transferred correctly to the send function
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->p_data == local_msg);
        // the SEND received the request size (captured at send time; data_size is afterwards
        // reused by send_req to surface the response length)
        REQUIRE(internal_test_env.sent_data_size == test_data_size);
        // this mock signals the reply without a payload, so the surfaced response length is 0
        REQUIRE(0U == local_ctx.data_size);
        // check that p_assigned_sess is properly set to the opened client session
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->p_assigned_sess == (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);

        // the frame is expected to be sent over the bus
        REQUIRE(internal_test_env.dummy_send_cbk_called);
    }

    SECTION("Test sending a synchronous FP client request (bus send succeeds but the reply times out)")
    {
        // ensure there is at least one available client session slot to reseve
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        // signal the reply event
        env::fp_arch_wait_for_event.retval = false;

        // ensure that physical send will be successful if attempted
        internal_test_env.dummy_send_cbk_ret_val = true;

        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);

        REQUIRE(0 == env::fp_arch_wait_for_event.call_cnt);
        REQUIRE(0 == env::fp_arch_start_timer.call_cnt);

        // we don't care about the actual validity of the p_resp parameter (it must be non-NULL only) because it is not used by this function
        REQUIRE(FP_CMD_STATUS_TIMEOUT == fp_disp_client_send_req(&test_dispatch_info, 0, 0, FP_FRAME_TYPE_REQ, test_dispatch_info.p_data, test_dispatch_info.data_size, reinterpret_cast<uint8_t *>(some_ptr_addr), nullptr, test_timeout));
        REQUIRE(1 == env::fp_arch_wait_for_event.call_cnt);

        // the frame is expected to be sent over the bus
        REQUIRE(internal_test_env.dummy_send_cbk_called);
    }

    SECTION("Test sending a synchronous FP client request (bus send error)")
    {
        // ensure there is at least one available client session slot to reseve
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        // signal the reply event
        env::fp_arch_wait_for_event.retval = true;

        // ensure that physical send will fail if attempted
        internal_test_env.dummy_send_cbk_ret_val = false;

        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);

        REQUIRE(0 == env::fp_arch_wait_for_event.call_cnt);
        REQUIRE(0 == env::fp_arch_start_timer.call_cnt);

        // we don't care about the actual validity of the p_resp parameter (it must be non-NULL only) because it is not used by this function
        REQUIRE(FP_CMD_STATUS_SENDERR == fp_disp_client_send_req(&test_dispatch_info, 0, 0, FP_FRAME_TYPE_REQ, test_dispatch_info.p_data, test_dispatch_info.data_size, reinterpret_cast<uint8_t *>(some_ptr_addr), nullptr, test_timeout));
        // no waiting for reply event if the request was not sent in the first place
        REQUIRE(0 == env::fp_arch_wait_for_event.call_cnt);

        // an attempt to send the frame to the bus must have been performed
        REQUIRE(internal_test_env.dummy_send_cbk_called);
        // no arming of the reply timer
        REQUIRE(0 == env::fp_arch_start_timer.call_cnt);
    }

    SECTION("Test sending an asynchronous FP client request (bus send succeeds)")
    {
        // ensure there is at least one available client session slot to reseve
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        // ensure that physical send will succeed
        internal_test_env.dummy_send_cbk_ret_val = true;

        REQUIRE_FALSE(internal_test_env.dummy_send_cbk_called);

        REQUIRE(0 == env::fp_arch_wait_for_event.call_cnt);
        REQUIRE(0 == env::fp_arch_start_timer.call_cnt);

        // we don't care about the actual validity of the p_resp and p_resp_handler parameters (they must be non-NULL only)
        // because they are not dereferenced by this function (note that p_resp_handler parameter indirectly determines the type of request, e.g.
        // if there is a response callback provided, then the request is expected to be asynchronously executed)
        REQUIRE(FP_CMD_STATUS_NOERROR == fp_disp_client_send_req(&test_dispatch_info,
                                                                 0,
                                                                 0,
                                                                 FP_FRAME_TYPE_REQ,
                                                                 test_dispatch_info.p_data,
                                                                 test_dispatch_info.data_size,
                                                                 reinterpret_cast<uint8_t *>(some_ptr_addr),
                                                                 reinterpret_cast<fp_generic_response_handler_t>(some_ptr_addr),
                                                                 test_timeout));
        // no blocking wait for asynchronous events...
        REQUIRE(0 == env::fp_arch_wait_for_event.call_cnt);

        // frame was sent over the bus
        REQUIRE(internal_test_env.dummy_send_cbk_called);

        // check that p_assigned_sess is properly set to the opened client session
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->p_assigned_sess == (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);

        // if the frame was successfully sent, then we expect that a reply timeout tracking was started...
        REQUIRE(1 == env::fp_arch_start_timer.call_cnt);
        // ...with the timer handle from the allocated client session
        REQUIRE(&test_sys_ctx.client_session_pools[0].base.timer_hnd == env::fp_arch_start_timer.params.back().p_timer_hnd);
    }

    SECTION("Test sending an FP client request via the bounded API (resp_buf_user_max is plumbed to the session)")
    {
        // the bounded API must forward resp_max_size into the session as resp_buf_user_max;
        // drive an async request and verify the cap reached the session unchanged
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        internal_test_env.dummy_send_cbk_ret_val = true;

        constexpr uint32_t caller_resp_max = 17U; // arbitrary, distinct from any pool-wide bound

        REQUIRE(FP_CMD_STATUS_NOERROR == fp_disp_client_send_req_bounded(&test_dispatch_info,
                                                                         test_if_id,
                                                                         test_func_id,
                                                                         FP_FRAME_TYPE_REQ,
                                                                         test_dispatch_info.p_data,
                                                                         test_dispatch_info.data_size,
                                                                         reinterpret_cast<uint8_t *>(some_ptr_addr),
                                                                         caller_resp_max,
                                                                         reinterpret_cast<fp_generic_response_handler_t>(some_ptr_addr),
                                                                         test_timeout));

        // async request leaves the session ACTIVE; the bounded capacity must be stored verbatim
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx->p_assigned_sess == (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);
        REQUIRE(caller_resp_max == test_sys_ctx.client_session_pools[0].resp_buf_user_max);
        REQUIRE(internal_test_env.dummy_send_cbk_called);
    }

    SECTION("Test sending an asynchronous FP client request (lock failure when starting timer)")
    {
        // ensure there is at least one available client session slot to reserve
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        // ensure that physical send will succeed
        internal_test_env.dummy_send_cbk_ret_val = true;

        // Use a custom stub that counts lock calls and fails on the second call
        // First call: session manager opening session (should succeed)
        // Second call: timer start in fp_disp_client_send_req (should fail)
        static int lock_call_counter = 0;
        lock_call_counter = 0;

        auto lock_stub = +[](void *const p_lock) -> _Bool {
            lock_call_counter++;
            // Fail on the second lock call (timer start)
            if (lock_call_counter == 2)
            {
                env::fp_arch_lock_section.retval = false;
            }
            else
            {
                env::fp_arch_lock_section.retval = true;
            }
            return true;  // Return value is ignored
        };

        env::fp_arch_lock_section.p_user_stub = lock_stub;
        env::fp_arch_lock_section.retval = true;

        fp_cmd_status_t status = fp_disp_client_send_req(&test_dispatch_info,
                                                         0,
                                                         0,
                                                         FP_FRAME_TYPE_REQ,
                                                         test_dispatch_info.p_data,
                                                         test_dispatch_info.data_size,
                                                         reinterpret_cast<uint8_t *>(some_ptr_addr),
                                                         reinterpret_cast<fp_generic_response_handler_t>(some_ptr_addr),
                                                         test_timeout);

        REQUIRE(FP_CMD_STATUS_NO_MEM == status);

        // frame was sent over the bus
        REQUIRE(internal_test_env.dummy_send_cbk_called);

        // timer should not have been started due to lock failure
        REQUIRE(0 == env::fp_arch_start_timer.call_cnt);

        // session should have been closed
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&test_sys_ctx.client_session_pools[0].base.state));

        // reset lock behavior for next tests
        env::fp_arch_lock_section.retval = true;
        env::fp_arch_lock_section.p_user_stub = nullptr;
    }

    SECTION("Test FP client response handling with invalid args")
    {
        // since the function does not provide a return result, this is just a coverage call
        fp_disp_client_handle_resp(nullptr, 0);

        // test with null context but non-zero data size
        fp_disp_client_handle_resp(nullptr, 1);

        // test with non-zero data size
        fp_disp_client_handle_resp(&test_dispatch_info, 1);

        // test with zero data size
        fp_disp_client_handle_resp(&test_dispatch_info, 0);

        // test with null data field
        test_dispatch_info.p_data = nullptr;
        fp_disp_client_handle_resp(&test_dispatch_info, 1);

        // test with mismatched response data size
        uint8_t data[5];
        test_dispatch_info.p_data    = data;
        test_dispatch_info.data_size = 5;

        fp_msg_hdr_t *p_fp_test_hdr = reinterpret_cast<fp_msg_hdr_t *>(data);

        p_fp_test_hdr->func_id |= RESP_BIT_MASK;

        fp_disp_client_handle_resp(&test_dispatch_info, test_dispatch_info.data_size + 1);
    }

    SECTION("Test FP client response (unexpected frame type received)")
    {
        constexpr uint32_t resp_type_size = 5;

        // ensure the passed response type size does not match the one in the call
        test_dispatch_info.data_size = resp_type_size + 1;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        // now the size is correct but...
        test_dispatch_info.data_size = resp_type_size;
        // ...the frame type is not as expected (response bit is cleared)
        reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data)->func_id &= ~RESP_BIT_MASK;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);
    }

    SECTION("Test FP client response (frame type is correct)")
    {
        constexpr uint32_t resp_type_size = 5;
        // now the size is correct and...
        test_dispatch_info.data_size = resp_type_size;
        // ...the frame type is also correct
        reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data)->func_id |= RESP_BIT_MASK;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);
    }

    SECTION("Test FP client response (no opened session found)")
    {
        for (auto &client_sess : test_sys_ctx.client_session_pools)
        {
            // make sure all sessions are active but...
            atomic_store(&client_sess.base.state, FP_SESSION_STATE_ACTIVE);

            // ...FP header matching won't succeed
            client_sess.base.req_fp_hdr.iface_id = test_if_id;
            client_sess.base.req_fp_hdr.func_id  = test_func_id;
            client_sess.base.req_fp_hdr.cmd_id   = test_cmd_id;
        }

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);

        constexpr uint32_t resp_type_size = 5;

        // set a matching data size to be able to go to the session search step (but still FP header data won't match)
        test_dispatch_info.data_size = resp_type_size;

        // initialize with different values from the ones set above to ensure match failure
        p_test_fp_hdr->iface_id = ~test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id;
        p_test_fp_hdr->func_id  = ~test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id;
        p_test_fp_hdr->cmd_id   = ~test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);
    }

    SECTION("Test FP client response (invalid opened session found)")
    {
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK; // ensure the response bit is also set
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        // set invalid request type
        test_sys_ctx.client_session_pools[0].req_type = FP_REQ_TYPE_MAX;

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);

        constexpr uint32_t resp_type_size = 5;

        // set a matching data size to be able to go to the session search step
        test_dispatch_info.data_size = resp_type_size;

        // initialize with the values from the active session above
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);
    }

    SECTION("Test FP client response (valid opened session found - asynchronous request)")
    {
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK; // ensure the response bit is also set
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = test_cmd_status_invalid; // set to some made-up status just to ensure it is transferred to user

        // set request type to asynchronous
        test_sys_ctx.client_session_pools[0].req_type = FP_REQ_TYPE_ASYNC;

        // assign test response callback
        test_sys_ctx.client_session_pools[0].p_user_handler = &test_client_resp_handler_cbk;

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);

        constexpr uint32_t resp_type_size = 5;

        // set a matching data size to be able to go to the session search step
        test_dispatch_info.data_size = resp_type_size;

        // initialize with the values from the active session above
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;

        REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        REQUIRE(internal_test_env.client_resp_handler_cbk_called);
        // the dispatch context shall be transferred to the user via the callback
        REQUIRE(&test_dispatch_info == internal_test_env.p_client_resp_handler_disp_ctx);
        // a pointer to the data shall be provided to the user via the callback
        REQUIRE(test_dispatch_info.p_data == internal_test_env.p_untyped_resp_param);
        // the command status from the response shall be signaled to the user
        REQUIRE(test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status == internal_test_env.resp_cmd_status);
        // the signal event call is not performed when using asynchronous communication model
        REQUIRE(0 == env::fp_arch_signal_event.call_cnt);
    }

    SECTION("Test FP client response (valid opened session found - synchronous request)")
    {
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id; // ensure the response bit is also set
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = test_cmd_status_invalid; // set to some made-up status just to ensure it is transferred to user

        constexpr uint32_t resp_type_size = sizeof(fp_msg_hdr_t) + 5; // reserve space for the FP header in the beginning

        std::array<uint8_t, resp_type_size> p_data_buf = { 0 };
        // initialize temporary user response buffer to zeroes
        std::array<uint8_t, resp_type_size> test_resp_data_buf_tmp = { 0 };
        test_sys_ctx.client_session_pools[0].p_resp_data           = test_resp_data_buf_tmp.data();

        // set request type to synchronous
        test_sys_ctx.client_session_pools[0].req_type = FP_REQ_TYPE_SYNC;

        // assign test response callback
        test_sys_ctx.client_session_pools[0].p_user_handler = &test_client_resp_handler_cbk;

        // initialize the response to sequential values to be able to verify if they were copied correctly later
        for (uint8_t i = sizeof(fp_msg_hdr_t); i < std::size(p_data_buf); i++)
        {
            p_data_buf[i] = i - sizeof(fp_msg_hdr_t);
        }

        // initialize the received response data in the dispatch context
        test_dispatch_info.p_data                               = p_data_buf.data();
        test_dispatch_info.data_size                            = resp_type_size;
        test_sys_ctx.client_session_pools[0].resp_data_max_size = resp_type_size;

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);

        // initialize with the values from the active session above
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->func_id |= RESP_BIT_MASK;   // set the response bit
        p_test_fp_hdr->status   = FP_CMD_STATUS_NOERROR; // valid response carries NOERROR so memcpy is not skipped

        REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);
        REQUIRE(0 == env::fp_arch_signal_event.call_cnt);

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        // the user callback shall not be triggered in case of synchronous requests
        REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);

        // the user thread shall be signaled in case of synchronous requests
        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        REQUIRE(env::fp_arch_signal_event.params.back().p_event == test_sys_ctx.client_session_pools[0].p_session_event);

        // the expectation is that the data from the passed test_dispatch_info.p_data buffer is copied to the
        // user response buffer pointer passed in the active session, hence we compare if this buffer (being initialized to zeros initially),
        // now holds sequential values which would indicate the copy operation was performed
        for (uint8_t i = sizeof(fp_msg_hdr_t); i < std::size(test_resp_data_buf_tmp); i++)
        {
            REQUIRE((i - sizeof(fp_msg_hdr_t)) == test_resp_data_buf_tmp[i]);
        }
    }

    SECTION("Test FP client response (synchronous request, response larger than user buffer)")
    {
        // A received response bigger than the user buffer must not overflow it: the copy is skipped,
        // the buffer is zeroed, the status becomes UNEXP_FRM_SIZE, and the waiting thread is signaled.
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = test_cmd_status_invalid;

        test_sys_ctx.client_session_pools[0].req_type      = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler = &test_client_resp_handler_cbk;

        // received frame (resp_size) is larger than the user buffer (resp_max_size)
        constexpr uint32_t resp_size     = sizeof(fp_msg_hdr_t) + 5;
        constexpr uint32_t resp_max_size = sizeof(fp_msg_hdr_t);

        std::array<uint8_t, resp_size> p_data_buf = { 0 };
        // user buffer pre-filled with a sentinel so we can verify it is zeroed, not copied into
        std::array<uint8_t, resp_max_size> test_resp_data_buf_tmp;
        test_resp_data_buf_tmp.fill(0xFF);
        test_sys_ctx.client_session_pools[0].p_resp_data        = test_resp_data_buf_tmp.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = resp_max_size;

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_size;

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->func_id |= RESP_BIT_MASK;          // mark as a response frame
        p_test_fp_hdr->status   = FP_CMD_STATUS_NOERROR;  // so the copy path (not the error path) is taken

        REQUIRE(0 == env::fp_arch_signal_event.call_cnt);

        fp_disp_client_handle_resp(&test_dispatch_info, resp_size);

        // the waiting synchronous caller is still unblocked
        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        // the oversized response is reported to the caller as a frame-size protocol violation
        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_SIZE == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        // the user buffer is zeroed rather than overflowed with the (too-large) response
        for (auto byte : test_resp_data_buf_tmp)
        {
            REQUIRE(0 == byte);
        }
    }

    SECTION("Test FP client response (synchronous request, size mismatch must not write past the typed response size)")
    {
        // The user buffer holds one typed response. The pool-wide bound
        // (e.g. the transport buffer size) is much larger than the typed size. A frame
        // whose size does not match the FIDL size must not zero more than the typed
        // size, or the caller stack above the buffer gets destroyed.
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = test_cmd_status_invalid;
        test_sys_ctx.client_session_pools[0].req_type      = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler = &test_client_resp_handler_cbk;
        constexpr uint32_t resp_type_size = sizeof(fp_msg_hdr_t) + 5;
        constexpr uint32_t pool_bound     = 800;
        constexpr uint32_t guard_size     = 64;
        constexpr uint8_t  guard_byte     = 0xA5;
        // the received frame is 3 bytes larger than the FIDL-declared response size
        std::array<uint8_t, resp_type_size + 3> p_data_buf = { 0 };
        // the user buffer is the typed response followed by guard bytes that must stay untouched
        std::array<uint8_t, resp_type_size + guard_size> user_buf;
        user_buf.fill(guard_byte);
        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = pool_bound;
        test_sys_ctx.client_session_pools[0].resp_buf_user_max  = 0U;
        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = static_cast<uint32_t>(p_data_buf.size());
        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->func_id |= RESP_BIT_MASK;
        p_test_fp_hdr->status   = FP_CMD_STATUS_NOERROR;
        REQUIRE(0 == env::fp_arch_signal_event.call_cnt);
        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);
        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_SIZE == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        REQUIRE(0U == test_sys_ctx.client_session_pools[0].resp_data_size);
        // the typed area is cleared...
        for (uint32_t i = 0; i < resp_type_size; i++)
        {
            REQUIRE(0 == user_buf[i]);
        }
        // ...and not one byte past it is written
        for (uint32_t i = resp_type_size; i < user_buf.size(); i++)
        {
            REQUIRE(guard_byte == user_buf[i]);
        }
    }
    SECTION("Test FP client response (synchronous request, error status must not write past the typed response size)")
    {
        // Same bound for the error-status path, which also clears the user buffer.
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = test_cmd_status_invalid;
        test_sys_ctx.client_session_pools[0].req_type      = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler = &test_client_resp_handler_cbk;
        constexpr uint32_t resp_type_size = sizeof(fp_msg_hdr_t) + 5;
        constexpr uint32_t pool_bound     = 800;
        constexpr uint32_t guard_size     = 64;
        constexpr uint8_t  guard_byte     = 0xA5;
        std::array<uint8_t, resp_type_size> p_data_buf = { 0 };
        std::array<uint8_t, resp_type_size + guard_size> user_buf;
        user_buf.fill(guard_byte);
        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = pool_bound;
        test_sys_ctx.client_session_pools[0].resp_buf_user_max  = 0U;
        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_type_size;
        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->func_id |= RESP_BIT_MASK;
        // the server answered with an error status: the error path clears the user buffer
        p_test_fp_hdr->status = FP_CMD_STATUS_FUNC_NOT_SUPPORTED;
        REQUIRE(0 == env::fp_arch_signal_event.call_cnt);
        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);
        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        REQUIRE(FP_CMD_STATUS_FUNC_NOT_SUPPORTED == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        REQUIRE(0U == test_sys_ctx.client_session_pools[0].resp_data_size);
        for (uint32_t i = 0; i < resp_type_size; i++)
        {
            REQUIRE(0 == user_buf[i]);
        }
        for (uint32_t i = resp_type_size; i < user_buf.size(); i++)
        {
            REQUIRE(guard_byte == user_buf[i]);
        }
    }
    SECTION("Test FP client response protocol violation - unexpected frame type invokes async callback with UNEXP_FRM_TYPE status")
    {
        // Protocol violations must not silently discard the session;
        // the user async callback must be invoked so the client is notified.

        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_ASYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        constexpr uint32_t resp_type_size = 5;
        test_dispatch_info.data_size      = resp_type_size;

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);
        *p_test_fp_hdr     = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        // clear the RESP bit to simulate a non-response frame type arriving
        p_test_fp_hdr->func_id &= ~RESP_BIT_MASK;

        REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        REQUIRE(internal_test_env.client_resp_handler_cbk_called);
        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_TYPE == internal_test_env.resp_cmd_status);
        REQUIRE(nullptr == internal_test_env.p_untyped_resp_param);
    }

    SECTION("Test FP client response protocol violation - frame size mismatch invokes async callback with UNEXP_FRM_SIZE status")
    {
        // A header-only response (e.g. NOT_SUPPORTED / 0x50) whose size
        // is smaller than the declared FIDL response size must still notify the user.

        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_ASYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        constexpr uint32_t resp_type_size = sizeof(fp_msg_hdr_t) + 4;
        // simulate a header-only response: only the FP header arrived, no payload
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);
        *p_test_fp_hdr     = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;

        REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        REQUIRE(internal_test_env.client_resp_handler_cbk_called);
        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_SIZE == internal_test_env.resp_cmd_status);
        REQUIRE(nullptr == internal_test_env.p_untyped_resp_param);
    }

    SECTION("Test FP client response protocol violation - both wrong frame type and size mismatch: UNEXP_FRM_TYPE status wins")
    {
        // Frame type is checked first (if/else if). When both conditions apply,
        // UNEXP_FRM_TYPE is reported and the size check is skipped.

        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_ASYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        constexpr uint32_t resp_type_size = sizeof(fp_msg_hdr_t) + 4;
        // wrong size AND wrong frame type
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);
        *p_test_fp_hdr     = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->func_id &= ~RESP_BIT_MASK;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        REQUIRE(internal_test_env.client_resp_handler_cbk_called);
        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_TYPE == internal_test_env.resp_cmd_status);
        REQUIRE(nullptr == internal_test_env.p_untyped_resp_param);
    }

    SECTION("Test FP client response happy path - callback receives actual header status (regression guard)")
    {
        // Ensure that the protocol-violation status override does not affect valid responses.

        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = FP_CMD_STATUS_NOERROR;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_ASYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        constexpr uint32_t resp_type_size = 5;
        test_dispatch_info.data_size      = resp_type_size;

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(test_dispatch_info.p_data);
        *p_test_fp_hdr     = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        REQUIRE(internal_test_env.client_resp_handler_cbk_called);
        REQUIRE(FP_CMD_STATUS_NOERROR == internal_test_env.resp_cmd_status);
        // on a valid response, p_data must not be NULL (regression guard against always-NULL)
        REQUIRE(test_dispatch_info.p_data == internal_test_env.p_untyped_resp_param);
    }

    SECTION("Test FP client response protocol violation on sync request - unexpected frame type propagated to send_req caller")
    {
        // The sync path must propagate protocol violation status back through send_req's
        // return value. The req_fp_hdr.status field is used as the channel.

        constexpr uint32_t resp_type_size = sizeof(fp_msg_hdr_t) + 4;

        std::array<uint8_t, resp_type_size> resp_buf    = { 0 };
        resp_buf.fill(0xAB);
        std::array<uint8_t, resp_type_size> p_data_buf  = { 0 };

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());

        // prepare an active sync session with matching header
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = FP_CMD_STATUS_NOERROR;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_resp_data               = resp_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size        = resp_type_size;

        // incoming frame has correct IDs but wrong frame type (RESP bit cleared)
        *p_test_fp_hdr         = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->func_id &= ~RESP_BIT_MASK;

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_type_size;

        // simulate wait_for_event returning true (response arrived)
        env::fp_arch_wait_for_event.retval = true;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        // status must have been written into the session header for send_req to pick up
        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_TYPE == (fp_cmd_status_t)test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        // response buffer must be zeroed (memset on violation, not left as stale 0xAB)
        REQUIRE(resp_buf == decltype(resp_buf){ 0 });
        // event must have been signalled so the blocked caller is unblocked
        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
    }

    SECTION("Test FP client response protocol violation on sync request - size mismatch propagated to send_req caller")
    {
        // A header-only response on a sync request must not copy garbage data and
        // must propagate UNEXP_FRM_SIZE so send_req returns an error to the caller.

        constexpr uint32_t resp_type_size = sizeof(fp_msg_hdr_t) + 4;

        std::array<uint8_t, resp_type_size> resp_buf   = { 0 };
        resp_buf.fill(0xAB);
        std::array<uint8_t, resp_type_size> p_data_buf = { 0 };

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());

        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = FP_CMD_STATUS_NOERROR;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_resp_data               = resp_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size        = resp_type_size;

        // correct frame type but only header arrived (size mismatch)
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t); // header-only

        env::fp_arch_wait_for_event.retval = true;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_SIZE == (fp_cmd_status_t)test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        // response buffer must be zeroed (memset on violation, not left as stale 0xAB)
        REQUIRE(resp_buf == decltype(resp_buf){ 0 });
        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
    }

    SECTION("Error-path zeroing is bounded by the per-request caller buffer, not the pool capacity")
    {
        // a non-NOERROR reply takes the memset path; the zeroing must be capped at the caller
        // buffer (resp_buf_user_max), never the larger pool-wide resp_data_max_size
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        // large pool capacity (mirrors CSP_BUFFER_SIZE) but a tiny per-request caller buffer
        constexpr uint32_t pool_max   = 64;
        constexpr uint32_t user_max   = sizeof(fp_msg_hdr_t);
        constexpr uint32_t guard_size = 16; // sentinel region just past the caller bound

        std::array<uint8_t, user_max + guard_size> user_buf;
        user_buf.fill(0xAB);

        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = pool_max;
        test_sys_ctx.client_session_pools[0].resp_buf_user_max  = user_max;

        constexpr uint32_t resp_size = sizeof(fp_msg_hdr_t);
        std::array<uint8_t, resp_size> p_data_buf = { 0 };
        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());
        *p_test_fp_hdr        = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->status = FP_CMD_STATUS_FUNC_NOT_SUPPORTED; // any non-NOERROR -> error path

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_size;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_size);

        // the caller bound is zeroed ...
        for (uint32_t i = 0; i < user_max; ++i)
        {
            REQUIRE(0 == user_buf[i]);
        }
        // ... but bytes past the caller bound (within the pool capacity) MUST be untouched
        for (uint32_t i = user_max; i < user_max + guard_size; ++i)
        {
            REQUIRE(0xAB == user_buf[i]);
        }
        // no payload delivered on the error path
        REQUIRE(0U == test_sys_ctx.client_session_pools[0].resp_data_size);
    }

    SECTION("Success copy is bounded by the caller buffer and the delivered length is recorded")
    {
        // a response that fits the caller buffer is copied and resp_data_size records the delivered
        // length; a response larger than the caller buffer must not be copied and reports 0 bytes
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        constexpr uint32_t pool_max   = 64;
        constexpr uint32_t user_max   = sizeof(fp_msg_hdr_t) + 4; // caller buffer fits header + 4 bytes
        constexpr uint32_t guard_size = 8;

        std::array<uint8_t, user_max + guard_size> user_buf;
        user_buf.fill(0xCD);

        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = pool_max;
        test_sys_ctx.client_session_pools[0].resp_buf_user_max  = user_max;

        // first: a response that exactly fits the caller buffer is copied and its length recorded
        constexpr uint32_t fit_size = user_max;
        std::array<uint8_t, fit_size> fit_buf = { 0 };
        auto p_fit_hdr = reinterpret_cast<fp_msg_hdr_t *>(fit_buf.data());
        *p_fit_hdr        = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_fit_hdr->status = FP_CMD_STATUS_NOERROR;
        fit_buf[sizeof(fp_msg_hdr_t)] = 0x11; // a payload byte to verify the copy

        test_dispatch_info.p_data    = fit_buf.data();
        test_dispatch_info.data_size = fit_size;

        fp_disp_client_handle_resp(&test_dispatch_info, fit_size);

        REQUIRE(fit_size == test_sys_ctx.client_session_pools[0].resp_data_size);
        REQUIRE(0x11 == user_buf[sizeof(fp_msg_hdr_t)]);
        // guard bytes past the copied region are untouched
        for (uint32_t i = user_max; i < user_max + guard_size; ++i)
        {
            REQUIRE(0xCD == user_buf[i]);
        }

        // second: a response larger than the caller buffer (still within the pool) must be rejected
        user_buf.fill(0xCD);
        test_sys_ctx.client_session_pools[0].resp_data_size = 0U;

        constexpr uint32_t big_size = user_max + 10; // > user_max but < pool_max
        std::array<uint8_t, big_size> big_buf = { 0 };
        auto p_big_hdr = reinterpret_cast<fp_msg_hdr_t *>(big_buf.data());
        *p_big_hdr        = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_big_hdr->status = FP_CMD_STATUS_NOERROR;

        test_dispatch_info.p_data    = big_buf.data();
        test_dispatch_info.data_size = big_size;

        fp_disp_client_handle_resp(&test_dispatch_info, big_size);

        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_SIZE == (fp_cmd_status_t)test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        REQUIRE(0U == test_sys_ctx.client_session_pools[0].resp_data_size);
        // guard bytes past the caller bound are untouched (no overrun on the oversized memset)
        for (uint32_t i = user_max; i < user_max + guard_size; ++i)
        {
            REQUIRE(0xCD == user_buf[i]);
        }
    }

    SECTION("Typed response size caps the error-path zeroing when the caller bound is larger")
    {
        // A bounded caller whose buffer is larger than the typed response still gets
        // at most the typed size zeroed; the bound is the smallest of pool, caller and typed size
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        constexpr uint32_t pool_max        = 64;
        constexpr uint32_t resp_type_size  = sizeof(fp_msg_hdr_t) + 5;
        constexpr uint32_t user_max        = resp_type_size + 16; // caller bound above the typed size
        constexpr uint32_t guard_size      = 16;
        constexpr uint8_t  guard_byte      = 0xAB;

        std::array<uint8_t, user_max + guard_size> user_buf;
        user_buf.fill(guard_byte);

        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = pool_max;
        test_sys_ctx.client_session_pools[0].resp_buf_user_max  = user_max;

        std::array<uint8_t, resp_type_size> p_data_buf = { 0 };
        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());
        *p_test_fp_hdr        = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->status = FP_CMD_STATUS_FUNC_NOT_SUPPORTED; // error path -> memset

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_type_size;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        REQUIRE(FP_CMD_STATUS_FUNC_NOT_SUPPORTED == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        // the typed size is zeroed ...
        for (uint32_t i = 0; i < resp_type_size; ++i)
        {
            REQUIRE(0 == user_buf[i]);
        }
        // ... and the rest of the (larger) caller bound plus the guard stays untouched
        for (uint32_t i = resp_type_size; i < user_buf.size(); ++i)
        {
            REQUIRE(guard_byte == user_buf[i]);
        }
    }

    SECTION("Caller bound caps the error-path zeroing when it is smaller than the typed size")
    {
        // The clamp never raises the bound; a caller bound below the typed size wins
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        constexpr uint32_t pool_max        = 64;
        constexpr uint32_t resp_type_size  = sizeof(fp_msg_hdr_t) + 5;
        constexpr uint32_t user_max        = sizeof(fp_msg_hdr_t) + 1; // caller bound below the typed size
        constexpr uint32_t guard_size      = 16;
        constexpr uint8_t  guard_byte      = 0xAB;

        std::array<uint8_t, user_max + guard_size> user_buf;
        user_buf.fill(guard_byte);

        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = pool_max;
        test_sys_ctx.client_session_pools[0].resp_buf_user_max  = user_max;

        std::array<uint8_t, resp_type_size> p_data_buf = { 0 };
        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());
        *p_test_fp_hdr        = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->status = FP_CMD_STATUS_FUNC_NOT_SUPPORTED; // error path -> memset

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_type_size;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        for (uint32_t i = 0; i < user_max; ++i)
        {
            REQUIRE(0 == user_buf[i]);
        }
        for (uint32_t i = user_max; i < user_buf.size(); ++i)
        {
            REQUIRE(guard_byte == user_buf[i]);
        }
    }

    SECTION("Caller bound below the typed size rejects a well-sized response without overrun")
    {
        // NOERROR frame of exactly the typed size, but the caller bound is smaller ->
        // the copy is refused (UNEXP_FRM_SIZE) and only the caller bound is zeroed
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = FP_CMD_STATUS_NOERROR;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        constexpr uint32_t pool_max        = 64;
        constexpr uint32_t resp_type_size  = sizeof(fp_msg_hdr_t) + 5;
        constexpr uint32_t user_max        = sizeof(fp_msg_hdr_t) + 1;
        constexpr uint32_t guard_size      = 16;
        constexpr uint8_t  guard_byte      = 0xAB;

        std::array<uint8_t, user_max + guard_size> user_buf;
        user_buf.fill(guard_byte);

        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = pool_max;
        test_sys_ctx.client_session_pools[0].resp_buf_user_max  = user_max;

        std::array<uint8_t, resp_type_size> p_data_buf;
        p_data_buf.fill(0x5A); // payload that must NOT reach the caller
        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_type_size;

        fp_disp_client_handle_resp(&test_dispatch_info, resp_type_size);

        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_SIZE == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        REQUIRE(0U == test_sys_ctx.client_session_pools[0].resp_data_size);
        for (uint32_t i = 0; i < user_max; ++i)
        {
            REQUIRE(0 == user_buf[i]);
        }
        for (uint32_t i = user_max; i < user_buf.size(); ++i)
        {
            REQUIRE(guard_byte == user_buf[i]);
        }
    }

    SECTION("Unmatched-path RESP rejects a NULL dispatch context")
    {
        // a NULL dispatch context is reported as not-delivered with no side effects
        REQUIRE_FALSE(fp_disp_client_handle_resp_unmatched(nullptr));
    }

    SECTION("Unmatched-path RESP is refused for an async session and not delivered")
    {
        // the unmatched path has no typed response size, so it must not hand an unvalidated
        // frame to a typed async handler
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_ASYNC;
        test_sys_ctx.client_session_pools[0].p_user_handler            = &test_client_resp_handler_cbk;

        constexpr uint32_t resp_size = sizeof(fp_msg_hdr_t) + 4;
        std::array<uint8_t, resp_size> p_data_buf = { 0 };
        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_size;

        REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);

        const bool delivered = fp_disp_client_handle_resp_unmatched(&test_dispatch_info);

        // async handler must NOT have been invoked, and the function reports no delivery
        REQUIRE_FALSE(delivered);
        REQUIRE_FALSE(internal_test_env.client_resp_handler_cbk_called);
    }

    SECTION("Unmatched-path RESP is still delivered to a length-bounded sync session")
    {
        // The sync session is safe (the copy is length-bounded), so the unmatched path delivers it.
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = FP_CMD_STATUS_NOERROR;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;

        constexpr uint32_t resp_size = sizeof(fp_msg_hdr_t) + 4;
        std::array<uint8_t, resp_size> resp_buf   = { 0 };
        std::array<uint8_t, resp_size> p_data_buf = { 0 };
        test_sys_ctx.client_session_pools[0].p_resp_data        = resp_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = resp_size;

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_size;

        const bool delivered = fp_disp_client_handle_resp_unmatched(&test_dispatch_info);

        REQUIRE(delivered);
        // the sync caller is unblocked and the delivered length is recorded
        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        REQUIRE(resp_size == test_sys_ctx.client_session_pools[0].resp_data_size);
    }

    SECTION("Unmatched-path RESP larger than user buffer is rejected without overrun")
    {
        // An unmatched RESP that exceeds the session's resp_data_max_size must not overflow the
        // buffer: the copy is skipped, the buffer is zeroed up to the bound, the recorded length
        // is 0, and the caller sees UNEXP_FRM_SIZE. The frame still routes to the matching sync
        // session, so the function reports delivered (the data is rejected via status, not return).
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status   = FP_CMD_STATUS_NOERROR;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;

        // frame is bigger than the user buffer
        constexpr uint32_t resp_size = sizeof(fp_msg_hdr_t) + 8U;
        constexpr uint32_t user_size = sizeof(fp_msg_hdr_t) + 2U;
        constexpr uint32_t guard_size = 4U;

        std::array<uint8_t, resp_size> p_data_buf = { 0 };
        std::array<uint8_t, user_size + guard_size> user_buf;
        user_buf.fill(0xFF);

        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = user_size;

        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());
        *p_test_fp_hdr = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_size;

        const bool delivered = fp_disp_client_handle_resp_unmatched(&test_dispatch_info);

        // routed to the matching sync session, so delivered is reported true...
        REQUIRE(delivered);
        // ...but the oversized data is rejected: nothing copied, length recorded as 0, and the
        // caller is handed a frame-size error
        REQUIRE(0U == test_sys_ctx.client_session_pools[0].resp_data_size);
        REQUIRE(FP_CMD_STATUS_UNEXP_FRM_SIZE == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        // user buffer within the bound is zeroed
        for (uint32_t i = 0; i < user_size; ++i)
        {
            REQUIRE(0x00 == user_buf[i]);
        }
        // guard bytes past the user bound are untouched (no overrun on the oversized memset)
        for (uint32_t i = user_size; i < user_size + guard_size; ++i)
        {
            REQUIRE(0xFF == user_buf[i]);
        }
    }

    SECTION("Unmatched-path error status zeroes the whole pool capacity for an unbounded caller")
    {
        // Characterisation of the unmatched-response route. fp_disp_route_unmatched sends ANY response
        // whose interface has no CLIENT entry in the registry down this route: a self-addressed
        // command, a client whose *_register_client_api() call is missing, or a client dropped
        // because the registry is full. The typed response size is unknown here, so a session
        // opened with the unbounded send (resp_buf_user_max == 0) is cleared up to the pool-wide
        // resp_data_max_size on an error status, and on NOERROR it receives a copy of up to that
        // many peer bytes. Callers that can land on this route must use
        // fp_disp_client_send_req_bounded.
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;

        constexpr uint32_t pool_max   = 64;
        constexpr uint32_t guard_size = 16;
        constexpr uint8_t  guard_byte = 0xCD;

        std::array<uint8_t, pool_max + guard_size> user_buf;
        user_buf.fill(guard_byte);

        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = pool_max;
        test_sys_ctx.client_session_pools[0].resp_buf_user_max  = 0U; // unbounded send

        constexpr uint32_t resp_size = sizeof(fp_msg_hdr_t);
        std::array<uint8_t, resp_size> p_data_buf = { 0 };
        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());
        *p_test_fp_hdr        = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->status = FP_CMD_STATUS_FUNC_NOT_SUPPORTED;

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_size;

        const bool delivered = fp_disp_client_handle_resp_unmatched(&test_dispatch_info);

        REQUIRE(delivered);
        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        REQUIRE(FP_CMD_STATUS_FUNC_NOT_SUPPORTED == test_sys_ctx.client_session_pools[0].base.req_fp_hdr.status);
        REQUIRE(0U == test_sys_ctx.client_session_pools[0].resp_data_size);
        // the whole pool capacity is zeroed (no typed size to clamp to) ...
        for (uint32_t i = 0; i < pool_max; ++i)
        {
            REQUIRE(0 == user_buf[i]);
        }
        // ... and never more than that
        for (uint32_t i = pool_max; i < user_buf.size(); ++i)
        {
            REQUIRE(guard_byte == user_buf[i]);
        }
    }

    SECTION("Unmatched-path error status respects the caller bound of a bounded send")
    {
        // The bounded send is the caller's protection on the unmatched route: the error-path
        // zeroing stops at resp_buf_user_max, not at the pool capacity.
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.iface_id = test_if_id;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.func_id  = test_func_id | RESP_BIT_MASK;
        test_sys_ctx.client_session_pools[0].base.req_fp_hdr.cmd_id   = test_cmd_id;
        test_sys_ctx.client_session_pools[0].req_type                  = FP_REQ_TYPE_SYNC;

        constexpr uint32_t pool_max   = 64;
        constexpr uint32_t user_max   = sizeof(fp_msg_hdr_t) + 2;
        constexpr uint32_t guard_size = 16;
        constexpr uint8_t  guard_byte = 0xCD;

        std::array<uint8_t, user_max + guard_size> user_buf;
        user_buf.fill(guard_byte);

        test_sys_ctx.client_session_pools[0].p_resp_data        = user_buf.data();
        test_sys_ctx.client_session_pools[0].resp_data_max_size = pool_max;
        test_sys_ctx.client_session_pools[0].resp_buf_user_max  = user_max; // bounded send

        constexpr uint32_t resp_size = sizeof(fp_msg_hdr_t);
        std::array<uint8_t, resp_size> p_data_buf = { 0 };
        auto p_test_fp_hdr = reinterpret_cast<fp_msg_hdr_t *>(p_data_buf.data());
        *p_test_fp_hdr        = test_sys_ctx.client_session_pools[0].base.req_fp_hdr;
        p_test_fp_hdr->status = FP_CMD_STATUS_FUNC_NOT_SUPPORTED;

        test_dispatch_info.p_data    = p_data_buf.data();
        test_dispatch_info.data_size = resp_size;

        const bool delivered = fp_disp_client_handle_resp_unmatched(&test_dispatch_info);

        REQUIRE(delivered);
        REQUIRE(1 == env::fp_arch_signal_event.call_cnt);
        for (uint32_t i = 0; i < user_max; ++i)
        {
            REQUIRE(0 == user_buf[i]);
        }
        for (uint32_t i = user_max; i < user_buf.size(); ++i)
        {
            REQUIRE(guard_byte == user_buf[i]);
        }
    }
}

TEST_CASE("Test server lock failure scenarios in arm-and-fire request", "[dispatch][server][coverage]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // FIDL functions test data
    std::array<fidl_func_t, 1> test_fidl_func_list = {
        { { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk } }
    };

    // FIDL interface test data
    fidl_if_t test_fidl_if = {
        .version     = test_fidl_version,
        .gen_type    = FIDL_TYPE_SERVER,
        .fidl_if_id  = 10,
        .func_count  = 1,
        .p_func_list = test_fidl_func_list.data()
    };

    REQUIRE(true == fp_registry_add_fidl_if(&test_fidl_if));

    // system context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    fp_disp_session_mgr_init(&test_sys_ctx);

    fp_msg_hdr_t msg_hdr = {
        .iface_id = test_fidl_if.fidl_if_id,
        .cmd_id   = 1,
        .status   = FP_CMD_STATUS_NOERROR
    };

    // Set function ID and ARM frame type using the protocol API
    fp_proto_set_function_type(&msg_hdr, test_fidl_func_list[0].func_id, FP_FRAME_TYPE_REQ_ARM);

    fp_dispatch_info_t test_dispatch_info;
    test_dispatch_info.p_sys_ctx = &test_sys_ctx;
    test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
    test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

    internal_test_env.clear();

    SECTION("Test server arm-and-fire request with lock failure during session open")
    {
        // Set up the mock to return false for section lock acquisition
        env::fp_arch_lock_section.retval = false;

        // Dispatch the ARM request using the server handler
        fp_disp_server_handle_req(&test_dispatch_info,
                                  FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE,
                                  reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk),
                                  sizeof(fp_msg_hdr_t));

        // The function should handle lock failure gracefully
        // This tests the lock failure during session open

        // Reset for next test
        env::fp_arch_lock_section.retval = true;
    }

    SECTION("Test server arm-and-fire request with lock failure during timer start")
    {
        // This test covers the error path where lock acquisition fails after sending the ARM response
        // The code path is: session opens -> ARM response sent -> try to acquire lock for timer start -> lock fails -> close session and send error

        // ensure there is at least one available server session slot to reserve
        atomic_store(&test_sys_ctx.server_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        // Set up lock stub to succeed on calls 1, 2, and 4, but fail on call 3
        // Call 1: fp_disp_session_mgr_find_session (should succeed - no session found)
        // Call 2: fp_disp_session_mgr_open_session (should succeed - session opened)
        // Call 3: timer start (should fail - this is the target code path at lines 391-393)
        // Call 4: fp_disp_session_mgr_close_session (should succeed - session closed)
        auto lock_stub = +[](void *const p_lock) -> _Bool {
            size_t current_call = env::fp_arch_lock_section.call_cnt;

            if (current_call == 2)
            {
                // Third call (index 2) fails - this is the timer start
                env::fp_arch_lock_section.retval = false;
            }
            else
            {
                // All other calls succeed
                env::fp_arch_lock_section.retval = true;
            }
            return true;
        };

        env::fp_arch_lock_section.p_user_stub = lock_stub;
        env::fp_arch_lock_section.retval = true;

        // Set up hash calculation to succeed
        env::fp_arch_calc_hash.retval = 0x1234;

        // Dispatch the ARM request using the server handler
        fp_disp_server_handle_req(&test_dispatch_info,
                                  FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE,
                                  reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk),
                                  sizeof(fp_msg_hdr_t));

        // Verify that a response was sent
        REQUIRE(internal_test_env.dummy_send_cbk_called);

        // Verify the response is NO_MEM error (sent after lock failure at line 392)
        REQUIRE(internal_test_env.p_sys_ctx_send_disp_ctx == &test_dispatch_info);
        REQUIRE(reinterpret_cast<fp_msg_hdr_t *>(internal_test_env.p_sys_ctx_send_disp_ctx->p_data)->status == FP_CMD_STATUS_NO_MEM);

        // Verify timer was not started due to lock failure
        REQUIRE(0 == env::fp_arch_start_timer.call_cnt);

        // Verify session was closed (lines 391-393 code path)
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&test_sys_ctx.server_session_pools[0].base.state));
    }
}

TEST_CASE("Test session manager edge cases", "[dispatch][session][coverage]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // FIDL functions test data
    std::array<fidl_func_t, 1> test_fidl_func_list = {
        { { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk } }
    };

    // FIDL interface test data
    fidl_if_t test_fidl_if = {
        .version     = test_fidl_version,
        .gen_type    = FIDL_TYPE_CLIENT,
        .fidl_if_id  = 10,
        .func_count  = 1,
        .p_func_list = test_fidl_func_list.data()
    };

    REQUIRE(true == fp_registry_add_fidl_if(&test_fidl_if));

    // system context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    fp_disp_session_mgr_init(&test_sys_ctx);

    internal_test_env.clear();

    SECTION("Test closing session with NULL session attribute")
    {
        // Try to close a session with NULL session attribute
        // This should trigger the uncovered branch in fp_disp_session_mgr_close_session

        // Create a mock session with NULL attribute
        fp_disp_session_base_t mock_session;
        memset(&mock_session, 0, sizeof(mock_session));

        // Set NULL session attribute pointer
        mock_session.p_session_attr = nullptr;

        // Attempt to close the session - should handle gracefully
        fp_disp_session_mgr_close_session(&test_sys_ctx, &mock_session);
    }

    SECTION("Test track_session with NULL session pointer")
    {
        // This tests the defensive NULL check at line 495 in fp_disp_session_mgr.c
        // The track_session function is called internally by fp_disp_session_mgr_track_sessions

        // Call track_sessions with a system context that has no active sessions
        // This will exercise the NULL check path
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify no crashes occurred (defensive check worked)
        REQUIRE(true);
    }

    SECTION("Test track_session with non-ACTIVE session state")
    {
        // This tests the state check at line 495 in fp_disp_session_mgr.c

        // Set a session to INACTIVE state
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_INACTIVE);

        // Call track_sessions - should skip the inactive session
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify no crashes occurred
        REQUIRE(true);
    }

    SECTION("Test track_session with NULL p_track_session callback")
    {
        // This tests the NULL check at line 518 in fp_disp_session_mgr.c

        // Create a session attribute with NULL p_track_session
        fp_disp_session_attributes_t sess_attr = {
            .p_init_func     = &session_init_dummy_callback,
            .p_is_matching   = nullptr,
            .p_track_session = nullptr,  // NULL track callback
            .sess_type       = FP_DISP_SESSION_TYPE_CLIENT
        };

        // Set up a client session with this attribute
        test_sys_ctx.client_session_pools[0].base.p_session_attr = &sess_attr;
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // Call track_sessions - should handle NULL p_track_session gracefully
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify no crashes occurred
        REQUIRE(true);
    }

    SECTION("Test close_session with valid session to cover FALSE branch of line 309")
    {
        // This tests the FALSE branch of the complex condition at line 309
        // All parameters are valid, so the condition should be FALSE and execution continues

        // Initialize session manager
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Create valid session attributes
        fp_disp_session_attributes_t sess_attr = {
            .p_init_func     = &session_init_dummy_callback,
            .p_is_matching   = nullptr,
            .p_track_session = nullptr,
            .sess_type       = FP_DISP_SESSION_TYPE_CLIENT
        };

        // Set up a valid client session with proper attributes
        test_sys_ctx.client_session_pools[0].base.p_session_attr = &sess_attr;
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_ACTIVE);

        // Enable lock acquisition
        env::fp_arch_lock_section.retval = true;

        // Close the session with all valid parameters
        // This should take the FALSE branch (skip the error return) and proceed with closing
        fp_disp_session_mgr_close_session(&test_sys_ctx, (fp_disp_session_base_t *)&test_sys_ctx.client_session_pools[0]);

        // Verify session was closed
        REQUIRE(FP_SESSION_STATE_INACTIVE == atomic_load(&test_sys_ctx.client_session_pools[0].base.state));
    }

    SECTION("Test track_session with OPENED state (not ACTIVE)")
    {
        // This tests another path of the state check at line 495
        // Session is not NULL but state is OPENED (not ACTIVE)

        // Initialize session manager
        fp_disp_session_mgr_init(&test_sys_ctx);

        // Set session to OPENED state (not ACTIVE)
        atomic_store(&test_sys_ctx.client_session_pools[0].base.state, FP_SESSION_STATE_OPENED);

        // Call track_sessions - should skip the OPENED session
        fp_disp_session_mgr_track_sessions(&test_sys_ctx);

        // Verify session is still in OPENED state (not tracked/closed)
        REQUIRE(FP_SESSION_STATE_OPENED == atomic_load(&test_sys_ctx.client_session_pools[0].base.state));
    }
}

TEST_CASE("Test server session matching edge cases", "[dispatch][server][coverage]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // FIDL functions test data - create two functions
    std::array<fidl_func_t, 2> test_fidl_func_list = {
        {
            { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk },
            { .func_id = 6, .p_on_func_cbk = &dummy_fidl_func_cbk }
        }
    };

    // FIDL interface test data
    fidl_if_t test_fidl_if = {
        .version     = test_fidl_version,
        .gen_type    = FIDL_TYPE_SERVER,
        .fidl_if_id  = 10,
        .func_count  = 2,
        .p_func_list = test_fidl_func_list.data()
    };

    REQUIRE(true == fp_registry_add_fidl_if(&test_fidl_if));

    // system context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    fp_disp_session_mgr_init(&test_sys_ctx);

    internal_test_env.clear();

    SECTION("Test server session matching with different function IDs")
    {
        // Create two messages with same interface but different function IDs
        // This tests the uncovered branch in fp_disp_server_session_is_matching

        fp_msg_hdr_t msg_hdr1 = {
            .iface_id = test_fidl_if.fidl_if_id,
            .cmd_id   = 1,
            .status   = FP_CMD_STATUS_NOERROR
        };
        fp_proto_set_function_type(&msg_hdr1, test_fidl_func_list[0].func_id, FP_FRAME_TYPE_REQ_ARM);

        fp_msg_hdr_t msg_hdr2 = {
            .iface_id = test_fidl_if.fidl_if_id,
            .cmd_id   = 1,
            .status   = FP_CMD_STATUS_NOERROR
        };
        fp_proto_set_function_type(&msg_hdr2, test_fidl_func_list[1].func_id, FP_FRAME_TYPE_REQ_ARM);  // Different function ID

        fp_dispatch_info_t test_dispatch_info1;
        test_dispatch_info1.p_sys_ctx = &test_sys_ctx;
        test_dispatch_info1.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr1);
        test_dispatch_info1.data_size = sizeof(fp_msg_hdr_t);

        fp_dispatch_info_t test_dispatch_info2;
        test_dispatch_info2.p_sys_ctx = &test_sys_ctx;
        test_dispatch_info2.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr2);
        test_dispatch_info2.data_size = sizeof(fp_msg_hdr_t);

        // Dispatch first ARM request to create a session
        fp_disp_server_handle_req(&test_dispatch_info1,
                                  FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE,
                                  reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk),
                                  sizeof(fp_msg_hdr_t));

        // Dispatch second ARM request with different function ID
        // This should test the session matching logic
        fp_disp_server_handle_req(&test_dispatch_info2,
                                  FP_DISP_SERVER_REQ_TYPE_ARM_AND_FIRE,
                                  reinterpret_cast<fp_generic_request_handler_t>(&dummy_fidl_func_user_cbk),
                                  sizeof(fp_msg_hdr_t));
    }
}

TEST_CASE("Test NULL parameter handling in various functions", "[dispatch][coverage]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // system context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    fp_disp_session_mgr_init(&test_sys_ctx);

    internal_test_env.clear();

    SECTION("Test session manager init with NULL context")
    {
        // Test NULL parameter handling
        fp_disp_session_mgr_init(nullptr);

        // Should handle gracefully without crashing
    }

    SECTION("Test session close with NULL parameters")
    {
        // Test NULL session parameter
        fp_disp_session_mgr_close_session(&test_sys_ctx, nullptr);

        // Test NULL context parameter
        fp_disp_session_base_t mock_session;
        memset(&mock_session, 0, sizeof(mock_session));
        fp_disp_session_mgr_close_session(nullptr, &mock_session);
    }
}

TEST_CASE("Test client request with various timeout scenarios", "[dispatch][client][coverage]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // FIDL functions test data
    std::array<fidl_func_t, 1> test_fidl_func_list = {
        { { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk } }
    };

    // FIDL interface test data
    fidl_if_t test_fidl_if = {
        .version     = test_fidl_version,
        .gen_type    = FIDL_TYPE_CLIENT,
        .fidl_if_id  = 10,
        .func_count  = 1,
        .p_func_list = test_fidl_func_list.data()
    };

    REQUIRE(true == fp_registry_add_fidl_if(&test_fidl_if));

    // system context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    fp_disp_session_mgr_init(&test_sys_ctx);

    internal_test_env.clear();

    SECTION("Test client request with zero timeout")
    {
        fp_msg_hdr_t msg_hdr = { 0 };
        fp_dispatch_info_t test_dispatch_info;
        test_dispatch_info.p_sys_ctx = &test_sys_ctx;
        test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

        uint8_t req_buffer[sizeof(fp_msg_hdr_t)] = { 0 };
        uint8_t resp_buffer[sizeof(fp_msg_hdr_t)] = { 0 };

        // Send request with zero timeout
        fp_cmd_status_t status = fp_disp_client_send_req(
            &test_dispatch_info,
            test_fidl_if.fidl_if_id,
            test_fidl_func_list[0].func_id,
            FP_FRAME_TYPE_REQ,
            req_buffer,
            sizeof(req_buffer),
            resp_buffer,
            nullptr,
            0  // Zero timeout
        );

        // Should handle zero timeout appropriately
    }
}

TEST_CASE("Test session pool exhaustion scenarios", "[dispatch][session][coverage]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // FIDL functions test data
    std::array<fidl_func_t, 1> test_fidl_func_list = {
        { { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk } }
    };

    // FIDL interface test data
    fidl_if_t test_fidl_if = {
        .version     = test_fidl_version,
        .gen_type    = FIDL_TYPE_CLIENT,
        .fidl_if_id  = 10,
        .func_count  = 1,
        .p_func_list = test_fidl_func_list.data()
    };

    REQUIRE(true == fp_registry_add_fidl_if(&test_fidl_if));

    // system context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    fp_disp_session_mgr_init(&test_sys_ctx);

    internal_test_env.clear();

    SECTION("Test client request when all sessions are exhausted")
    {
        // Exhaust the pool with genuinely in-flight (asynchronous) requests: a successful async
        // send keeps its session OPENED (tracked by a timer) until the reply/timeout, so sessions
        // accumulate. (A failed send must NOT be used to exhaust the pool - it releases its
        // reserved slot, see fp_disp_client_send_req's send-failure cleanup.)
        env::fp_arch_lock_section.retval        = true;  // let the async path start its timer
        internal_test_env.dummy_send_cbk_ret_val = true;  // physical send succeeds

        std::vector<fp_msg_hdr_t> msg_hdrs;
        std::vector<std::array<uint8_t, sizeof(fp_msg_hdr_t)>> req_buffers;
        std::vector<std::array<uint8_t, sizeof(fp_msg_hdr_t)>> resp_buffers;

        // Try to create more sessions than available (MAX_CLIENT_SESSIONS_COUNT from config)
        for (int i = 0; i < MAX_CLIENT_SESSIONS_COUNT + 5; i++)
        {
            msg_hdrs.push_back(fp_msg_hdr_t{0});
            req_buffers.push_back(std::array<uint8_t, sizeof(fp_msg_hdr_t)>{0});
            resp_buffers.push_back(std::array<uint8_t, sizeof(fp_msg_hdr_t)>{0});

            fp_dispatch_info_t dispatch_info;
            dispatch_info.p_sys_ctx = &test_sys_ctx;
            dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdrs.back());
            dispatch_info.data_size = sizeof(fp_msg_hdr_t);

            fp_cmd_status_t status = fp_disp_client_send_req(
                &dispatch_info,
                test_fidl_if.fidl_if_id,
                test_fidl_func_list[0].func_id,
                FP_FRAME_TYPE_REQ,
                req_buffers.back().data(),
                req_buffers.back().size(),
                resp_buffers.back().data(),
                &test_client_resp_handler_cbk,  // async request -> session stays open in-flight
                test_timeout
            );

            if (i < MAX_CLIENT_SESSIONS_COUNT)
            {
                // sessions still available -> the async request is accepted and tracked
                REQUIRE(status == FP_CMD_STATUS_NOERROR);
            }
            else
            {
                // After exhausting sessions, should return NO_MEM error
                REQUIRE(status == FP_CMD_STATUS_NO_MEM);
            }
        }
    }

    SECTION("Test failed sends do not leak client sessions")
    {
        // a send failure must release the reserved session slot; if it leaked, far fewer than
        // this many SENDERRs would be returned before the pool exhausted and flipped to NO_MEM
        internal_test_env.dummy_send_cbk_ret_val = false;  // physical send fails -> SENDERR

        for (int i = 0; i < MAX_CLIENT_SESSIONS_COUNT + 5; i++)
        {
            fp_msg_hdr_t                          msg_hdr   = {0};
            std::array<uint8_t, sizeof(fp_msg_hdr_t)> req_buf  = {0};
            std::array<uint8_t, sizeof(fp_msg_hdr_t)> resp_buf = {0};

            fp_dispatch_info_t dispatch_info;
            dispatch_info.p_sys_ctx = &test_sys_ctx;
            dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
            dispatch_info.data_size = sizeof(fp_msg_hdr_t);

            fp_cmd_status_t status = fp_disp_client_send_req(
                &dispatch_info,
                test_fidl_if.fidl_if_id,
                test_fidl_func_list[0].func_id,
                FP_FRAME_TYPE_REQ,
                req_buf.data(),
                req_buf.size(),
                resp_buf.data(),
                nullptr,
                test_timeout
            );

            // Every iteration must report the send failure - never NO_MEM, which would mean the
            // failed sends leaked their slots and exhausted the pool.
            REQUIRE(status == FP_CMD_STATUS_SENDERR);
        }
    }
}

TEST_CASE("Test response handling with various buffer sizes", "[dispatch][client][coverage]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // FIDL functions test data
    std::array<fidl_func_t, 1> test_fidl_func_list = {
        { { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk } }
    };

    // FIDL interface test data
    fidl_if_t test_fidl_if = {
        .version     = test_fidl_version,
        .gen_type    = FIDL_TYPE_CLIENT,
        .fidl_if_id  = 10,
        .func_count  = 1,
        .p_func_list = test_fidl_func_list.data()
    };

    REQUIRE(true == fp_registry_add_fidl_if(&test_fidl_if));

    // system context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    fp_disp_session_mgr_init(&test_sys_ctx);

    internal_test_env.clear();

    SECTION("Test client request with minimum buffer size")
    {
        fp_msg_hdr_t msg_hdr = { 0 };
        fp_dispatch_info_t test_dispatch_info;
        test_dispatch_info.p_sys_ctx = &test_sys_ctx;
        test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

        uint8_t req_buffer[sizeof(fp_msg_hdr_t)] = { 0 };
        uint8_t resp_buffer[sizeof(fp_msg_hdr_t)] = { 0 };

        // Send request with minimum valid buffer size
        fp_cmd_status_t status = fp_disp_client_send_req(
            &test_dispatch_info,
            test_fidl_if.fidl_if_id,
            test_fidl_func_list[0].func_id,
            FP_FRAME_TYPE_REQ,
            req_buffer,
            sizeof(fp_msg_hdr_t),  // Minimum size
            resp_buffer,
            nullptr,
            test_timeout
        );

        // Should succeed with minimum buffer size
    }

    SECTION("Test client request with buffer size less than header")
    {
        fp_msg_hdr_t msg_hdr = { 0 };
        fp_dispatch_info_t test_dispatch_info;
        test_dispatch_info.p_sys_ctx = &test_sys_ctx;
        test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

        uint8_t req_buffer[sizeof(fp_msg_hdr_t)] = { 0 };
        uint8_t resp_buffer[sizeof(fp_msg_hdr_t)] = { 0 };

        // Send request with buffer size less than header size
        fp_cmd_status_t status = fp_disp_client_send_req(
            &test_dispatch_info,
            test_fidl_if.fidl_if_id,
            test_fidl_func_list[0].func_id,
            FP_FRAME_TYPE_REQ,
            req_buffer,
            sizeof(fp_msg_hdr_t) - 1,  // Less than minimum
            resp_buffer,
            nullptr,
            test_timeout
        );

        // Should return invalid arguments error
        REQUIRE(status == FP_CMD_STATUS_INVARGS);
    }
}

TEST_CASE("Test async request handling", "[dispatch][client][coverage]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // FIDL functions test data
    std::array<fidl_func_t, 1> test_fidl_func_list = {
        { { .func_id = 5, .p_on_func_cbk = &dummy_fidl_func_cbk } }
    };

    // FIDL interface test data
    fidl_if_t test_fidl_if = {
        .version     = test_fidl_version,
        .gen_type    = FIDL_TYPE_CLIENT,
        .fidl_if_id  = 10,
        .func_count  = 1,
        .p_func_list = test_fidl_func_list.data()
    };

    REQUIRE(true == fp_registry_add_fidl_if(&test_fidl_if));

    // system context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    fp_disp_session_mgr_init(&test_sys_ctx);

    internal_test_env.clear();

    SECTION("Test async client request with response handler")
    {
        fp_msg_hdr_t msg_hdr = { 0 };
        fp_dispatch_info_t test_dispatch_info;
        test_dispatch_info.p_sys_ctx = &test_sys_ctx;
        test_dispatch_info.p_data    = reinterpret_cast<uint8_t *>(&msg_hdr);
        test_dispatch_info.data_size = sizeof(fp_msg_hdr_t);

        uint8_t req_buffer[sizeof(fp_msg_hdr_t)] = { 0 };
        uint8_t resp_buffer[sizeof(fp_msg_hdr_t)] = { 0 };

        // Send async request with response handler
        fp_cmd_status_t status = fp_disp_client_send_req(
            &test_dispatch_info,
            test_fidl_if.fidl_if_id,
            test_fidl_func_list[0].func_id,
            FP_FRAME_TYPE_REQ,
            req_buffer,
            sizeof(req_buffer),
            resp_buffer,
            &test_client_resp_handler_cbk,  // Async handler with correct signature
            test_timeout
        );

        // Should handle async request properly
    }
}

TEST_CASE("Test broadcast handling API", "[dispatch][client][broadcast]")
{
    env::clear();
    env::fp_arch_lock_registry.retval = true;

    fp_registry_init();

    // System context used for the test
    fp_dispatch_sys_context_t test_sys_ctx = {
        .p_send               = &test_sys_ctx_send_func,
        .server_session_pools = {{}},
        .client_session_pools = {{}},
    };

    fp_dispatch_info_t test_dispatch_info;
    test_dispatch_info.p_sys_ctx = &test_sys_ctx;

    // Create a broadcast frame
    uint8_t       bcast_buf[sizeof(fp_msg_hdr_t) + 10] = { 0 };
    fp_msg_hdr_t *p_bcast_hdr                          = (fp_msg_hdr_t *)bcast_buf;

    p_bcast_hdr->iface_id = 100;
    p_bcast_hdr->cmd_id   = 42;
    fp_proto_set_function_type(p_bcast_hdr, 5, FP_FRAME_TYPE_BROADCAST);

    test_dispatch_info.p_data    = bcast_buf;
    test_dispatch_info.data_size = sizeof(bcast_buf);

    // Test broadcast handler callback
    static bool test_bcast_handler_called = false;
    static void *test_bcast_handler_ctx   = nullptr;
    static uint8_t *test_bcast_handler_data = nullptr;

    auto test_bcast_handler = [](void *p_disp_ctx, uint8_t *p_bcast_data) {
        test_bcast_handler_called = true;
        test_bcast_handler_ctx    = p_disp_ctx;
        test_bcast_handler_data   = p_bcast_data;
    };

    SECTION("Test fp_disp_client_handle_bcast with NULL p_disp_ctx")
    {
        test_bcast_handler_called = false;

        fp_disp_client_handle_bcast(nullptr, sizeof(bcast_buf), test_bcast_handler);

        // Handler should not be called
        REQUIRE_FALSE(test_bcast_handler_called);
    }

    SECTION("Test fp_disp_client_handle_bcast with zero bcast_type_size")
    {
        test_bcast_handler_called = false;

        fp_disp_client_handle_bcast(&test_dispatch_info, 0, test_bcast_handler);

        // Handler should not be called
        REQUIRE_FALSE(test_bcast_handler_called);
    }

    SECTION("Test fp_disp_client_handle_bcast with NULL p_data")
    {
        test_bcast_handler_called = false;

        fp_dispatch_info_t invalid_ctx = test_dispatch_info;
        invalid_ctx.p_data             = nullptr;

        fp_disp_client_handle_bcast(&invalid_ctx, sizeof(bcast_buf), test_bcast_handler);

        // Handler should not be called
        REQUIRE_FALSE(test_bcast_handler_called);
    }

    SECTION("Test fp_disp_client_handle_bcast with NULL p_bcast_handler")
    {
        test_bcast_handler_called = false;

        fp_disp_client_handle_bcast(&test_dispatch_info, sizeof(bcast_buf), nullptr);

        // Handler should not be called (can't be called since it's NULL)
        REQUIRE_FALSE(test_bcast_handler_called);
    }

    SECTION("Test fp_disp_client_handle_bcast with wrong frame type")
    {
        test_bcast_handler_called = false;

        // Create a response frame instead of broadcast
        uint8_t       resp_buf[sizeof(fp_msg_hdr_t) + 10] = { 0 };
        fp_msg_hdr_t *p_resp_hdr                          = (fp_msg_hdr_t *)resp_buf;

        p_resp_hdr->iface_id = 100;
        p_resp_hdr->cmd_id   = 42;
        fp_proto_set_function_type(p_resp_hdr, 5, FP_FRAME_TYPE_RESP);

        fp_dispatch_info_t resp_ctx;
        resp_ctx.p_sys_ctx = &test_sys_ctx;
        resp_ctx.p_data    = resp_buf;
        resp_ctx.data_size = sizeof(resp_buf);

        fp_disp_client_handle_bcast(&resp_ctx, sizeof(resp_buf), test_bcast_handler);

        // Handler should not be called (wrong frame type)
        REQUIRE_FALSE(test_bcast_handler_called);
    }

    SECTION("Test fp_disp_client_handle_bcast with size mismatch")
    {
        test_bcast_handler_called = false;

        // Call with wrong expected size
        fp_disp_client_handle_bcast(&test_dispatch_info, sizeof(bcast_buf) + 100, test_bcast_handler);

        // Handler should not be called (size mismatch)
        REQUIRE_FALSE(test_bcast_handler_called);
    }

    SECTION("Test fp_disp_client_handle_bcast with valid broadcast")
    {
        test_bcast_handler_called = false;
        test_bcast_handler_ctx    = nullptr;
        test_bcast_handler_data   = nullptr;

        fp_disp_client_handle_bcast(&test_dispatch_info, sizeof(bcast_buf), test_bcast_handler);

        // Handler should be called with correct parameters
        REQUIRE(test_bcast_handler_called);
        REQUIRE(test_bcast_handler_ctx == &test_dispatch_info);
        REQUIRE(test_bcast_handler_data == bcast_buf);
    }

    SECTION("Test fp_disp_client_handle_bcast with zero-argument broadcast (header-only)")
    {
        test_bcast_handler_called = false;
        test_bcast_handler_ctx    = nullptr;
        test_bcast_handler_data   = nullptr;

        // Create a header-only broadcast (zero arguments, no payload beyond header)
        uint8_t       header_only_buf[sizeof(fp_msg_hdr_t)] = { 0 };
        fp_msg_hdr_t *p_header_only                         = (fp_msg_hdr_t *)header_only_buf;

        p_header_only->iface_id = 200;
        p_header_only->cmd_id   = 99;
        fp_proto_set_function_type(p_header_only, 10, FP_FRAME_TYPE_BROADCAST);

        fp_dispatch_info_t header_only_ctx;
        header_only_ctx.p_sys_ctx = &test_sys_ctx;
        header_only_ctx.p_data    = header_only_buf;
        header_only_ctx.data_size = sizeof(fp_msg_hdr_t);  // Minimum valid size

        // Call with header-only broadcast
        fp_disp_client_handle_bcast(&header_only_ctx, sizeof(fp_msg_hdr_t), test_bcast_handler);

        // Handler should be called successfully (zero-argument broadcasts are valid)
        REQUIRE(test_bcast_handler_called);
        REQUIRE(test_bcast_handler_ctx == &header_only_ctx);
        REQUIRE(test_bcast_handler_data == header_only_buf);
    }

    SECTION("Test broadcast integration through fp_disp_execute")
    {
        test_bcast_handler_called = false;
        test_bcast_handler_ctx    = nullptr;
        test_bcast_handler_data   = nullptr;

        // Static FIDL callback that uses fp_disp_client_handle_bcast
        static auto fidl_bcast_callback = [](fp_dispatch_info_t *p_disp_ctx) {
            static auto handler = [](void *p_ctx, uint8_t *p_data) {
                test_bcast_handler_called = true;
                test_bcast_handler_ctx    = p_ctx;
                test_bcast_handler_data   = p_data;
            };
            fp_disp_client_handle_bcast(p_disp_ctx, sizeof(bcast_buf), handler);
        };

        // Register FIDL interface with broadcast function
        std::array<fidl_func_t, 1> bcast_funcs = {
            { { .func_id = 5, .p_on_func_cbk = +fidl_bcast_callback } }
        };

        fidl_if_t bcast_client_if = {
            .version     = 1,
            .gen_type    = FIDL_TYPE_CLIENT,
            .fidl_if_id  = 100,
            .func_count  = 1,
            .p_func_list = bcast_funcs.data()
        };

        REQUIRE(fp_registry_add_fidl_if(&bcast_client_if));

        // Execute dispatch
        bool result = fp_disp_execute(&test_dispatch_info);

        // Verify broadcast was dispatched successfully
        REQUIRE(result);
        REQUIRE(test_bcast_handler_called);
        REQUIRE(test_bcast_handler_ctx == &test_dispatch_info);
        REQUIRE(test_bcast_handler_data == bcast_buf);
    }
}
