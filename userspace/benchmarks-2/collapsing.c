#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define NUM_REGIONS 512  // 512 x 2MB = 1GB total
#define REGION_SIZE (2 * 1024 * 1024)  // 2MB per region
#define TOTAL_SIZE (NUM_REGIONS * REGION_SIZE)
#define NUM_ITERATIONS 5000  // Many more iterations for real workload
#define SAMPLE_INTERVAL 1000  // Print stats less frequently

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

// Get AnonHugePages from /proc/meminfo (system-wide)
// Safe to use when THP is disabled (never) - only our process creates THPs via collapser
long get_anon_hugepages_kb() {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    
    char line[256];
    long ahp = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "AnonHugePages:", 14) == 0) {
            sscanf(line + 14, "%ld", &ahp);
            break;
        }
    }
    fclose(f);
    return ahp;
}

// Compute intensive work on the memory regions
unsigned long compute_sum(char *base, size_t len) {
    unsigned long sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += (unsigned char)base[i];
    }
    return sum;
}

// Perform intensive memory operations with real page faults
void perform_memory_ops(char *base, size_t len, int iteration) {
    volatile unsigned long sum = 0;
    
    // Heavy sequential access - triggers real page faults and TLB misses
    for (size_t i = 0; i < len; i += 4096) {  // Touch every page
        sum += (unsigned char)base[i];
        // Also read some inner cache lines to stress TLB
        sum += (unsigned char)base[i + 64];
        sum += (unsigned char)base[i + 128];
        sum += (unsigned char)base[i + 192];
    }
    
    // Strided pattern across 2MB boundaries - benefits greatly from THPs
    for (size_t i = 0; i < len; i += REGION_SIZE) {
        for (size_t j = 0; j < 4096; j += 64) {  // First page of each 2MB region
            sum += (unsigned char)base[i + j];
        }
    }
    
    // Occasional writes to keep pages active
    if (iteration % 100 == 0) {
        for (size_t i = 0; i < len; i += 8192) {  // Every other page
            base[i] = (iteration + i) % 256;
        }
    }
}

int main(int argc, char *argv[]) {
    printf("=== THP Collapsing Benchmark ===\n");
    printf("Configuration:\n");
    printf("  - Total memory: %d MB (%d regions x 2MB)\n", TOTAL_SIZE / (1024*1024), NUM_REGIONS);
    printf("  - Iterations: %d\n", NUM_ITERATIONS);
    printf("  - PID: %d\n", getpid());
    printf("  - Monitoring: /proc/meminfo AnonHugePages\n");
    printf("\n");

    // Allocate memory using mmap with 2MB alignment hint
    // This helps the kernel recognize these as THP candidates
    void *base = mmap(NULL, TOTAL_SIZE, PROT_READ | PROT_WRITE, 
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    
    printf("Memory allocated at: %p\n", base);
    
    // Check if memory is 2MB aligned (helpful for THP collapsing)
    if (((unsigned long)base & (REGION_SIZE - 1)) == 0) {
        printf("✓ Memory is 2MB-aligned (good for THP)\n");
    } else {
        printf("⚠ Memory is NOT 2MB-aligned (offset: 0x%lx)\n", 
               (unsigned long)base & (REGION_SIZE - 1));
    }
    
    printf("THP is disabled initially - pages will be 4KB base pages\n");
    printf("Run with THP collapser to promote these to huge pages\n");
    printf("\n");

    // Initialize memory with a pattern
    printf("Initializing memory with pattern...\n");
    for (size_t i = 0; i < TOTAL_SIZE; i++) {
        ((char*)base)[i] = i % 256;
    }
    
    // Print only first few and last few region addresses to avoid spam
    printf("\nMemory regions (for THP collapser):\n");
    printf("  Region 0: %p\n", base);
    printf("  Region 1: %p\n", base + REGION_SIZE);
    printf("  ...\n");
    printf("  Region %d: %p\n", NUM_REGIONS - 1, base + ((NUM_REGIONS - 1) * REGION_SIZE));
    printf("  (Total: %d regions)\n", NUM_REGIONS);
    printf("\n");

    // Initial memory stats
    long initial_rss = get_rss_kb();
    long initial_ahp = get_anon_hugepages_kb();
    printf("Initial memory state:\n");
    printf("  RSS: %ld KB (%.1f MB)\n", initial_rss, initial_rss / 1024.0);
    printf("  AnonHugePages (system): %ld KB\n", initial_ahp);
    printf("\n");

    // Warm-up: touch all memory to ensure allocation
    printf("Warming up... (allocating physical pages)\n");
    unsigned long warmup_sum = compute_sum((char*)base, TOTAL_SIZE);
    printf("Warmup complete (checksum: %lu)\n", warmup_sum);
    
    long after_warmup_rss = get_rss_kb();
    printf("After warmup RSS: %ld KB (%.1f MB)\n\n", after_warmup_rss, after_warmup_rss / 1024.0);

    // Wait a moment for external tools to attach
    printf("Ready for THP collapser. Waiting 5 seconds...\n");
    printf("(Enable collapser now if not already running)\n");
    sleep(5);

    printf("\n=== Starting Benchmark ===\n\n");
    printf("%8s %12s %12s %15s\n", 
           "Iter", "Time(s)", "AnonHP(KB)", "IterTime(ms)");
    printf("--------------------------------------------------\n");

    double start_time = get_timestamp();
    
    // Track cumulative iteration time for average
    double total_iter_time = 0.0;
    int samples_collected = 0;
    
    // Main benchmark loop - intensive memory workload
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        double iter_start = get_timestamp();
        
        // Perform intensive memory operations
        perform_memory_ops((char*)base, TOTAL_SIZE, iter);
        
        double iter_end = get_timestamp();
        double iter_time_ms = (iter_end - iter_start) * 1000.0;
        total_iter_time += iter_time_ms;
        samples_collected++;
        
        // Periodically print statistics (less frequently for performance)
        if (iter % SAMPLE_INTERVAL == 0 || iter == NUM_ITERATIONS - 1) {
            long ahp = get_anon_hugepages_kb();
            double elapsed = iter_end - start_time;
            double avg_iter_time = total_iter_time / samples_collected;
            
            printf("%8d %12.3f %12ld %15.3f", iter, elapsed, ahp, avg_iter_time);
            
            // Highlight THP promotion
            if (ahp > initial_ahp + 100) {  // Threshold to detect promotion
                printf(" ← THP ACTIVE!");
            }
            printf("\n");
            fflush(stdout);
            
            // Reset for next sampling period
            total_iter_time = 0.0;
            samples_collected = 0;
        }
    }
    
    double end_time = get_timestamp();
    double total_time = end_time - start_time;
    
    printf("\n=== Benchmark Complete ===\n");
    printf("Total time: %.3f seconds\n", total_time);
    printf("Average iteration time: %.3f ms\n", (total_time / NUM_ITERATIONS) * 1000.0);
    printf("Iterations per second: %.2f\n", NUM_ITERATIONS / total_time);
    printf("Memory throughput: %.2f GB/s\n", 
           (TOTAL_SIZE / (1024.0 * 1024.0 * 1024.0)) * NUM_ITERATIONS / total_time);
    
    // Final memory verification
    unsigned long final_sum = compute_sum((char*)base, TOTAL_SIZE);
    printf("\nFinal checksum: %lu\n", final_sum);
    
    long final_rss = get_rss_kb();
    long final_ahp = get_anon_hugepages_kb();
    printf("\nFinal memory state:\n");
    printf("  RSS: %ld KB (%.1f MB)\n", final_rss, final_rss / 1024.0);
    printf("  AnonHugePages: %ld KB (%.1f MB)\n", final_ahp, final_ahp / 1024.0);
    printf("  Delta AnonHugePages: %+ld KB (%.1f MB)\n", 
           final_ahp - initial_ahp, (final_ahp - initial_ahp) / 1024.0);
    
    if (final_ahp > initial_ahp + 100) {
        long promoted_mb = (final_ahp - initial_ahp) / 1024;
        printf("\n✓ SUCCESS: THP collapsing detected!\n");
        printf("  Promoted: %ld MB of huge pages\n", promoted_mb);
        printf("  Coverage: %.1f%% of total memory\n", 
               (final_ahp - initial_ahp) * 100.0 / (TOTAL_SIZE / 1024));
    } else {
        printf("\n○ No THP promotion detected (collapser not active or not effective)\n");
    }

    // Cleanup
    if (munmap(base, TOTAL_SIZE) != 0) {
        perror("munmap failed");
        return 1;
    }

    return 0;
}
