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
else()
    target_sources(toy PRIVATE ${PROJECT_SOURCE_DIR}/deps/linenoise/linenoise.c)
    target_link_libraries(toy PRIVATE m)
endif()
target_include_directories(toy PRIVATE ${PROJECT_SOURCE_DIR}/deps/linenoise)

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
