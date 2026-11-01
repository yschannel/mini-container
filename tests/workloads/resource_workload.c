#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static unsigned long long parse(const char *text, unsigned long long maximum) {
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno || end == text || *end || value == 0 || value > maximum) {
        fprintf(stderr, "invalid bounded workload argument: %s\n", text);
        exit(2);
    }
    return value;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: minic-workload cpu SECONDS|memory MIB|wait SECONDS\n");
        return 2;
    }
    if (strcmp(argv[1], "cpu") == 0) {
        unsigned long long seconds = parse(argv[2], 30);
        struct timespec start, now;
        if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) return 3;
        volatile uint64_t work = 0;
        do {
            for (unsigned i = 0; i < 10000; ++i) ++work;
            if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 3;
        } while (((int64_t)(now.tv_sec - start.tv_sec) * 1000000000LL +
                  (now.tv_nsec - start.tv_nsec)) < (int64_t)seconds * 1000000000LL);
        return work == 0 ? 3 : 0;
    }
    if (strcmp(argv[1], "memory") == 0) {
        size_t mib = (size_t)parse(argv[2], 256);
        size_t size = mib * 1024 * 1024;
        volatile unsigned char *data = malloc(size);
        if (!data) return 3;
        for (size_t i = 0; i < size; i += 4096) data[i] = 1;
        sleep(2);
        free((void *)data);
        return 0;
    }
    if (strcmp(argv[1], "wait") == 0) {
        sleep((unsigned)parse(argv[2], 30));
        return 0;
    }
    fprintf(stderr, "unknown workload mode\n");
    return 2;
}
