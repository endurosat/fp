# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

cmake_minimum_required( VERSION 3.11 )

project(start-test LANGUAGES C CXX)

set(CMP_NAME svc_fp)

set(UNIT_TEST_PRJ_FILES_LIST
                 test.cpp
                 stubs.cpp
)

include(swpf_unit_test_helper.cmake)


