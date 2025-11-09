#include "linux/mm_types.h"
#include "linux/pagewalk.h"
#include "linux/syscalls.h"
#include "asm/syscall_wrapper.h"
#include "linux/sched.h"


int num_pages_considered = 0;
int on_pte_entry(pte_t *pte, unsigned long addr,
			 unsigned long next, struct mm_walk *walk) {
	num_pages_considered++;
	return 0;
}

struct mm_walk_ops ops = {
	.pte_entry = on_pte_entry
};

SYSCALL_DEFINE0(enable_thp_pebs_shrinking) {
	num_pages_considered = 0;
	printk("Enabled syscall tpagu\n");
	struct task_struct* p;
	int vma_cnt = 0;
	int p_count = 0;
	read_lock(&tasklist_lock);
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

			walk_page_range(mm, vma->vm_start, vma->vm_end, &ops, NULL);
		}

		mmap_read_unlock(mm);
	}
	read_unlock(&tasklist_lock);
	// printk("p = %d, ", p_count);
	printk("vma cnt: %d , p count = %d, num_pages_considered = %d\n", vma_cnt, p_count, num_pages_considered);
	return 0;
}

