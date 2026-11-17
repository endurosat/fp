# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

cmake_minimum_required(VERSION 3.11)
project(svc_fp VERSION 1.0 DESCRIPTION "EnduroSat FP core service implementation")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Determine library type based on platform
if(NOT DEFINED FP_LIBRARY_TYPE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(FP_LIBRARY_TYPE SHARED)
    else()
        set(FP_LIBRARY_TYPE STATIC)
    endif()
endif()

list(APPEND SOURCES
       ${PROJECT_SOURCE_DIR}/src/fp_registry.c
       ${PROJECT_SOURCE_DIR}/src/fp_disp.c
       ${PROJECT_SOURCE_DIR}/src/fp_disp_client.c
       ${PROJECT_SOURCE_DIR}/src/fp_disp_server.c
       ${PROJECT_SOURCE_DIR}/src/fp_disp_session_mgr.c
       ${PROJECT_SOURCE_DIR}/src/fp_internal.c
       ${PROJECT_SOURCE_DIR}/src/fp_proto.c
)

add_library(${PROJECT_NAME} ${FP_LIBRARY_TYPE} ${SOURCES})
add_library("fp" ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/inc)

if(FP_LIBRARY_TYPE STREQUAL "STATIC")
    target_compile_definitions(${PROJECT_NAME} PUBLIC FP_STATIC)
elseif(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FP_EXPORTS)
endif()

if(TARGET svc_fp_cfg)
    target_link_libraries(${PROJECT_NAME} PUBLIC svc_fp_cfg)
endif()

