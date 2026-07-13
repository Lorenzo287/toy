#ifndef TF_DEBUG_CONTROL_H
#define TF_DEBUG_CONTROL_H

#include "tf_exec.h"

typedef enum {
    TF_DEBUG_RUN_PAUSED,
    TF_DEBUG_RUN_STEP_IN,
    TF_DEBUG_RUN_STEP_OVER,
    TF_DEBUG_RUN_STEP_OUT,
    TF_DEBUG_RUN_CONTINUE
} tf_debug_run_mode;

typedef enum {
    TF_DEBUG_STOP_NONE,
    TF_DEBUG_STOP_ENTRY,
    TF_DEBUG_STOP_STEP,
    TF_DEBUG_STOP_BREAKPOINT
} tf_debug_stop_reason;

typedef enum {
    TF_DEBUG_BREAK_LINE,
    TF_DEBUG_BREAK_WORD
} tf_debug_breakpoint_kind;

typedef struct {
    size_t id;
    tf_debug_breakpoint_kind kind;
    char *source_name;
    char *word_name;
    size_t line;
    uint32_t offset;
    bool resolved;
} tf_debug_breakpoint;

typedef struct {
    tf_debug_breakpoint *breakpoints;
    size_t breakpoint_count;
    size_t breakpoint_capacity;
    size_t next_breakpoint_id;
    size_t resume_depth;
    size_t last_breakpoint_id;
    tf_debug_run_mode mode;
    bool first_stop;
} tf_debug_control;

void tf_debug_control_init(tf_debug_control *control, bool stop_on_entry);
void tf_debug_control_dispose(tf_debug_control *control);
void tf_debug_control_pause(tf_debug_control *control);
void tf_debug_control_resume(tf_debug_control *control,
                             tf_debug_run_mode mode, size_t frame_depth);

tf_debug_stop_reason tf_debug_control_on_event(
    tf_debug_control *control, const tf_debug_event *event,
    const char *word_name);

size_t tf_debug_control_add_line_breakpoint(tf_debug_control *control,
                                            const char *source_name,
                                            size_t line,
                                            const tf_debug_event *current_event);
size_t tf_debug_control_add_word_breakpoint(tf_debug_control *control,
                                            const char *word_name);
bool tf_debug_control_remove_breakpoint(tf_debug_control *control, size_t id);
void tf_debug_control_clear_breakpoints(tf_debug_control *control);
size_t tf_debug_control_breakpoint_count(const tf_debug_control *control);
const tf_debug_breakpoint *tf_debug_control_breakpoint_at(
    const tf_debug_control *control, size_t index);
size_t tf_debug_control_last_breakpoint(const tf_debug_control *control);

#endif  // TF_DEBUG_CONTROL_H
