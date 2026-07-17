#ifndef TOY_NOB_BUILD_H
#define TOY_NOB_BUILD_H

/* Compiler configuration, incremental build graph, and module linking. */

typedef enum {
    COMPILER_GCC,
    COMPILER_CLANG,
    COMPILER_MSVC,
    COMPILER_CLANG_CL,
} Compiler;

typedef enum {
    MODE_RELEASE,
    MODE_DEBUG,
    MODE_ALLOC,
    MODE_LEAK,
    MODE_PROFILE,
} Build_Mode;

typedef struct {
    Compiler compiler;
    Build_Mode mode;
    size_t jobs;
    const char *test_filter;
    const char *build_dir;
    const char *object_dir;
    const char *module_dir;
    const char *test_module_dir;
    const char *runtime_lib;
    const char *toy_exe;
    File_Paths include_dirs;
    File_Paths library_dirs;
    File_Paths libraries;
} Build_Config;

typedef struct {
    const char *file;
    const char *output;
    Cmd command;
} Compile_Command;

typedef struct {
    Compile_Command *items;
    size_t count;
    size_t capacity;
} Compile_Commands;

static const char *runtime_sources[] = {
    "src/tf_alloc.c",
    "src/tf_terminal.c",
    "src/tf_context.c",
    "src/tf_debug_control.c",
    "src/tf_debug_inspect.c",
    "src/tf_dictionary.c",
    "src/generated/tf_docs.c",
    "src/tf_exec.c",
    "src/tf_parser.c",
    "src/tf_builtins_control.c",
    "src/tf_builtins_core.c",
    "src/tf_builtins_data.c",
    "src/tf_builtins_io.c",
    "src/tf_builtins_meta.c",
    "src/tf_builtins_sys.c",
    "src/tf_modules.c",
    "src/tf_native_loader.c",
    "src/tf_obj.c",
    "src/toy.c",
};

static const char *cli_sources[] = {
    "src/cli/main.c",
    "src/cli/tf_debug_protocol.c",
    "src/cli/tf_repl.c",
#ifdef _WIN32
    "deps/linenoise/linenoise_win.c",
#else
    "deps/linenoise/linenoise.c",
#endif
};

static const char *core_c_tests[] = {
    "tests/c/test_embed_api.c",
    "tests/c/test_debug_control.c",
    "tests/c/test_debug_inspection.c",
    "tests/c/test_linenoise.c",
};

static const char *static_library_path(const Build_Config *config,
                                       const char *name) {
#if defined(_WIN32)
    if (config->compiler != COMPILER_GCC) {
        return temp_sprintf("%s/%s.lib", config->build_dir, name);
    }
#endif
    return temp_sprintf("%s/lib%s.a", config->build_dir, name);
}

static const char *compiler_name(Compiler compiler) {
    switch (compiler) {
    case COMPILER_GCC: return "gcc";
    case COMPILER_CLANG: return "clang";
    case COMPILER_MSVC: return "msvc";
    case COMPILER_CLANG_CL: return "clang-cl";
    }
    return "unknown";
}

static const char *compiler_executable(Compiler compiler) {
    switch (compiler) {
    case COMPILER_GCC: return "gcc";
    case COMPILER_CLANG: return "clang";
    case COMPILER_MSVC: return "cl";
    case COMPILER_CLANG_CL: return "clang-cl";
    }
    return "cc";
}

static const char *mode_name(Build_Mode mode) {
    switch (mode) {
    case MODE_RELEASE: return "release";
    case MODE_DEBUG: return "debug";
    case MODE_ALLOC: return "alloc";
    case MODE_LEAK: return "leak";
    case MODE_PROFILE: return "profile";
    }
    return "unknown";
}

static bool is_msvc_style(Compiler compiler) {
    return compiler == COMPILER_MSVC || compiler == COMPILER_CLANG_CL;
}

static bool starts_with(const char *value, const char *prefix) {
    return strncmp(value, prefix, strlen(prefix)) == 0;
}

static bool ends_with(const char *value, const char *suffix) {
    size_t value_len = strlen(value);
    size_t suffix_len = strlen(suffix);
    return value_len >= suffix_len &&
           strcmp(value + value_len - suffix_len, suffix) == 0;
}

static void print_usage(const char *program) {
    fprintf(stderr, "Toy build\n\n");
    fprintf(stderr, "Usage: %s [options] <command> [command args]\n\n",
            program);
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  build                 Build the runtime and Toy CLI\n");
    fprintf(stderr, "  test                  Run the default test suite\n");
    fprintf(stderr, "  examples              Build the C embedding examples\n");
    fprintf(stderr, "  module <name> <file>  Build a native module\n");
    fprintf(stderr, "  bindgen <name> <json> Generate and build a native module\n");
    fprintf(stderr, "  run                   Build and run the Toy CLI\n");
    fprintf(stderr, "  clean                 Remove build outputs\n");
    fprintf(stderr, "  help                  Show this help\n\n");
    fprintf(stderr, "Nob options (place before run):\n");
    fprintf(stderr, "  --cc <compiler>       clang, gcc, msvc, or clang-cl\n");
    fprintf(stderr, "  --mode <mode>         release (default), debug, alloc, leak, profile\n");
    fprintf(stderr, "  -j, --jobs <count>    Maximum parallel compiler processes\n");
    fprintf(stderr, "  --filter <text>       Run matching tests only\n\n");
    fprintf(stderr, "External dependency options (repeatable):\n");
    fprintf(stderr, "  --include <directory> Add a C include directory\n");
    fprintf(stderr, "  --lib-dir <directory> Add a library search directory\n");
    fprintf(stderr, "  --lib <name-or-path>  Link a library\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  nob --mode debug build\n");
    fprintf(stderr, "  nob --mode debug run --tdb program.toy\n");
    fprintf(stderr, "  nob test --filter modules\n");
    fprintf(stderr, "  nob module sample.native path/to/module.c\n");
    fprintf(stderr, "  nob bindgen clib examples/interop/bindgen/clib.json\n");
    fprintf(stderr, "  nob run examples/programs/factorial.toy\n");
}

static bool parse_compiler(const char *value, Compiler *compiler) {
    if (strcmp(value, "gcc") == 0) *compiler = COMPILER_GCC;
    else if (strcmp(value, "clang") == 0) *compiler = COMPILER_CLANG;
    else if (strcmp(value, "msvc") == 0 || strcmp(value, "cl") == 0) {
        *compiler = COMPILER_MSVC;
    } else if (strcmp(value, "clang-cl") == 0) {
        *compiler = COMPILER_CLANG_CL;
    } else {
        return false;
    }
    return true;
}

static bool parse_mode(const char *value, Build_Mode *mode) {
    if (strcmp(value, "release") == 0) *mode = MODE_RELEASE;
    else if (strcmp(value, "debug") == 0) *mode = MODE_DEBUG;
    else if (strcmp(value, "alloc") == 0) *mode = MODE_ALLOC;
    else if (strcmp(value, "leak") == 0) *mode = MODE_LEAK;
    else if (strcmp(value, "profile") == 0) *mode = MODE_PROFILE;
    else return false;
    return true;
}

static bool parse_count(const char *value, size_t *count) {
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (value[0] == '\0' || *end != '\0' || parsed == 0) return false;
    *count = (size_t)parsed;
    return true;
}

static bool program_on_path(const char *program) {
#ifdef _WIN32
    char buffer[MAX_PATH];
    return SearchPathA(NULL, program, ".exe", MAX_PATH, buffer, NULL) != 0;
#else
    const char *path = getenv("PATH");
    if (!path) return false;
    const char *cursor = path;
    while (*cursor) {
        const char *separator = strchr(cursor, ':');
        size_t length = separator ? (size_t)(separator - cursor) : strlen(cursor);
        const char *candidate = temp_sprintf("%.*s/%s", (int)length, cursor,
                                             program);
        if (access(candidate, X_OK) == 0) return true;
        if (!separator) break;
        cursor = separator + 1;
    }
    return false;
#endif
}

static bool ensure_directory(const char *path) {
    if (file_exists(path)) return true;
    return mkdir_if_not_exists(path);
}

static bool configure_paths(Build_Config *config) {
    config->build_dir = temp_sprintf("build/%s/%s",
                                     compiler_name(config->compiler),
                                     mode_name(config->mode));
    config->object_dir = temp_sprintf("%s/obj", config->build_dir);
    config->module_dir = temp_sprintf("%s/modules", config->build_dir);
    config->test_module_dir = temp_sprintf("%s/test-modules",
                                           config->build_dir);
    config->runtime_lib = static_library_path(config, "toy_runtime");
    config->toy_exe = temp_sprintf("%s/toy%s", config->build_dir,
                                   TOY_EXE_SUFFIX);

    if (!ensure_directory("build")) return false;
    if (!ensure_directory(temp_sprintf("build/%s",
                                       compiler_name(config->compiler)))) {
        return false;
    }
    if (!ensure_directory(config->build_dir)) return false;
    if (!ensure_directory(config->object_dir)) return false;
    if (!ensure_directory(config->module_dir)) return false;
    if (!ensure_directory(config->test_module_dir)) return false;
    if (!ensure_directory(temp_sprintf("%s/tests", config->build_dir))) {
        return false;
    }
    if (!ensure_directory(temp_sprintf("%s/generated", config->build_dir))) {
        return false;
    }
    return ensure_directory(temp_sprintf("%s/test-work", config->build_dir));
}

static char *object_path(const Build_Config *config, const char *source) {
    char *name = temp_sprintf("%s", source);
    for (char *cursor = name; *cursor; ++cursor) {
        unsigned char character = (unsigned char)*cursor;
        if (!isalnum(character) && *cursor != '_' && *cursor != '-') {
            *cursor = '_';
        }
    }
#ifdef _WIN32
    return temp_sprintf("%s/%s.obj", config->object_dir, name);
#else
    return temp_sprintf("%s/%s.o", config->object_dir, name);
#endif
}

static bool collect_matching_files(const char *directory, const char *suffix,
                                   File_Paths *paths) {
    File_Paths entries = {0};
    if (!read_entire_dir(directory, &entries)) return false;
    for (size_t i = 0; i < entries.count; ++i) {
        const char *name = entries.items[i];
        if (ends_with(name, suffix)) {
            da_append(paths, temp_sprintf("%s/%s", directory, name));
        }
    }
    da_free(entries);
    return true;
}

static bool collect_header_dependencies(const Build_Config *config,
                                        File_Paths *dependencies) {
    da_append(dependencies, "nob.c");
    da_append(dependencies, "tools/nob/build.h");
    da_append(dependencies, "deps/nob/nob.h");
    if (!collect_matching_files("include", ".h", dependencies)) return false;
    if (!collect_matching_files("src", ".h", dependencies)) return false;
    if (!collect_matching_files("src/cli", ".h", dependencies)) return false;
    if (!collect_matching_files("src/generated", ".inc", dependencies)) return false;
    da_append(dependencies, "deps/linenoise/linenoise.h");
    if (config->mode == MODE_LEAK && file_exists("deps/stb_leakcheck/stb_leakcheck.h")) {
        da_append(dependencies, "deps/stb_leakcheck/stb_leakcheck.h");
    }
    return true;
}

static void append_mode_compile_flags(Cmd *command,
                                      const Build_Config *config) {
    if (is_msvc_style(config->compiler)) {
        switch (config->mode) {
        case MODE_RELEASE: cmd_append(command, "/O2", "/DNDEBUG"); break;
        case MODE_DEBUG: cmd_append(command, "/Od", "/Z7"); break;
        case MODE_ALLOC:
            cmd_append(command, "/O2", "/DNDEBUG", "/DTF_ALLOC_STATS");
            break;
        case MODE_LEAK:
            cmd_append(command, "/Od", "/Z7", "/DSTB_LEAKCHECK", "/Ideps");
            break;
        case MODE_PROFILE:
            cmd_append(command, "/O2", "/Z7", "/Oy-");
            break;
        }
    } else {
        switch (config->mode) {
        case MODE_RELEASE: cmd_append(command, "-O3", "-DNDEBUG"); break;
        case MODE_DEBUG: cmd_append(command, "-O0", "-g"); break;
        case MODE_ALLOC:
            cmd_append(command, "-O3", "-DNDEBUG", "-DTF_ALLOC_STATS");
            break;
        case MODE_LEAK:
#ifdef _WIN32
            cmd_append(command, "-O0", "-g", "-DSTB_LEAKCHECK", "-Ideps");
#else
            cmd_append(command, "-O0", "-g", "-fsanitize=leak",
                       "-fno-omit-frame-pointer");
#endif
            break;
        case MODE_PROFILE:
            cmd_append(command, "-O2", "-g", "-fno-omit-frame-pointer");
            break;
        }
    }
}

static void append_compile_flags(Cmd *command, const Build_Config *config) {
    cmd_append(command, compiler_executable(config->compiler));
    if (is_msvc_style(config->compiler)) {
        cmd_append(command, "/nologo", "/std:c11", "/W3",
                   "/D_CRT_SECURE_NO_WARNINGS", "/Iinclude", "/Isrc",
                   "/Ideps/linenoise",
                   "/DTOY_SHARED_SUFFIX=\"" TOY_SHARED_SUFFIX_VALUE "\"");
    } else {
        cmd_append(command, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
                   "-Iinclude", "-Isrc", "-Ideps/linenoise",
                   "-DTOY_SHARED_SUFFIX=\"" TOY_SHARED_SUFFIX_VALUE "\"");
#if defined(_WIN32)
        if (config->compiler == COMPILER_CLANG) {
            cmd_append(command, "-D_CRT_SECURE_NO_WARNINGS");
        }
#endif
    }
    for (size_t i = 0; i < config->include_dirs.count; ++i) {
        if (is_msvc_style(config->compiler)) {
            cmd_append(command, temp_sprintf("/I%s",
                                             config->include_dirs.items[i]));
        } else {
            cmd_append(command, "-I", config->include_dirs.items[i]);
        }
    }
    append_mode_compile_flags(command, config);
}

static bool library_is_path(const char *library) {
    return strchr(library, '/') || strchr(library, '\\') ||
           ends_with(library, ".a") || ends_with(library, ".lib") ||
           ends_with(library, ".so") || ends_with(library, ".dylib") ||
           ends_with(library, ".dll");
}

static void append_link_flags(Cmd *command, const Build_Config *config,
                              bool with_external_libraries) {
    if (is_msvc_style(config->compiler)) {
        if (with_external_libraries) {
            for (size_t i = 0; i < config->libraries.count; ++i) {
                const char *library = config->libraries.items[i];
                cmd_append(command, library_is_path(library) ? library :
                           temp_sprintf("%s.lib", library));
            }
        }
#ifdef _WIN32
        cmd_append(command, "user32.lib");
#endif
        bool needs_link_options =
            (with_external_libraries && config->library_dirs.count > 0) ||
            config->mode == MODE_PROFILE || config->mode == MODE_DEBUG ||
            config->mode == MODE_LEAK;
        if (needs_link_options) cmd_append(command, "/link");
        if (with_external_libraries) {
            for (size_t i = 0; i < config->library_dirs.count; ++i) {
                cmd_append(command, temp_sprintf("/LIBPATH:%s",
                           config->library_dirs.items[i]));
            }
        }
        if (config->mode == MODE_PROFILE || config->mode == MODE_DEBUG ||
            config->mode == MODE_LEAK) {
            cmd_append(command, "/DEBUG");
        }
    } else {
        if (with_external_libraries) {
            for (size_t i = 0; i < config->library_dirs.count; ++i) {
                cmd_append(command, "-L", config->library_dirs.items[i]);
            }
            for (size_t i = 0; i < config->libraries.count; ++i) {
                const char *library = config->libraries.items[i];
                cmd_append(command, library_is_path(library) ? library :
                           temp_sprintf("-l%s", library));
            }
        }
#ifdef _WIN32
        cmd_append(command, "-luser32");
#else
        cmd_append(command, "-lm");
#ifndef __APPLE__
        cmd_append(command, "-ldl");
#endif
        if (config->mode == MODE_LEAK) cmd_append(command, "-fsanitize=leak");
#endif
    }
}

static void record_compile_command(Compile_Commands *commands,
                                   const char *source, const char *output,
                                   const Cmd *command) {
    Compile_Command record = {
        .file = source,
        .output = output,
        .command = {0},
    };
    da_append_many(&record.command, command->items, command->count);
    da_append(commands, record);
}

static bool write_json_string(FILE *file, const char *value) {
    if (fputc('"', file) == EOF) return false;
    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor; ++cursor) {
        switch (*cursor) {
        case '"': if (fputs("\\\"", file) < 0) return false; break;
        case '\\': if (fputs("\\\\", file) < 0) return false; break;
        case '\b': if (fputs("\\b", file) < 0) return false; break;
        case '\f': if (fputs("\\f", file) < 0) return false; break;
        case '\n': if (fputs("\\n", file) < 0) return false; break;
        case '\r': if (fputs("\\r", file) < 0) return false; break;
        case '\t': if (fputs("\\t", file) < 0) return false; break;
        default:
            if (*cursor < 0x20) {
                if (fprintf(file, "\\u%04x", *cursor) < 0) return false;
            } else if (fputc(*cursor, file) == EOF) {
                return false;
            }
        }
    }
    return fputc('"', file) != EOF;
}

static bool write_compile_commands(const Compile_Commands *commands) {
    const char *directory = get_current_dir_temp();
    FILE *file = fopen("build/compile_commands.json", "wb");
    if (!file) {
        nob_log(ERROR, "could not write compile_commands.json: %s",
                strerror(errno));
        return false;
    }

    bool ok = fputs("[\n", file) >= 0;
    for (size_t i = 0; ok && i < commands->count; ++i) {
        const Compile_Command *record = &commands->items[i];
        ok = fputs("  {\"directory\":", file) >= 0;
        if (ok) ok = write_json_string(file, directory);
        if (ok) ok = fputs(",\"file\":", file) >= 0;
        if (ok) ok = write_json_string(file, record->file);
        if (ok) ok = fputs(",\"output\":", file) >= 0;
        if (ok) ok = write_json_string(file, record->output);
        if (ok) ok = fputs(",\"arguments\":[", file) >= 0;
        for (size_t j = 0; ok && j < record->command.count; ++j) {
            if (j > 0) ok = fputc(',', file) != EOF;
            if (ok) ok = write_json_string(file, record->command.items[j]);
        }
        if (ok) ok = fputs(i + 1 == commands->count ? "]}\n" : "]},\n",
                           file) >= 0;
    }
    if (ok) ok = fputs("]\n", file) >= 0;
    if (fclose(file) != 0) ok = false;
    if (ok) nob_log(INFO, "generated compile_commands.json");
    return ok;
}

static bool source_needs_rebuild(const char *output, const char *source,
                                 const File_Paths *headers,
                                 bool *needs_rebuild_out) {
    File_Paths inputs = {0};
    da_append(&inputs, source);
    da_append_many(&inputs, headers->items, headers->count);
    int result = needs_rebuild(output, inputs.items, inputs.count);
    da_free(inputs);
    if (result < 0) return false;
    *needs_rebuild_out = result != 0;
    return true;
}

static bool schedule_compile_options(
    const Build_Config *config, const char *source, const char *output,
    const File_Paths *headers, Compile_Commands *compile_commands,
    Procs *processes, bool module_object,
    const File_Paths *additional_include_dirs,
    const File_Paths *definitions) {
    Cmd command = {0};
    append_compile_flags(&command, config);
    if (is_msvc_style(config->compiler) &&
        starts_with(source, "deps/linenoise/")) {
        cmd_append(&command, "/wd4267");
    }
    if (module_object && !is_msvc_style(config->compiler)) {
#ifndef _WIN32
        cmd_append(&command, "-fPIC");
#endif
        cmd_append(&command, "-fvisibility=hidden");
    }
    if (additional_include_dirs) {
        for (size_t i = 0; i < additional_include_dirs->count; ++i) {
            if (is_msvc_style(config->compiler)) {
                cmd_append(&command, temp_sprintf("/I%s",
                           additional_include_dirs->items[i]));
            } else {
                cmd_append(&command, "-I", additional_include_dirs->items[i]);
            }
        }
    }
    if (definitions) {
        for (size_t i = 0; i < definitions->count; ++i) {
            cmd_append(&command, temp_sprintf(
                is_msvc_style(config->compiler) ? "/D%s" : "-D%s",
                definitions->items[i]));
        }
    }
    if (is_msvc_style(config->compiler)) {
        cmd_append(&command, "/c", source, temp_sprintf("/Fo:%s", output));
    } else {
        cmd_append(&command, "-c", source, "-o", output);
    }
    record_compile_command(compile_commands, source, output, &command);

    bool rebuild = false;
    if (!source_needs_rebuild(output, source, headers, &rebuild)) return false;
    if (!rebuild) return true;

    if (!cmd_run(&command, .async = processes, .max_procs = config->jobs)) {
        return false;
    }
    return true;
}

static bool schedule_compile_ex(const Build_Config *config,
                                const char *source, const char *output,
                                const File_Paths *headers,
                                Compile_Commands *compile_commands,
                                Procs *processes, bool module_object) {
    return schedule_compile_options(config, source, output, headers,
                                    compile_commands, processes,
                                    module_object, NULL, NULL);
}

static bool schedule_compile(const Build_Config *config, const char *source,
                             const char *output, const File_Paths *headers,
                             Compile_Commands *compile_commands,
                             Procs *processes) {
    return schedule_compile_ex(config, source, output, headers,
                               compile_commands, processes, false);
}

static bool archive_library(const Build_Config *config, const char *output,
                            const File_Paths *objects) {
    int rebuild = needs_rebuild(output, objects->items, objects->count);
    if (rebuild < 0) return false;
    if (!rebuild) return true;
    if (file_exists(output) && !delete_file(output)) {
        return false;
    }

    Cmd command = {0};
#if defined(_WIN32)
    if (config->compiler == COMPILER_CLANG ||
        config->compiler == COMPILER_CLANG_CL) {
        cmd_append(&command, "llvm-lib", "/nologo",
                   temp_sprintf("/OUT:%s", output));
    } else if (config->compiler == COMPILER_MSVC) {
        cmd_append(&command, "lib", "/nologo",
                   temp_sprintf("/OUT:%s", output));
    } else
#endif
    {
        cmd_append(&command, "ar", "rcs", output);
    }
    da_append_many(&command, objects->items, objects->count);
    return cmd_run(&command, .dont_reset = false);
}

static bool link_shared_module(const Build_Config *config,
                               const char *output,
                               const File_Paths *objects,
                               bool with_external_libraries) {
    int rebuild = needs_rebuild(output, objects->items, objects->count);
    if (rebuild < 0) return false;
    if (!rebuild) return true;

    Cmd command = {0};
    cmd_append(&command, compiler_executable(config->compiler));
    if (is_msvc_style(config->compiler)) {
        cmd_append(&command, "/nologo", "/LD");
        da_append_many(&command, objects->items, objects->count);
        cmd_append(&command, temp_sprintf("/Fe:%s", output));
    } else {
#ifdef __APPLE__
        cmd_append(&command, "-dynamiclib");
#else
        cmd_append(&command, "-shared");
#endif
        da_append_many(&command, objects->items, objects->count);
        cmd_append(&command, "-o", output);
    }
    append_link_flags(&command, config, with_external_libraries);
    return cmd_run(&command, .dont_reset = false);
}

static bool valid_module_name(const char *name) {
    if (!name || name[0] == '\0') return false;
    for (const unsigned char *cursor = (const unsigned char *)name;
         *cursor; ++cursor) {
        if (!isalnum(*cursor) && *cursor != '.' && *cursor != '_' &&
            *cursor != '-') {
            return false;
        }
    }
    return true;
}

static bool build_module(const Build_Config *config, const char *name,
                         const char *source,
                         Compile_Commands *compile_commands) {
    if (!valid_module_name(name)) {
        nob_log(ERROR, "module name must contain only letters, digits, '.', '_', or '-'");
        return false;
    }
    if (!file_exists(source)) {
        nob_log(ERROR, "module source does not exist: %s", source);
        return false;
    }

    File_Paths headers = {0};
    File_Paths objects = {0};
    Procs processes = {0};
    const char *object = object_path(config, source);
    const char *output = temp_sprintf("%s/toy_%s%s", config->module_dir,
                                      name, TOY_SHARED_SUFFIX_VALUE);
    da_append(&objects, object);

    bool ok = collect_header_dependencies(config, &headers);
    /* External include/link options are not timestamped inputs. Rebuild the
       small module object on each explicit module command so changed options
       cannot silently reuse an incompatible object or shared library. */
    if (ok && file_exists(object)) ok = delete_file(object);
    if (ok) {
        ok = schedule_compile_ex(config, source, object, &headers,
                                 compile_commands, &processes, true);
    }
    if (!procs_flush(&processes)) ok = false;
    if (ok) {
        ok = link_shared_module(config, output, &objects, true);
    }
    if (ok) nob_log(INFO, "built native module %s", output);

    da_free(processes);
    da_free(headers);
    da_free(objects);
    return ok;
}

static bool link_executable_options(const Build_Config *config,
                                    const char *output,
                                    const File_Paths *objects,
                                    bool with_runtime,
                                    bool with_external_libraries) {
    File_Paths inputs = {0};
    da_append_many(&inputs, objects->items, objects->count);
    if (with_runtime) da_append(&inputs, config->runtime_lib);
    int rebuild = needs_rebuild(output, inputs.items, inputs.count);
    da_free(inputs);
    if (rebuild < 0) return false;
    if (!rebuild) return true;

    Cmd command = {0};
    cmd_append(&command, compiler_executable(config->compiler));
    if (is_msvc_style(config->compiler)) cmd_append(&command, "/nologo");
    da_append_many(&command, objects->items, objects->count);
    if (with_runtime) cmd_append(&command, config->runtime_lib);
    if (is_msvc_style(config->compiler)) {
        cmd_append(&command, temp_sprintf("/Fe:%s", output));
    } else {
        cmd_append(&command, "-o", output);
    }
    append_link_flags(&command, config, with_external_libraries);
    return cmd_run(&command, .dont_reset = false);
}

static bool link_executable(const Build_Config *config, const char *output,
                            const File_Paths *objects, bool with_runtime) {
    return link_executable_options(config, output, objects, with_runtime,
                                   false);
}

static bool run_generator(const Build_Config *config, const char *manifest,
                          const char *output) {
    (void)config;
    const char *inputs[] = {manifest, "tools/generate-binding.js"};
    int rebuild = needs_rebuild(output, inputs, ARRAY_LEN(inputs));
    if (rebuild < 0) return false;
    if (!rebuild) return true;
    if (!program_on_path("node")) {
        nob_log(ERROR, "Node.js is required to generate C bindings");
        return false;
    }
    Cmd command = {0};
    cmd_append(&command, "node", "tools/generate-binding.js", manifest,
               output);
    return cmd_run(&command, .dont_reset = false);
}

static bool build_generated_module(const Build_Config *config,
                                   const char *name, const char *manifest,
                                   Compile_Commands *compile_commands) {
    if (!file_exists(manifest)) {
        nob_log(ERROR, "binding manifest does not exist: %s", manifest);
        return false;
    }
    const char *generated = temp_sprintf("%s/generated/toy_%s.c",
                                         config->build_dir, name);
    if (!run_generator(config, manifest, generated)) return false;
    return build_module(config, name, generated, compile_commands);
}

static bool build_examples(const Build_Config *config,
                           Compile_Commands *compile_commands) {
    const char *sources[] = {
        "examples/embedding/embed.c",
        "examples/embedding/callbacks.c",
        "examples/embedding/values.c",
    };
    const char *names[] = {
        "toy_embed_example",
        "toy_embed_callbacks_example",
        "toy_embed_values_example",
    };
    File_Paths headers = {0};
    Procs processes = {0};
    File_Paths objects = {0};
    bool ok = collect_header_dependencies(config, &headers);
    for (size_t i = 0; ok && i < ARRAY_LEN(sources); ++i) {
        const char *object = object_path(config, sources[i]);
        da_append(&objects, object);
        ok = schedule_compile(config, sources[i], object, &headers,
                              compile_commands, &processes);
    }
    if (!procs_flush(&processes)) ok = false;
    for (size_t i = 0; ok && i < ARRAY_LEN(sources); ++i) {
        File_Paths one_object = {0};
        da_append(&one_object, objects.items[i]);
        const char *output = temp_sprintf("%s/%s%s", config->build_dir,
                                          names[i], TOY_EXE_SUFFIX);
        ok = link_executable(config, output, &one_object, true);
        if (ok) nob_log(INFO, "built %s", output);
        da_free(one_object);
    }
    da_free(objects);
    da_free(processes);
    da_free(headers);
    return ok;
}

static bool build_core(const Build_Config *config,
                       Compile_Commands *compile_commands) {
    File_Paths headers = {0};
    File_Paths runtime_objects = {0};
    File_Paths cli_objects = {0};
    Procs processes = {0};
    bool ok = collect_header_dependencies(config, &headers);

    for (size_t i = 0; ok && i < ARRAY_LEN(runtime_sources); ++i) {
        const char *output = object_path(config, runtime_sources[i]);
        da_append(&runtime_objects, output);
        ok = schedule_compile(config, runtime_sources[i], output, &headers,
                              compile_commands, &processes);
    }
    for (size_t i = 0; ok && i < ARRAY_LEN(cli_sources); ++i) {
        const char *output = object_path(config, cli_sources[i]);
        da_append(&cli_objects, output);
        ok = schedule_compile(config, cli_sources[i], output, &headers,
                              compile_commands, &processes);
    }
    if (!procs_flush(&processes)) ok = false;
    if (ok) {
        ok = archive_library(config, config->runtime_lib, &runtime_objects);
    }
    if (ok) ok = link_executable(config, config->toy_exe, &cli_objects, true);

    da_free(processes);
    da_free(headers);
    da_free(runtime_objects);
    da_free(cli_objects);
    if (ok) {
        nob_log(INFO, "built %s", config->toy_exe);
        nob_log(INFO, "built %s", config->runtime_lib);
    }
    return ok;
}

static bool remove_tree(const char *path) {
    if (!file_exists(path)) return true;
    Nob_File_Type type = get_file_type(path);
    if (type != FILE_DIRECTORY) return delete_file(path);

    File_Paths children = {0};
    if (!read_entire_dir(path, &children)) return false;
    bool ok = true;
    for (size_t i = 0; ok && i < children.count; ++i) {
        const char *name = children.items[i];
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        ok = remove_tree(temp_sprintf("%s/%s", path, name));
    }
    da_free(children);
    return ok && delete_file(path);
}

#endif  // TOY_NOB_BUILD_H
