cmake_minimum_required(VERSION 3.10)
project(toy C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(BUILD_MODE "None" CACHE STRING "None | AllocationStats | LeakCheck | Profile")
set_property(CACHE BUILD_MODE PROPERTY STRINGS None AllocationStats LeakCheck Profile)
message(STATUS "Build mode: ${BUILD_MODE}")

file(GLOB SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.c")
add_executable(toy ${SOURCES})
target_include_directories(toy PRIVATE ${PROJECT_SOURCE_DIR}/include)

if(WIN32)
    target_sources(toy PRIVATE ${PROJECT_SOURCE_DIR}/deps/linenoise/linenoise_win.c)
    if(MSVC)
        set_source_files_properties(
            ${PROJECT_SOURCE_DIR}/deps/linenoise/linenoise_win.c
            PROPERTIES COMPILE_OPTIONS "/wd4267"
        )
    endif()
else()
    target_sources(toy PRIVATE ${PROJECT_SOURCE_DIR}/deps/linenoise/linenoise.c)
    target_link_libraries(toy PRIVATE m)
endif()
target_include_directories(toy PRIVATE ${PROJECT_SOURCE_DIR}/deps/linenoise)

# ── Tests ─────────────────────────────────────────────────────────

include(CTest)

if(BUILD_TESTING)
    set(TOY_TEST_TIMEOUT 10 CACHE STRING "Timeout in seconds for each Toy test")
    set(TOY_TEST_DIR "${PROJECT_SOURCE_DIR}/toy/tests")
    set(TOY_TEST_RUNNER "${PROJECT_SOURCE_DIR}/cmake/RunToyCase.cmake")

    if(EXISTS "${TOY_TEST_DIR}/testlib.toy")
        set(TOY_TESTLIB "${TOY_TEST_DIR}/testlib.toy")
    else()
        set(TOY_TESTLIB "")
    endif()

    function(add_toy_test_case TEST_SOURCE CASE_KIND CASE_LABEL)
        get_filename_component(TEST_FILENAME "${TEST_SOURCE}" NAME)
        get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE)
        set(EXPECTED_FILE "")

        if(CASE_KIND STREQUAL "fail")
            set(EXPECTED_FILE "${TOY_TEST_DIR}/${TEST_NAME}.stderr")
            if(NOT EXISTS "${EXPECTED_FILE}")
                message(FATAL_ERROR
                    "Negative test ${TEST_FILENAME} requires ${TEST_NAME}.stderr")
            endif()
        elseif(CASE_KIND STREQUAL "output")
            set(EXPECTED_FILE "${TOY_TEST_DIR}/${TEST_NAME}.stdout")
            if(NOT EXISTS "${EXPECTED_FILE}")
                message(FATAL_ERROR
                    "Output test ${TEST_FILENAME} requires ${TEST_NAME}.stdout")
            endif()
        endif()

        add_test(
            NAME "${TEST_NAME}"
            COMMAND "${CMAKE_COMMAND}"
                "-DTOY_EXECUTABLE=$<TARGET_FILE:toy>"
                "-DTEST_SOURCE=${TEST_SOURCE}"
                "-DTESTLIB_SOURCE=${TOY_TESTLIB}"
                "-DEXPECTED_FILE=${EXPECTED_FILE}"
                "-DCASE_KIND=${CASE_KIND}"
                "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-work"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-work/$<CONFIG>/${TEST_NAME}"
                -P "${TOY_TEST_RUNNER}"
        )
        set_tests_properties("${TEST_NAME}" PROPERTIES
            LABELS "toy;${CASE_LABEL}"
            TIMEOUT "${TOY_TEST_TIMEOUT}"
        )
    endfunction()

    file(GLOB TOY_POSITIVE_TESTS CONFIGURE_DEPENDS "${TOY_TEST_DIR}/test_*.toy")
    file(GLOB TOY_NEGATIVE_TESTS CONFIGURE_DEPENDS "${TOY_TEST_DIR}/fail_*.toy")
    file(GLOB TOY_OUTPUT_TESTS CONFIGURE_DEPENDS "${TOY_TEST_DIR}/output_*.toy")

    foreach(TEST_SOURCE IN LISTS TOY_POSITIVE_TESTS)
        add_toy_test_case("${TEST_SOURCE}" "test" "positive")
    endforeach()
    foreach(TEST_SOURCE IN LISTS TOY_NEGATIVE_TESTS)
        add_toy_test_case("${TEST_SOURCE}" "fail" "negative")
    endforeach()
    foreach(TEST_SOURCE IN LISTS TOY_OUTPUT_TESTS)
        add_toy_test_case("${TEST_SOURCE}" "output" "output")
    endforeach()
endif()

# ── Baseline warnings ─────────────────────────────────────────────────────────

if(MSVC)
    target_compile_options(toy PRIVATE /W3)
    target_compile_definitions(toy PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(toy PRIVATE -Wall -Wextra -Wpedantic)
    if(WIN32 AND NOT MINGW)
        target_compile_definitions(toy PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# ── LeakCheck / Profile override CMAKE_BUILD_TYPE ────────────────────────────

if(BUILD_MODE STREQUAL "AllocationStats")
    target_compile_definitions(toy PRIVATE TF_ALLOC_STATS)

elseif(BUILD_MODE STREQUAL "LeakCheck")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    if(WIN32)
        target_compile_definitions(toy PRIVATE STB_LEAKCHECK)
        target_include_directories(toy PRIVATE ${PROJECT_SOURCE_DIR}/deps)
    else()
        target_compile_options(toy PRIVATE -fsanitize=leak -fno-omit-frame-pointer)
        target_link_options(toy PRIVATE -fsanitize=leak)
    endif()

elseif(BUILD_MODE STREQUAL "Profile")
    if(MINGW)
        message(FATAL_ERROR "Profiling with MinGW is not supported.")
    endif()
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE) # -O2 -g built-in
    if(MSVC)
        target_compile_options(toy PRIVATE /Oy-)
        target_link_options(toy PRIVATE /DEBUG)
    else()
        target_compile_options(toy PRIVATE -fno-omit-frame-pointer)
    endif()
endif()
