#include "engine.h"
#include <stdio.h>
#include <string.h>

#include <stdlib.h>

int main(int argc, char** argv) {
    // TODO: parse the arguments in argv. 
    // You can expect argv[1] to be the mode
    // You can expect argv[2] to be the filepath
    // You can expect argv[3] to be the target word

    const char *mode;
    char *filename;
    char *target;

    if(argc != 4) {
        printf("Incorrect number of arguments. Expected: ./build/grep <MODE=count|instance> <input_file> <target_word>\n");
        return 1;
    }


    mode = argv[1];
    filename = argv[2];
    target = argv[3];

    if (strcmp(mode, "count")== 0) {
        int count = search_count(filename, target);
        printf("Found: %d of %s in %s\n", count, target, filename);
    } else if (strcmp(mode, "instance") == 0) {
        struct count_result result = search_instance(filename, target);

        printf("Found %d of %s in %s\n", result.count, target, filename);

        for(int i =0; i < result.count; i++) {
            printf("res.instances[%d]: %s\n", i, result.instances[i]);
            free(result.instances[i]);
        }
        free(result.instances);
    } else {
        fprintf(stderr, "error check");
        return 1;
    }

    return 0;
    
}