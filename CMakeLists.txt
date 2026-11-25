# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

cmake_minimum_required(VERSION 3.11)
project(svc_fp VERSION 1.0 DESCRIPTION "EnduroSat FP core service implementation")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Library type: STATIC or SHARED. Defaults to SHARED on Windows and Linux and to
# STATIC on every other platform. Override it with -DFP_LIBRARY_TYPE=STATIC|SHARED,
# or set it in the parent project before add_subdirectory().
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(FP_DEFAULT_LIBRARY_TYPE SHARED)
else()
    set(FP_DEFAULT_LIBRARY_TYPE STATIC)
endif()

if(NOT DEFINED FP_LIBRARY_TYPE)
    set(FP_LIBRARY_TYPE ${FP_DEFAULT_LIBRARY_TYPE} CACHE STRING "fp library type: STATIC or SHARED")
    set_property(CACHE FP_LIBRARY_TYPE PROPERTY STRINGS STATIC SHARED)
endif()

string(TOUPPER "${FP_LIBRARY_TYPE}" FP_LIBRARY_TYPE_UPPER)
if(NOT FP_LIBRARY_TYPE_UPPER STREQUAL "STATIC" AND NOT FP_LIBRARY_TYPE_UPPER STREQUAL "SHARED")
    message(FATAL_ERROR "FP_LIBRARY_TYPE is '${FP_LIBRARY_TYPE}'; use STATIC or SHARED")
endif()
message(STATUS "fp library type: ${FP_LIBRARY_TYPE_UPPER}")

list(APPEND SOURCES
       ${PROJECT_SOURCE_DIR}/src/fp_registry.c
       ${PROJECT_SOURCE_DIR}/src/fp_disp.c
       ${PROJECT_SOURCE_DIR}/src/fp_disp_client.c
       ${PROJECT_SOURCE_DIR}/src/fp_disp_server.c
       ${PROJECT_SOURCE_DIR}/src/fp_disp_session_mgr.c
       ${PROJECT_SOURCE_DIR}/src/fp_internal.c
       ${PROJECT_SOURCE_DIR}/src/fp_proto.c
)

add_library(${PROJECT_NAME} ${FP_LIBRARY_TYPE_UPPER} ${SOURCES})
add_library("fp" ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/inc)

# FP_STATIC gives FP_API an empty definition (inc/fp_defs.h), so that a static build
# has no dllimport declarations. FP_EXPORTS selects dllexport while the DLL is built.
if(FP_LIBRARY_TYPE_UPPER STREQUAL "STATIC")
    target_compile_definitions(${PROJECT_NAME} PUBLIC FP_STATIC)
elseif(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FP_EXPORTS)
endif()

if(TARGET svc_fp_cfg)
    target_link_libraries(${PROJECT_NAME} PUBLIC svc_fp_cfg)
endif()

