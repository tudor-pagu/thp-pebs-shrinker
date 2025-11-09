#include "asm/pgtable.h"
#include "linux/mm_types.h"
#include "linux/pagewalk.h"
#include "linux/syscalls.h"
#include "asm/syscall_wrapper.h"
#include "linux/sched.h"


int num_pages_considered = 0;
int num_pages_not_present = 0;
int num_pages_young = 0;

#define HUGE_PAGE_BITS 21 
// we're assuming you have the last 12 bits for the in-page offset (4096), 
// then the 9 bits before that are the offset into the PTE array 

struct page_walk_private {
	long long last_huge_page_nr;
	int num_pages_present;
};

static bool should_promote_huge_page(int num_pages_present) {
	return num_pages_present >= 144; // >= 40% present rate
}

static int promote_huge_page(struct mm_struct *mm, unsigned long address) {
	// TODO: Figure out if its ok to do this while not holding the proc lock, probably it is.
	// Its better to do this with no locks
	struct folio* folio = __folio_alloc(GFP_TRANSHUGE, HPAGE_PMD_ORDER, numa_node_id(), NULL);
	if (!folio) {
		return -ENOMEM;
	}

	folio_put(folio);

	return 0;
}

int on_pte_entry(pte_t *pte, unsigned long addr,
			 unsigned long next, struct mm_walk *walk) {
	struct page_walk_private* private = (struct page_walk_private*)(walk->private);
	u64 pmd_page_number = (addr >> PMD_SHIFT);
	if (pmd_page_number != private->last_huge_page_nr) {
		// deal with last huge page
		if (private->last_huge_page_nr != -1 && should_promote_huge_page(private->num_pages_present)) {
			promote_huge_page(walk->mm, addr);
		}
	}

	num_pages_considered++;

	if (!pte_present(*pte)) {
		num_pages_not_present++;	
		return 0;
	}

	if (pte_young(*pte)) {
		num_pages_young++;

		ptep_test_and_clear_young(walk->vma, addr, pte);
		return 0;
	}

	return 0;
}

struct mm_walk_ops ops = {
	.pte_entry = on_pte_entry
};

SYSCALL_DEFINE0(enable_thp_pebs_shrinking) {
	num_pages_considered = 0;
	num_pages_not_present = 0;
	num_pages_young = 0;

	printk("Enabled syscall tpagu\n");
	struct task_struct* p;
	int vma_cnt = 0;
	int p_count = 0;
	rcu_read_lock(); // I think its needed for process list.
	for_each_process(p) {
		if (!p) {
			continue;
		}

		p_count++;


		struct mm_struct* mm = get_task_mm(p);
		if (!mm) {
			continue;
		}
		mmap_read_lock(mm);

		struct vma_iterator vmi;
		struct vm_area_struct *vma;
		vma_iter_init(&vmi, mm, 0);
		for_each_vma(vmi,vma) {
			vma_cnt++;
			if (vma->vm_flags & (VM_IO|VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP)) {
				// skipped VMA
				continue;
			}
			
			struct page_walk_private* private = kmalloc(sizeof(*private), GFP_KERNEL);
			private->last_huge_page_nr = -1;
			private->num_pages_present = 0;
			walk_page_range(mm, vma->vm_start, vma->vm_end, &ops, private);
			kfree(private);
		}

		mmap_read_unlock(mm);


		mmput(mm);
	}
	rcu_read_unlock();
	// printk("p = %d, ", p_count);
	printk("vma cnt: %d , p count = %d, num_pages_considered = %d, num pages young = %d, num pages not considered = %d\n", vma_cnt, p_count, num_pages_considered, num_pages_young, num_pages_not_present);
	return 0;
}

