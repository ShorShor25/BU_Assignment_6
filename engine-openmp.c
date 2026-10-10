#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <omp.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 256
#define NUM_THREADS 4

struct chunk_result {
    int count;
    char **instances;
    int capacity;
};

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

static long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        return -1;
    }
    return (long)st.st_size;
}

static int search_count_chunk(const char *filename, const char *target, long start, long end) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return 0;
    }

    if (start > 0) {
        fseek(fp, start, SEEK_SET);
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
    int local_count = 0;

    while (ftell(fp) <= end && (read = getline(&line, &len, fp)) != -1) {
        local_count += count_occurrences_in_line(line, target);
    }

    free(line);
    fclose(fp);
    return local_count;
}

static struct chunk_result search_instance_chunk(const char *filename, const char *target, long start, long end) {
    struct chunk_result res;
    res.count = 0;
    res.capacity = 16;
    res.instances = malloc(res.capacity * sizeof(char *));

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return res;
    }

    if (start > 0) {
        fseek(fp, start, SEEK_SET);
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

    while (ftell(fp) <= end && (read = getline(&line, &len, fp)) != -1) {
        int occurrences = count_occurrences_in_line(line, target);
        if (occurrences > 0) {
            char *trimmed = trim_whitespace(line);

            for (int i = 0; i < occurrences; i++) {
                if (res.count >= res.capacity) {
                    res.capacity *= 2;
                    char **temp = realloc(res.instances, res.capacity * sizeof(char *));
                    if (!temp) {
                        free(trimmed);
                        free(line);
                        fclose(fp);
                        return res;
                    }
                    res.instances = temp;
                }
                res.instances[res.count] = (i == 0) ? trimmed : strdup(trimmed);
                res.count++;
            }
        }
    }

    free(line);
    fclose(fp);
    return res;
}

int search_count(char *filename, char *target) {
    long file_size = get_file_size(filename);
    if (file_size <= 0) {
        return 0;
    }

    int thread_count = NUM_THREADS;
    if (file_size < thread_count) {
        thread_count = 1;
    }

    long chunk_size = file_size / thread_count;
    int partial_counts[thread_count];

    #pragma omp parallel for num_threads(thread_count)
    for (int i = 0; i < thread_count; i++) {
        long start = i * chunk_size;
        long end = (i == thread_count - 1) ? file_size : (start + chunk_size);
        partial_counts[i] = search_count_chunk(filename, target, start, end);
    }

    int total_count = 0;
    for (int i = 0; i < thread_count; i++) {
        total_count += partial_counts[i];
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

    int thread_count = NUM_THREADS;
    if (file_size < thread_count) {
        thread_count = 1;
    }

    long chunk_size = file_size / thread_count;
    struct chunk_result partial[thread_count];

    #pragma omp parallel for num_threads(thread_count)
    for (int i = 0; i < thread_count; i++) {
        long start = i * chunk_size;
        long end = (i == thread_count - 1) ? file_size : (start + chunk_size);
        partial[i] = search_instance_chunk(filename, target, start, end);
    }

    for (int i = 0; i < thread_count; i++) {
        result.count += partial[i].count;
    }

    if (result.count > 0) {
        result.instances = malloc(result.count * sizeof(char *));
        int current_idx = 0;

        for (int i = 0; i < thread_count; i++) {
            for (int j = 0; j < partial[i].count; j++) {
                result.instances[current_idx++] = partial[i].instances[j];
            }
            free(partial[i].instances);
        }
    } else {
        for (int i = 0; i < thread_count; i++) {
            free(partial[i].instances);
        }
    }

    return result;
}