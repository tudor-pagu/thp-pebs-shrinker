#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

const int n = 4096 * 512 * 10;
const int pg_size = 4096;
const int huge_pg_size = 4096 * 512;

int main() {
    void *p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    madvise(p, n, MADV_HUGEPAGE);

    char* a = p;

    for (int repeat = 0; repeat < 100; ++repeat) {
        for (int j = 0; j < 5; ++j) {
            for (int i = huge_pg_size * j; i < huge_pg_size * (j + 1); ++i) {
                a[i] = 'a';
            }
        }
        for (int j = 5; j < 10; ++j) {
            for (int i = huge_pg_size * j; i < huge_pg_size * j + 256 * pg_size; ++i) {
                a[i] = 'a';
            }
        }
    }

    // print some addresses to check if necessary
    {
        int j = 9;
        for (int i = huge_pg_size * j; i < huge_pg_size * j + 10 * pg_size; i += pg_size) {
            printf("%p\n", &(a[i]));
        }
    }

    while (1) {}

    if (munmap(p, n) != 0)  {
        perror("munmap");
        return 1;
    }

    return 0;
}
