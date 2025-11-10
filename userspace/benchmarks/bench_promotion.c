// bench_promotion.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

// Usage: ./bench_promotion [stride]
// stride: distance between touched pages in bytes (default: 32 * 4096)
int main(int argc, char **argv) {
    size_t len = 4096UL * 512 * 2; // 2 huge pages
    size_t stride = (argc > 1 ? atol(argv[1]) : 4096 * 32); // touch every 32 pages
    void *p = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    madvise(p, len, MADV_HUGEPAGE);

    for (int pass = 0; pass < 10; pass++) {
        for (size_t i = 0; i < len; i += stride)
            ((char*)p)[i] = pass;
        printf("Pass %d done, stride=%zu\n", pass, stride);
        int out = system("grep -i AnonHuge /proc/meminfo | head -n1");
        if (out == -1) {
           perror("system");
        }
        usleep(100000);
    }

    munmap(p, len);
    return 0;
}
