cmake_minimum_required(VERSION 3.10)

foreach(REQUIRED_VARIABLE TOY_EXECUTABLE TEST_SOURCE CASE_KIND WORK_ROOT WORK_DIR)
    if(NOT DEFINED ${REQUIRED_VARIABLE} OR "${${REQUIRED_VARIABLE}}" STREQUAL "")
        message(FATAL_ERROR "RunToyCase.cmake requires ${REQUIRED_VARIABLE}")
    endif()
endforeach()

if(NOT EXISTS "${TOY_EXECUTABLE}")
    message(FATAL_ERROR "Toy executable does not exist: ${TOY_EXECUTABLE}")
endif()
if(NOT EXISTS "${TEST_SOURCE}")
    message(FATAL_ERROR "Toy test does not exist: ${TEST_SOURCE}")
endif()
if(NOT CASE_KIND STREQUAL "test"
        AND NOT CASE_KIND STREQUAL "fail"
        AND NOT CASE_KIND STREQUAL "output")
    message(FATAL_ERROR "Unknown Toy test kind: ${CASE_KIND}")
endif()

function(normalize_line_endings INPUT_VARIABLE OUTPUT_VARIABLE)
    string(REPLACE "\r\n" "\n" NORMALIZED "${${INPUT_VARIABLE}}")
    string(REPLACE "\r" "\n" NORMALIZED "${NORMALIZED}")
    set(${OUTPUT_VARIABLE} "${NORMALIZED}" PARENT_SCOPE)
endfunction()

function(report_failure SUMMARY)
    message(FATAL_ERROR
        "${SUMMARY}\n"
        "--- stdout ---\n${NORMALIZED_STDOUT}\n"
        "--- stderr ---\n${NORMALIZED_STDERR}\n"
        "--- end streams ---")
endfunction()

function(report_mismatch SUMMARY EXPECTED_LABEL EXPECTED)
    message(FATAL_ERROR
        "${SUMMARY}\n"
        "--- expected ${EXPECTED_LABEL} ---\n${EXPECTED}\n"
        "--- stdout ---\n${NORMALIZED_STDOUT}\n"
        "--- stderr ---\n${NORMALIZED_STDERR}\n"
        "--- end streams ---")
endfunction()

get_filename_component(WORK_ROOT "${WORK_ROOT}" ABSOLUTE)
get_filename_component(WORK_DIR "${WORK_DIR}" ABSOLUTE)
file(TO_CMAKE_PATH "${WORK_ROOT}" WORK_ROOT)
file(TO_CMAKE_PATH "${WORK_DIR}" WORK_DIR)
string(REGEX REPLACE "/+$" "" WORK_ROOT "${WORK_ROOT}")
set(WORK_ROOT_PREFIX "${WORK_ROOT}/")
string(FIND "${WORK_DIR}" "${WORK_ROOT_PREFIX}" WORK_ROOT_POSITION)
if(NOT WORK_ROOT_POSITION EQUAL 0)
    message(FATAL_ERROR
        "Toy test work directory must be inside ${WORK_ROOT}: ${WORK_DIR}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(COPY "${TEST_SOURCE}" DESTINATION "${WORK_DIR}")

if(DEFINED TESTLIB_SOURCE AND NOT "${TESTLIB_SOURCE}" STREQUAL "")
    if(NOT EXISTS "${TESTLIB_SOURCE}")
        message(FATAL_ERROR "Toy test library does not exist: ${TESTLIB_SOURCE}")
    endif()
    file(COPY "${TESTLIB_SOURCE}" DESTINATION "${WORK_DIR}")
endif()

get_filename_component(TEST_FILENAME "${TEST_SOURCE}" NAME)
set(TEST_COPY "${WORK_DIR}/${TEST_FILENAME}")

execute_process(
    COMMAND "${TOY_EXECUTABLE}" "${TEST_COPY}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE STDOUT
    ERROR_VARIABLE STDERR
)

normalize_line_endings(STDOUT NORMALIZED_STDOUT)
normalize_line_endings(STDERR NORMALIZED_STDERR)

if(CASE_KIND STREQUAL "test")
    if(NOT "${RESULT}" STREQUAL "0")
        report_failure("${TEST_FILENAME} exited with ${RESULT}; expected 0")
    endif()
elseif(CASE_KIND STREQUAL "fail")
    if(NOT "${RESULT}" STREQUAL "1")
        report_failure("${TEST_FILENAME} exited with ${RESULT}; expected 1")
    endif()
    if(NOT DEFINED EXPECTED_FILE OR NOT EXISTS "${EXPECTED_FILE}")
        message(FATAL_ERROR
            "Negative test ${TEST_FILENAME} requires an existing .stderr sidecar")
    endif()
    file(READ "${EXPECTED_FILE}" EXPECTED_STDERR)
    normalize_line_endings(EXPECTED_STDERR NORMALIZED_EXPECTED_STDERR)
    string(REGEX REPLACE "\n$" "" NORMALIZED_EXPECTED_STDERR
        "${NORMALIZED_EXPECTED_STDERR}")
    if("${NORMALIZED_EXPECTED_STDERR}" STREQUAL "")
        message(FATAL_ERROR
            "Negative test ${TEST_FILENAME} has an empty .stderr sidecar")
    endif()
    string(FIND
        "${NORMALIZED_STDERR}"
        "${NORMALIZED_EXPECTED_STDERR}"
        MATCH_POSITION
    )
    if(MATCH_POSITION EQUAL -1)
        report_mismatch(
            "${TEST_FILENAME} stderr did not contain the expected fragment"
            "stderr fragment"
            "${NORMALIZED_EXPECTED_STDERR}"
        )
    endif()
elseif(CASE_KIND STREQUAL "output")
    if(NOT "${RESULT}" STREQUAL "0")
        report_failure("${TEST_FILENAME} exited with ${RESULT}; expected 0")
    endif()
    if(NOT "${NORMALIZED_STDERR}" STREQUAL "")
        report_failure("${TEST_FILENAME} wrote unexpected stderr")
    endif()
    if(NOT DEFINED EXPECTED_FILE OR NOT EXISTS "${EXPECTED_FILE}")
        message(FATAL_ERROR
            "Output test ${TEST_FILENAME} requires an existing .stdout sidecar")
    endif()
    file(READ "${EXPECTED_FILE}" EXPECTED_STDOUT)
    normalize_line_endings(EXPECTED_STDOUT NORMALIZED_EXPECTED_STDOUT)
    if(NOT "${NORMALIZED_STDOUT}" STREQUAL "${NORMALIZED_EXPECTED_STDOUT}")
        report_mismatch(
            "${TEST_FILENAME} stdout did not exactly match ${EXPECTED_FILE}"
            "stdout"
            "${NORMALIZED_EXPECTED_STDOUT}"
        )
    endif()
endif()
