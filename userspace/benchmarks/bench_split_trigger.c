// bench_split_trigger.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

int main() {
    size_t len = 4096UL * 512; // 1 huge page
    void *p = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    madvise(p, len, MADV_HUGEPAGE);
    memset(p, 1, len);

    printf("Before MADV_DONTNEED:\n");
    int out1 = system("grep -i AnonHuge /proc/meminfo");
    if (out1 == -1) {
        perror("system");
    }

    // Free middle part (simulate jemalloc MADV_DONTNEED)
    void *mid = (char *)p + len / 2;
    madvise(mid, 4096 * 10, MADV_DONTNEED);

    printf("After MADV_DONTNEED:\n");
    int out2 = system("grep -i AnonHuge /proc/meminfo");
    if (out2 == -1) {
        perror("system");
    }

    munmap(p, len);
    return 0;
}
