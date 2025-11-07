I'm going to write parts of linux that I find here that are interesting to us.

- linux/mm/huge_memory.c - seems to be a file where the khugepaged logic happens for THP.


pglist_data seems important
```
 * On NUMA machines, each NUMA node would have a pg_data_t to describe
 * it's memory layout. On UMA machines there is a single pglist_data which
 * describes the whole memory.
 *
 * Memory statistics and page replacement data structures are maintained on a
 * per-zone basis.
 */
typedef struct pglist_data {
```
```

this is super imporant, if we enable transparent hugepage then we have this queue that seems
to encode the splits we need to do?

/*
 * The array of struct pages for flatmem.
 * It must be declared for SPARSEMEM as well because there are configurations
 * that rely on that.
 */
extern struct page *mem_map;

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
struct deferred_split {
	spinlock_t split_queue_lock;
	struct list_head split_queue;
	unsigned long split_queue_len;
};
#endif


folios seem relevant:
 * A folio is a physically, virtually and logically contiguous set
 * of bytes.  It is a power-of-two in size, and it is aligned to that
 * same power-of-two.  It is at least as large as %PAGE_SIZE.  If it is
 * in the page cache, it is at a file offset which is a multiple of that
 * power-of-two.  It may be mapped into userspace at an address which is
 * at an arbitrary page offset, but its kernel virtual address is aligned
 * to its size.
 */
struct folio {


/* partially_mapped=false won't clear PG_partially_mapped folio flag */
void deferred_split_folio(struct folio *folio, bool partially_mapped)

this is where we actually add deffered splits
line 4120: ds_queue->split_queue_len++;



perf_event.h


/**
 * sys_perf_event_open - open a performance event, associate it to a task/cpu
 *
 * @attr_uptr:	event_id type attributes for monitoring/sampling
 * @pid:		target pid
 * @cpu:		target cpu
 * @group_fd:		group leader event fd
 * @flags:		perf event open flags
 */
SYSCALL_DEFINE5(perf_event_open,
		struct perf_event_attr __user *, attr_uptr,
		pid_t, pid, int, cpu, int, group_fd, unsigned long, flags)

in linux/kernel/events/core.c


/**
 * struct perf_event - performance event kernel representation:
 */
struct perf_event {
#ifdef CONFIG_PERF_EVENTS

perf_event.h
