#ifndef TOY_NOB_BENCHMARKS_H
#define TOY_NOB_BENCHMARKS_H

/* Cross-platform benchmark discovery, sampling, and wall-clock reporting. */

static int compare_benchmark_names(const void *left, const void *right) {
    const char *const *a = left;
    const char *const *b = right;
    return strcmp(*a, *b);
}

static int compare_samples(const void *left, const void *right) {
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;
    return (a > b) - (a < b);
}

static bool append_benchmark_path(File_Paths *paths, const char *name) {
    const char *filename = ends_with(name, ".toy")
        ? name
        : temp_sprintf("%s.toy", name);
    const char *path = starts_with(filename, "benchmarks/") ||
                               starts_with(filename, "benchmarks\\")
        ? filename
        : temp_sprintf("benchmarks/%s", filename);
    if (!file_exists(path)) {
        nob_log(ERROR, "benchmark does not exist: %s", path);
        return false;
    }
    da_append(paths, path);
    return true;
}

static bool collect_benchmarks(const File_Paths *requested,
                               File_Paths *paths) {
    if (requested->count > 0) {
        for (size_t i = 0; i < requested->count; ++i) {
            if (!append_benchmark_path(paths, requested->items[i])) {
                return false;
            }
        }
        return true;
    }

    File_Paths entries = {0};
    if (!read_entire_dir("benchmarks", &entries)) return false;
    qsort(entries.items, entries.count, sizeof(entries.items[0]),
          compare_benchmark_names);
    for (size_t i = 0; i < entries.count; ++i) {
        if (ends_with(entries.items[i], ".toy") &&
            !append_benchmark_path(paths, entries.items[i])) {
            da_free(entries);
            return false;
        }
    }
    da_free(entries);
    return true;
}

static bool run_benchmarks(const char *toy, const File_Paths *requested,
                           size_t runs) {
    if (!file_exists(toy)) {
        nob_log(ERROR, "Toy executable does not exist: %s", toy);
        return false;
    }

    File_Paths paths = {0};
    if (!collect_benchmarks(requested, &paths)) {
        da_free(paths);
        return false;
    }

    uint64_t *samples = malloc(sizeof(*samples) * runs);
    if (!samples) {
        nob_log(ERROR, "could not allocate benchmark samples");
        da_free(paths);
        return false;
    }

    bool ok = true;
    for (size_t i = 0; ok && i < paths.count; ++i) {
        const char *path = paths.items[i];
        fprintf(stderr, "\n%s\n", path_name(path));
        for (size_t run = 0; run < runs; ++run) {
            Cmd command = {0};
            cmd_append(&command, toy, "--file", path);
            uint64_t start = nanos_since_unspecified_epoch();
            Nob_Log_Level previous_level = minimal_log_level;
            minimal_log_level = WARNING;
            bool ran = cmd_run(&command);
            minimal_log_level = previous_level;
            if (!ran) {
                ok = false;
                break;
            }
            samples[run] = nanos_since_unspecified_epoch() - start;
            fprintf(stderr, "run %zu: %.3f ms wall\n", run + 1,
                    (double)samples[run] / 1000000.0);
        }
        if (!ok) break;

        qsort(samples, runs, sizeof(*samples), compare_samples);
        double median = runs % 2 == 1
            ? (double)samples[runs / 2]
            : ((double)samples[runs / 2 - 1] +
               (double)samples[runs / 2]) / 2.0;
        fprintf(stderr, "median: %.3f ms wall\n", median / 1000000.0);
    }

    free(samples);
    da_free(paths);
    return ok;
}

#endif  // TOY_NOB_BENCHMARKS_H
