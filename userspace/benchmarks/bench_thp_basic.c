// bench_thp_basic.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>

void print_thp_info() {
    int out = system("grep -i huge /proc/meminfo | grep AnonHugePages");
    if (out == -1) {
        perror("system");
    }
}

// Usage: ./bench_thp_basic [length] [use_huge]
// length: size of memory to allocate in bytes (default: 20MB)
// use_huge: 1 to use MADV_HUGEPAGE, 0 to use MADV_NOHUGEPAGE (default: 0)
int main(int argc, char **argv) {
    size_t len = (argc > 1 ? atol(argv[1]) : 4096UL * 512 * 10); // 10 huge pages = 20MB
    int use_huge = (argc > 2 ? atoi(argv[2]) : 0);

    void *p = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    if (use_huge) {
        madvise(p, len, MADV_HUGEPAGE);
        printf("Using MADV_HUGEPAGE\n");
    } else {
        madvise(p, len, MADV_NOHUGEPAGE);
        printf("Using MADV_NOHUGEPAGE\n");
    }

    clock_t start = clock();
    for (size_t i = 0; i < len; i += 4096)
        ((char *)p)[i] = 'a';
    clock_t end = clock();

    printf("Write time: %.3f s\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    volatile char sum = 0;
    for (size_t i = 0; i < len; i += 4096)
        sum += ((char *)p)[i];
    end = clock();
    printf("Read time:  %.3f s (sum=%d)\n", (double)(end - start) / CLOCKS_PER_SEC, sum);

    print_thp_info();
    munmap(p, len);
    return 0;
}
