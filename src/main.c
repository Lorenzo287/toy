#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_debug_protocol.h"
#include "tf_exec.h"
#include "tf_lib.h"
#include "tf_repl.h"

typedef struct {
    const char *filename;
    char *eval;
    int script_argc;
    char **script_argv;
    bool debug, tdb, debug_protocol, help, interactive;
} cli_config;

static int parse_args(int argc, char **argv, cli_config *config);

int main(int argc, char **argv) {
    signal(SIGINT, tf_vm_handle_sigint);

    cli_config config = {0};
    tf_ret ret = parse_args(argc, argv, &config);
    if (ret == TF_ERR || config.help) {
        fprintf(stderr, "=== Toy Interpreter ===\n");
        fprintf(stderr,
                "Usage: %s [--debug|-d] [--tdb] [--eval|-e code] "
                "[filename] [args...]\n",
                argv[0]);
        fprintf(stderr, "\nRunning without filename starts the REPL\n");
        fprintf(stderr, "--debug shows parsed tokens and stack after execution\n");
        fprintf(stderr, "--tdb steps through file, eval, or REPL input\n");
        fprintf(stderr, "--eval executes program passed as argument\n");
        return ret;
    }

    FILE *protocol_output = NULL;
    if (config.debug_protocol) {
        protocol_output = tf_debug_protocol_open_output();
        if (!protocol_output) {
            fprintf(stderr, "failed to open debug protocol output\n");
            return TF_ERR;
        }
    }
    tf_console_init();

    tf_ctx *ctx = tf_ctx_new(config.script_argc, config.script_argv);
    if (!ctx) return TF_ERR;

    tf_debug_protocol *protocol = NULL;
    if (config.debug_protocol) {
        protocol = tf_debug_protocol_new(protocol_output, config.filename);
        if (!protocol) {
            tf_ctx_free(ctx);
            return TF_ERR;
        }
        tf_debug_protocol_install(ctx, protocol);
    } else {
        tf_tdb_set_enabled(ctx, config.tdb);
    }

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
    if (protocol) {
        tf_debug_protocol_install(ctx, NULL);
        tf_debug_protocol_finish(protocol, result);
        tf_debug_protocol_free(protocol);
    } else {
        tf_tdb_set_enabled(ctx, false);
    }
    tf_ctx_free(ctx);
    tf_control_state_cache_clear();
    tf_obj_cache_clear();

#ifdef TF_ALLOC_STATS
    fprintf(stderr, "\n=== allocation statistics ===\n");
    tf_alloc_stats_dump();
#endif

#ifdef STB_LEAKCHECK
    stb_leakcheck_dumpmem();
#endif
    return result;
}

static int parse_args(int argc, char **argv, cli_config *config) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            config->debug = true;
        } else if (strcmp(argv[i], "--tdb") == 0) {
            config->tdb = true;
        } else if (strcmp(argv[i], "--debug-protocol") == 0) {
            config->debug_protocol = true;
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
    if (config->debug_protocol &&
        (config->interactive || config->eval || config->tdb || config->debug)) {
        fprintf(stderr,
                "--debug-protocol requires a file and cannot be combined with "
                "--debug, --tdb, or --eval\n");
        return TF_ERR;
    }
    return TF_OK;
}
