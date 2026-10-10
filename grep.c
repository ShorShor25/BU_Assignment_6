#include "engine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <count|instance> <input_file> <target_word>\n",
                argv[0]);
        return 1;
    }

    char *mode = argv[1];
    char *filepath = argv[2];
    char *target = argv[3];

    if (strcmp(mode, "count") == 0) {
        int count = search_count(filepath, target);
        printf("Found: %d of %s in %s\n", count, target, filepath);
    } else if (strcmp(mode, "instance") == 0) {
        struct count_result res = search_instance(filepath, target);
        printf("Found: %d of %s in %s\n", res.count, target, filepath);
        for (int i = 0; i < res.count; i++) {
            printf("res.instances[%d]: %s\n", i, res.instances[i]);
            free(res.instances[i]);
        }
        free(res.instances);
    } else {
        fprintf(stderr, "Invalid mode '%s'. Use 'count' or 'instance'.\n", mode);
        return 1;
    }

    return 0;
}