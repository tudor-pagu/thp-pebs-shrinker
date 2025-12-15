#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

const int n = 4096 * 512;
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

    while (1) {
        for (int i = 0; i < pg_size; ++i) {
            a[i] = 'a';
        }
    }

    if (munmap(p, n) != 0)  {
        perror("munmap");
        return 1;
    }

    return 0;
}
