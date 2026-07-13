#include "tf_lib.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "tf_obj.h"
#include "tf_exec.h"
#include "tf_alloc.h"
#include "tf_lexer.h"

static int read_line_dynamic(FILE *fp, char **buf, size_t *cap,
                             size_t *line_len) {
    *line_len = 0;
    int c = 0;
    while ((c = fgetc(fp)) != EOF && c != '\n') {
        size_t needed = *line_len + 2;
        if (needed > *cap) {
            size_t new_cap = *cap == 0 ? 128 : *cap;
            while (new_cap < needed) new_cap *= 2;
            *buf = tf_xrealloc(*buf, new_cap);
            *cap = new_cap;
        }
        (*buf)[(*line_len)++] = (char)c;
    }

    if (c == EOF && ferror(fp)) return -1;
    if (c == EOF && *line_len == 0) return 0;
    if (*line_len > 0 && (*buf)[*line_len - 1] == '\r') (*line_len)--;
    if (*buf) (*buf)[*line_len] = '\0';
    return 1;
}

tf_ret tf_printf(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_peek(ctx, 0);

    if (o->type != TF_OBJ_TYPE_STR) {
        o = tf_stack_pop(ctx);
        tf_obj_print_value(o);
        tf_obj_release(o);
        return TF_OK;
    }

    const char *fmt = o->str.ptr;
    size_t fmt_len = o->str.len;

    // Count placeholders and check for escapes
    size_t count = 0;
    bool has_format = false;
    for (size_t i = 0; i < fmt_len; i++) {
        if (fmt[i] == '{') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                count++;
                has_format = true;
                i++;
            } else if (i + 1 < fmt_len && fmt[i + 1] == '{') {
                has_format = true;
                i++;
            }
        } else if (fmt[i] == '}') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                has_format = true;
                i++;
            }
        }
    }

    if (!has_format) {
        o = tf_stack_pop(ctx);
        tf_obj_print_value(o);
        tf_obj_release(o);
        return TF_OK;
    }

    if (tf_stack_len(ctx) - 1 < count) {
        tf_ctx_runtime_errorf(ctx,
                              "'printf' expected %zu format argument%s, found %zu\n",
                              count, count == 1 ? "" : "s",
                              tf_stack_len(ctx) - 1);
        return TF_ERR;
    }

    o = tf_stack_pop(ctx);
    size_t arg_idx = 0;
    for (size_t i = 0; i < fmt_len; i++) {
        if (fmt[i] == '{') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                tf_obj_print_value(tf_stack_peek(ctx, count - 1 - arg_idx));
                arg_idx++;
                i++;
            } else if (i + 1 < fmt_len && fmt[i + 1] == '{') {
                putchar('{');
                i++;
            } else {
                putchar('{');
            }
        } else if (fmt[i] == '}') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                putchar('}');
                i++;
            } else {
                putchar('}');
            }
        } else {
            putchar(fmt[i]);
        }
    }

    for (size_t i = 0; i < count; i++) {
        tf_obj *arg = tf_stack_pop(ctx);
        tf_obj_release(arg);
    }
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_print(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    tf_obj_print_value(o);
    putchar('\n');
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_dot(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_peek(ctx, 0);
    tf_obj_print_value(o);
    printf("\n");
    return TF_OK;
}

tf_ret tf_stack(tf_ctx *ctx) {
    size_t len = tf_stack_len(ctx);
    printf("<%zu> ", len);
    for (size_t i = 0; i < len; i++) {
        tf_obj_print_value_colored(tf_stack_peek(ctx, len - 1 - i));
        printf(" ");
    }
    printf("\n");
    return TF_OK;
}

tf_ret tf_stack_source(tf_ctx *ctx) {
    size_t len = tf_stack_len(ctx);
    printf("<%zu> ", len);
    for (size_t i = 0; i < len; i++) {
        tf_obj_print_source_colored(tf_stack_peek(ctx, len - 1 - i));
        printf(" ");
    }
    printf("\n");
    return TF_OK;
}

tf_ret tf_clear(tf_ctx *ctx) {
    ctx->suppress_repl_status = true;
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(out, &info)) {
        DWORD cell_count = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
        COORD home = {0, 0};
        DWORD written = 0;

        if (FillConsoleOutputCharacterA(out, ' ', cell_count, home, &written) &&
            FillConsoleOutputAttribute(out, info.wAttributes, cell_count, home,
                                       &written) &&
            SetConsoleCursorPosition(out, home)) {
            return TF_OK;
        }
    }
#endif

    printf("\x1b[H\x1b[2J");
    fflush(stdout);
    return TF_OK;
}

tf_ret tf_key(tf_ctx *ctx) {
    int c = getchar();
    if (c == EOF) {
        tf_ctx_runtime_errorf(ctx, "'read-key' failed to read input\n");
        return TF_ERR;
    }
    unsigned char byte = (unsigned char)c;
    tf_stack_push(ctx, tf_obj_new_string((const char *)&byte, 1));
    return TF_OK;
}

tf_ret tf_input(tf_ctx *ctx) {
    char *buf = NULL;
    size_t cap = 0;
    size_t len = 0;
    int status = read_line_dynamic(stdin, &buf, &cap, &len);
    if (status <= 0) {
        free(buf);
        tf_ctx_runtime_errorf(ctx, "'read-line' failed to read input\n");
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_string_take(buf, len));
    return TF_OK;
}

static char *source_directory(tf_ctx *ctx) {
    if (!ctx->current_span.source) return tf_xstrdup(".");
    const char *source = tf_source_file_name(ctx->current_span.source);
    if (!source || source[0] == '<') return tf_xstrdup(".");

    const char *separator = NULL;
    for (const char *p = source; *p; p++) {
        if (*p == '/' || *p == '\\') separator = p;
    }
    if (!separator) return tf_xstrdup(".");

    size_t len = (size_t)(separator - source);
    if (len == 0) len = 1;
    char *directory = tf_xmalloc(len + 1);
    memcpy(directory, source, len);
    directory[len] = '\0';
    return directory;
}

static char *join_path(const char *directory, const char *relative) {
    size_t directory_len = strlen(directory);
    size_t relative_len = strlen(relative);
    bool needs_separator = directory_len > 0 &&
                           directory[directory_len - 1] != '/' &&
                           directory[directory_len - 1] != '\\';
    char *path = tf_xmalloc(directory_len + (needs_separator ? 1 : 0) +
                            relative_len + 1);
    memcpy(path, directory, directory_len);
    size_t out = directory_len;
    if (needs_separator) path[out++] = '/';
    memcpy(path + out, relative, relative_len + 1);
    return path;
}

static bool path_is_absolute(const char *path) {
    if (!path || path[0] == '\0') return false;
    if (path[0] == '/' || path[0] == '\\') return true;
    return isalpha((unsigned char)path[0]) && path[1] == ':' &&
           (path[2] == '/' || path[2] == '\\');
}

typedef enum {
    SOURCE_READ_OK,
    SOURCE_READ_NOT_FOUND,
    SOURCE_READ_ERROR
} source_read_status;

static source_read_status read_source_file(const char *path, char **source) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return errno == ENOENT || errno == ENOTDIR ? SOURCE_READ_NOT_FOUND
                                                   : SOURCE_READ_ERROR;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return SOURCE_READ_ERROR;
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return SOURCE_READ_ERROR;
    }
    rewind(fp);

    char *buffer = tf_xmalloc((size_t)size + 1);
    size_t read = fread(buffer, 1, (size_t)size, fp);
    fclose(fp);
    if (read != (size_t)size) {
        free(buffer);
        return SOURCE_READ_ERROR;
    }
    buffer[read] = '\0';
    *source = buffer;
    return SOURCE_READ_OK;
}

static source_read_status resolve_source_file(tf_ctx *ctx,
                                              const char *requested,
                                              char **resolved, char **source) {
    if (path_is_absolute(requested)) {
        *resolved = tf_xstrdup(requested);
        return read_source_file(*resolved, source);
    }

    char *directory = source_directory(ctx);
    if (strcmp(directory, ".") == 0) {
        free(directory);
        *resolved = tf_xstrdup(requested);
        return read_source_file(*resolved, source);
    }

    *resolved = join_path(directory, requested);
    free(directory);
    source_read_status status = read_source_file(*resolved, source);
    if (status != SOURCE_READ_NOT_FOUND) return status;

    free(*resolved);
    *resolved = tf_xstrdup(requested);
    return read_source_file(*resolved, source);
}

tf_ret tf_load(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;

    tf_obj *path = tf_stack_pop(ctx);
    char *resolved = NULL;
    char *source = NULL;
    source_read_status status =
        resolve_source_file(ctx, path->str.ptr, &resolved, &source);
    if (status != SOURCE_READ_OK) {
        tf_ctx_runtime_errorf(
            ctx, status == SOURCE_READ_NOT_FOUND ? "failed to load '%s'\n"
                                                 : "'load' failed to read '%s'\n",
            path->str.ptr);
        free(resolved);
        tf_obj_release(path);
        return TF_ERR;
    }

    tf_obj *prg = tf_lexer_parse(resolved, source);
    free(source);
    free(resolved);
    if (!prg) {
        tf_obj_release(path);
        return TF_ERR;
    }

    tf_source_file_set_module(prg->span.source,
                              tf_current_module_index(ctx));
    tf_frame_push_program(ctx, prg);
    tf_obj_release(prg);
    tf_obj_release(path);
    return TF_OK;
}

typedef struct {
    size_t module_index;
    size_t alias_owner;
    char *alias_name;
    size_t alias_len;
} require_state;

static tf_ret require_finish(tf_ctx *ctx, void *raw_state, bool *done) {
    (void)ctx;
    (void)raw_state;
    *done = true;
    return TF_OK;
}

static void require_cleanup(tf_ctx *ctx, void *raw_state, tf_ret status) {
    require_state *state = raw_state;
    if (status != TF_OK && state->alias_name) {
        tf_module_alias_remove(ctx, state->alias_owner, state->alias_name,
                               state->alias_len, state->module_index);
    }
    tf_module_finish(ctx, state->module_index, status);
    free(state->alias_name);
    free(state);
}

static char *module_relative_path(const char *name, size_t len) {
    size_t path_len = len + 4;
    char *path = tf_xmalloc(path_len + 1);
    for (size_t i = 0; i < len; i++) {
        path[i] = name[i] == '.' ? '/' : name[i];
    }
    memcpy(path + len, ".toy", 5);
    return path;
}

static void release_require_args(tf_obj *name, tf_obj *alias) {
    tf_obj_release(name);
    if (alias) tf_obj_release(alias);
}

static bool alias_name_valid(const char *name, size_t len) {
    return !memchr(name, '.', len) && tf_module_name_valid(name, len);
}

static bool alias_matches_module(tf_ctx *ctx, size_t owner_module_index,
                                 tf_obj *alias, tf_obj *module_name) {
    size_t target = tf_module_alias_find(ctx, owner_module_index,
                                         alias->str.ptr, alias->str.len);
    if (target == (size_t)-1) return true;
    const tf_module *module = tf_module_get(ctx, target);
    if (module && module->name_len == module_name->str.len &&
        memcmp(module->name, module_name->str.ptr, module->name_len) == 0) {
        return true;
    }
    tf_ctx_runtime_errorf(
        ctx, "module alias '%s' already refers to '%s'\n", alias->str.ptr,
        module ? module->name : "<unknown>");
    return false;
}

static tf_ret require_module(tf_ctx *ctx, tf_obj *name, tf_obj *alias) {
    size_t owner_module_index = tf_current_module_index(ctx);
    if (!tf_module_name_valid(name->str.ptr, name->str.len)) {
        tf_ctx_runtime_errorf(ctx, "invalid module name '%s'\n", name->str.ptr);
        release_require_args(name, alias);
        return TF_ERR;
    }
    if (alias && !alias_name_valid(alias->str.ptr, alias->str.len)) {
        tf_ctx_runtime_errorf(ctx, "invalid module alias '%s'\n",
                              alias->str.ptr);
        release_require_args(name, alias);
        return TF_ERR;
    }
    if (alias && !alias_matches_module(ctx, owner_module_index, alias, name)) {
        release_require_args(name, alias);
        return TF_ERR;
    }

    size_t existing = tf_module_find(ctx, name->str.ptr, name->str.len);
    if (existing != (size_t)-1) {
        const tf_module *module = tf_module_get(ctx, existing);
        if (module->state == TF_MODULE_LOADED) {
            if (alias &&
                !tf_module_alias_add(ctx, owner_module_index, alias->str.ptr,
                                     alias->str.len, existing)) {
                tf_ctx_runtime_errorf(ctx, "failed to register module alias '%s'\n",
                                      alias->str.ptr);
                release_require_args(name, alias);
                return TF_ERR;
            }
            release_require_args(name, alias);
            return TF_OK;
        }
        if (module->state == TF_MODULE_LOADING) {
            tf_ctx_runtime_errorf(
                ctx, "cyclic module dependency involving '%s'\n",
                name->str.ptr);
        } else {
            tf_ctx_runtime_errorf(ctx, "module '%s' previously failed to load\n",
                                  name->str.ptr);
        }
        release_require_args(name, alias);
        return TF_ERR;
    }

    tf_word *namespace_conflict =
        tf_dict_namespace_conflict(ctx, name->str.ptr, name->str.len);
    if (namespace_conflict) {
        tf_ctx_runtime_errorf(ctx,
                              "module '%s' conflicts with existing word '%s'\n",
                              name->str.ptr, namespace_conflict->name);
        release_require_args(name, alias);
        return TF_ERR;
    }

    char *relative = module_relative_path(name->str.ptr, name->str.len);
    char *path = NULL;
    char *source = NULL;
    source_read_status read_status =
        resolve_source_file(ctx, relative, &path, &source);
    free(relative);

    if (read_status != SOURCE_READ_OK) {
        tf_ctx_runtime_errorf(
            ctx, read_status == SOURCE_READ_NOT_FOUND
                     ? "failed to find module '%s'\n"
                     : "failed to read module '%s'\n",
            name->str.ptr);
        free(path);
        release_require_args(name, alias);
        return TF_ERR;
    }

    size_t module_index = tf_module_begin(ctx, name->str.ptr, name->str.len,
                                          path);
    tf_obj *program = tf_lexer_parse(path, source);
    free(source);
    free(path);
    if (!program) {
        tf_module_finish(ctx, module_index, TF_ERR);
        tf_ctx_set_error(ctx, "module parsing failed");
        release_require_args(name, alias);
        return TF_ERR;
    }
    tf_source_file_set_module(program->span.source, module_index);

    if (alias &&
        !tf_module_alias_add(ctx, owner_module_index, alias->str.ptr,
                             alias->str.len, module_index)) {
        tf_ctx_runtime_errorf(ctx, "failed to register module alias '%s'\n",
                              alias->str.ptr);
        tf_module_finish(ctx, module_index, TF_ERR);
        tf_obj_release(program);
        release_require_args(name, alias);
        return TF_ERR;
    }

    require_state *state = tf_xmalloc(sizeof(*state));
    state->module_index = module_index;
    state->alias_owner = owner_module_index;
    state->alias_name = NULL;
    state->alias_len = 0;
    if (alias) {
        state->alias_name = tf_xmalloc(alias->str.len + 1);
        memcpy(state->alias_name, alias->str.ptr, alias->str.len + 1);
        state->alias_len = alias->str.len;
    }
    tf_frame_push_native(ctx, require_finish, require_cleanup, state);
    tf_frame_push_program_module(ctx, program, module_index);
    tf_obj_release(program);
    release_require_args(name, alias);
    return TF_OK;
}

tf_ret tf_require(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    return require_module(ctx, tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR), NULL);
}

tf_ret tf_require_as(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_SYMBOL) ||
        !tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }
    tf_obj *alias = tf_stack_pop_type(ctx, TF_OBJ_TYPE_SYMBOL);
    tf_obj *name = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    return require_module(ctx, name, alias);
}

tf_ret tf_readf(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_ctx_runtime_errorf(ctx, "'read-file' failed to open '%s'\n", path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        tf_ctx_runtime_errorf(ctx, "'read-file' failed to seek '%s'\n", path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        tf_ctx_runtime_errorf(ctx, "'read-file' failed to read size for '%s'\n",
                              path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }
    rewind(fp);

    char *buf = tf_xmalloc((size_t)size + 1);
    size_t n_read = fread(buf, 1, (size_t)size, fp);
    if (n_read != (size_t)size) {
        fclose(fp);
        free(buf);
        tf_ctx_runtime_errorf(ctx, "'read-file' failed to read all bytes from '%s'\n",
                              path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }
    buf[n_read] = '\0';
    fclose(fp);

    tf_stack_push(ctx, tf_obj_new_string_take(buf, n_read));
    tf_obj_release(path);
    return TF_OK;
}

tf_ret tf_writef(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_STR) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }
    tf_obj *content = tf_stack_pop(ctx);
    tf_obj *path = tf_stack_pop(ctx);

    FILE *fp = fopen(path->str.ptr, "wb");
    if (!fp) {
        tf_ctx_runtime_errorf(ctx, "'write-file' failed to open '%s'\n", path->str.ptr);
        tf_obj_release(content);
        tf_obj_release(path);
        return TF_ERR;
    }

    size_t n_written = fwrite(content->str.ptr, 1, content->str.len, fp);
    fclose(fp);

    tf_ret res = (n_written == content->str.len) ? TF_OK : TF_ERR;
    if (res == TF_ERR) {
        tf_ctx_runtime_errorf(ctx, "'write-file' failed to write all bytes to '%s'\n",
                              path->str.ptr);
    }
    tf_obj_release(content);
    tf_obj_release(path);
    return res;
}

tf_ret tf_delf(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    int res = remove(path->str.ptr);
    if (res != 0) {
        tf_ctx_runtime_errorf(ctx, "'delete-file' failed to delete '%s'\n", path->str.ptr);
    }
    tf_obj_release(path);
    return (res == 0) ? TF_OK : TF_ERR;
}

tf_ret tf_readl(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_ctx_runtime_errorf(ctx, "'read-lines' failed to open '%s'\n", path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }

    tf_obj *lines = tf_obj_new_vector();
    char *buf = NULL;
    size_t cap = 0;
    size_t line_len = 0;
    int status = 0;
    while ((status = read_line_dynamic(fp, &buf, &cap, &line_len)) > 0) {
        tf_vector_push(lines, tf_obj_new_string(buf, line_len));
    }
    free(buf);
    if (status < 0) {
        fclose(fp);
        tf_ctx_runtime_errorf(ctx, "'read-lines' failed while reading '%s'\n",
                              path->str.ptr);
        tf_obj_release(lines);
        tf_obj_release(path);
        return TF_ERR;
    }
    fclose(fp);

    tf_stack_push(ctx, lines);
    tf_obj_release(path);
    return TF_OK;
}

tf_ret tf_exists_q(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

#ifdef _WIN32
    struct _stat info;
    bool exists = _stat(path->str.ptr, &info) == 0;
#else
    struct stat info;
    bool exists = stat(path->str.ptr, &info) == 0;
#endif

    tf_stack_push(ctx, tf_obj_new_bool(exists));
    tf_obj_release(path);
    return TF_OK;
}
