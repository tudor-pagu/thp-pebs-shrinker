#include "asm/page.h"
#include "asm/pgtable.h"
#include "linux/highmem.h"
#include "linux/mm.h"
#include "linux/mm_types.h"
#include "linux/pagewalk.h"
#include "linux/seqlock.h"
#include "linux/syscalls.h"
#include "asm/syscall_wrapper.h"
#include "linux/sched.h"
#include <asm/tlb.h>
#include "internal.h"


int num_pages_considered = 0;
int num_pages_not_present = 0;
int num_pages_young = 0;

#define HUGE_PAGE_BITS 21 
// we're assuming you have the last 12 bits for the in-page offset (4096), 
// then the 9 bits before that are the offset into the PTE array 

struct page_walk_private {
	long long last_huge_page_nr;
	int num_pages_present;
	struct xarray huge_pages_to_collapse;
};

static bool should_promote_huge_page(int num_pages_present) {
	return num_pages_present >= 144; // >= 40% present rate
}

static bool hugepage_vma_revalidate(struct mm_struct *mm, unsigned long address)
{
	struct vm_area_struct *vma;
	vma = find_vma(mm, address);
	if (!vma)
		return false;

	return true;
}

static int promote_huge_page(struct mm_struct *mm, struct vm_area_struct* vma, unsigned long address) {
	printk("trying to promote huge page at VMA address =  %lu\n", address);
	// TODO: Figure out if its ok to do this while not holding the proc lock, probably it is.
	// Its better to do this with no locks
	
	// align the address up to huge page boundary 
	address = address & HPAGE_PMD_MASK;
	struct folio* folio = __folio_alloc(GFP_TRANSHUGE, HPAGE_PMD_ORDER, numa_node_id(), NULL);
	int result = 0;
	spinlock_t* pmd_ptl;
	struct mmu_notifier_range range;
	
	if (!folio) {
		result = -ENOMEM;
		goto ret;
	}

	spinlock_t* p_lock;
	pte_t* pte = get_locked_pte(mm, address, &p_lock);
	if (!pte) {
		result = -ENOENT;
		goto folio_put;
	}

	pte_t* _pte;
	unsigned long _address;

	int num_zero_or_swapped_or_none = 0;
	int total = 0;
	int index;

	printk("got to for loop! base VMA = %lu", address);
	for (index = 0, _address = address, _pte = pte; _pte < pte + HPAGE_PMD_NR; _pte++, _address += PAGE_SIZE, index++ ) {
		total++;

		pte_t pteval = ptep_get(_pte);
		if (!pte_present(pteval) || pte_none(pteval) || is_zero_pfn(pte_pfn(pteval))) {
			num_zero_or_swapped_or_none ++;
		}
		struct page* normal_page = vm_normal_page(vma, _address,pteval);
		if (!normal_page || is_zone_device_page(normal_page)) {
			result = -ENOENT;
			goto unlock;
		}

		struct folio* normal_page_folio = page_folio(normal_page);
		VM_BUG_ON_FOLIO(!folio_test_anon(normal_page_folio), normal_page_folio);

		if (folio_maybe_mapped_shared(normal_page_folio)) {
			// lets not bother with shared pages now
			result = -ENOENT;
			goto unlock;
		}

		struct page* to_page = folio_page(folio, index);
		copy_highpage(to_page, normal_page);
	}

	mmap_read_unlock(mm);
	mmap_write_lock(mm);
	if (!hugepage_vma_revalidate(mm, address)) {
		result = -ENOENT;
		goto write_unlock;
	}
	
	pmd_t *pmd = mm_find_pmd(mm, address);
	pmd_t _pmd;

	if (!pmd) {
		result = -ENOENT;
		goto write_unlock;
	}

	if (!vma->anon_vma) {
		printk("no anon_vma.. stopping early\n");
		result = -ENOENT;
		goto write_unlock;
	}

	vma_start_write(vma);
	anon_vma_lock_write(vma->anon_vma);

	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, mm, address, address + HPAGE_PMD_SIZE);
	mmu_notifier_invalidate_range_start(&range);

	pmd_ptl = pmd_lock(mm, pmd);
	if (!pmd_ptl) {
		result = -ENOENT;
		mmu_notifier_invalidate_range_end(&range);
		goto write_unlock;
	}

	/* From khugepaged.c :
	 * This removes any huge TLB entry from the CPU so we won't allow
	 * huge and small TLB entries for the same virtual address to
	 * avoid the risk of CPU bugs in that area.
	 *
	 * Parallel GUP-fast is fine since GUP-fast will back off when
	 * it detects PMD is changed.
	 */
	_pmd = pmdp_collapse_flush(vma, address, pmd); // old pmd. we can use it to unmap PTEs, but now we just leak.
	spin_unlock(pmd_ptl);
	mmu_notifier_invalidate_range_end(&range);

	tlb_remove_table_sync_one();

	pmd_t _new_pmd = mk_huge_pmd(&folio->page, vma->vm_page_prot);
	_new_pmd = maybe_pmd_mkwrite(pmd_mkdirty(_new_pmd), vma);

	spin_lock(pmd_ptl);
	BUG_ON(!pmd_none(*pmd));
	folio_add_new_anon_rmap(folio, vma, address, RMAP_EXCLUSIVE);
	folio_add_lru_vma(folio, vma);
	pgtable_t pgtable = pmd_pgtable(_pmd);
	pgtable_trans_huge_deposit(mm, pmd, pgtable);
	set_pmd_at(mm, address, pmd, _pmd);
	update_mmu_cache_pmd(vma, address, pmd);
	deferred_split_folio(folio, false);
	spin_unlock(pmd_ptl);
	
	folio = NULL;
	
	anon_vma_unlock_write(vma->anon_vma); // once we moved out PTEs we can move this up I think.


	
write_unlock:
	mmap_write_unlock(mm);
	mmap_read_lock(mm);


	// printk("trying to promote huge page! num zero or swapped = %d , total = %d\n", num_zero_or_swapped_or_none, total);

unlock:
	pte_unmap_unlock(pte, p_lock);

folio_put:
	// deallocate folio 
	if (folio) {
		folio_put(folio);
	}
ret:
	if (result == 0) {
		printk("got to critical section well!\n");
	}
	return result;
}

//TODO make this make sense
int on_pte_entry(pte_t *pte, unsigned long addr,
			 unsigned long next, struct mm_walk *walk) { struct page_walk_private* private = (struct page_walk_private*)(walk->private);
	u64 pmd_page_number = (addr >> PMD_SHIFT);
	if (pmd_page_number != private->last_huge_page_nr) {
		// deal with last huge page
		if (private->last_huge_page_nr != -1 && should_promote_huge_page(private->num_pages_present)) {
			// store this in the xarray for later splitting
			xa_store(&private->huge_pages_to_collapse,private->last_huge_page_nr, xa_mk_value( (private->last_huge_page_nr << PMD_SHIFT) ), GFP_ATOMIC);
			// promote_huge_page(walk->mm, walk->vma, addr);
		}
		private->last_huge_page_nr = pmd_page_number;
		private->num_pages_present = 1;
	} else {
		private -> num_pages_present ++;
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

int on_pmd_entry(pmd_t *pmd, unsigned long addr,
			 unsigned long next, struct mm_walk *walk) { struct page_walk_private* private = (struct page_walk_private*)(walk->private);
	// ignore pmd entries! we DONT want to break up existing THPs while running this.
	if (pmd_huge_pte(mm, pmd)) {
		walk->action = ACTION_CONTINUE;
	} else {
		walk->action = ACTION_SUBTREE;
	}
	return 0;
}

struct mm_walk_ops ops = {
	.pte_entry = on_pte_entry,
	.pmd_entry = on_pmd_entry // if we dont provide a PMD handler, THPs are broken down by the page walk.
};

SYSCALL_DEFINE0(enable_thp_pebs_shrinking) {
	num_pages_considered = 0;
	num_pages_not_present = 0;
	num_pages_young = 0;

	printk("Enabled syscall tpagu\n");
	struct task_struct* p;
	int vma_cnt = 0;
	int result = 0;
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
			if (!private) {
				result = -ENOMEM;
				goto unlock;

			}

			private->last_huge_page_nr = -1;
			private->num_pages_present = 0;
			xa_init(&private->huge_pages_to_collapse);

			walk_page_range(mm, vma->vm_start, vma->vm_end, &ops, private);

			unsigned long ind;
			void* entry;
			xa_for_each(&private->huge_pages_to_collapse, ind,  entry) {
				if (xa_is_value(entry)) {
					printk("collapsing entry at virtual address %lu ...", xa_to_value(entry));
					promote_huge_page(mm, vma, xa_to_value(entry));
				}
			}

			kfree(private);
		}

		mmap_read_unlock(mm);


		mmput(mm);
	}
unlock:
	rcu_read_unlock();
	// printk("p = %d, ", p_count);
	printk("vma cnt: %d , p count = %d, num_pages_considered = %d, num pages young = %d, num pages not considered = %d\n", vma_cnt, p_count, num_pages_considered, num_pages_young, num_pages_not_present);
	return result;
}

