#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>

// #define MAX_LINE_LENGTH 256

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

int search_count(char *filename, char *target) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return 0;
    }

    int total_count = 0;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, fp)) != -1) {
        total_count += count_occurrences_in_line(line, target);
    }

    free(line);
    fclose(fp);
    return total_count;
}

struct count_result search_instance(char *filename, char *target) {
    struct count_result result;
    result.count = 0;
    result.instances = NULL;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return result;
    }

    size_t capacity = 16;
    result.instances = malloc(capacity * sizeof(char *));
    if (!result.instances) {
        fclose(fp);
        return result;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, fp)) != -1) {
        int occurrences = count_occurrences_in_line(line, target);
        if (occurrences > 0) {
            char *trimmed = trim_whitespace(line);

            for (int i = 0; i < occurrences; i++) {
                if ((size_t)result.count >= capacity) {
                    capacity *= 2;
                    char **temp = realloc(result.instances, capacity * sizeof(char *));
                    if (!temp) {
                        free(trimmed);
                        free(line);
                        fclose(fp);
                        return result;
                    }
                    result.instances = temp;
                }
                result.instances[result.count] = (i == 0) ? trimmed : strdup(trimmed);
                result.count++;
            }
        }
    }

    free(line);
    fclose(fp);
    return result;
}