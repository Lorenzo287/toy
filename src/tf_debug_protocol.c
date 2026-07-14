#include "tf_debug_protocol.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tf_alloc.h"
#include "tf_debug_control.h"
#include "tf_obj.h"

struct tf_debug_protocol {
    FILE *output;
    char *program_path;
    tf_debug_control control;
};

static void write_json_string(FILE *output, const char *text);
static void write_source_value(FILE *output, tf_obj *value);
static tf_debug_action debug_protocol_hook(tf_ctx *ctx,
                                            const tf_debug_event *event,
                                            void *userdata);

FILE *tf_debug_protocol_open_output(void) {
    return stdout;
}

tf_debug_protocol *tf_debug_protocol_new(FILE *output,
                                         const char *program_path) {
    if (!output || !program_path) return NULL;
    tf_debug_protocol *protocol = tf_xcalloc(1, sizeof(*protocol));
    protocol->output = output;
    protocol->program_path = tf_xstrdup(program_path);
    tf_debug_control_init(&protocol->control, true);
    return protocol;
}

void tf_debug_protocol_install(tf_ctx *ctx, tf_debug_protocol *protocol) {
    tf_debug_set_hook(ctx, protocol ? debug_protocol_hook : NULL, protocol);
}

void tf_debug_protocol_finish(tf_debug_protocol *protocol, tf_ret result) {
    if (!protocol) return;
    fflush(stdout);
    fflush(stderr);
    fputc(0x1e, protocol->output);
    fprintf(protocol->output,
            "{\"event\":\"terminated\",\"exitCode\":%d}\n",
            (int)result);
    fflush(protocol->output);
}

void tf_debug_protocol_free(tf_debug_protocol *protocol) {
    if (!protocol) return;
    tf_debug_control_dispose(&protocol->control);
    free(protocol->program_path);
    free(protocol);
}

static void write_json_char(FILE *output, unsigned char c) {
    switch (c) {
    case '"':
        fputs("\\\"", output);
        break;
    case '\\':
        fputs("\\\\", output);
        break;
    case '\b':
        fputs("\\b", output);
        break;
    case '\f':
        fputs("\\f", output);
        break;
    case '\n':
        fputs("\\n", output);
        break;
    case '\r':
        fputs("\\r", output);
        break;
    case '\t':
        fputs("\\t", output);
        break;
    default:
        if (c < 0x20) {
            fprintf(output, "\\u%04x", c);
        } else {
            fputc(c, output);
        }
        break;
    }
}

static void write_json_mem(FILE *output, const char *text, size_t len) {
    for (size_t i = 0; i < len; i++) {
        write_json_char(output, (unsigned char)text[i]);
    }
}

static void write_json_string(FILE *output, const char *text) {
    fputc('"', output);
    if (text) write_json_mem(output, text, strlen(text));
    fputc('"', output);
}

static void write_source_char(FILE *output, char c) {
    write_json_char(output, (unsigned char)c);
}

static void write_source_cstr(FILE *output, const char *text) {
    write_json_mem(output, text, strlen(text));
}

static void write_source_escaped_string(FILE *output, const char *text,
                                        size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        switch (c) {
        case '\\':
            write_source_cstr(output, "\\\\");
            break;
        case '"':
            write_source_cstr(output, "\\\"");
            break;
        case '\n':
            write_source_cstr(output, "\\n");
            break;
        case '\r':
            write_source_cstr(output, "\\r");
            break;
        case '\t':
            write_source_cstr(output, "\\t");
            break;
        default:
            if (c < 0x20 || c >= 0x7f) {
                char escaped[5];
                snprintf(escaped, sizeof escaped, "\\x%02x", c);
                write_source_cstr(output, escaped);
            } else {
                write_source_char(output, (char)c);
            }
            break;
        }
    }
}

static void write_source_value(FILE *output, tf_obj *value) {
    char number[64];
    switch (value->type) {
    case TF_OBJ_TYPE_INT:
        snprintf(number, sizeof number, "%" PRId64, value->i);
        write_source_cstr(output, number);
        break;
    case TF_OBJ_TYPE_FLOAT:
        tf_format_double(number, sizeof number, value->f);
        write_source_cstr(output, number);
        if (isfinite(value->f) && !strpbrk(number, ".eE")) {
            write_source_cstr(output, ".0");
        }
        break;
    case TF_OBJ_TYPE_BOOL:
        write_source_cstr(output, value->b ? "true" : "false");
        break;
    case TF_OBJ_TYPE_STR:
        write_source_char(output, '"');
        write_source_escaped_string(output, value->str.ptr, value->str.len);
        write_source_char(output, '"');
        break;
    case TF_OBJ_TYPE_SYMBOL:
        write_source_char(output, '\'');
        write_json_mem(output, value->str.ptr, value->str.len);
        break;
    case TF_OBJ_TYPE_CALL:
        write_json_mem(output, value->str.ptr, value->str.len);
        break;
    case TF_OBJ_TYPE_VARFETCH:
        write_source_char(output, '$');
        write_json_mem(output, value->str.ptr, value->str.len);
        break;
    case TF_OBJ_TYPE_VARLIST:
        write_source_char(output, '|');
        for (size_t i = 0; i < value->vector.len; i++) {
            if (i > 0) write_source_char(output, ' ');
            write_json_mem(output, value->vector.elem[i]->str.ptr,
                           value->vector.elem[i]->str.len);
        }
        write_source_char(output, '|');
        break;
    case TF_OBJ_TYPE_VECTOR:
        write_source_char(output, '[');
        for (size_t i = 0; i < value->vector.len; i++) {
            if (i > 0) write_source_char(output, ' ');
            write_source_value(output, value->vector.elem[i]);
        }
        write_source_char(output, ']');
        break;
    case TF_OBJ_TYPE_LIST: {
        write_source_char(output, '(');
        tf_list_node *node = value->list.head;
        while (node) {
            write_source_value(output, node->value);
            node = node->next;
            if (node) write_source_char(output, ' ');
        }
        write_source_char(output, ')');
        break;
    }
    case TF_OBJ_TYPE_MAP:
        write_source_char(output, '{');
        for (size_t i = 0; i < value->map.len; i++) {
            if (i > 0) write_source_char(output, ' ');
            write_source_value(output, value->map.entries[i].key);
            write_source_char(output, ' ');
            write_source_value(output, value->map.entries[i].value);
        }
        write_source_char(output, '}');
        break;
    case TF_OBJ_TYPE_SET:
        write_source_cstr(output, "#{");
        for (size_t i = 0; i < value->set.len; i++) {
            if (i > 0) write_source_char(output, ' ');
            write_source_value(output, value->set.entries[i].item);
        }
        write_source_char(output, '}');
        break;
    case TF_OBJ_TYPE_DEQUE:
        write_source_cstr(output, "deque[");
        for (size_t i = 0; i < value->deque.len; i++) {
            if (i > 0) write_source_char(output, ' ');
            write_source_value(output, tf_deque_get(value, i));
        }
        write_source_char(output, ']');
        break;
    case TF_OBJ_TYPE_PQUEUE: {
        write_source_cstr(output, "pqueue[");
        tf_obj *copy = tf_pqueue_clone(value);
        for (size_t i = 0; copy->pqueue.len > 0; i++) {
            tf_obj *priority = NULL;
            tf_obj *item = NULL;
            tf_pqueue_pop(copy, &priority, &item);
            if (i > 0) write_source_char(output, ' ');
            write_source_char(output, '[');
            write_source_value(output, priority);
            write_source_char(output, ' ');
            write_source_value(output, item);
            write_source_char(output, ']');
            tf_obj_release(priority);
            tf_obj_release(item);
        }
        tf_obj_release(copy);
        write_source_char(output, ']');
        break;
    }
    case TF_OBJ_TYPE_RESOURCE:
        write_source_cstr(output, "<resource:");
        write_json_mem(output, value->resource.type_name,
                       value->resource.type_len);
        write_source_char(output, '>');
        break;
    }
}

static void write_span(FILE *output, tf_source_span span) {
    const char *source = span.source ? tf_source_file_name(span.source) : NULL;
    fputs("\"source\":", output);
    write_json_string(output, source ? source : "<unknown>");
    fprintf(output, ",\"line\":%zu,\"column\":%zu", (size_t)span.line,
            (size_t)span.col);
}

static void write_debug_value(FILE *output, const char *name, tf_obj *value) {
    fputs("{\"name\":", output);
    write_json_string(output, name);
    fputs(",\"type\":", output);
    write_json_string(output, tf_obj_type_name(value));
    fputs(",\"value\":\"", output);
    write_source_value(output, value);
    fputs("\"}", output);
}

static void write_stopped_event(tf_ctx *ctx, const tf_debug_event *event,
                                tf_debug_protocol *protocol,
                                const char *reason) {
    fflush(stdout);
    fflush(stderr);
    FILE *output = protocol->output;
    fputc(0x1e, output);
    fputs("{\"event\":\"stopped\",\"reason\":", output);
    write_json_string(output, reason);
    fputc(',', output);
    write_span(output, event->span);

    fputs(",\"frames\":[", output);
    size_t frame_count = tf_debug_frame_count(ctx);
    for (size_t depth = 0; depth < frame_count; depth++) {
        tf_debug_frame_info frame;
        if (!tf_debug_get_frame(ctx, depth, &frame)) continue;
        if (depth > 0) fputc(',', output);
        fprintf(output, "{\"id\":%zu,\"name\":", depth);
        write_json_string(output,
                          frame.kind == TF_FRAME_NATIVE
                              ? "<native continuation>"
                              : (frame.word_name ? frame.word_name : "<program>"));
        fputc(',', output);
        write_span(output, depth == 0 ? event->span : frame.location);
        fprintf(output, ",\"pc\":%zu,\"length\":%zu,\"captures\":[",
                frame.pc, frame.program_len);
        size_t capture_count = tf_debug_capture_count(ctx, depth);
        for (size_t i = 0; i < capture_count; i++) {
            tf_debug_capture_info capture;
            if (!tf_debug_get_capture(ctx, depth, i, &capture)) continue;
            if (i > 0) fputc(',', output);
            write_debug_value(output, capture.name, capture.value);
        }
        fputs("]}", output);
    }
    fputc(']', output);

    fputs(",\"stack\":[", output);
    size_t stack_count = tf_stack_len(ctx);
    for (size_t i = 0; i < stack_count; i++) {
        tf_obj *value = tf_stack_peek(ctx, stack_count - 1 - i);
        if (i > 0) fputc(',', output);
        char name[32];
        snprintf(name, sizeof name, "[%zu]", i);
        write_debug_value(output, name, value);
    }
    fputs("]}\n", output);
    fflush(output);
}

static void write_protocol_error(tf_debug_protocol *protocol,
                                 const char *message) {
    fputc(0x1e, protocol->output);
    fputs("{\"event\":\"error\",\"message\":", protocol->output);
    write_json_string(protocol->output, message);
    fputs("}\n", protocol->output);
    fflush(protocol->output);
}

static tf_debug_action wait_for_command(tf_debug_protocol *protocol,
                                        const tf_debug_event *event) {
    char command[256];
    while (fgets(command, sizeof command, stdin)) {
        char *end = command + strlen(command);
        while (end > command && (end[-1] == '\r' || end[-1] == '\n' ||
                                 end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }
        if (strcmp(command, "step") == 0) {
            tf_debug_control_resume(&protocol->control, TF_DEBUG_RUN_STEP_IN,
                                    event->frame_depth);
            return TF_DEBUG_STEP;
        }
        if (strcmp(command, "next") == 0) {
            tf_debug_control_resume(&protocol->control, TF_DEBUG_RUN_STEP_OVER,
                                    event->frame_depth);
            return TF_DEBUG_STEP;
        }
        if (strcmp(command, "step-out") == 0) {
            tf_debug_control_resume(&protocol->control, TF_DEBUG_RUN_STEP_OUT,
                                    event->frame_depth);
            return TF_DEBUG_STEP;
        }
        if (strcmp(command, "continue") == 0) {
            tf_debug_control_resume(&protocol->control, TF_DEBUG_RUN_CONTINUE,
                                    event->frame_depth);
            return TF_DEBUG_STEP;
        }
        if (strcmp(command, "abort") == 0) return TF_DEBUG_ABORT;
        if (strcmp(command, "clear-breakpoints") == 0) {
            tf_debug_control_clear_breakpoints(&protocol->control);
            continue;
        }
        if (strncmp(command, "break ", 6) == 0) {
            errno = 0;
            char *number_end = NULL;
            unsigned long long line = strtoull(command + 6, &number_end, 10);
            if (errno == 0 && number_end != command + 6 &&
                *number_end == '\0' && line > 0 && line <= SIZE_MAX) {
                tf_debug_control_add_line_breakpoint(
                    &protocol->control, protocol->program_path, (size_t)line,
                    event);
            } else {
                write_protocol_error(protocol, "invalid breakpoint line");
            }
            continue;
        }
        write_protocol_error(protocol, "unknown debug protocol command");
    }
    return TF_DEBUG_ABORT;
}

static tf_debug_action debug_protocol_hook(tf_ctx *ctx,
                                            const tf_debug_event *event,
                                            void *userdata) {
    tf_debug_protocol *protocol = userdata;
    tf_debug_frame_info frame;
    const char *word_name =
        tf_debug_get_frame(ctx, 0, &frame) ? frame.word_name : NULL;
    tf_debug_stop_reason stop = tf_debug_control_on_event(
        &protocol->control, event, word_name);
    if (stop == TF_DEBUG_STOP_NONE) return TF_DEBUG_STEP;

    const char *reason = stop == TF_DEBUG_STOP_ENTRY         ? "entry"
                         : stop == TF_DEBUG_STOP_BREAKPOINT ? "breakpoint"
                                                            : "step";
    write_stopped_event(ctx, event, protocol, reason);
    return wait_for_command(protocol, event);
}
