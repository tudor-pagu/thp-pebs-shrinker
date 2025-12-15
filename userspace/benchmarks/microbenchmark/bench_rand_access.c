#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static uint64_t xorshift64(uint64_t *s) {
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

static void usage(const char *p) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -n <thp_count>     number of 2MB chunks to mmap (default 30)\n"
        "  -b <batch>         random touches per batch (default 10000)\n"
        "  -p <pause_ms>      pause between batches (default 200)\n"
        "  -t <seconds>       total runtime in seconds (default 20)\n"
        "  --read             do read touches (default is write)\n"
        "  --nohuge           madvise MADV_NOHUGEPAGE\n"
        "  --huge             madvise MADV_HUGEPAGE\n"
        "  --prefault         touch one byte per 4KB page up front (NOT sparse)\n"
        "  --seed <u64>       RNG seed (default: time)\n",
        p);
}

int main(int argc, char **argv) {
    const size_t THP = 2UL * 1024 * 1024;
    const size_t PAGE = 4096;

    int thp_count = 30;
    int batch = 10000;
    int pause_ms = 200;
    int seconds = 20;
    int do_read = 0;
    int advise = 0; // 0 none, 1 huge, 2 nohuge
    int prefault = 0;
    uint64_t seed = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) thp_count = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-b") && i + 1 < argc) batch = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) pause_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) seconds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--read")) do_read = 1;
        else if (!strcmp(argv[i], "--huge")) advise = 1;
        else if (!strcmp(argv[i], "--nohuge")) advise = 2;
        else if (!strcmp(argv[i], "--prefault")) prefault = 1;
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else {
            usage(argv[0]);
            return 2;
        }
    }

    if (seed == 0) seed = (uint64_t)time(NULL) ^ ((uint64_t)getpid() << 32);

    size_t size = (size_t)thp_count * THP;

    void *base = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return 1;
    }

    if (advise == 1) madvise(base, size, MADV_HUGEPAGE);
    else if (advise == 2) madvise(base, size, MADV_NOHUGEPAGE);

    printf("pid=%d base=%p size=%zu bytes (%.2f MiB)\n",
           getpid(), base, size, (double)size / (1024.0 * 1024.0));
    fflush(stdout);

    volatile unsigned char sink = 0;

    if (prefault) {
        for (size_t off = 0; off < size; off += PAGE) {
            volatile unsigned char *p = (unsigned char *)base + off;
            *p = (unsigned char)(*p + 1);
        }
        printf("prefault done\n");
        fflush(stdout);
    }

    struct timespec start_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    while (1) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - start_ts.tv_sec)
                       + (now.tv_nsec - start_ts.tv_nsec) / 1e9;
        if (elapsed >= seconds) break;

        for (int i = 0; i < batch; i++) {
            uint64_t r = xorshift64(&seed);
            size_t off = (size_t)(r % size);

            off = (off / PAGE) * PAGE;

            volatile unsigned char *p = (unsigned char *)base + off;
            if (do_read) {
                sink ^= *p;
            } else {
                *p = (unsigned char)(*p + 1);
            }
        }

        if (pause_ms > 0) usleep((useconds_t)pause_ms * 1000);
    }

    if (do_read) fprintf(stderr, "sink=%u\n", (unsigned)sink);

    munmap(base, size);
    return 0;
}
