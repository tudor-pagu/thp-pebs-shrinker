#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main() {
    const int N = 100000;
    size_t len = 4096 * 512 * 10; // map 10 would be huge pages.

    void *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    // madvise(p, len, MADV_HUGEPAGE);

    // touch the memory
    for (int i = 0; i < len; i++) {
        ((char*) p)[i] = 'a';
    }
    while(1){};

    if (munmap(p, len) != 0)  {
        perror("munmap");
        return 1;
    }

    return 0;
}
