cmake_minimum_required(VERSION 3.10)
project(toy C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

option(TOY_BUILD_RAYLIB
       "Build the optional Raylib native module and Toy-driven example" OFF)
option(TOY_BUILD_FFI
       "Build the experimental libffi native module" OFF)

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
    src/tf_native_loader.c
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
target_link_libraries(toy_runtime PUBLIC ${CMAKE_DL_LIBS})
target_compile_definitions(toy_runtime PRIVATE
    TOY_SHARED_SUFFIX=\"${CMAKE_SHARED_MODULE_SUFFIX}\")

add_library(toy_module_support STATIC src/toy_module.c)
add_library(toy::module_support ALIAS toy_module_support)
target_include_directories(toy_module_support PUBLIC
                           ${PROJECT_SOURCE_DIR}/include)
set_target_properties(toy_module_support PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden)

include(cmake/ToyBinding.cmake)

if(TOY_BINDING_NODE)
    toy_add_generated_binding(
        toy_bindgen_clib_example
        examples/bindings/clib.json
        toy_clib
        EXCLUDE_FROM_ALL)
endif()

add_executable(toy ${TOY_CLI_SOURCES})
target_include_directories(toy PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(toy PRIVATE toy_runtime)

add_executable(toy_embed_example EXCLUDE_FROM_ALL examples/c/embed.c)
target_link_libraries(toy_embed_example PRIVATE toy::runtime)

add_executable(toy_embed_callbacks_example EXCLUDE_FROM_ALL
               examples/c/embed_callbacks.c)
target_link_libraries(toy_embed_callbacks_example PRIVATE toy::runtime)

add_executable(toy_embed_values_example EXCLUDE_FROM_ALL
               examples/c/embed_values.c)
target_link_libraries(toy_embed_values_example PRIVATE toy::runtime)

if(TOY_BUILD_FFI)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(TOY_LIBFFI QUIET IMPORTED_TARGET libffi)
    endif()

    if(TARGET PkgConfig::TOY_LIBFFI)
        set(TOY_LIBFFI_TARGET PkgConfig::TOY_LIBFFI)
    else()
        find_path(TOY_LIBFFI_INCLUDE_DIR ffi.h)
        find_library(TOY_LIBFFI_LIBRARY NAMES ffi libffi)
        if(NOT TOY_LIBFFI_INCLUDE_DIR OR NOT TOY_LIBFFI_LIBRARY)
            message(FATAL_ERROR
                "libffi was not found; install it or disable TOY_BUILD_FFI")
        endif()

        add_library(toy_libffi_external UNKNOWN IMPORTED)
        set_target_properties(toy_libffi_external PROPERTIES
            IMPORTED_LOCATION "${TOY_LIBFFI_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${TOY_LIBFFI_INCLUDE_DIR}")
        set(TOY_LIBFFI_TARGET toy_libffi_external)
    endif()

    add_library(toy_ffi_module MODULE bindings/ffi/toy_ffi.c)
    target_link_libraries(toy_ffi_module PRIVATE
        toy::module_support ${TOY_LIBFFI_TARGET} ${CMAKE_DL_LIBS})
    set_target_properties(toy_ffi_module PROPERTIES
        PREFIX ""
        OUTPUT_NAME "toy_ffi"
        C_VISIBILITY_PRESET hidden)
endif()

if(TOY_BUILD_RAYLIB)
    if(NOT TARGET raylib AND NOT TARGET raylib::raylib)
        find_package(raylib CONFIG QUIET)
    endif()

    if(TARGET raylib::raylib)
        set(TOY_RAYLIB_TARGET raylib::raylib)
    elseif(TARGET raylib)
        set(TOY_RAYLIB_TARGET raylib)
    else()
        find_path(TOY_RAYLIB_INCLUDE_DIR raylib.h PATH_SUFFIXES include)
        find_library(TOY_RAYLIB_LIBRARY NAMES raylib PATH_SUFFIXES lib)
        if(NOT TOY_RAYLIB_INCLUDE_DIR OR NOT TOY_RAYLIB_LIBRARY)
            message(FATAL_ERROR
                "Raylib was not found; set CMAKE_PREFIX_PATH to its prefix")
        endif()

        add_library(toy_raylib_external UNKNOWN IMPORTED)
        set_target_properties(toy_raylib_external PROPERTIES
            IMPORTED_LOCATION "${TOY_RAYLIB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${TOY_RAYLIB_INCLUDE_DIR}")
        if(WIN32)
            set_property(TARGET toy_raylib_external PROPERTY
                INTERFACE_LINK_LIBRARIES "opengl32;gdi32;winmm")
        endif()
        set(TOY_RAYLIB_TARGET toy_raylib_external)
    endif()

    add_library(toy_raylib STATIC bindings/raylib/toy_raylib.c)
    add_library(toy::raylib ALIAS toy_raylib)
    target_include_directories(toy_raylib
        PUBLIC ${PROJECT_SOURCE_DIR}/bindings/raylib)
    target_link_libraries(toy_raylib PUBLIC toy::runtime ${TOY_RAYLIB_TARGET})

    add_executable(toy_raylib_example EXCLUDE_FROM_ALL
                   examples/c/raylib_host.c)
    target_link_libraries(toy_raylib_example PRIVATE toy::raylib)

    add_library(toy_raylib_module MODULE
                bindings/raylib/toy_raylib.c)
    target_compile_definitions(toy_raylib_module PRIVATE TOY_SHARED_MODULE)
    target_include_directories(toy_raylib_module PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/bindings/raylib)
    target_link_libraries(toy_raylib_module PRIVATE toy::module_support
                                                    ${TOY_RAYLIB_TARGET})
    set_target_properties(toy_raylib_module PROPERTIES
        PREFIX ""
        OUTPUT_NAME "toy_raylib"
        C_VISIBILITY_PRESET hidden)
endif()

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

    add_executable(test_raylib_module
                   tests/c/test_raylib_module.c
                   bindings/raylib/toy_raylib.c)
    target_include_directories(test_raylib_module PRIVATE
        ${PROJECT_SOURCE_DIR}/tests/c/stubs
        ${PROJECT_SOURCE_DIR}/bindings/raylib)
    target_link_libraries(test_raylib_module PRIVATE toy_runtime)
    if(MSVC)
        target_compile_options(test_raylib_module PRIVATE /W3)
    else()
        target_compile_options(test_raylib_module PRIVATE
                               -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME test_raylib_module COMMAND test_raylib_module)
    set_tests_properties(test_raylib_module PROPERTIES
        LABELS "c-api;binding;positive"
        TIMEOUT "${TOY_TEST_TIMEOUT}"
    )

    add_library(test_native_plugin MODULE
                tests/c/test_native_plugin.c)
    target_include_directories(test_native_plugin PRIVATE
                               ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(test_native_plugin PRIVATE toy::module_support)
    set_target_properties(test_native_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "toy_test.plugin"
        C_VISIBILITY_PRESET hidden)

    add_library(test_native_bad_plugin MODULE
                tests/c/test_native_bad_plugin.c)
    target_include_directories(test_native_bad_plugin PRIVATE
                               ${PROJECT_SOURCE_DIR}/include)
    set_target_properties(test_native_bad_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "toy_test.bad"
        C_VISIBILITY_PRESET hidden)

    add_executable(test_native_loader tests/c/test_native_loader.c)
    target_link_libraries(test_native_loader PRIVATE toy_runtime)
    add_dependencies(test_native_loader test_native_plugin
                     test_native_bad_plugin)
    if(MSVC)
        target_compile_options(test_native_plugin PRIVATE /W3)
        target_compile_options(test_native_bad_plugin PRIVATE /W3)
        target_compile_options(test_native_loader PRIVATE /W3)
    else()
        target_compile_options(test_native_plugin PRIVATE
                               -Wall -Wextra -Wpedantic)
        target_compile_options(test_native_bad_plugin PRIVATE
                               -Wall -Wextra -Wpedantic)
        target_compile_options(test_native_loader PRIVATE
                               -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME test_native_loader
             COMMAND test_native_loader
                     "$<TARGET_FILE_DIR:test_native_plugin>")
    set_tests_properties(test_native_loader PROPERTIES
        LABELS "c-api;native-module;positive"
        TIMEOUT "${TOY_TEST_TIMEOUT}"
    )

    if(TOY_BINDING_NODE)
        add_test(NAME test_binding_generator_js
                 COMMAND "${TOY_BINDING_NODE}"
                         "${PROJECT_SOURCE_DIR}/tests/tools/test_generate_binding.js")
        set_tests_properties(test_binding_generator_js PROPERTIES
            LABELS "binding-generator;positive"
            TIMEOUT "${TOY_TEST_TIMEOUT}"
        )

        add_library(test_bindgen_fixture SHARED
                    tests/c/test_bindgen_fixture.c)
        target_compile_definitions(test_bindgen_fixture PRIVATE
                                   TEST_BINDGEN_FIXTURE_BUILD)
        target_include_directories(test_bindgen_fixture PUBLIC
                                   ${PROJECT_SOURCE_DIR}/tests/c)
        set_target_properties(test_bindgen_fixture PROPERTIES
            PREFIX ""
            OUTPUT_NAME "toy_bindgen_fixture")

        toy_add_generated_binding(
            test_generated_binding
            tests/bindings/test_bindgen.json
            "toy_test.bindgen"
            EXCLUDE_FROM_ALL)
        target_include_directories(test_generated_binding PRIVATE
                                   ${PROJECT_SOURCE_DIR}/tests/c)
        target_link_libraries(test_generated_binding PRIVATE
                              test_bindgen_fixture)

        add_executable(test_bindgen_module tests/c/test_bindgen_module.c)
        target_link_libraries(test_bindgen_module PRIVATE toy_runtime)
        add_dependencies(test_bindgen_module test_generated_binding)
        if(MSVC)
            target_compile_options(test_bindgen_fixture PRIVATE /W3)
            target_compile_options(test_bindgen_module PRIVATE /W3)
        else()
            target_compile_options(test_bindgen_fixture PRIVATE
                                   -Wall -Wextra -Wpedantic)
            target_compile_options(test_bindgen_module PRIVATE
                                   -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME test_bindgen_module
                 COMMAND test_bindgen_module
                         "$<TARGET_FILE_DIR:test_generated_binding>")
        set_tests_properties(test_bindgen_module PROPERTIES
            LABELS "binding-generator;c-api;native-module;positive"
            TIMEOUT "${TOY_TEST_TIMEOUT}"
        )
    endif()

    if(TARGET toy_ffi_module)
        add_library(test_ffi_fixture SHARED tests/c/test_ffi_fixture.c)
        set_target_properties(test_ffi_fixture PROPERTIES
            PREFIX ""
            OUTPUT_NAME "toy_ffi_fixture")

        add_executable(test_ffi_module tests/c/test_ffi_module.c)
        target_link_libraries(test_ffi_module PRIVATE toy_runtime)
        add_dependencies(test_ffi_module toy_ffi_module test_ffi_fixture)
        if(MSVC)
            target_compile_options(test_ffi_fixture PRIVATE /W3)
            target_compile_options(test_ffi_module PRIVATE /W3)
        else()
            target_compile_options(test_ffi_fixture PRIVATE
                                   -Wall -Wextra -Wpedantic)
            target_compile_options(test_ffi_module PRIVATE
                                   -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME test_ffi_module
                 COMMAND test_ffi_module
                         "$<TARGET_FILE_DIR:toy_ffi_module>"
                         "$<TARGET_FILE:test_ffi_fixture>")
        set_tests_properties(test_ffi_module PROPERTIES
            LABELS "c-api;native-module;ffi;positive"
            TIMEOUT "${TOY_TEST_TIMEOUT}"
        )
    endif()

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

set(TOY_WARNING_TARGETS toy_runtime toy_module_support toy toy_embed_example
                        toy_embed_callbacks_example toy_embed_values_example)
if(TARGET toy_bindgen_clib_example)
    list(APPEND TOY_WARNING_TARGETS toy_bindgen_clib_example)
endif()
if(TARGET test_generated_binding)
    list(APPEND TOY_WARNING_TARGETS test_generated_binding)
endif()
if(TARGET toy_ffi_module)
    list(APPEND TOY_WARNING_TARGETS toy_ffi_module)
endif()
if(TARGET toy_raylib)
    list(APPEND TOY_WARNING_TARGETS toy_raylib toy_raylib_example
                                   toy_raylib_module)
endif()

foreach(TOY_TARGET IN LISTS TOY_WARNING_TARGETS)
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
        if(TARGET test_raylib_module)
            target_compile_definitions(test_raylib_module PRIVATE
                                       STB_LEAKCHECK)
            target_include_directories(test_raylib_module PRIVATE
                                       ${PROJECT_SOURCE_DIR}/deps)
        endif()
        if(TARGET test_native_loader)
            target_compile_definitions(test_native_loader PRIVATE
                                       STB_LEAKCHECK)
            target_include_directories(test_native_loader PRIVATE
                                       ${PROJECT_SOURCE_DIR}/deps)
        endif()
        if(TARGET test_ffi_module)
            target_compile_definitions(test_ffi_module PRIVATE STB_LEAKCHECK)
            target_include_directories(test_ffi_module PRIVATE
                                       ${PROJECT_SOURCE_DIR}/deps)
        endif()
        if(TARGET test_bindgen_module)
            target_compile_definitions(test_bindgen_module PRIVATE
                                       STB_LEAKCHECK)
            target_include_directories(test_bindgen_module PRIVATE
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
