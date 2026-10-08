#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>

#include<stdbool.h>
#define MAX_LINE_LENGTH 256


static int is_word_char(unsigned char character) {
    return isalnum(character) || character == '_';
}

static int count_line_matches(const char *line, const char *target) {
    size_t target_length = strlen(target);
    const char *cursor = line;
    const char *match;
    int count =0; 

    while ((match = strstr(cursor, target)) !=NULL) {
        count++;

        cursor = match +1;

    }
    return count;

}

static char *copy_trimmed_line(const char *line) {
    const char *start = line;
    size_t length;

    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }
    length = strlen(start);
    while( length>0 && isspace((unsigned char)start[length - 1])) {
        length--;
    }

    char *copy = malloc(length +1);
    if(copy == NULL) {
        return NULL;
    }

    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
    
}


int search_count(char *filename, char *target) {
    FILE *file;
    char *line = NULL;
    size_t capacity = 0;
    ssize_t line_length;
    int count = 0;

    if (target ==NULL || target[0] == '\0') {
        return 0;
    }

    file = fopen(filename, "r");

    if(file == NULL ) {
        return 0;
    }

    while ((line_length = getline(&line, &capacity, file)) != -1) {
        (void) line_length;
        count += count_line_matches(line, target);

    }
        free(line);
        fclose(file);
        return count;

   
}

struct count_result search_instance(char *filename,char *target){
    struct count_result result = {0, NULL};
    FILE *file;
    char *line = NULL;
    size_t line_capacity = 0;
    size_t instances_capacity = 0;
    ssize_t line_length;

    if(target ==NULL || target[0] == '\0') {
        return result;
    }

    file = fopen(filename, "r");

    if(file == NULL ) {
        return result;
    }

    while((line_length = getline(&line, &line_capacity, file)) != -1) {
        int matches;
        (void) line_length;
        matches = count_line_matches(line, target);

        for(int i =0; i < matches; i++) {
            if((size_t)result.count == instances_capacity) {
                size_t new_capacity = instances_capacity == 0 ? 16 : instances_capacity * 2;
                char ** new_instances = realloc(result.instances, new_capacity * sizeof(*result.instances));

                if(new_instances==NULL) {
                    for (int j = 0; j < result.count; j++) {
                        free(result.instances[j]);
                    }
                    free(result.instances);
                    free(line);
                    fclose(file);
                    result.count = 0;
                    result.instances = NULL;
                    return result;
                }
                result.instances = new_instances;
                instances_capacity = new_capacity;
            }
                result.instances[result.count] = copy_trimmed_line(line);
                if(result.instances[result.count]==NULL) {
                    for (int j = 0; j < result.count; j++) {
                        free(result.instances[j]);
                    }
                    free(result.instances);
                    free(line);
                    fclose(file);
                    result.count = 0;
                    result.instances = NULL;
                    return result;
                }
                result.count++;            
        }
    }
    free(line);
    fclose(file);
    return result;

}