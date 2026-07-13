cmake_minimum_required(VERSION 3.10)
project(toy C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(BUILD_MODE "None" CACHE STRING "None | AllocationStats | LeakCheck | Profile")
set_property(CACHE BUILD_MODE PROPERTY STRINGS None AllocationStats LeakCheck Profile)
message(STATUS "Build mode: ${BUILD_MODE}")

set(TOY_RUNTIME_SOURCES
    src/tf_alloc.c
    src/tf_console.c
    src/tf_debug_control.c
    src/tf_docs.c
    src/tf_exec.c
    src/tf_lexer.c
    src/tf_lib_control.c
    src/tf_lib_core.c
    src/tf_lib_data.c
    src/tf_lib_io.c
    src/tf_lib_meta.c
    src/tf_lib_sys.c
    src/tf_obj.c
    src/tf_runtime.c
    src/toy.c
)

set(TOY_CLI_SOURCES
    src/main.c
    src/tf_debug_protocol.c
    src/tf_repl.c
)

add_library(toy_runtime STATIC ${TOY_RUNTIME_SOURCES})
add_library(toy::runtime ALIAS toy_runtime)
target_include_directories(toy_runtime PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_executable(toy ${TOY_CLI_SOURCES})
target_include_directories(toy PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(toy PRIVATE toy_runtime)

add_executable(toy_embed_example EXCLUDE_FROM_ALL examples/c/embed.c)
target_link_libraries(toy_embed_example PRIVATE toy::runtime)

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
    target_link_libraries(toy_runtime PUBLIC m)
endif()
target_include_directories(toy PRIVATE ${PROJECT_SOURCE_DIR}/deps/linenoise)

# ── Tests ─────────────────────────────────────────────────────────

include(CTest)

if(BUILD_TESTING)
    set(TOY_TEST_TIMEOUT 10 CACHE STRING "Timeout in seconds for each Toy test")
    set(TOY_TEST_DIR "${PROJECT_SOURCE_DIR}/tests/toy")
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

    set(DEBUG_PROTOCOL_SOURCE "${TOY_TEST_DIR}/test_debug_protocol.toy")
    add_test(
        NAME test_debug_protocol_transport
        COMMAND "${CMAKE_COMMAND}"
            "-DTOY_EXECUTABLE=$<TARGET_FILE:toy>"
            "-DTEST_SOURCE=${DEBUG_PROTOCOL_SOURCE}"
            "-DTEST_SOURCE_DIR=${TOY_TEST_DIR}"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-work/$<CONFIG>/debug-protocol"
            -P "${PROJECT_SOURCE_DIR}/cmake/RunDebugProtocol.cmake"
    )
    set_tests_properties(test_debug_protocol_transport PROPERTIES
        LABELS "toy;positive"
        TIMEOUT "${TOY_TEST_TIMEOUT}"
    )

    add_executable(test_embed_api tests/c/test_embed_api.c)
    target_link_libraries(test_embed_api PRIVATE toy_runtime)
    if(MSVC)
        target_compile_options(test_embed_api PRIVATE /W3)
    else()
        target_compile_options(test_embed_api PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME test_embed_api COMMAND test_embed_api)
    set_tests_properties(test_embed_api PROPERTIES
        LABELS "c-api;positive"
        TIMEOUT "${TOY_TEST_TIMEOUT}"
    )

    add_executable(test_debug_control tests/c/test_debug_control.c)
    target_link_libraries(test_debug_control PRIVATE toy_runtime)
    if(MSVC)
        target_compile_options(test_debug_control PRIVATE /W3)
    else()
        target_compile_options(test_debug_control PRIVATE
                               -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME test_debug_control COMMAND test_debug_control)
    set_tests_properties(test_debug_control PROPERTIES
        LABELS "c-unit;positive"
        TIMEOUT "${TOY_TEST_TIMEOUT}"
    )

    add_executable(test_debug_inspection tests/c/test_debug_inspection.c)
    target_link_libraries(test_debug_inspection PRIVATE toy_runtime)
    if(MSVC)
        target_compile_options(test_debug_inspection PRIVATE /W3)
    else()
        target_compile_options(test_debug_inspection PRIVATE
                               -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME test_debug_inspection COMMAND test_debug_inspection)
    set_tests_properties(test_debug_inspection PROPERTIES
        LABELS "c-unit;positive"
        TIMEOUT "${TOY_TEST_TIMEOUT}"
    )
endif()

# ── Baseline warnings ─────────────────────────────────────────────────────────

foreach(TOY_TARGET IN ITEMS toy_runtime toy toy_embed_example)
    if(MSVC)
        target_compile_options(${TOY_TARGET} PRIVATE /W3)
        target_compile_definitions(${TOY_TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${TOY_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
        if(WIN32 AND NOT MINGW)
            target_compile_definitions(${TOY_TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
    endif()
endforeach()

# ── LeakCheck / Profile override CMAKE_BUILD_TYPE ────────────────────────────

if(BUILD_MODE STREQUAL "AllocationStats")
    target_compile_definitions(toy_runtime PRIVATE TF_ALLOC_STATS)
    target_compile_definitions(toy PRIVATE TF_ALLOC_STATS)

elseif(BUILD_MODE STREQUAL "LeakCheck")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    if(WIN32)
        target_compile_definitions(toy_runtime PRIVATE STB_LEAKCHECK)
        target_compile_definitions(toy PRIVATE STB_LEAKCHECK)
        target_include_directories(toy_runtime PRIVATE ${PROJECT_SOURCE_DIR}/deps)
        target_include_directories(toy PRIVATE ${PROJECT_SOURCE_DIR}/deps)
        if(TARGET test_embed_api)
            target_compile_definitions(test_embed_api PRIVATE STB_LEAKCHECK)
            target_include_directories(test_embed_api PRIVATE
                                       ${PROJECT_SOURCE_DIR}/deps)
        endif()
        if(TARGET test_debug_control)
            target_compile_definitions(test_debug_control PRIVATE STB_LEAKCHECK)
            target_include_directories(test_debug_control PRIVATE
                                       ${PROJECT_SOURCE_DIR}/deps)
        endif()
        if(TARGET test_debug_inspection)
            target_compile_definitions(test_debug_inspection PRIVATE
                                       STB_LEAKCHECK)
            target_include_directories(test_debug_inspection PRIVATE
                                       ${PROJECT_SOURCE_DIR}/deps)
        endif()
    else()
        target_compile_options(toy_runtime PRIVATE -fsanitize=leak
                                                   -fno-omit-frame-pointer)
        target_compile_options(toy PRIVATE -fsanitize=leak -fno-omit-frame-pointer)
        target_link_options(toy_runtime PUBLIC -fsanitize=leak)
    endif()

elseif(BUILD_MODE STREQUAL "Profile")
    if(MINGW)
        message(FATAL_ERROR "Profiling with MinGW is not supported.")
    endif()
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE) # -O2 -g built-in
    if(MSVC)
        target_compile_options(toy_runtime PRIVATE /Oy-)
        target_compile_options(toy PRIVATE /Oy-)
        target_link_options(toy PRIVATE /DEBUG)
    else()
        target_compile_options(toy_runtime PRIVATE -fno-omit-frame-pointer)
        target_compile_options(toy PRIVATE -fno-omit-frame-pointer)
    endif()
endif()
