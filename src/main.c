#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_exec.h"
#include "tf_lib.h"
#include "tf_repl.h"

typedef struct {
    const char *filename;
    char *eval;
    int script_argc;
    char **script_argv;
    bool debug, help, interactive;
} cli_config;

static int parse_args(int argc, char **argv, cli_config *config);

int main(int argc, char **argv) {
    signal(SIGINT, tf_vm_handle_sigint);
    tf_console_init();

    cli_config config = {NULL, NULL, 0, NULL, false, false, false};
    tf_ret ret = parse_args(argc, argv, &config);
    if (ret == TF_ERR || config.help) {
        fprintf(stderr, "=== Toy Interpreter ===\n");
        fprintf(stderr,
                "Usage: %s [--debug|-d] [--eval|-e code] [filename] [args...]\n",
                argv[0]);
        fprintf(stderr, "\nRunning without filename starts the REPL\n");
        fprintf(stderr, "--debug shows parsed tokens and stack after execution\n");
        fprintf(stderr, "--eval executes program passed as argument\n");
        return ret;
    }

    tf_ctx *ctx = tf_ctx_new(config.script_argc, config.script_argv);
    if (!ctx) { return TF_ERR; }

    tf_ret result = TF_OK;

    if (result == TF_OK) {
        if (config.eval != NULL) {
            result = tf_run_string(ctx, config.eval, config.debug);
            free(config.eval);
        } else if (config.interactive) {
            result = tf_run_repl(ctx, config.debug);
        } else {
            result = tf_run_file(ctx, config.filename, config.debug);
        }
    }
    tf_ctx_free(ctx);
    tf_control_state_cache_clear();
    tf_obj_cache_clear();

#ifdef TF_ALLOC_STATS
    fprintf(stderr, "\n=== allocation statistics ===\n");
    tf_alloc_stats_dump();
#endif

#ifdef STB_LEAKCHECK
    printf("\n=== stb_leakcheck_dumpmem output ===\n");
    stb_leakcheck_dumpmem();
#endif
    return result;
}

static int parse_args(int argc, char **argv, cli_config *config) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            config->debug = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            config->help = true;
        } else if (strcmp(argv[i], "--eval") == 0 || strcmp(argv[i], "-e") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "-e requires an argument\n");
                return TF_ERR;
            }
            config->eval = tf_xmalloc(strlen(argv[i + 1]) + 1);
            strcpy(config->eval, argv[i + 1]);
            i++;  // consume eval argument
        } else if (config->filename == NULL) {
            config->filename = argv[i];
            config->script_argc = argc - i;
            config->script_argv = &argv[i];
            break;
        }
    }
    config->interactive = (config->filename == NULL && config->eval == NULL);
    return TF_OK;
}
