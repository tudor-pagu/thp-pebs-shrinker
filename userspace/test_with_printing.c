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
  //  madvise(p, len, MADV_HUGEPAGE);

    // touch the memory
    for (int i = 0; i < len; i++) { // only touch first 10 pages
        ((char*) p)[i] = (i % 256);
        if ( (i & ( (1 << 21) - 1)) == 0 ) {
                printf("looking at address %llu\n", (unsigned long)(p + i));
        }
    }
    while(1){
            sleep(1);
            unsigned long s = 0;
            for(int i = 0; i < len; i++) {
                    s += ((char*)p)[i];
            }
            printf("sum s = %d\n", s);
    };

    if (munmap(p, len) != 0)  {
        perror("munmap");
        return 1;
    }

    return 0;
}
