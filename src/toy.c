#include "toy.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tf_alloc.h"
#include "tf_exec.h"
#include "tf_lib.h"
#include "tf_obj.h"
#include "tf_runtime.h"

static bool state_is_idle(toy_state *state) {
    if (!state) return false;
    if (state->call_stack_len == 0) return true;
    tf_ctx_set_error(state, "the Toy state is already executing");
    return false;
}

static toy_status api_errorf(toy_state *state, const char *format, ...) {
    if (!state) return TOY_ERROR;

    va_list args;
    va_start(args, format);
    va_list count_args;
    va_copy(count_args, args);
    int length = vsnprintf(NULL, 0, format, count_args);
    va_end(count_args);
    if (length < 0) {
        va_end(args);
        tf_ctx_set_error(state, "native module registration failed");
        return TOY_ERROR;
    }

    char *message = tf_xmalloc((size_t)length + 1);
    vsnprintf(message, (size_t)length + 1, format, args);
    va_end(args);
    tf_ctx_set_error(state, message);
    free(message);
    return TOY_ERROR;
}

static void free_native_names(char **names, size_t count) {
    if (!names) return;
    for (size_t i = 0; i < count; i++) free(names[i]);
    free(names);
}

static const tf_module *qualified_word_owner(toy_state *state,
                                             const char *name,
                                             size_t name_len) {
    for (size_t i = 1; i < state->modules.len; i++) {
        const tf_module *module = &state->modules.entries[i];
        if (name_len <= module->name_len + 1) continue;
        if (memcmp(name, module->name, module->name_len) == 0 &&
            name[module->name_len] == '.') {
            return module;
        }
    }
    return NULL;
}

toy_state *toy_state_new(const toy_state_config *config) {
    toy_state *state = tf_ctx_new(0, NULL);
    if (!state || !config) return state;
    tf_ctx_set_output(state, config->output, config->output_userdata);
    tf_ctx_set_diagnostic(state, config->diagnostic,
                          config->diagnostic_userdata);
    return state;
}

void toy_state_free(toy_state *state) {
    if (!state) return;
    tf_ctx_free(state);
    tf_control_state_cache_clear();
    tf_obj_cache_clear();
}

toy_status toy_eval(toy_state *state, const char *source_name,
                    const char *source) {
    if (!state || !source) return TOY_ERROR;
    if (!state_is_idle(state)) return TOY_ERROR;
    return tf_eval_source(state, source_name ? source_name : "<eval>", source);
}

toy_status toy_call(toy_state *state, const char *word) {
    if (!state || !word || word[0] == '\0') return TOY_ERROR;
    if (!state_is_idle(state)) return TOY_ERROR;

    tf_obj *program = tf_obj_new_vector();
    tf_vector_push(program, tf_obj_new_call(word, strlen(word)));
    tf_ret result = tf_vm_exec(state, program);
    tf_obj_release(program);
    return result;
}

toy_status toy_register_word(toy_state *state, const char *name,
                             toy_native_fn function) {
    if (!state || !name || name[0] == '\0' || !function) return TOY_ERROR;
    if (!state_is_idle(state)) return TOY_ERROR;
    const tf_module *owner = qualified_word_owner(state, name, strlen(name));
    if (owner) {
        return api_errorf(state, "word '%s' belongs to registered module '%s'",
                          name, owner->name);
    }
    tf_dict_set_native_copy(state, name, function);
    return TOY_OK;
}

toy_status tf_register_module(toy_state *state,
                              const toy_native_module *module) {
    if (!state) return TOY_ERROR;
    if (!module || !module->name || module->name[0] == '\0') {
        return api_errorf(state, "native module descriptor is invalid");
    }

    size_t module_name_len = strlen(module->name);
    if (!tf_module_name_valid(module->name, module_name_len)) {
        return api_errorf(state, "invalid native module name '%s'",
                          module->name);
    }
    if (!module->words || module->word_count == 0) {
        return api_errorf(state, "native module '%s' has no words",
                          module->name);
    }
    if (tf_module_find(state, module->name, module_name_len) != (size_t)-1) {
        return api_errorf(state, "module '%s' is already registered",
                          module->name);
    }
    tf_word *namespace_conflict =
        tf_dict_namespace_conflict(state, module->name, module_name_len);
    if (namespace_conflict) {
        return api_errorf(state,
                          "native module '%s' conflicts with existing word '%s'",
                          module->name, namespace_conflict->name);
    }

    char **qualified_names =
        tf_xcalloc(module->word_count, sizeof(char *));
    for (size_t i = 0; i < module->word_count; i++) {
        const toy_native_word *word = &module->words[i];
        if (!word->name || !word->callback) {
            free_native_names(qualified_names, module->word_count);
            return api_errorf(state,
                              "native module '%s' has an invalid word descriptor",
                              module->name);
        }

        size_t word_name_len = strlen(word->name);
        if (!tf_module_word_name_valid(word->name, word_name_len)) {
            free_native_names(qualified_names, module->word_count);
            return api_errorf(state, "invalid native word name '%s' in module '%s'",
                              word->name, module->name);
        }
        if (module_name_len > SIZE_MAX - 2 ||
            word_name_len > SIZE_MAX - module_name_len - 2) {
            free_native_names(qualified_names, module->word_count);
            return api_errorf(state, "native module word name is too long");
        }

        size_t qualified_len = module_name_len + 1 + word_name_len;
        char *qualified = tf_xmalloc(qualified_len + 1);
        memcpy(qualified, module->name, module_name_len);
        qualified[module_name_len] = '.';
        memcpy(qualified + module_name_len + 1, word->name,
               word_name_len + 1);
        qualified_names[i] = qualified;

        for (size_t j = 0; j < i; j++) {
            if (strcmp(qualified_names[j], qualified) == 0) {
                toy_status status = api_errorf(
                    state, "native module word '%s' is duplicated", qualified);
                free_native_names(qualified_names, module->word_count);
                return status;
            }
        }
        if (tf_dict_lookup_name(state, qualified, qualified_len)) {
            toy_status status =
                api_errorf(state, "word '%s' is already defined", qualified);
            free_native_names(qualified_names, module->word_count);
            return status;
        }
    }

    size_t module_index =
        tf_module_add_native(state, module->name, module_name_len);
    for (size_t i = 0; i < module->word_count; i++) {
        tf_dict_add_native_scoped(state, qualified_names[i],
                                  strlen(qualified_names[i]), module_index,
                                  module->words[i].callback);
    }
    free_native_names(qualified_names, module->word_count);
    return TOY_OK;
}

toy_status toy_register_module(toy_state *state,
                               const toy_native_module *module) {
    if (!state || !state_is_idle(state)) return TOY_ERROR;
    return tf_register_module(state, module);
}

size_t toy_stack_size(toy_state *state) {
    return state ? tf_stack_len(state) : 0;
}

toy_type toy_stack_type(toy_state *state, size_t depth) {
    tf_obj *value = state ? tf_stack_peek(state, depth) : NULL;
    if (!value) return TOY_TYPE_MISSING;

    switch (value->type) {
    case TF_OBJ_TYPE_BOOL:
        return TOY_TYPE_BOOL;
    case TF_OBJ_TYPE_INT:
        return TOY_TYPE_INT;
    case TF_OBJ_TYPE_FLOAT:
        return TOY_TYPE_FLOAT;
    case TF_OBJ_TYPE_STR:
        return TOY_TYPE_STRING;
    case TF_OBJ_TYPE_SYMBOL:
        return TOY_TYPE_SYMBOL;
    case TF_OBJ_TYPE_CALL:
        return TOY_TYPE_CALL;
    case TF_OBJ_TYPE_VECTOR:
        return TOY_TYPE_VECTOR;
    case TF_OBJ_TYPE_LIST:
        return TOY_TYPE_LIST;
    case TF_OBJ_TYPE_MAP:
        return TOY_TYPE_MAP;
    case TF_OBJ_TYPE_SET:
        return TOY_TYPE_SET;
    case TF_OBJ_TYPE_DEQUE:
        return TOY_TYPE_DEQUE;
    case TF_OBJ_TYPE_PQUEUE:
        return TOY_TYPE_PQUEUE;
    case TF_OBJ_TYPE_RESOURCE:
        return TOY_TYPE_RESOURCE;
    case TF_OBJ_TYPE_VARLIST:
    case TF_OBJ_TYPE_VARFETCH:
        return TOY_TYPE_INTERNAL;
    }
    return TOY_TYPE_INTERNAL;
}

bool toy_get_bool(toy_state *state, size_t depth, bool *value) {
    tf_obj *object = state ? tf_stack_peek(state, depth) : NULL;
    if (!object || object->type != TF_OBJ_TYPE_BOOL || !value) return false;
    *value = object->b;
    return true;
}

bool toy_get_int(toy_state *state, size_t depth, int64_t *value) {
    tf_obj *object = state ? tf_stack_peek(state, depth) : NULL;
    if (!object || object->type != TF_OBJ_TYPE_INT || !value) return false;
    *value = object->i;
    return true;
}

bool toy_get_float(toy_state *state, size_t depth, double *value) {
    tf_obj *object = state ? tf_stack_peek(state, depth) : NULL;
    if (!object || object->type != TF_OBJ_TYPE_FLOAT || !value) return false;
    *value = object->f;
    return true;
}

bool toy_get_string(toy_state *state, size_t depth, const char **data,
                    size_t *length) {
    tf_obj *object = state ? tf_stack_peek(state, depth) : NULL;
    if (!object || object->type != TF_OBJ_TYPE_STR || !data || !length) {
        return false;
    }
    *data = object->str.ptr;
    *length = object->str.len;
    return true;
}

bool toy_get_resource(toy_state *state, size_t depth,
                      const char *expected_type, void **resource) {
    tf_obj *object = state ? tf_stack_peek(state, depth) : NULL;
    if (!object || object->type != TF_OBJ_TYPE_RESOURCE || !expected_type ||
        expected_type[0] == '\0' || !resource) {
        return false;
    }
    size_t expected_len = strlen(expected_type);
    if (object->resource.type_len != expected_len ||
        memcmp(object->resource.type_name, expected_type, expected_len) != 0) {
        return false;
    }
    *resource = object->resource.pointer;
    return true;
}

bool toy_get_resource_type(toy_state *state, size_t depth,
                           const char **type_name) {
    tf_obj *object = state ? tf_stack_peek(state, depth) : NULL;
    if (!object || object->type != TF_OBJ_TYPE_RESOURCE || !type_name) {
        return false;
    }
    *type_name = object->resource.type_name;
    return true;
}

bool toy_pop(toy_state *state, size_t count) {
    if (!state || tf_stack_len(state) < count) return false;
    for (size_t i = 0; i < count; i++) {
        tf_obj_release(tf_stack_pop(state));
    }
    return true;
}

toy_status toy_push_bool(toy_state *state, bool value) {
    if (!state) return TOY_ERROR;
    tf_stack_push(state, tf_obj_new_bool(value));
    return TOY_OK;
}

toy_status toy_push_int(toy_state *state, int64_t value) {
    if (!state) return TOY_ERROR;
    tf_stack_push(state, tf_obj_new_int(value));
    return TOY_OK;
}

toy_status toy_push_float(toy_state *state, double value) {
    if (!state) return TOY_ERROR;
    tf_stack_push(state, tf_obj_new_float(value));
    return TOY_OK;
}

toy_status toy_push_string(toy_state *state, const char *data, size_t length) {
    if (!state || (!data && length > 0)) return TOY_ERROR;
    tf_stack_push(state, tf_obj_new_string(data ? data : "", length));
    return TOY_OK;
}

toy_status toy_push_resource(toy_state *state, const char *type_name,
                             void *resource,
                             toy_resource_destructor destructor,
                             void *destructor_userdata) {
    if (!state) return TOY_ERROR;
    if (!type_name || type_name[0] == '\0') {
        return api_errorf(state, "resource type name must not be empty");
    }
    size_t type_len = strlen(type_name);
    if (!tf_module_name_valid(type_name, type_len)) {
        return api_errorf(state, "resource type name '%s' is invalid",
                          type_name);
    }
    if (!resource) {
        return api_errorf(state, "resource pointer must not be NULL");
    }
    tf_stack_push(state, tf_obj_new_resource(
                             type_name, type_len, resource, destructor,
                             destructor_userdata));
    return TOY_OK;
}

const char *toy_get_error(toy_state *state) {
    return tf_ctx_last_error(state);
}

void toy_clear_error(toy_state *state) {
    tf_ctx_clear_error(state);
}

toy_status toy_fail(toy_state *state, const char *message) {
    if (!state) return TOY_ERROR;
    tf_ctx_runtime_errorf(state, "%s\n", message ? message : "native error");
    return TOY_ERROR;
}

void toy_interrupt(toy_state *state) {
    tf_ctx_interrupt(state);
}
