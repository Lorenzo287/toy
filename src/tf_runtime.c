#include "tf_runtime.h"

#include "tf_lexer.h"

tf_obj *tf_parse_source(const char *filename, const char *source) {
    if (!source) return NULL;
    return tf_lexer_parse(filename ? filename : "<eval>", source);
}

tf_ret tf_eval_source(tf_ctx *ctx, const char *filename, const char *source) {
    if (!ctx || !source) return TF_ERR;

    tf_ctx_clear_error(ctx);

    tf_obj *program = tf_parse_source(filename, source);
    if (!program) {
        tf_ctx_set_error(ctx, "source parsing failed");
        return TF_ERR;
    }

    tf_ret result = tf_vm_exec(ctx, program);
    tf_obj_release(program);
    return result;
}
