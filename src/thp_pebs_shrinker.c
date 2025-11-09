#include "linux/container_of.h"
#include "linux/drbd.h"
#include "linux/fs.h"
#include "linux/gfp_types.h"
#include "linux/pagemap.h"
#include "linux/input.h"
#include "linux/kallsyms.h"
#include "linux/list.h"
#include "linux/mmzone.h"
#include "linux/perf_event.h"
#include "linux/rculist.h"
#include "linux/rcupdate.h"
#include "linux/spinlock.h"
#include <linux/hashtable.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

// somehow we will decide to split a huge page (based on statistics we collect)
static bool should_split_huge_page(struct page *page) { return true; }

struct pglist_data * (*first_online_pgdat_stub)(void); 

static void thp_pebs_shrinker(void *none) {
}

// good for experimentation and avoiding kernel recompiling
struct zone * (*next_zone_stub) (struct zone *zone);

struct page * (*pfn_to_online_page_stub) (unsigned long pfn);

void (*__folio_lock_stub) (struct folio *);
void (*folio_unlock_stub) (struct folio *);
int (*split_folio_to_list_stub)(struct folio *folio, struct list_head *list);

#define next_zone next_zone_stub
#define split_folio_to_list split_folio_to_list_stub

// copy and pasted from mm/huge_memory.c
static void split_huge_pages_all(void)
{
	struct zone *zone;
	struct page *page;
	struct folio *folio;
	unsigned long pfn, max_zone_pfn;
	unsigned long total = 0, split = 0;

	printk("Split all THPs\n");
	for_each_zone(zone) {
		if (!managed_zone(zone))
			continue;
		max_zone_pfn = zone_end_pfn(zone);
		for (pfn = zone->zone_start_pfn; pfn < max_zone_pfn; pfn++) {
			int nr_pages;

			page = pfn_to_online_page(pfn);
			if (!page || PageTail(page))
				continue;
			folio = page_folio(page);
			if (!folio_try_get(folio))
				continue;

			if (unlikely(page_folio(page) != folio))
				goto next;

			if (zone != folio_zone(folio))
				goto next;

			if (!folio_test_large(folio)
				|| folio_test_hugetlb(folio)
				|| !folio_test_lru(folio))
				goto next;

			total++;
			folio_lock(folio);
			nr_pages = folio_nr_pages(folio);
			if (!split_folio(folio))
				split++;
			pfn += nr_pages - 1;
			folio_unlock(folio);
next:
			folio_put(folio);
			cond_resched();
		}
	}

	printk("%lu of %lu THP split\n", split, total);
}

static void scan_vmas_for_hotness(void) {
    
	struct vma_iterator vmi;
	struct vm_area_struct *vma;
	struct mm_slot* mm = mm_slot_alloc(mm_slot_cache);
	vma_iter_init(&vmi, mm, 0);
    for_each_vma(vmi, vma) {

    }
}


#define ALL_STORES 0x82d0
static struct perf_event_attr wd_hw_attr = {
	.type		= PERF_TYPE_SOFTWARE,
	.config		= PERF_COUNT_SW_CPU_CLOCK,
	.size		= sizeof(struct perf_event_attr),
	.pinned		= 1,
	.disabled	= 1,
};


static void watchdog_overflow_callback(struct perf_event *event,
				       struct perf_sample_data *data,
				       struct pt_regs *regs) {
    printk("got called from pebs!\n"); }

static void register_pebs(void) {
    unsigned int cpu;
	struct perf_event_attr *wd_attr;
	struct perf_event *evt;

    cpu = raw_smp_processor_id();
	wd_attr = &wd_hw_attr;
	wd_attr->sample_period = 1000;
	evt = perf_event_create_kernel_counter(wd_attr, cpu, NULL,
					       watchdog_overflow_callback, NULL);
    if (IS_ERR(evt)) {
        printk("failed to create event...\n");
        long err = PTR_ERR(evt);
        pr_err("perf: create failed cpu=%u err=%ld\n", cpu, err);
        return;
	} else {
        printk("created event!\n");
    }

}


static int __init mod_init(void) {
    // first_online_pgdat_stub = (struct pglist_data *(*)(void))kallsyms_lookup_name("first_online_pgdat");
    struct pglist_data* pgdat;
    next_zone_stub = (struct zone *(*)(struct zone *))kallsyms_lookup_name("next_zone");
    pfn_to_online_page_stub = (struct page *(*)(unsigned long)) kallsyms_lookup_name("pfn_to_online_page");
    __folio_lock_stub = (void (*)(struct folio *) )kallsyms_lookup_name("__folio_lock");
    folio_unlock_stub = (void (*)(struct folio *) )kallsyms_lookup_name("folio_unlock");
    split_folio_to_list_stub = (int (*)(struct folio *, struct list_head *))kallsyms_lookup_name("split_folio_to_list");

    printk("loaded module...\n");
    split_huge_pages_all();
    printk("split everything...\n");
    register_pebs();

    return 0;
}

static void __exit mod_exit(void) { return; }

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL"); // NON-GPL cannot access GPL-only symbols
MODULE_AUTHOR("EPFL CS477");
MODULE_DESCRIPTION("A module for the project.");
MODULE_VERSION("1.0");
