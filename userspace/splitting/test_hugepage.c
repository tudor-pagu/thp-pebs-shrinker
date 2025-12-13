#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <strings.h> // for strncasecmp

#define SMAPS_LINES 30

// Waits for the user to press Enter before continuing
void wait_for_enter()
{
    printf("Press Enter to continue...\n");
    while (getchar() != '\n')
        ;
}

// Function to parse /proc/<pid>/smaps and print relevant info for given addr
void print_smaps(pid_t pid, void *addr, const char *label)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/smaps", pid);

    FILE *f = fopen(path, "r");
    if (!f)
    {
        perror("fopen");
        return;
    }

    unsigned long start = (unsigned long)addr;
    char line[256];
    int printing = 0;
    int lines_printed = 0;

    printf("\n--- %s ---\n", label);

    while (fgets(line, sizeof(line), f))
    {
        // The first token in smaps is the memory range: start-end
        unsigned long line_start, line_end;
        if (sscanf(line, "%lx-%lx", &line_start, &line_end) == 2)
        {
            if (start >= line_start && start < line_end)
                printing = 1, lines_printed = 0;
            else
                printing = 0;
        }

        if (printing && lines_printed < SMAPS_LINES)
        {
            printf("%s", line);
            lines_printed++;
            if (strncasecmp(line, "VmFlags:", 8) == 0)
                break;
        }
    }
    fclose(f);
}

int main()
{
    pid_t pid = getpid();
    size_t sz = 2 * 1024 * 1024; // 2MB hugepage

    // STEP 1: Allocate anonymous memory region (2MB) with mmap.
    // This is where we expect a hugepage to be used.
    // Flags explained:
    // PROT_READ | PROT_WRITE: Read and write permissions
    // MAP_PRIVATE | MAP_ANONYMOUS: Private mapping not backed by any file
    unsigned char *ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    // STEP 0: Ask user to check that the thp_split kernel module is loaded.
    // This module is required for splitting hugepages via /proc/thp_split.
    printf("Step 0: Check that THP split module is loaded.\n");
    printf("You should see 'thp_split' in the output:\n");
    system("lsmod | grep thp_split");
    wait_for_enter();

    // STEP 1: Show the address of the allocated memory and check smaps before touching memory.
    // At this point, the memory is not yet backed by physical pages.
    printf("\nStep 1: Allocated 2MB hugepage at %p (PID %d)\n", ptr, pid);
    printf("Step 1a: Before touching memory, check /proc/<pid>/smaps snippet.\n");
    print_smaps(pid, ptr, "smaps before touching memory");
    printf("Explanation: AnonHugePages should be 0 before writing.\n");
    wait_for_enter();

    // STEP 2: Write to the entire memory region to ensure physical allocation.
    // This should trigger the kernel to back the region with a hugepage.
    printf("\nStep 2: Touch memory with pattern 0..255 to ensure allocation.\n");
    for (size_t i = 0; i < sz; i++)
        ptr[i] = i % 256;
    printf("Memory touched with pattern 0..255.\n");
    print_smaps(pid, ptr, "smaps after touching memory");
    printf("Explanation: AnonHugePages should now be 2048 kB, showing a hugepage was used.\n");
    wait_for_enter();

    // STEP 3: Request the kernel module to split the hugepage at the given address.
    // This is done by writing "<pid> <address>" to /proc/thp_split.
    printf("\nStep 3: Execute THP split via module.\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "echo \"%d %p\" | sudo tee /proc/thp_split", pid, ptr);
    printf("Command: %s\n", cmd);
    system(cmd);

    // STEP 4: Show the last kernel message to confirm the split was successful.
    printf("\nStep 4: Check last kernel message confirming split.\n");
    printf("Command: %s\n", "dmesg | tail -1");
    system("dmesg | tail -1");
    printf("Explanation: Should show 'split_huge_page successful'.\n");

    // STEP 5: Check smaps again to confirm that the hugepage was split.
    // AnonHugePages should now be 0 for this region.
    printf("\nStep 5: Display /proc/<pid>/smaps snippet after splitting.\n");
    print_smaps(pid, ptr, "smaps after splitting");
    printf("Explanation: AnonHugePages should now be 0, confirming split.\n");
    wait_for_enter();

    // STEP 6: Verify that the memory contents are unchanged after the split.
    // This ensures that splitting the hugepage did not corrupt data.
    printf("\nStep 6: Verify memory pattern remains intact.\n");
    int ok = 1;
    for (size_t i = 0; i < sz; i++)
    {
        if (ptr[i] != (i % 256))
        {
            ok = 0;
            break;
        }
    }
    printf(ok ? "Memory verification successful! Pattern intact.\n"
              : "Memory mismatch detected!\n");

    printf("Demo complete.\n");
    return 0;
}
