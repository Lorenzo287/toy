#include "tf_debug_control.h"

#include <stdio.h>

#ifdef STB_LEAKCHECK
#include "tf_alloc.h"
#endif

#define CHECK(condition, message)                                             \
    do {                                                                      \
        if (!(condition)) {                                                   \
            fprintf(stderr, "debug control check failed: %s\n", message); \
            return 1;                                                         \
        }                                                                     \
    } while (0)

int main(void) {
    tf_source_file *source = tf_source_file_new("program.toy");
    tf_debug_event event = {
        .span = {.source = source, .offset = 0, .line = 1, .col = 1},
        .pc = 0,
        .frame_depth = 1,
    };
    tf_debug_control control;
    tf_debug_control_init(&control, true);

    CHECK(tf_debug_control_on_event(&control, &event, "<program>") ==
              TF_DEBUG_STOP_ENTRY,
          "initial entry stop");

    tf_debug_control_resume(&control, TF_DEBUG_RUN_STEP_IN, event.frame_depth);
    event.pc = 1;
    CHECK(tf_debug_control_on_event(&control, &event, "<program>") ==
              TF_DEBUG_STOP_STEP,
          "step-in stop");

    tf_debug_control_resume(&control, TF_DEBUG_RUN_STEP_OVER, 1);
    event.frame_depth = 2;
    CHECK(tf_debug_control_on_event(&control, &event, "nested") ==
              TF_DEBUG_STOP_NONE,
          "step-over skips deeper frames");
    event.frame_depth = 1;
    CHECK(tf_debug_control_on_event(&control, &event, "<program>") ==
              TF_DEBUG_STOP_STEP,
          "step-over stops in original frame");

    tf_debug_control_resume(&control, TF_DEBUG_RUN_STEP_OUT, 2);
    event.frame_depth = 2;
    CHECK(tf_debug_control_on_event(&control, &event, "nested") ==
              TF_DEBUG_STOP_NONE,
          "step-out skips current frame");
    event.frame_depth = 1;
    CHECK(tf_debug_control_on_event(&control, &event, "<program>") ==
              TF_DEBUG_STOP_STEP,
          "step-out stops in caller");

    size_t line_id = tf_debug_control_add_line_breakpoint(
        &control, "program.toy", 4, &event);
    size_t word_id =
        tf_debug_control_add_word_breakpoint(&control, "update");
    CHECK(line_id == 1 && word_id == 2, "stable breakpoint ids");
    CHECK(tf_debug_control_breakpoint_count(&control) == 2,
          "breakpoint count");
    size_t current_id = tf_debug_control_add_line_breakpoint(
        &control, "program.toy", event.span.line, &event);
    CHECK(current_id == 3 &&
              tf_debug_control_breakpoint_at(&control, 2)->resolved,
          "current line resolves to paused instruction");
    CHECK(tf_debug_control_remove_breakpoint(&control, current_id),
          "remove current-line breakpoint");

    tf_debug_control_resume(&control, TF_DEBUG_RUN_CONTINUE, 1);
    event.span.line = 3;
    event.span.offset = 10;
    event.pc = 2;
    CHECK(tf_debug_control_on_event(&control, &event, "<program>") ==
              TF_DEBUG_STOP_NONE,
          "continue skips ordinary instructions");
    event.span.line = 4;
    event.span.offset = 20;
    CHECK(tf_debug_control_on_event(&control, &event, "<program>") ==
              TF_DEBUG_STOP_BREAKPOINT,
          "line breakpoint stop");
    CHECK(tf_debug_control_last_breakpoint(&control) == line_id,
          "line breakpoint id");
    CHECK(tf_debug_control_breakpoint_at(&control, 0)->resolved,
          "line breakpoint resolution");

    tf_debug_control_resume(&control, TF_DEBUG_RUN_CONTINUE, 1);
    event.span.offset = 21;
    CHECK(tf_debug_control_on_event(&control, &event, "<program>") ==
              TF_DEBUG_STOP_NONE,
          "line breakpoint matches one instruction");
    event.span.line = 5;
    event.pc = 0;
    CHECK(tf_debug_control_on_event(&control, &event, "update") ==
              TF_DEBUG_STOP_BREAKPOINT,
          "word breakpoint stop");
    CHECK(tf_debug_control_last_breakpoint(&control) == word_id,
          "word breakpoint id");

    CHECK(tf_debug_control_remove_breakpoint(&control, line_id),
          "remove breakpoint");
    CHECK(!tf_debug_control_remove_breakpoint(&control, line_id),
          "removed breakpoint is absent");
    tf_debug_control_clear_breakpoints(&control);
    CHECK(tf_debug_control_breakpoint_count(&control) == 0,
          "clear breakpoints");

    tf_debug_control_dispose(&control);
    tf_source_file_release(source);
#ifdef STB_LEAKCHECK
    stb_leakcheck_dumpmem();
#endif
    return 0;
}
