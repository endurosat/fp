# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

if (NOT DEFINED UNIT_TEST_PRJ_FILES_LIST)
    message(FATAL_ERROR "Please define the UNIT_TEST_PRJ_FILES_LIST CMake list variable before using this helper script! It must contain as elements all files which are part of the unit test!")
endif()

if (NOT DEFINED CMP_NAME)
    message(FATAL_ERROR "Please define the CMP_NAME CMake variable before using this helper script! It must be set to the CMake name of the component under test!")
endif()

message(STATUS "Preparing unit test build for component: '${CMP_NAME}'")
message(STATUS "Unit test sources to build: '${UNIT_TEST_PRJ_FILES_LIST}'")

# Set before Catch2 is configured: a fetched Catch2 names its libraries by build type.
set(CMAKE_BUILD_TYPE Debug)

Include(FetchContent)

find_package(Catch2 QUIET)

# If Catch2 is not found, fetch it using FetchContent
if (NOT Catch2_FOUND)
    message(STATUS "Catch2 not found, fetching it...")

    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.7.1
      GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(Catch2)
else()
    message(STATUS "Catch2 found: ${Catch2_DIR}")
endif()

set(TEST_CMP_ABS_PATH ${PROJECT_SOURCE_DIR}/..)

enable_testing()

add_executable(${PROJECT_NAME} 
               ${UNIT_TEST_PRJ_FILES_LIST}
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -ftest-coverage -fprofile-arcs -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -ftest-coverage -fprofile-arcs")

add_subdirectory(${PROJECT_SOURCE_DIR}/.. ${CMP_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain PUBLIC ${CMP_NAME})
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(${CMP_NAME} PRIVATE ${PROJECT_SOURCE_DIR})

add_custom_target(run-test
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    COMMENT "Run unit test"
    DEPENDS ${PROJECT_NAME}
)

# create an absolute path for use with gcovr which is very strict about filter paths
file(REAL_PATH "${PROJECT_SOURCE_DIR}/.." TEST_CMP_ABS_PATH)

set(COV_REPORT_DIR "${CMAKE_BINARY_DIR}/cov-report")

find_program(GCOVR_EXECUTABLE NAMES gcovr)
if (NOT GCOVR_EXECUTABLE)
    message(FATAL_ERROR "gcovr not found")
else()
    message(STATUS "gcovr found: ${GCOVR_EXECUTABLE}")
endif()

add_custom_target(gen-test-coverage
    # print the text coverage report for the CI to capture the coverage information in the output
    COMMAND ${CMAKE_COMMAND} -E echo "Printing coverage information"
    COMMAND ${GCOVR_EXECUTABLE} --filter "${TEST_CMP_ABS_PATH}/src/*"
    # make sure the coverage report folder exists because gcovr will fail if not
    COMMAND mkdir -p ${COV_REPORT_DIR}

    COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage report for use by SonarQube"
    COMMAND ${GCOVR_EXECUTABLE} --filter "${TEST_CMP_ABS_PATH}/src/*" --sonarqube ${COV_REPORT_DIR}/sonar-cov.xml

    COMMAND ${CMAKE_COMMAND} -E echo "Generating Cobertura XML report for GitLab integration"
    COMMAND ${GCOVR_EXECUTABLE} --filter "${TEST_CMP_ABS_PATH}/src/*" --cobertura --cobertura-pretty -o ${COV_REPORT_DIR}/cov-report-cobertura.xml

    COMMAND ${CMAKE_COMMAND} -E echo "Generating HTML detailed report"
    COMMAND ${GCOVR_EXECUTABLE} --filter "${TEST_CMP_ABS_PATH}/src/*" --html-nested --output ${COV_REPORT_DIR}/cov-report.html --root ${TEST_CMP_ABS_PATH}
    BYPRODUCTS ${COV_REPORT_DIR} ${CMAKE_BINARY_DIR}/cov-report-cobertura.xml ${COV_REPORT_DIR}/coverage.info
    COMMENT "Generating coverage report"
    DEPENDS run-test
    VERBATIM)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})
