#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define NUM_REGIONS 512  // 2048 x 2MB = 4GB total
#define REGION_SIZE (2 * 1024 * 1024)  // 2MB per region
#define TOTAL_SIZE (NUM_REGIONS * REGION_SIZE)
#define PAGES_PER_REGION 512  // 512 x 4KB = 2MB
#define PAGE_SIZE 4096

// Mode definitions
typedef enum {
    MODE_SPARSE,            // Dense access pattern - benefits from THP collapsing
    MODE_CONCENTRATED,      // Sparse access pattern - benefits from THP splitting
    MODE_INIT_THEN_CONCENTRATE  // First create THPs with dense access, then switch to sparse
} AccessMode;

// Get current timestamp in seconds
double get_timestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Get RSS (Resident Set Size) in KB
long get_rss_kb() {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    
    char line[256];
    long rss = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &rss);
            break;
        }
    }
    fclose(f);
    return rss;
}

// Get VmSize (total virtual memory) in KB
long get_vmsize_kb() {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    
    char line[256];
    long vmsize = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%ld", &vmsize);
            break;
        }
    }
    fclose(f);
    return vmsize;
}

// Get MemFree from /proc/meminfo (system-wide free memory)
long get_memfree_kb() {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    
    char line[256];
    long memfree = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line + 8, "%ld", &memfree);
            break;
        }
    }
    fclose(f);
    return memfree;
}

// Get detailed memory stats
void print_memory_stats(const char *label) {
    long vmsize = get_vmsize_kb();
    long rss = get_rss_kb();
    long memfree = get_memfree_kb();
    
    printf("\n=== %s ===\n", label);
    printf("  VmSize (allocated): %ld KB (%.1f MB)\n", vmsize, vmsize / 1024.0);
    printf("  RSS (resident): %ld KB (%.1f MB)\n", rss, rss / 1024.0);
    printf("  System MemFree: %ld KB (%.1f MB)\n", memfree, memfree / 1024.0);
    printf("\n");
}

// SPARSE MODE: Dense access pattern across entire memory
// This benefits from THP collapsing - accesses span entire 2MB regions
void perform_sparse_access(char *base, size_t len, int iteration) {
    volatile unsigned long sum = 0;
    
    // Touch every page across all regions - high spatial locality
    for (size_t i = 0; i < len; i += PAGE_SIZE) {
        sum += (unsigned char)base[i];
        sum += (unsigned char)base[i + 64];   // Additional cache line
        sum += (unsigned char)base[i + 128];
    }
    
    // Cross 2MB boundaries frequently - benefits from THP
    for (size_t i = 0; i < len; i += REGION_SIZE) {
        for (size_t j = 0; j < PAGE_SIZE * 4; j += 64) {
            sum += (unsigned char)base[i + j];
        }
    }
    
    // Occasional writes
    if (iteration % 100 == 0) {
        for (size_t i = 0; i < len; i += PAGE_SIZE * 2) {
            base[i] = (iteration + i) % 256;
        }
    }
}

// CONCENTRATED MODE: Sparse access pattern - only few pages per 2MB region
// This benefits from THP splitting - most of each 2MB page is unused
void perform_concentrated_access(char *base, size_t len, int iteration) {
    volatile unsigned long sum = 0;
    
    // Only touch 10% of pages in each 2MB region (51 out of 512 pages)
    // This creates sparse THPs that waste memory
    for (size_t region = 0; region < NUM_REGIONS; region++) {
        size_t region_base = region * REGION_SIZE;
        
        // Access only specific pages within each region (concentrated pattern)
        // Pages 0, 10, 20, 30, ... 500 (every 10th page)
        for (int page_offset = 0; page_offset < 512; page_offset += 10) {
            size_t addr = region_base + (page_offset * PAGE_SIZE);
            if (addr >= len) break;
            
            sum += (unsigned char)base[addr];
            sum += (unsigned char)base[addr + 64];
            sum += (unsigned char)base[addr + 128];
            
            // Read from multiple cache lines in the used pages
            for (int cl = 0; cl < 512; cl += 64) {
                sum += (unsigned char)base[addr + cl];
            }
        }
    }
    
    // Occasional writes to active pages only
    if (iteration % 50 == 0) {
        for (size_t region = 0; region < NUM_REGIONS; region++) {
            size_t region_base = region * REGION_SIZE;
            for (int page_offset = 0; page_offset < 512; page_offset += 10) {
                size_t addr = region_base + (page_offset * PAGE_SIZE);
                if (addr >= len) break;
                base[addr] = (iteration + addr) % 256;
            }
        }
    }
}

// Initialize memory with appropriate pattern for each mode
void initialize_memory(char *base, size_t len, AccessMode mode) {
    if (mode == MODE_SPARSE || mode == MODE_INIT_THEN_CONCENTRATE) {
        // Initialize all memory - will use all pages
        // For init_then_concentrate, this creates THPs that will later become sparse
        const char *mode_str = (mode == MODE_SPARSE) ? "SPARSE" : "INIT_THEN_CONCENTRATE";
        printf("Initializing %s mode: touching all pages...\n", mode_str);
        for (size_t i = 0; i < len; i++) {
            base[i] = i % 256;
        }
    } else {
        // Initialize only the pages we'll actually use (10% of each region)
        printf("Initializing CONCENTRATED mode: touching only 10%% of pages per region...\n");
        for (size_t region = 0; region < NUM_REGIONS; region++) {
            size_t region_base = region * REGION_SIZE;
            for (int page_offset = 0; page_offset < 512; page_offset += 10) {
                size_t addr = region_base + (page_offset * PAGE_SIZE);
                if (addr >= len) break;
                
                // Initialize entire page
                for (size_t j = 0; j < PAGE_SIZE; j++) {
                    base[addr + j] = (addr + j) % 256;
                }
            }
        }
    }
}

// Run benchmark for a specific mode
void run_benchmark(char *base, size_t len, AccessMode mode, int num_iterations) {
    const char *mode_name = (mode == MODE_SPARSE) ? "SPARSE" : 
                           (mode == MODE_CONCENTRATED) ? "CONCENTRATED" : 
                           "INIT_THEN_CONCENTRATE";
    const int sample_interval = 1000;
    
    printf("\n=== Running %s Mode Benchmark ===\n", mode_name);
    printf("Expected behavior:\n");
    if (mode == MODE_SPARSE) {
        printf("  - Dense access across all memory\n");
        printf("  - THP collapsing should IMPROVE performance\n");
        printf("  - Memory usage: high (all pages used)\n");
    } else if (mode == MODE_CONCENTRATED) {
        printf("  - Sparse access (10%% of pages per 2MB region)\n");
        printf("  - THP splitting should REDUCE memory waste\n");
        printf("  - Memory usage should DECREASE after splitting\n");
    } else {  // MODE_INIT_THEN_CONCENTRATE
        printf("  - Phase 1 (20%%): Dense access to CREATE THPs via collapser\n");
        printf("  - Phase 2 (80%%): Sparse access (10%% pages) to TRIGGER splitting\n");
        printf("  - Should see: THPs created, then memory freed after split\n");
    }
    printf("\n");
    
    // Initialize memory with appropriate pattern
    initialize_memory(base, len, mode);
    
    long init_rss = get_rss_kb();
    long init_memfree = get_memfree_kb();
    printf("After initialization:\n");
    printf("  RSS: %ld KB (%.1f MB)\n", init_rss, init_rss / 1024.0);
    printf("  System MemFree: %ld KB (%.1f MB)\n", init_memfree, init_memfree / 1024.0);
    
    printf("\nWaiting 5 seconds (enable collapser/splitter now)...\n");
    sleep(5);
    
    printf("\n%8s %12s %12s %12s %12s %15s\n", 
           "Iter", "Time(s)", "VmSize(MB)", "RSS(MB)", "MemFree(MB)", "AvgIter(ms)");
    printf("-------------------------------------------------------------------------\n");
    
    double start_time = get_timestamp();
    double total_iter_time = 0.0;
    int samples_collected = 0;
    
    for (int iter = 0; iter < num_iterations; iter++) {
        double iter_start = get_timestamp();
        
        // Perform access pattern based on mode
        if (mode == MODE_SPARSE) {
            perform_sparse_access(base, len, iter);
        } else if (mode == MODE_CONCENTRATED) {
            perform_concentrated_access(base, len, iter);
        } else {  // MODE_INIT_THEN_CONCENTRATE
            // Phase 1 (first 20%): Dense access to create THPs
            // Phase 2 (last 80%): Sparse access to trigger splitting
            int phase1_iters = num_iterations / 5;  // 20%
            if (iter < phase1_iters) {
                perform_sparse_access(base, len, iter);
                if (iter == phase1_iters - 1) {
                    printf("\n>>> Phase 1 complete - THPs should now be created <<<\n");
                    long mid_rss = get_rss_kb();
                    long mid_memfree = get_memfree_kb();
                    printf(">>> Current state: RSS=%.1f MB, System MemFree=%.1f MB <<<\n", 
                           mid_rss / 1024.0, mid_memfree / 1024.0);
                    printf(">>> PID: %d <<<\n", getpid());
                    printf("\nReady to switch to Phase 2 (sparse access pattern)\n");
                    printf("Press Enter to continue to Phase 2...\n");
                    fflush(stdout);
                    getchar();
                    printf("\n>>> Switching to Phase 2: Sparse access pattern <<<\n");
                    printf(">>> PEBS splitter should now detect and split sparse THPs <<<\n\n");
                }
            } else {
                perform_concentrated_access(base, len, iter);
            }
        }
        
        double iter_end = get_timestamp();
        double iter_time_ms = (iter_end - iter_start) * 1000.0;
        total_iter_time += iter_time_ms;
        samples_collected++;
        
        if (iter % sample_interval == 0 || iter == num_iterations - 1) {
            long vmsize = get_vmsize_kb();
            long rss = get_rss_kb();
            long memfree = get_memfree_kb();
            double elapsed = iter_end - start_time;
            double avg_iter_time = total_iter_time / samples_collected;
            
            printf("%8d %12.3f %12.1f %12.1f %12.1f %15.3f", 
                   iter, elapsed, vmsize / 1024.0, rss / 1024.0, memfree / 1024.0, avg_iter_time);
            
            // Highlight changes
            if (mode == MODE_SPARSE && memfree < init_memfree - 100000) {
                printf(" ← Memory used!");
            } else if (mode == MODE_CONCENTRATED && rss < init_rss - 10000) {
                printf(" ← Memory freed!");
            } else if (mode == MODE_INIT_THEN_CONCENTRATE) {
                int phase1_iters = num_iterations / 20;
                if (iter < phase1_iters && memfree < init_memfree - 100000) {
                    printf(" ← Phase 1: Memory used!");
                } else if (iter >= phase1_iters && rss < init_rss - 10000) {
                    printf(" ← Phase 2: Memory freed!");
                }
            }
            printf("\n");
            fflush(stdout);
            
            total_iter_time = 0.0;
            samples_collected = 0;
        }
    }
    
    double end_time = get_timestamp();
    double total_time = end_time - start_time;
    
    long final_rss = get_rss_kb();
    long final_memfree = get_memfree_kb();
    
    printf("\n=== %s Mode Results ===\n", mode_name);
    printf("Performance:\n");
    printf("  Total time: %.3f seconds\n", total_time);
    printf("  Average iteration: %.3f ms\n", (total_time / num_iterations) * 1000.0);
    printf("  Throughput: %.2f iterations/sec\n", num_iterations / total_time);
    
    long final_vmsize = get_vmsize_kb();
    
    printf("\nMemory Usage:\n");
    printf("  VmSize: %ld KB (%.1f MB) - total allocated\n", final_vmsize, final_vmsize / 1024.0);
    printf("  Initial RSS: %ld KB (%.1f MB)\n", init_rss, init_rss / 1024.0);
    printf("  Final RSS: %ld KB (%.1f MB)\n", final_rss, final_rss / 1024.0);
    printf("  RSS Delta: %+ld KB (%.1f MB)\n", 
           final_rss - init_rss, (final_rss - init_rss) / 1024.0);
    
    // Show what percentage of allocated memory is actually resident
    if (final_vmsize > 0) {
        double resident_pct = (final_rss * 100.0) / final_vmsize;
        printf("  Resident percentage: %.1f%%\n", resident_pct);
    }
    
    printf("\nSystem Memory State:\n");
    printf("  Initial MemFree: %ld KB (%.1f MB)\n", init_memfree, init_memfree / 1024.0);
    printf("  Final MemFree: %ld KB (%.1f MB)\n", final_memfree, final_memfree / 1024.0);
    printf("  MemFree Delta: %+ld KB (%.1f MB)\n", 
           final_memfree - init_memfree, (final_memfree - init_memfree) / 1024.0);
    
    // Mode-specific success criteria
    if (mode == MODE_SPARSE) {
        long memory_consumed = init_memfree - final_memfree;
        if (memory_consumed > 100000) {  // > 100MB consumed from system
            printf("\n✓ SUCCESS: Memory consumed (%.1f MB) - THP collapsing likely occurred!\n",
                   memory_consumed / 1024.0);
            printf("  Performance should be improved due to reduced TLB misses\n");
        } else {
            printf("\n○ No significant system memory change detected\n");
        }
    } else if (mode == MODE_CONCENTRATED || mode == MODE_INIT_THEN_CONCENTRATE) {
        long memory_saved = init_rss - final_rss;
        if (memory_saved > 50000) {  // > 50MB saved
            printf("\n✓ SUCCESS: Significant memory freed!\n");
            printf("  Saved: %.1f MB (%.1f%% reduction)\n", 
                   memory_saved / 1024.0, memory_saved * 100.0 / init_rss);
            printf("  THP splitting likely occurred\n");
            if (mode == MODE_INIT_THEN_CONCENTRATE) {
                printf("  The init phase successfully created THPs, then splitting freed memory\n");
            }
        } else if (memory_saved > 0) {
            printf("\n~ Partial memory reduction: %.1f MB\n", memory_saved / 1024.0);
        } else {
            printf("\n○ No significant memory reduction detected\n");
            printf("  THP splitter may not be active\n");
        }
    }
}

int main(int argc, char *argv[]) {
    AccessMode mode = MODE_SPARSE;  // Default mode
    int num_iterations = 10000;     // Default iterations
    
    // Parse command line arguments
    if (argc >= 2) {
        if (strcmp(argv[1], "sparse") == 0) {
            mode = MODE_SPARSE;
        } else if (strcmp(argv[1], "concentrated") == 0) {
            mode = MODE_CONCENTRATED;
        } else if (strcmp(argv[1], "init_then_concentrate") == 0) {
            mode = MODE_INIT_THEN_CONCENTRATE;
        } else if (strcmp(argv[1], "both") == 0) {
            // Run both modes sequentially
            printf("=== Adaptive THP Benchmark - BOTH MODES ===\n");
            printf("Will run SPARSE mode first, then CONCENTRATED mode\n\n");
        } else {
            fprintf(stderr, "Usage: %s [sparse|concentrated|init_then_concentrate|both] [iterations]\n", argv[0]);
            return 1;
        }
    }
    
    if (argc >= 3) {
        num_iterations = atoi(argv[2]);
        if (num_iterations <= 0) {
            fprintf(stderr, "Invalid iteration count\n");
            return 1;
        }
    }
    
    printf("=== Adaptive THP Benchmark ===\n");
    printf("Configuration:\n");
    printf("  - Total memory: %d MB (%d regions x 2MB)\n", 
           TOTAL_SIZE / (1024*1024), NUM_REGIONS);
    printf("  - Iterations per mode: %d\n", num_iterations);
    printf("  - PID: %d\n", getpid());
    printf("  - Monitoring: /proc/meminfo MemFree and process RSS\n");
    printf("\n");
    
    // Allocate memory
    void *base = mmap(NULL, TOTAL_SIZE, PROT_READ | PROT_WRITE, 
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    
    printf("Memory allocated at: %p\n", base);
    if (((unsigned long)base & (REGION_SIZE - 1)) == 0) {
        printf("✓ Memory is 2MB-aligned\n\n");
    } else {
        printf("⚠ Memory is NOT 2MB-aligned\n\n");
    }
    
    // Run benchmark(s)
    if (argc >= 2 && strcmp(argv[1], "both") == 0) {
        // Run SPARSE mode first
        run_benchmark((char*)base, TOTAL_SIZE, MODE_SPARSE, num_iterations);
        
        printf("\n\n");
        printf("========================================\n");
        printf("Waiting 10 seconds before next mode...\n");
        printf("(Switch from collapser to splitter)\n");
        printf("========================================\n\n");
        sleep(10);
        
        // Run CONCENTRATED mode second
        run_benchmark((char*)base, TOTAL_SIZE, MODE_CONCENTRATED, num_iterations);
    } else {
        // Run single mode
        run_benchmark((char*)base, TOTAL_SIZE, mode, num_iterations);
    }

    // GET CHAR LOOP
    while (1) {
        printf("\nPress Enter to exit the benchmark and clean up memory...\n");
        fflush(stdout);
        getchar();
        break;
    }
    
    // Cleanup
    printf("\n=== Benchmark Complete ===\n");
    if (munmap(base, TOTAL_SIZE) != 0) {
        perror("munmap failed");
        return 1;
    }
    
    return 0;
}
