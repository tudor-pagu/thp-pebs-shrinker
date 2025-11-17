#include "asm/page.h"
#include "linux/delay.h"
#include "linux/highmem.h"
#include "linux/kthread.h"
#include "linux/llist.h"
#include <linux/huge_mm.h>
#include "linux/mm.h"
#include "linux/mm_types.h"
#include "linux/pagewalk.h"
#include "linux/seqlock.h"
#include "linux/syscalls.h"
#include "asm/syscall_wrapper.h"
#include "linux/sched.h"
#include <asm/tlb.h>
#include "internal.h"
#include "thp_pebs_shrinker.h"

// int num_pages_considered = 0;
// int num_pages_not_present = 0;
// int num_pages_young = 0;
//
// #define HUGE_PAGE_BITS 21
// we're assuming you have the last 12 bits for the in-page offset (4096),
// then the 9 bits before that are the offset into the PTE array

// struct page_walk_private {
// 	long long last_huge_page_nr;
// 	int num_pages_present;
// 	struct xarray huge_pages_to_collapse;
// };
//
// static bool should_promote_huge_page(int num_pages_present)
// {
// 	return num_pages_present >= 144; // >= 40% present rate
// }
//
//
// static int promote_huge_page(struct mm_struct *mm, struct vm_area_struct *vma,
// 			     unsigned long address)
// {
// 	address = address & HPAGE_PMD_MASK;
// 	// we reuse the implementation from khugepaged
// 	int ret = thp_collapse_anonymous_pmd(mm, address);
// 	if (ret == 0) {
// 		printk("promoted huge page for real\n");
// 	}
// 	return ret;
// }
//
// //TODO make this make sense
// int on_pte_entry(pte_t *pte, unsigned long addr, unsigned long next,
// 		 struct mm_walk *walk)
// {
// 	struct page_walk_private *private =
// 		(struct page_walk_private *)(walk->private);
// 	u64 pmd_page_number = (addr >> PMD_SHIFT);
// 	if (pmd_page_number != private->last_huge_page_nr) {
// 		// deal with last huge page
// 		if (private->last_huge_page_nr != -1 &&
// 		    should_promote_huge_page(private->num_pages_present)) {
// 			// store this in the xarray for later splitting
// 			xa_store(&private->huge_pages_to_collapse,
// 				 private->last_huge_page_nr,
// 				 xa_mk_value((private->last_huge_page_nr
// 					      << PMD_SHIFT)),
// 				 GFP_ATOMIC);
// 			// promote_huge_page(walk->mm, walk->vma, addr);
// 		}
// 		private->last_huge_page_nr = pmd_page_number;
// 		private->num_pages_present = 1;
// 	} else {
// 		private->num_pages_present++;
// 	}
//
// 	num_pages_considered++;
//
// 	if (!pte_present(*pte)) {
// 		num_pages_not_present++;
// 		return 0;
// 	}
//
// 	if (pte_young(*pte)) {
// 		num_pages_young++;
//
// 		ptep_test_and_clear_young(walk->vma, addr, pte);
// 		return 0;
// 	}
//
// 	return 0;
// }
//
// int on_pmd_entry(pmd_t *pmd, unsigned long addr, unsigned long next,
// 		 struct mm_walk *walk)
// {
// 	struct page_walk_private *private =
// 		(struct page_walk_private *)(walk->private);
// 	// ignore pmd entries! we DONT want to break up existing THPs while running this.
// 	if (pmd_huge_pte(mm, pmd)) {
// 		walk->action = ACTION_CONTINUE;
// 	} else {
// 		walk->action = ACTION_SUBTREE;
// 	}
// 	return 0;
// }
//
// struct mm_walk_ops ops = {
// 	.pte_entry = on_pte_entry,
// 	.pmd_entry =
// 		on_pmd_entry // if we dont provide a PMD handler, THPs are broken down by the page walk.
// };

// SYSCALL_DEFINE0(enable_thp_pebs_shrinking)
// {
// 	num_pages_considered = 0;
// 	num_pages_not_present = 0;
// 	num_pages_young = 0;
//
// 	printk("Enabled syscall tpagu\n");
// 	struct task_struct *p;
// 	int vma_cnt = 0;
// 	int result = 0;
// 	int p_count = 0;
// 	rcu_read_lock(); // I think its needed for process list.
// 	for_each_process(p) {
// 		if (!p) {
// 			continue;
// 		}
//
// 		p_count++;
//
// 		struct mm_struct *mm = get_task_mm(p);
// 		if (!mm) {
// 			continue;
// 		}
// 		mmap_read_lock(mm);
//
// 		struct vma_iterator vmi;
// 		struct vm_area_struct *vma;
// 		vma_iter_init(&vmi, mm, 0);
// 		for_each_vma(vmi, vma) {
// 			vma_cnt++;
// 			if (vma->vm_flags &
// 			    (VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP)) {
// 				// skipped VMA
// 				continue;
// 			}
//
// 			struct page_walk_private *private =
// 				kmalloc(sizeof(*private), GFP_KERNEL);
// 			if (!private) {
// 				result = -ENOMEM;
// 				goto unlock;
// 			}
//
// 			private->last_huge_page_nr = -1;
// 			private->num_pages_present = 0;
// 			xa_init(&private->huge_pages_to_collapse);
//
// 			walk_page_range(mm, vma->vm_start, vma->vm_end, &ops,
// 					private);
//
// 			unsigned long ind;
// 			void *entry;
// 			xa_for_each(&private->huge_pages_to_collapse, ind,
// 				    entry) {
// 				if (xa_is_value(entry)) {
// 					promote_huge_page(mm, vma,
// 							  xa_to_value(entry));
// 				}
// 			}
//
// 			xa_destroy(&private->huge_pages_to_collapse);
// 			kfree(private);
// 		}
//
// 		mmap_read_unlock(mm);
//
// 		mmput(mm);
// 	}
// unlock:
// 	rcu_read_unlock();
// 	// printk("p = %d, ", p_count);
// 	printk("vma cnt: %d , p count = %d, num_pages_considered = %d, num pages young = %d, num pages not considered = %d\n",
// 	       vma_cnt, p_count, num_pages_considered, num_pages_young,
// 	       num_pages_not_present);
// 	return result;
// }


struct utilization_bit_vector {
	DECLARE_BITMAP(bitmap, 512);
};

struct pmd_to_split_deferred_node {
	unsigned long addr;
	struct mm_struct* mm;
	struct llist_node node;
};

static LLIST_HEAD(pmd_to_split_deferred_list);

enum poll_result {
	SUCCESS,
	BACK_OFF,
};

static int poll_split_list_once(void) {
	if (llist_empty(&pmd_to_split_deferred_list)) {
		return BACK_OFF; // back off
	}

	struct llist_node* first = llist_del_first(&pmd_to_split_deferred_list);
	struct pmd_to_split_deferred_node* first_node = container_of(first, struct pmd_to_split_deferred_node, node);
	thp_collapse_anonymous_pmd(first_node->mm, first_node->addr);
	kfree(first_node);
	return SUCCESS;
}

static int thp_collapser_thread(void *data) {
	while(!kthread_should_stop()) {
		int result = poll_split_list_once();
		if (result == BACK_OFF) {
			msleep(10);
		}
	}
	return 0;
}

// we should not be in atomic context here... but we probably are holding
// mmap_lock
int record_page_fault_for_thp_shrinking(struct mm_struct* mm, unsigned long addr, int nr_pages) {
	// I'm assuming this can't ever happen. If it does happen,
	// I want to know about it
	BUG_ON(nr_pages > 512);
	
	u64 pmd_page_number = (addr >> PMD_SHIFT);
	void* entry = xa_load(mm->thp_usage, pmd_page_number);
	struct utilization_bit_vector* bit_vector;
	if (entry == NULL) {
		bit_vector = kmalloc(sizeof(*bit_vector), GFP_KERNEL);
		if (!bit_vector) {
			return -ENOMEM;
		}
		xa_store(mm->thp_usage, pmd_page_number, bit_vector, GFP_KERNEL);
	} else {
		bit_vector = (struct utilization_bit_vector*)entry;
	}

	// address is like:
	// | ... | 9 bits of offset within PTE | 12 bits of offset within Page
	// this shuold be the index of the page we are within the PTE page, 
	// so from 0 to 511 inclusive.
	int base_page_within_pmd = ((addr & ((1 << 21) - 1) ) >> 12);
	BUG_ON(base_page_within_pmd < 0 || base_page_within_pmd >= 512);

	// set the appropiate bits atomically!
	for (int i = 0; i < nr_pages; i++) {
		set_bit(base_page_within_pmd + i, bit_vector->bitmap);
	}

	// very simple criterion
	int weight = bitmap_weight(bit_vector->bitmap, 512);
	if (weight >= 256) {
		struct pmd_to_split_deferred_node* node = kmalloc(sizeof(*node), GFP_KERNEL);
		if (!node) {
			return -ENOMEM;
		}
		node->mm = mm;
		node->addr = addr;
		llist_add(&node->node, &pmd_to_split_deferred_list);
	}

	return 0;
}


struct task_struct* collapser_thread;
static DEFINE_MUTEX(collapser_mutex);

SYSCALL_DEFINE0(enable_thp_pebs_shrinking)
{
	static bool thread_started = false;
	int ret = 0;

	mutex_lock(&collapser_mutex);

	if (thread_started) {
		thread_started = false;
		kthread_stop(collapser_thread);
		ret = -1;
	} else {
		thread_started = true;
		collapser_thread = kthread_run(thp_collapser_thread, NULL, "thp_collapser_thread");
		ret = 0;
	}

	mutex_unlock(&collapser_mutex);
	return ret;
}


