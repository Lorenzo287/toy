#include "toy.h"

#include <string.h>

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

toy_state *toy_state_new(void) {
    return tf_ctx_new(0, NULL);
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

toy_status toy_register_native(toy_state *state, const char *name,
                               toy_native_fn function) {
    if (!state || !name || name[0] == '\0' || !function) return TOY_ERROR;
    if (!state_is_idle(state)) return TOY_ERROR;
    tf_dict_set_native(state, name, function);
    return TOY_OK;
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

const char *toy_last_error(toy_state *state) {
    return tf_ctx_last_error(state);
}

void toy_clear_error(toy_state *state) {
    tf_ctx_clear_error(state);
}

toy_status toy_error(toy_state *state, const char *message) {
    if (!state) return TOY_ERROR;
    tf_ctx_runtime_errorf(state, "%s\n", message ? message : "native error");
    return TOY_ERROR;
}

void toy_interrupt(toy_state *state) {
    tf_ctx_interrupt(state);
}
