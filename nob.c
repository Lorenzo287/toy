#define NOBDEF static inline
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "deps/nob/nob.h"

#include <stdint.h>

#ifdef _WIN32
#define TOY_EXE_SUFFIX ".exe"
#define TOY_SHARED_SUFFIX_VALUE ".dll"
#else
#ifdef __APPLE__
#define TOY_SHARED_SUFFIX_VALUE ".dylib"
#else
#define TOY_SHARED_SUFFIX_VALUE ".so"
#endif
#define TOY_EXE_SUFFIX ""
#endif

#include "tools/nob/build.h"

#include "tools/nob/tests.h"

static bool run_toy(const Build_Config *config, char **arguments,
                    int argument_count) {
    Cmd command = {0};
    cmd_append(&command, config->toy_exe);
    for (int i = 0; i < argument_count; ++i) {
        cmd_append(&command, arguments[i]);
    }
    return cmd_run(&command, .dont_reset = false);
}

int main(int argc, char **argv) {
    GO_REBUILD_URSELF_PLUS(argc, argv, "deps/nob/nob.h",
                           "tools/nob/build.h",
                           "tools/nob/tests.h");
    const char *program = shift(argv, argc);
    const char *command = NULL;

    Build_Config config = {
        .compiler = COMPILER_GCC,
        .mode = MODE_RELEASE,
        .jobs = (size_t)nprocs(),
        .test_filter = NULL,
    };
    char **program_arguments = NULL;
    int program_argument_count = 0;
    const char *target_name = NULL;
    const char *target_source = NULL;

    while (argc > 0) {
        const char *argument = shift(argv, argc);
        if (!command && argument[0] != '-') {
            command = argument;
            if (strcmp(command, "run") == 0) {
                program_arguments = argv;
                program_argument_count = argc;
                argc = 0;
            }
        } else if (strcmp(argument, "--cc") == 0) {
            if (argc == 0 || !parse_compiler(shift(argv, argc),
                                              &config.compiler)) {
                nob_log(ERROR, "--cc requires clang, gcc, msvc, or clang-cl");
                return 1;
            }
        } else if (starts_with(argument, "--cc=")) {
            if (!parse_compiler(argument + strlen("--cc="),
                                &config.compiler)) {
                nob_log(ERROR, "unknown compiler: %s", argument);
                return 1;
            }
        } else if (strcmp(argument, "--mode") == 0) {
            if (argc == 0 || !parse_mode(shift(argv, argc), &config.mode)) {
                nob_log(ERROR, "--mode requires release, debug, alloc, leak, or profile");
                return 1;
            }
        } else if (starts_with(argument, "--mode=")) {
            if (!parse_mode(argument + strlen("--mode="), &config.mode)) {
                nob_log(ERROR, "unknown build mode: %s", argument);
                return 1;
            }
        } else if (strcmp(argument, "-j") == 0 ||
                   strcmp(argument, "--jobs") == 0) {
            if (argc == 0 || !parse_count(shift(argv, argc), &config.jobs)) {
                nob_log(ERROR, "%s requires a positive integer", argument);
                return 1;
            }
        } else if (strcmp(argument, "--filter") == 0) {
            if (argc == 0) {
                nob_log(ERROR, "--filter requires text");
                return 1;
            }
            config.test_filter = shift(argv, argc);
        } else if (strcmp(argument, "--include") == 0) {
            if (argc == 0) {
                nob_log(ERROR, "--include requires a directory");
                return 1;
            }
            da_append(&config.include_dirs, shift(argv, argc));
        } else if (starts_with(argument, "--include=")) {
            da_append(&config.include_dirs, argument + strlen("--include="));
        } else if (strcmp(argument, "--lib-dir") == 0) {
            if (argc == 0) {
                nob_log(ERROR, "--lib-dir requires a directory");
                return 1;
            }
            da_append(&config.library_dirs, shift(argv, argc));
        } else if (starts_with(argument, "--lib-dir=")) {
            da_append(&config.library_dirs, argument + strlen("--lib-dir="));
        } else if (strcmp(argument, "--lib") == 0) {
            if (argc == 0) {
                nob_log(ERROR, "--lib requires a name or path");
                return 1;
            }
            da_append(&config.libraries, shift(argv, argc));
        } else if (starts_with(argument, "--lib=")) {
            da_append(&config.libraries, argument + strlen("--lib="));
        } else if (strcmp(argument, "-h") == 0 ||
                   strcmp(argument, "--help") == 0) {
            print_usage(program);
            return 0;
        } else if (command && (strcmp(command, "module") == 0 ||
                              strcmp(command, "bindgen") == 0) &&
                   argument[0] != '-') {
            if (!target_name) target_name = argument;
            else if (!target_source) target_source = argument;
            else {
                nob_log(ERROR, "%s accepts exactly one name and input file",
                        command);
                return 1;
            }
        } else {
            nob_log(ERROR, "unknown option: %s", argument);
            print_usage(program);
            return 1;
        }
    }

    if (!command || strcmp(command, "help") == 0 || strcmp(command, "-h") == 0 ||
        strcmp(command, "--help") == 0) {
        print_usage(program);
        return 0;
    }
    if (strcmp(command, "clean") == 0) {
        Nob_Log_Level previous_level = minimal_log_level;
        minimal_log_level = WARNING;
        bool cleaned = remove_tree("build");
        minimal_log_level = previous_level;
        if (cleaned) nob_log(INFO, "removed build");
        return cleaned ? 0 : 1;
    }
    if (strcmp(command, "build") != 0 && strcmp(command, "test") != 0 &&
        strcmp(command, "examples") != 0 &&
        strcmp(command, "module") != 0 &&
        strcmp(command, "bindgen") != 0 && strcmp(command, "run") != 0) {
        nob_log(ERROR, "unknown command: %s", command);
        print_usage(program);
        return 1;
    }
    if ((strcmp(command, "module") == 0 ||
         strcmp(command, "bindgen") == 0) &&
        (!target_name || !target_source)) {
        nob_log(ERROR, "%s requires a module name and %s file", command,
                strcmp(command, "module") == 0 ? "C source" : "JSON manifest");
        return 1;
    }
    if (!program_on_path(compiler_executable(config.compiler))) {
        nob_log(ERROR, "compiler '%s' was not found on PATH",
                compiler_executable(config.compiler));
        if (config.compiler == COMPILER_MSVC) {
            nob_log(ERROR, "run Nob from a Visual Studio Developer PowerShell");
        }
        return 1;
    }
#ifdef _WIN32
    if (config.mode == MODE_PROFILE && config.compiler == COMPILER_GCC) {
        nob_log(ERROR, "profile mode with MinGW is not supported");
        return 1;
    }
#endif
    if (!configure_paths(&config)) return 1;

    nob_log(INFO, "compiler: %s", compiler_name(config.compiler));
    nob_log(INFO, "mode: %s", mode_name(config.mode));
    nob_log(INFO, "jobs: %zu", config.jobs);

    const char *root = get_current_dir_temp();
    Compile_Commands compile_commands = {0};
    bool needs_core = strcmp(command, "build") == 0 ||
                      strcmp(command, "test") == 0 ||
                      strcmp(command, "examples") == 0 ||
                      strcmp(command, "run") == 0;
    bool ok = !needs_core || build_core(&config, &compile_commands);
    if (ok && strcmp(command, "test") == 0) {
        ok = run_all_tests(&config, root, &compile_commands);
    }
    if (ok && strcmp(command, "module") == 0) {
        ok = build_module(&config, target_name, target_source,
                          &compile_commands);
    }
    if (ok && strcmp(command, "bindgen") == 0) {
        ok = build_generated_module(&config, target_name, target_source,
                                    &compile_commands);
    }
    if (ok && strcmp(command, "examples") == 0) {
        ok = build_examples(&config, &compile_commands);
    }
    if (ok) ok = write_compile_commands(&compile_commands);
    if (ok && strcmp(command, "run") == 0) {
        ok = run_toy(&config, program_arguments, program_argument_count);
    }

    for (size_t i = 0; i < compile_commands.count; ++i) {
        da_free(compile_commands.items[i].command);
    }
    da_free(compile_commands);
    da_free(config.include_dirs);
    da_free(config.library_dirs);
    da_free(config.libraries);
    return ok ? 0 : 1;
}
