#include "tf_debug_control.h"

#include <stdlib.h>
#include <string.h>

#include "tf_alloc.h"

static tf_debug_breakpoint *append_breakpoint(tf_debug_control *control) {
    if (control->breakpoint_count == control->breakpoint_capacity) {
        size_t capacity = control->breakpoint_capacity
                              ? control->breakpoint_capacity * 2
                              : 8;
        control->breakpoints = tf_xrealloc(
            control->breakpoints, capacity * sizeof(*control->breakpoints));
        control->breakpoint_capacity = capacity;
    }
    tf_debug_breakpoint *breakpoint =
        &control->breakpoints[control->breakpoint_count++];
    memset(breakpoint, 0, sizeof(*breakpoint));
    breakpoint->id = control->next_breakpoint_id++;
    return breakpoint;
}

static void release_breakpoint(tf_debug_breakpoint *breakpoint) {
    free(breakpoint->source_name);
    free(breakpoint->word_name);
}

void tf_debug_control_init(tf_debug_control *control, bool stop_on_entry) {
    memset(control, 0, sizeof(*control));
    control->mode = TF_DEBUG_RUN_PAUSED;
    control->first_stop = stop_on_entry;
    control->next_breakpoint_id = 1;
}

void tf_debug_control_dispose(tf_debug_control *control) {
    tf_debug_control_clear_breakpoints(control);
    free(control->breakpoints);
    memset(control, 0, sizeof(*control));
}

void tf_debug_control_pause(tf_debug_control *control) {
    control->mode = TF_DEBUG_RUN_PAUSED;
    control->last_breakpoint_id = 0;
}

void tf_debug_control_resume(tf_debug_control *control,
                             tf_debug_run_mode mode, size_t frame_depth) {
    control->mode = mode;
    control->resume_depth = frame_depth;
    control->last_breakpoint_id = 0;
}

static bool line_breakpoint_matches(tf_debug_breakpoint *breakpoint,
                                    const tf_debug_event *event) {
    if (!event->span.source || !breakpoint->source_name) return false;
    if (strcmp(tf_source_file_name(event->span.source),
               breakpoint->source_name) != 0 ||
        event->span.line != breakpoint->line) {
        return false;
    }
    if (!breakpoint->resolved) {
        breakpoint->offset = event->span.offset;
        breakpoint->resolved = true;
    }
    return event->span.offset == breakpoint->offset;
}

static bool breakpoint_matches(tf_debug_breakpoint *breakpoint,
                               const tf_debug_event *event,
                               const char *word_name) {
    if (breakpoint->kind == TF_DEBUG_BREAK_LINE) {
        return line_breakpoint_matches(breakpoint, event);
    }
    return event->pc == 0 && word_name && breakpoint->word_name &&
           strcmp(word_name, breakpoint->word_name) == 0;
}

static bool any_breakpoint_matches(tf_debug_control *control,
                                   const tf_debug_event *event,
                                   const char *word_name) {
    for (size_t i = 0; i < control->breakpoint_count; i++) {
        tf_debug_breakpoint *breakpoint = &control->breakpoints[i];
        if (breakpoint_matches(breakpoint, event, word_name)) {
            control->last_breakpoint_id = breakpoint->id;
            return true;
        }
    }
    return false;
}

tf_debug_stop_reason tf_debug_control_on_event(
    tf_debug_control *control, const tf_debug_event *event,
    const char *word_name) {
    tf_debug_stop_reason reason = TF_DEBUG_STOP_STEP;

    if (control->first_stop) {
        control->first_stop = false;
        reason = TF_DEBUG_STOP_ENTRY;
    } else if (control->mode == TF_DEBUG_RUN_CONTINUE) {
        if (!any_breakpoint_matches(control, event, word_name)) {
            return TF_DEBUG_STOP_NONE;
        }
        reason = TF_DEBUG_STOP_BREAKPOINT;
    } else if (control->mode == TF_DEBUG_RUN_STEP_OVER) {
        if (event->frame_depth > control->resume_depth) {
            return TF_DEBUG_STOP_NONE;
        }
    } else if (control->mode == TF_DEBUG_RUN_STEP_OUT) {
        if (event->frame_depth >= control->resume_depth) {
            return TF_DEBUG_STOP_NONE;
        }
    }

    control->mode = TF_DEBUG_RUN_PAUSED;
    return reason;
}

size_t tf_debug_control_add_line_breakpoint(tf_debug_control *control,
                                            const char *source_name,
                                            size_t line,
                                            const tf_debug_event *current_event) {
    if (!source_name || source_name[0] == '\0' || line == 0) return 0;
    tf_debug_breakpoint *breakpoint = append_breakpoint(control);
    breakpoint->kind = TF_DEBUG_BREAK_LINE;
    breakpoint->source_name = tf_xstrdup(source_name);
    breakpoint->line = line;
    if (current_event && current_event->span.source &&
        current_event->span.line == line &&
        strcmp(tf_source_file_name(current_event->span.source), source_name) ==
            0) {
        breakpoint->offset = current_event->span.offset;
        breakpoint->resolved = true;
    }
    return breakpoint->id;
}

size_t tf_debug_control_add_word_breakpoint(tf_debug_control *control,
                                            const char *word_name) {
    if (!word_name || word_name[0] == '\0') return 0;
    tf_debug_breakpoint *breakpoint = append_breakpoint(control);
    breakpoint->kind = TF_DEBUG_BREAK_WORD;
    breakpoint->word_name = tf_xstrdup(word_name);
    return breakpoint->id;
}

bool tf_debug_control_remove_breakpoint(tf_debug_control *control, size_t id) {
    for (size_t i = 0; i < control->breakpoint_count; i++) {
        if (control->breakpoints[i].id != id) continue;
        release_breakpoint(&control->breakpoints[i]);
        size_t remaining = control->breakpoint_count - i - 1;
        if (remaining > 0) {
            memmove(&control->breakpoints[i], &control->breakpoints[i + 1],
                    remaining * sizeof(*control->breakpoints));
        }
        control->breakpoint_count--;
        if (control->last_breakpoint_id == id) {
            control->last_breakpoint_id = 0;
        }
        return true;
    }
    return false;
}

void tf_debug_control_clear_breakpoints(tf_debug_control *control) {
    for (size_t i = 0; i < control->breakpoint_count; i++) {
        release_breakpoint(&control->breakpoints[i]);
    }
    control->breakpoint_count = 0;
    control->last_breakpoint_id = 0;
}

const tf_debug_breakpoint *tf_debug_control_breakpoint_at(
    const tf_debug_control *control, size_t index) {
    if (index >= control->breakpoint_count) return NULL;
    return &control->breakpoints[index];
}
