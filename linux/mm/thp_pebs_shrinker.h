#ifndef MM_THP_PEBS_SHRINKER
#define MM_THP_PEBS_SHRINKER

#include "linux/mm_types.h"

/**
 * This function gets called from the page fault handler, so it must threadsafe and fast.
 * It should just atomically update the utilization bit vectors.
 * vma is the vma where we mapped these pages
 * address is the beginning virutal address of the folio we map
 * nr_pages is the number of 4KB pages we mapped (its possible for one PTE page fault to map multiple pages)
*/
int record_page_fault_for_thp_shrinking(struct mm_struct* mm, unsigned long addr, int nr_pages);

#endif
