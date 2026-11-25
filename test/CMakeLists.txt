# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

cmake_minimum_required( VERSION 3.11 )

project(start-test LANGUAGES C CXX)

set(CMP_NAME svc_fp)

set(UNIT_TEST_PRJ_FILES_LIST
                 test.cpp
                 stubs.cpp
)

# Link the tests against a static fp by default: a Windows DLL build marks the fp data
# that the tests take the address of as dllimport. -DFP_LIBRARY_TYPE=SHARED still applies.
if(NOT DEFINED FP_LIBRARY_TYPE)
    set(FP_LIBRARY_TYPE STATIC CACHE STRING "fp library type for the unit tests: STATIC or SHARED")
endif()

include(swpf_unit_test_helper.cmake)


