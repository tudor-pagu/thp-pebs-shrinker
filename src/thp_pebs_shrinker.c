#include "linux/container_of.h"
#include "linux/drbd.h"
#include "linux/fs.h"
#include "linux/gfp_types.h"
#include "linux/pagemap.h"
#include "linux/input.h"
#include "linux/kallsyms.h"
#include "linux/list.h"
#include "linux/mmzone.h"
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

    // for_each_online_pgdat(pgdat) {
    //     printk("please node id = %d\n", pgdat->node_id);
    //     printk("number of zones = %d\n", pgdat->nr_zones);
    //     struct zone* zone;
    //
    //     int num_pages_considered = 0;
    //     int num_huge_pages_considered = 0;
    //     for (zone = pgdat->node_zones; zone; zone = next_zone_stub(zone)) {
    //         unsigned long start_pfn = zone->zone_start_pfn;
    //         unsigned long end_pfn = start_pfn + zone->spanned_pages;
    //         for (unsigned long pfn = start_pfn; pfn < end_pfn; pfn++) {
    //             struct page* page = pfn_to_online_page_stub(pfn);
    //             if (!page) {
    //                 continue;
    //             }
    //             num_pages_considered++;
    //
    //             struct folio *f = page_folio(page);
    //             if(folio_test_large(f) && folio_test_anon(f)) {
    //                 printk("splitting folio at pfn %lu\n", page_to_pfn(&f->page));
    //                 __folio_lock_stub(f);
    //                 split_huge_page(page);
    //        folio_unlock_stub(f);
    //                 num_huge_pages_considered++;
    //                 // printk("num pages: %du start pfn: %lu \n", f->_nr_pages, folio_pfn(f));
    //             }
    //             // printk("looking at online page with pfn %lu", pfn);
    //             // now we're looking at some physical 
    //         }
    //
    //         // printk("looking at pages from %lu to %lu", start_pfn, end_pfn);
    //         // if (zone->present_pages) {
    //         //     printk("looking at zone with %lu present pages", zone->present_pages);
    //         // }
    //     }
    //     printk(" in total, considered %d pages out of which %d were huge (THP)", num_pages_considered, num_huge_pages_considered);
    // }
    //
    // printk("----finish----\n");
    // thp_pebs_shrinker(NULL);
    return 0;
}

static void __exit mod_exit(void) { return; }

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL"); // NON-GPL cannot access GPL-only symbols
MODULE_AUTHOR("EPFL CS477");
MODULE_DESCRIPTION("A module for the project.");
MODULE_VERSION("1.0");
