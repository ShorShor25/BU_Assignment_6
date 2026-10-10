#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 256
#define NUM_THREADS 4

struct worker_args {
    char *filename;
    char *target;
    long start;
    long end;

    // Output fields (thread-local results to avoid data races)
    int local_count;
    char **local_instances;
    int local_capacity;
};

// Counts all substring occurrences of target in line
static int count_occurrences_in_line(const char *line, const char *target) {
    int count = 0;
    size_t target_len = strlen(target);
    if (target_len == 0) return 0;

    const char *p = line;
    while ((p = strstr(p, target)) != NULL) {
        count++;
        p += target_len;
    }
    return count;
}

// Trims leading and trailing whitespace into a newly allocated string
static char *trim_whitespace(const char *str) {
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    const char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    size_t len = (size_t)(end - str + 1);
    char *trimmed = malloc(len + 1);
    if (!trimmed) {
        return NULL;
    }
    memcpy(trimmed, str, len);
    trimmed[len] = '\0';
    return trimmed;
}

// Worker function for count-only search
static void *count_worker(void *arg) {
    struct worker_args *args = (struct worker_args *)arg;
    args->local_count = 0;

    FILE *fp = fopen(args->filename, "r");
    if (!fp) {
        return NULL;
    }

    // If not the very first chunk, advance to the start of the next complete line
    if (args->start > 0) {
        fseek(fp, args->start, SEEK_SET);
        int c;
        while ((c = fgetc(fp)) != EOF && c != '\n') {
            // Discard the remainder of the line started in the previous chunk
        }
    } else {
        fseek(fp, 0, SEEK_SET);
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // Read full lines until our position exceeds the end offset
    while (ftell(fp) <= args->end && (read = getline(&line, &len, fp)) != -1) {
        args->local_count += count_occurrences_in_line(line, args->target);
    }

    free(line);
    fclose(fp);
    return NULL;
}

// Worker function for instance collecting search
static void *instance_worker(void *arg) {
    struct worker_args *args = (struct worker_args *)arg;
    args->local_count = 0;
    args->local_capacity = 16;
    args->local_instances = malloc(args->local_capacity * sizeof(char *));

    FILE *fp = fopen(args->filename, "r");
    if (!fp) {
        return NULL;
    }

    // If not the very first chunk, skip to start of next complete line
    if (args->start > 0) {
        fseek(fp, args->start, SEEK_SET);
        int c;
        while ((c = fgetc(fp)) != EOF && c != '\n') {
            // Discard partial line
        }
    } else {
        fseek(fp, 0, SEEK_SET);
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while (ftell(fp) <= args->end && (read = getline(&line, &len, fp)) != -1) {
        int occurrences = count_occurrences_in_line(line, args->target);
        if (occurrences > 0) {
            char *trimmed = trim_whitespace(line);

            for (int i = 0; i < occurrences; i++) {
                if (args->local_count >= args->local_capacity) {
                    args->local_capacity *= 2;
                    char **temp = realloc(args->local_instances, args->local_capacity * sizeof(char *));
                    if (!temp) {
                        free(trimmed);
                        free(line);
                        fclose(fp);
                        return NULL;
                    }
                    args->local_instances = temp;
                }
                args->local_instances[args->local_count] = (i == 0) ? trimmed : strdup(trimmed);
                args->local_count++;
            }
        }
    }

    free(line);
    fclose(fp);
    return NULL;
}

// Helper to obtain the file size in bytes
static long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        return -1;
    }
    return (long)st.st_size;
}

int search_count(char *filename, char *target) {
    long file_size = get_file_size(filename);
    if (file_size <= 0) {
        return 0;
    }

    int num_threads = NUM_THREADS;
    if (file_size < num_threads) {
        num_threads = 1;
    }

    pthread_t threads[num_threads];
    struct worker_args args[num_threads];
    long chunk_size = file_size / num_threads;

    for (int i = 0; i < num_threads; i++) {
        args[i].filename = filename;
        args[i].target = target;
        args[i].start = i * chunk_size;
        args[i].end = (i == num_threads - 1) ? file_size : (args[i].start + chunk_size);
        args[i].local_count = 0;
        args[i].local_instances = NULL;

        pthread_create(&threads[i], NULL, count_worker, &args[i]);
    }

    int total_count = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_count += args[i].local_count;
    }

    return total_count;
}

struct count_result search_instance(char *filename, char *target) {
    struct count_result result;
    result.count = 0;
    result.instances = NULL;

    long file_size = get_file_size(filename);
    if (file_size <= 0) {
        return result;
    }

    int num_threads = NUM_THREADS;
    if (file_size < num_threads) {
        num_threads = 1;
    }

    pthread_t threads[num_threads];
    struct worker_args args[num_threads];
    long chunk_size = file_size / num_threads;

    for (int i = 0; i < num_threads; i++) {
        args[i].filename = filename;
        args[i].target = target;
        args[i].start = i * chunk_size;
        args[i].end = (i == num_threads - 1) ? file_size : (args[i].start + chunk_size);
        args[i].local_count = 0;
        args[i].local_instances = NULL;

        pthread_create(&threads[i], NULL, instance_worker, &args[i]);
    }

    // Join threads and calculate total count first
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        result.count += args[i].local_count;
    }

    if (result.count > 0) {
        result.instances = malloc(result.count * sizeof(char *));
        int current_idx = 0;

        // Preserve chunk order to keep instance order consistent
        for (int i = 0; i < num_threads; i++) {
            for (int j = 0; j < args[i].local_count; j++) {
                result.instances[current_idx++] = args[i].local_instances[j];
            }
            free(args[i].local_instances);
        }
    } else {
        for (int i = 0; i < num_threads; i++) {
            free(args[i].local_instances);
        }
    }

    return result;
}