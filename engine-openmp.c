#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <omp.h>
#include <ctype.h>
#include <pthread.h>

#define MAX_LINE_LENGTH 256

#define MAX_THREADS 4

struct chunk_result {
    struct count_result instances;
    size_t capacity;
    int count;
    int failed;
};

struct worker_args {
    char *filename;
    char *target;
    long id;
    long start;
    long end;
    int count;
    struct count_result result;
    size_t capacity;
    int failed;
    int started;
};

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

static void clear_result(struct count_result *result) {
    for (int i =0; i < result-> count; i++ ) {
        free(result-> instances[i]);
    }

    free(result-> instances);
    result-> instances = NULL;
    result-> count = 0;
}

static int append_instance(struct worker_args * args, const char * line) {
    char * copy;

    if ((size_t) args-> result.count == args-> capacity) {
        size_t new_capacity = args-> capacity == 0?16: args-> capacity * 2;
        char **new_instances = malloc(new_capacity * sizeof(*new_instances));

        if (new_instances == NULL) {

            return 0;
        }

        if (args-> result.count > 0) {
        memcpy(new_instances, args-> result.instances, (size_t) args-> result.count * sizeof(*new_instances));
    }
        free(args-> result.instances);
        args-> result.instances = new_instances;
        args-> capacity = new_capacity;

    }
    copy = copy_trimmed_line(line);
    if(copy == NULL) {
        return 0;
    }

    args-> result.instances[args-> result.count] = copy;
    args-> result.count++;
    return 1;
}

static void scan_chunk(struct worker_args *args, int collect_instances) {
    FILE *file = fopen(args-> filename, "r");
    char *line = malloc(MAX_LINE_LENGTH);
    size_t capacity = MAX_LINE_LENGTH;
    ssize_t line_length;

    if (line == NULL) {
        fclose(file);
        args-> failed =1;
        return;
    }

    if (args-> start > 0) {
        int previous;

        if(fseek(file, args-> start -1, SEEK_SET) !=0) {
            args-> failed = 1;
        } else {
            previous = fgetc(file);

            if(fseek(file, args-> start, SEEK_SET) !=0) {
                args-> failed = 1;
            } else if (previous != '\n') {
                (void)getline(&line, &capacity, file); 
            }
        }
    }
    while (!args-> failed) {

        long line_start = ftell(file);
        
        if (line_start < 0 || line_start >= args-> end) {
            break;
        }
        line_length = getline(&line, &capacity, file);
        if (line_length == -1) {
            break;
        }

        int matches = count_line_matches(line, args-> target);

        if(collect_instances) {
            for (int i = 0; i < matches; i++) {
                if (!append_instance(args, line)) {
                    args-> failed = 1;
                    break;
                }
            }
        } else {
            args-> count += matches;
        }
        
    }
    free(line);
    fclose(file);

    if (args-> failed) {
        args-> count = 0;
        clear_result(&args-> result);
    }

}

static int setup_chunks(char *filename,char *target, struct worker_args **args_out, long *nthreads_out) {
    struct stat file_info;
    long file_size;
    long nthreads;
    long chunk_size;
    struct worker_args *args;

    if(filename == NULL || target == NULL || target[0] == '\0' || stat(filename, &file_info) !=0 || file_info.st_size <=0) {
        return 0;
    }

    file_size = (long) file_info.st_size;
    nthreads = file_size < MAX_THREADS ? file_size : MAX_THREADS;
    chunk_size = file_size / nthreads;

    threads = malloc((size_t)nthreads * sizeof(*threads));
    args = malloc((size_t)nthreads * sizeof(*args));


    if(threads == NULL || args == NULL ) {
        free(threads);
        free(args);
        return 0;
    }

    for (long i = 0; i < nthreads; i++) {

        args[i].count = 0;
        args[i].result.count = 0;
        args[i].result.instances = NULL;
        args[i].capacity = 0;
        args[i].failed = 0;
        args[i].started = 0;


        args[i].filename = filename;
        args[i].target = target;
        args[i].id = i;
        args[i].start = i * chunk;
        args[i].end = (i == nthreads - 1) ? file_size : (i + 1) * chunk;
    }

    *args_out = args;
    *nthreads_out = nthreads;
    return 1;
}

static void run_workers (struct worker_args * args,  long nthreads, void *(*worker) (void *)) {
    for(long i = 0; i < nthreads; i++) {
        if (pthread_create(&threads[i], NULL, worker, &args[i]) == 0) {
            args[i].started = 1;

        } else {
            worker(&args[i]);
        }
    }

    for (long i = 0; i < nthreads; i++) {
        if (args[i].started) {
            pthread_join(threads[i], NULL);
        }
    }
}

int search_count(char *filename, char *target) {
    struct worker_args *args;
    long nthreads;
    int total = 0;
    int failed = 0;

    if (!setup_chunks(filename, target, &args, &nthreads)) {
        return 0;
    }

#pragma omp parallel for num_threads(nthreads) schedule(static)
    for( long i = 0; i < nthreads; i++) {
        scan_chunk(&args[i], 0);
    }

    for (long i = 0; i < nthreads; i++) {
        if(args[i].failed){
            failed = 1;
        }
        total +=args[i].count;

    }

        free(args);
        return failed ? 0 : total;

}

struct count_result search_instance(char *filename,char *target){
    struct count_result result = {0, NULL};
    struct worker_args *args;
    long nthreads;
    int total = 0;
    int failed = 0;

    if (!setup_chunks(filename, target, &args, &nthreads)) {
        return result;
    }


#pragma omp parallel for num_threads(nthreads) schedule(static)
    for( int i = 0; i < nthreads; i++) {
        
        scan_chunk( &args[i], 1);
    }

    for (long i = 0; i < nthreads; i++) {
        if(args[i].failed) {
            failed = 1;
        }

        total += args[i].result.count;

    }

    if(!failed && total > 0) {
        result.instances = malloc((size_t) total * sizeof(*result.instances));
        if (result.instances == NULL) {
            failed = 1;
        }
    }

    if (!failed) {
        for(long i = 0; i < nthreads; i++) {
            for(int j = 0; j < args[i].result.count; j++) {
                result.instances[result.count++] = args[i].result.instances[j];
                args[i].result.instances[j] = NULL;
            }
        }
    }

    for(long i = 0; i < nthreads; i++) {
        clear_result(&args[i].result);
    }

    free(args);
    return result;
}