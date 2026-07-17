#define TOY_MODULE_IMPLEMENTATION
#include "toy_module.h"

#include <sqlite3.h>

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQLITE_DATABASE_TYPE "sqlite.database"
#define SQLITE_STATEMENT_TYPE "sqlite.statement"

typedef struct {
    sqlite3 *handle;
} toy_sqlite_database;

typedef struct {
    sqlite3_stmt *handle;
    bool has_row;
} toy_sqlite_statement;

static toy_status failf(toy_state *state, const char *format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    return toy_fail(state, message);
}

static char *copy_c_string(toy_state *state, size_t depth, bool *contains_nul) {
    const char *data = NULL;
    size_t length = 0;
    *contains_nul = false;
    if (!toy_get_string(state, depth, &data, &length) || length == SIZE_MAX) {
        return NULL;
    }
    if (memchr(data, '\0', length)) {
        *contains_nul = true;
        return NULL;
    }
    char *copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, data, length);
    copy[length] = '\0';
    return copy;
}

static bool get_database(toy_state *state, size_t depth,
                         toy_sqlite_database **database) {
    void *resource = NULL;
    if (!toy_get_resource(state, depth, SQLITE_DATABASE_TYPE, &resource)) {
        return false;
    }
    *database = resource;
    return true;
}

static bool get_statement(toy_state *state, size_t depth,
                          toy_sqlite_statement **statement) {
    void *resource = NULL;
    if (!toy_get_resource(state, depth, SQLITE_STATEMENT_TYPE, &resource)) {
        return false;
    }
    *statement = resource;
    return true;
}

static bool get_index(toy_state *state, size_t depth, int *index) {
    int64_t value = 0;
    if (!toy_get_int(state, depth, &value) || value < 0 || value > INT_MAX) {
        return false;
    }
    *index = (int)value;
    return true;
}

static bool get_parameter(toy_state *state, size_t depth, int *index) {
    return get_index(state, depth, index) && *index > 0;
}

static bool get_column(toy_state *state, size_t statement_depth, size_t index_depth,
                       toy_sqlite_statement **statement, int *index) {
    if (!get_statement(state, statement_depth, statement) ||
        !get_index(state, index_depth, index)) {
        return false;
    }
    return *index < sqlite3_column_count((*statement)->handle);
}

static void destroy_database(void *resource, void *userdata) {
    (void)userdata;
    toy_sqlite_database *database = resource;
    sqlite3_close_v2(database->handle);
    free(database);
}

static void destroy_statement(void *resource, void *userdata) {
    (void)userdata;
    toy_sqlite_statement *statement = resource;
    sqlite3_finalize(statement->handle);
    free(statement);
}

static toy_status sqlite_open(toy_state *state) {
    if (toy_stack_type(state, 0) != TOY_TYPE_STRING) {
        return toy_fail(state, "sqlite.open expected a path string");
    }

    bool contains_nul = false;
    char *path = copy_c_string(state, 0, &contains_nul);
    if (!path) {
        if (contains_nul) {
            return toy_fail(state, "sqlite.open path contains an embedded NUL byte");
        }
        return toy_fail(state, "sqlite.open could not copy the path");
    }
    if (!toy_pop(state, 1)) {
        free(path);
        return toy_fail(state, "sqlite.open failed to pop its input");
    }

    sqlite3 *handle = NULL;
    int result = sqlite3_open(path, &handle);
    free(path);
    if (result != SQLITE_OK) {
        char message[512];
        snprintf(message, sizeof(message), "sqlite.open failed: %s",
                 handle ? sqlite3_errmsg(handle) : sqlite3_errstr(result));
        if (handle) sqlite3_close(handle);
        return toy_fail(state, message);
    }

    toy_sqlite_database *database = malloc(sizeof(*database));
    if (!database) {
        sqlite3_close(handle);
        return toy_fail(state, "sqlite.open could not allocate its database handle");
    }
    database->handle = handle;
    toy_status status = toy_push_resource(state, SQLITE_DATABASE_TYPE, database,
                                          destroy_database, NULL);
    if (status != TOY_OK) destroy_database(database, NULL);
    return status;
}

static toy_status sqlite_exec(toy_state *state) {
    toy_sqlite_database *database = NULL;
    if (!get_database(state, 1, &database) ||
        toy_stack_type(state, 0) != TOY_TYPE_STRING) {
        return toy_fail(state, "sqlite.exec expected a database and SQL string");
    }

    bool contains_nul = false;
    char *sql = copy_c_string(state, 0, &contains_nul);
    if (!sql) {
        if (contains_nul) {
            return toy_fail(state, "sqlite.exec SQL contains an embedded NUL byte");
        }
        return toy_fail(state, "sqlite.exec could not copy the SQL string");
    }

    char *sqlite_message = NULL;
    int result = sqlite3_exec(database->handle, sql, NULL, NULL, &sqlite_message);
    free(sql);
    char message[512] = {0};
    if (result != SQLITE_OK) {
        snprintf(message, sizeof(message), "sqlite.exec failed: %s",
                 sqlite_message ? sqlite_message : sqlite3_errmsg(database->handle));
    }
    sqlite3_free(sqlite_message);
    if (!toy_pop(state, 2)) {
        return toy_fail(state, "sqlite.exec failed to pop its inputs");
    }
    return result == SQLITE_OK ? TOY_OK : toy_fail(state, message);
}

static toy_status sqlite_prepare(toy_state *state) {
    toy_sqlite_database *database = NULL;
    if (!get_database(state, 1, &database) ||
        toy_stack_type(state, 0) != TOY_TYPE_STRING) {
        return toy_fail(state, "sqlite.prepare expected a database and SQL string");
    }

    bool contains_nul = false;
    char *sql = copy_c_string(state, 0, &contains_nul);
    if (!sql) {
        if (contains_nul) {
            return toy_fail(state,
                            "sqlite.prepare SQL contains an embedded NUL byte");
        }
        return toy_fail(state, "sqlite.prepare could not copy the SQL string");
    }

    sqlite3_stmt *handle = NULL;
    const char *tail = NULL;
    int result = sqlite3_prepare_v2(database->handle, sql, -1, &handle, &tail);
    char message[512] = {0};
    if (result != SQLITE_OK) {
        snprintf(message, sizeof(message), "sqlite.prepare failed: %s",
                 sqlite3_errmsg(database->handle));
    } else if (!handle) {
        snprintf(message, sizeof(message),
                 "sqlite.prepare expected a non-empty SQL statement");
        result = SQLITE_ERROR;
    } else {
        while (*tail == ' ' || *tail == '\t' || *tail == '\r' || *tail == '\n' ||
               *tail == ';') {
            ++tail;
        }
        if (*tail != '\0') {
            snprintf(message, sizeof(message),
                     "sqlite.prepare expected exactly one SQL statement");
            result = SQLITE_ERROR;
        }
    }
    free(sql);
    if (!toy_pop(state, 2)) {
        if (handle) sqlite3_finalize(handle);
        return toy_fail(state, "sqlite.prepare failed to pop its inputs");
    }
    if (result != SQLITE_OK) {
        if (handle) sqlite3_finalize(handle);
        return toy_fail(state, message);
    }

    toy_sqlite_statement *statement = malloc(sizeof(*statement));
    if (!statement) {
        sqlite3_finalize(handle);
        return toy_fail(state,
                        "sqlite.prepare could not allocate its statement handle");
    }
    statement->handle = handle;
    statement->has_row = false;
    toy_status status = toy_push_resource(state, SQLITE_STATEMENT_TYPE, statement,
                                          destroy_statement, NULL);
    if (status != TOY_OK) destroy_statement(statement, NULL);
    return status;
}

static toy_status sqlite_step(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    if (!get_statement(state, 0, &statement)) {
        return toy_fail(state, "sqlite.step expected a statement");
    }

    int result = sqlite3_step(statement->handle);
    bool has_row = result == SQLITE_ROW;
    statement->has_row = has_row;
    char message[512] = {0};
    if (result != SQLITE_ROW && result != SQLITE_DONE) {
        snprintf(message, sizeof(message), "sqlite.step failed: %s",
                 sqlite3_errmsg(sqlite3_db_handle(statement->handle)));
    }
    if (!toy_pop(state, 1)) {
        return toy_fail(state, "sqlite.step failed to pop its input");
    }
    if (result != SQLITE_ROW && result != SQLITE_DONE) {
        return toy_fail(state, message);
    }
    return toy_push_bool(state, has_row);
}

static toy_status sqlite_reset(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    if (!get_statement(state, 0, &statement)) {
        return toy_fail(state, "sqlite.reset expected a statement");
    }
    int result = sqlite3_reset(statement->handle);
    statement->has_row = false;
    char message[512] = {0};
    if (result != SQLITE_OK) {
        snprintf(message, sizeof(message), "sqlite.reset failed: %s",
                 sqlite3_errmsg(sqlite3_db_handle(statement->handle)));
    }
    if (!toy_pop(state, 1)) {
        return toy_fail(state, "sqlite.reset failed to pop its input");
    }
    return result == SQLITE_OK ? TOY_OK : toy_fail(state, message);
}

static toy_status sqlite_clear_bindings(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    if (!get_statement(state, 0, &statement)) {
        return toy_fail(state, "sqlite.clear-bindings expected a statement");
    }
    int result = sqlite3_clear_bindings(statement->handle);
    char message[512] = {0};
    if (result != SQLITE_OK) {
        snprintf(message, sizeof(message), "sqlite.clear-bindings failed: %s",
                 sqlite3_errmsg(sqlite3_db_handle(statement->handle)));
    }
    if (!toy_pop(state, 1)) {
        return toy_fail(state, "sqlite.clear-bindings failed to pop its input");
    }
    return result == SQLITE_OK ? TOY_OK : toy_fail(state, message);
}

static toy_status finish_bind(toy_state *state, toy_sqlite_statement *statement,
                              int result, size_t input_count, const char *word) {
    char message[512] = {0};
    if (result != SQLITE_OK) {
        snprintf(message, sizeof(message), "sqlite.%s failed: %s", word,
                 sqlite3_errmsg(sqlite3_db_handle(statement->handle)));
    }
    if (!toy_pop(state, input_count)) {
        return failf(state, "sqlite.%s failed to pop its inputs", word);
    }
    return result == SQLITE_OK ? TOY_OK : toy_fail(state, message);
}

static toy_status sqlite_bind_int(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    int index = 0;
    int64_t value = 0;
    if (!get_statement(state, 2, &statement) || !get_parameter(state, 1, &index) ||
        !toy_get_int(state, 0, &value)) {
        return toy_fail(
            state, "sqlite.bind-int expected a statement, positive index, and int");
    }
    int result = sqlite3_bind_int64(statement->handle, index, (sqlite3_int64)value);
    return finish_bind(state, statement, result, 3, "bind-int");
}

static toy_status sqlite_bind_float(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    int index = 0;
    double value = 0.0;
    if (!get_statement(state, 2, &statement) || !get_parameter(state, 1, &index) ||
        !toy_get_float(state, 0, &value)) {
        return toy_fail(
            state,
            "sqlite.bind-float expected a statement, positive index, and "
            "float");
    }
    int result = sqlite3_bind_double(statement->handle, index, value);
    return finish_bind(state, statement, result, 3, "bind-float");
}

static toy_status sqlite_bind_text(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    int index = 0;
    const char *data = NULL;
    size_t length = 0;
    if (!get_statement(state, 2, &statement) || !get_parameter(state, 1, &index) ||
        !toy_get_string(state, 0, &data, &length) || length > INT_MAX) {
        return toy_fail(state,
                        "sqlite.bind-text expected a statement, positive index, and "
                        "string no larger than INT_MAX bytes");
    }
    int result = sqlite3_bind_text(statement->handle, index, data, (int)length,
                                   SQLITE_TRANSIENT);
    return finish_bind(state, statement, result, 3, "bind-text");
}

static toy_status sqlite_bind_null(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    int index = 0;
    if (!get_statement(state, 1, &statement) || !get_parameter(state, 0, &index)) {
        return toy_fail(state,
                        "sqlite.bind-null expected a statement and positive index");
    }
    int result = sqlite3_bind_null(statement->handle, index);
    return finish_bind(state, statement, result, 2, "bind-null");
}

static toy_status sqlite_column_count(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    if (!get_statement(state, 0, &statement)) {
        return toy_fail(state, "sqlite.column-count expected a statement");
    }
    int count = sqlite3_column_count(statement->handle);
    if (!toy_pop(state, 1)) {
        return toy_fail(state, "sqlite.column-count failed to pop its input");
    }
    return toy_push_int(state, count);
}

static toy_status sqlite_column_name(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    int index = 0;
    if (!get_column(state, 1, 0, &statement, &index)) {
        return toy_fail(
            state, "sqlite.column-name expected a statement and valid column index");
    }
    const char *name = sqlite3_column_name(statement->handle, index);
    if (!name) {
        return toy_fail(state, "sqlite.column-name could not read the name");
    }
    size_t length = strlen(name);
    char *copy = malloc(length + 1);
    if (!copy) {
        return toy_fail(state, "sqlite.column-name could not copy the name");
    }
    memcpy(copy, name, length + 1);
    if (!toy_pop(state, 2)) {
        free(copy);
        return toy_fail(state, "sqlite.column-name failed to pop its inputs");
    }
    toy_status status = toy_push_string(state, copy, length);
    free(copy);
    return status;
}

static bool valid_row_column(toy_state *state, toy_sqlite_statement **statement,
                             int *index, const char *word) {
    if (!get_column(state, 1, 0, statement, index)) {
        failf(state, "sqlite.%s expected a statement and valid column index", word);
        return false;
    }
    if (!(*statement)->has_row) {
        failf(state, "sqlite.%s requires a current row", word);
        return false;
    }
    return true;
}

static toy_status sqlite_column_null(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    int index = 0;
    if (!valid_row_column(state, &statement, &index, "column-null?")) {
        return TOY_ERROR;
    }
    bool is_null = sqlite3_column_type(statement->handle, index) == SQLITE_NULL;
    if (!toy_pop(state, 2)) {
        return toy_fail(state, "sqlite.column-null? failed to pop its inputs");
    }
    return toy_push_bool(state, is_null);
}

static toy_status sqlite_column_int(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    int index = 0;
    if (!valid_row_column(state, &statement, &index, "column-int")) {
        return TOY_ERROR;
    }
    if (sqlite3_column_type(statement->handle, index) == SQLITE_NULL) {
        return toy_fail(state,
                        "sqlite.column-int column is NULL; use column-null? first");
    }
    sqlite3_int64 value = sqlite3_column_int64(statement->handle, index);
    if (!toy_pop(state, 2)) {
        return toy_fail(state, "sqlite.column-int failed to pop its inputs");
    }
    return toy_push_int(state, (int64_t)value);
}

static toy_status sqlite_column_float(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    int index = 0;
    if (!valid_row_column(state, &statement, &index, "column-float")) {
        return TOY_ERROR;
    }
    if (sqlite3_column_type(statement->handle, index) == SQLITE_NULL) {
        return toy_fail(
            state, "sqlite.column-float column is NULL; use column-null? first");
    }
    double value = sqlite3_column_double(statement->handle, index);
    if (!toy_pop(state, 2)) {
        return toy_fail(state, "sqlite.column-float failed to pop its inputs");
    }
    return toy_push_float(state, value);
}

static toy_status sqlite_column_text(toy_state *state) {
    toy_sqlite_statement *statement = NULL;
    int index = 0;
    if (!valid_row_column(state, &statement, &index, "column-text")) {
        return TOY_ERROR;
    }
    if (sqlite3_column_type(statement->handle, index) == SQLITE_NULL) {
        return toy_fail(state,
                        "sqlite.column-text column is NULL; use column-null? first");
    }
    const unsigned char *data = sqlite3_column_text(statement->handle, index);
    int byte_count = sqlite3_column_bytes(statement->handle, index);
    if (!data && byte_count != 0) {
        return toy_fail(state, "sqlite.column-text could not read the column");
    }
    size_t length = (size_t)byte_count;
    char *copy = malloc(length > 0 ? length : 1);
    if (!copy) {
        return toy_fail(state, "sqlite.column-text could not copy the column");
    }
    if (length > 0) memcpy(copy, data, length);
    if (!toy_pop(state, 2)) {
        free(copy);
        return toy_fail(state, "sqlite.column-text failed to pop its inputs");
    }
    toy_status status = toy_push_string(state, copy, length);
    free(copy);
    return status;
}

static toy_status sqlite_changes(toy_state *state) {
    toy_sqlite_database *database = NULL;
    if (!get_database(state, 0, &database)) {
        return toy_fail(state, "sqlite.changes expected a database");
    }
    int changes = sqlite3_changes(database->handle);
    if (!toy_pop(state, 1)) {
        return toy_fail(state, "sqlite.changes failed to pop its input");
    }
    return toy_push_int(state, changes);
}

static toy_status sqlite_last_insert_rowid(toy_state *state) {
    toy_sqlite_database *database = NULL;
    if (!get_database(state, 0, &database)) {
        return toy_fail(state, "sqlite.last-insert-rowid expected a database");
    }
    sqlite3_int64 rowid = sqlite3_last_insert_rowid(database->handle);
    if (!toy_pop(state, 1)) {
        return toy_fail(state, "sqlite.last-insert-rowid failed to pop its input");
    }
    return toy_push_int(state, (int64_t)rowid);
}

static const toy_native_word sqlite_words[] = {
    {"open", sqlite_open},
    {"exec", sqlite_exec},
    {"prepare", sqlite_prepare},
    {"step", sqlite_step},
    {"reset", sqlite_reset},
    {"clear-bindings", sqlite_clear_bindings},
    {"bind-int", sqlite_bind_int},
    {"bind-float", sqlite_bind_float},
    {"bind-text", sqlite_bind_text},
    {"bind-null", sqlite_bind_null},
    {"column-count", sqlite_column_count},
    {"column-name", sqlite_column_name},
    {"column-null?", sqlite_column_null},
    {"column-int", sqlite_column_int},
    {"column-float", sqlite_column_float},
    {"column-text", sqlite_column_text},
    {"changes", sqlite_changes},
    {"last-insert-rowid", sqlite_last_insert_rowid},
};

static const toy_module_export sqlite_module = {
    sizeof(toy_module_export),
    "sqlite",
    sqlite_words,
    sizeof(sqlite_words) / sizeof(sqlite_words[0]),
};

TOY_MODULE_EXPORT const toy_module_export *toy_module_init(
    uint32_t abi_version, const toy_module_api *api) {
    if (!toy_module_bind(abi_version, api)) return NULL;
    return &sqlite_module;
}
