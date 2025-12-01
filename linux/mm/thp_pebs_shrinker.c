#include "asm/page.h"
#include "linux/delay.h"
#include "linux/highmem.h"
#include "linux/kthread.h"
#include "linux/llist.h"
#include "linux/sysfs.h"
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
	struct mm_struct *mm;
	struct llist_node node;
};

static LLIST_HEAD(pmd_to_split_deferred_list);

enum poll_result {
	SUCCESS,
	BACK_OFF,
};

static int poll_split_list_once(void)
{
	if (llist_empty(&pmd_to_split_deferred_list)) {
		return BACK_OFF; // back off
	}

	struct llist_node *first = llist_del_first(&pmd_to_split_deferred_list);
	struct pmd_to_split_deferred_node *first_node =
		container_of(first, struct pmd_to_split_deferred_node, node);
	thp_collapse_anonymous_pmd(first_node->mm, first_node->addr);
	kfree(first_node);
	return SUCCESS;
}

static int thp_collapser_thread(void *data)
{
	while (!kthread_should_stop()) {
		int result = poll_split_list_once();
		if (result == BACK_OFF) {
			msleep(10);
		}
	}
	return 0;
}

// we should not be in atomic context here... but we probably are holding
// mmap_lock
int record_page_fault_for_thp_shrinking(struct mm_struct *mm,
					unsigned long addr, int nr_pages)
{
	// I'm assuming this can't ever happen. If it does happen,
	// I want to know about it
	BUG_ON(nr_pages > 512);

	u64 pmd_page_number = (addr >> PMD_SHIFT);
	void *entry = xa_load(mm->thp_usage, pmd_page_number);
	struct utilization_bit_vector *bit_vector;
	if (entry == NULL) {
		bit_vector = kmalloc(sizeof(*bit_vector), GFP_KERNEL);
		if (!bit_vector) {
			return -ENOMEM;
		}
		xa_store(mm->thp_usage, pmd_page_number, bit_vector,
			 GFP_KERNEL);
	} else {
		bit_vector = (struct utilization_bit_vector *)entry;
	}

	// address is like:
	// | ... | 9 bits of offset within PTE | 12 bits of offset within Page
	// this shuold be the index of the page we are within the PTE page,
	// so from 0 to 511 inclusive.
	int base_page_within_pmd = ((addr & ((1 << 21) - 1)) >> 12);
	BUG_ON(base_page_within_pmd < 0 || base_page_within_pmd >= 512);

	// set the appropiate bits atomically!
	for (int i = 0; i < nr_pages; i++) {
		set_bit(base_page_within_pmd + i, bit_vector->bitmap);
	}

	// very simple criterion
	int weight = bitmap_weight(bit_vector->bitmap, 512);
	if (weight >= 256) {
		struct pmd_to_split_deferred_node *node =
			kmalloc(sizeof(*node), GFP_KERNEL);
		if (!node) {
			return -ENOMEM;
		}
		node->mm = mm;
		node->addr = addr;
		llist_add(&node->node, &pmd_to_split_deferred_list);
	}

	return 0;
}

struct task_struct *collapser_thread = NULL;
static DEFINE_MUTEX(collapser_mutex);

static int thp_promoter_enable(void)
{
	int ret = 0;
	struct task_struct *t;

	// hold the lock only while accessing the shared data, collapser_mutex
	mutex_lock(&collapser_mutex);
	// someone else already enabled the thread before us, let's give up.
	if (collapser_thread != NULL) {
		ret = -EINVAL;
		goto unlock;
	}
	
	t = kthread_run(thp_collapser_thread, NULL, "thp_collapser_thread");
	if (IS_ERR(t)) {
		ret = PTR_ERR(t);
		pr_err("Error starting thp_collapser_thread: %d\n", ret);
	} else {
		collapser_thread = t;
	}

unlock:
	mutex_unlock(&collapser_mutex);
	return ret;
}

static void thp_promoter_disable(void)
{
	struct task_struct *t;

	mutex_lock(&collapser_mutex);
	t = collapser_thread;
	collapser_thread = NULL;
	mutex_unlock(&collapser_mutex);

	// we'd rather wait for the thread to stop while not holding the mutex.
	if (t) {
		kthread_stop(t);
	}
}

// --- PEBS START ----
#define ALL_STORES 0x82d0
static struct perf_event_attr wd_hw_attr = {
	.type = PERF_TYPE_SOFTWARE,
	.config = PERF_COUNT_SW_CPU_CLOCK,
	.size = sizeof(struct perf_event_attr),
	.pinned = 1,
	.disabled = 1,
};

static void watchdog_overflow_callback(struct perf_event *event,
				       struct perf_sample_data *data,
				       struct pt_regs *regs)
{
	printk("got called from pebs!\n");
}

static void register_pebs(void)
{
	static bool registered = false;
	if (registered) {
		printk("tpagu debug: skipping registering pebs event, already registered...\n");
		return;
	}

	unsigned int cpu;
	struct perf_event_attr *wd_attr;
	struct perf_event *evt;

	cpu = raw_smp_processor_id();
	wd_attr = &wd_hw_attr;
	wd_attr->sample_period = 1000;
	evt = perf_event_create_kernel_counter(
		wd_attr, cpu, NULL, watchdog_overflow_callback, NULL);
	if (IS_ERR(evt)) {
		printk("failed to create event...\n");
		long err = PTR_ERR(evt);
		pr_err("perf: create failed cpu=%u err=%ld\n", cpu, err);
		return;
	} else {
		registered = true;
		printk("created event!\n");
	}
}

// --- PEBS END ---

// ----- SYSFS_START -----
static void enable_pebs_shrinker(void)
{
	register_pebs();
}

enum thp_pebs_shrinker_flag {
	PEBS_SHRINKER_ENABLED,
	THP_PROMOTER,
};

static unsigned long thp_pebs_shrinker_flags = 0;

static ssize_t pebs_enabled_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	const char *output;
	if (test_bit(PEBS_SHRINKER_ENABLED, &thp_pebs_shrinker_flags)) {
		output = "[enable] disable";
	} else {
		output = "enable [disable]";
	}
	return sysfs_emit(buf, "%s\n", output);
}
static ssize_t pebs_enabled_store(struct kobject *kobj,
				  struct kobj_attribute *attr, const char *buf,
				  size_t count)
{
	ssize_t ret = count;
	if (sysfs_streq(buf, "enable")) {
		enable_pebs_shrinker();
		set_bit(PEBS_SHRINKER_ENABLED, &thp_pebs_shrinker_flags);
	} else if (sysfs_streq(buf, "disable"))
		clear_bit(PEBS_SHRINKER_ENABLED, &thp_pebs_shrinker_flags);
	else
		ret = -EINVAL;

	return ret;
}

static ssize_t thp_promoter_enabled_show(struct kobject *kobj,
					 struct kobj_attribute *attr, char *buf)
{
	const char *output;
	if (test_bit(THP_PROMOTER, &thp_pebs_shrinker_flags)) {
		output = "[enable] disable";
	} else {
		output = "enable [disable]";
	}
	return sysfs_emit(buf, "%s\n", output);
}

static ssize_t thp_promoter_enabled_store(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  const char *buf, size_t count)
{
	ssize_t ret = count;
	if (sysfs_streq(buf, "enable") && !test_bit(THP_PROMOTER, &thp_pebs_shrinker_flags)) {
		ret = thp_promoter_enable();
		// it's importanat that we set the bit (atomically) AFTER actually doing the work, not before.
		if (ret == 0) {
			set_bit(THP_PROMOTER, &thp_pebs_shrinker_flags);
		}
	} else if (sysfs_streq(buf, "disable") && test_bit(THP_PROMOTER, &thp_pebs_shrinker_flags)) {
		thp_promoter_disable();
		clear_bit(THP_PROMOTER, &thp_pebs_shrinker_flags);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static struct kobj_attribute pebs_shrinker_enabled_attr =
	__ATTR_RW(pebs_enabled);

static struct kobj_attribute thp_promoter_attr =
	__ATTR_RW(thp_promoter_enabled);

static struct attribute *pebs_shrinker_attr[] = {
	&pebs_shrinker_enabled_attr.attr,
	NULL,
};
static const struct attribute_group pebs_shrinker_attr_group = {
	.attrs = pebs_shrinker_attr
};

static int __init thp_pebs_shrinker_init(void)
{
	int err;
	struct kobject *thp_pebs_shrinker_kobj;
	thp_pebs_shrinker_kobj =
		kobject_create_and_add("thp_pebs_shrinker", mm_kobj);
	if (unlikely(!thp_pebs_shrinker_kobj)) {
		pr_err("failed to create thp pebs shrinker kobject\n");
		return -ENOMEM;
	}

	err = sysfs_create_group(thp_pebs_shrinker_kobj,
				 &pebs_shrinker_attr_group);
	if (err) {
		pr_err("failed to register thp pebs shrinker group\n");
		goto delete_obj;
	}

	return 0;

delete_obj:
	kobject_put(thp_pebs_shrinker_kobj);
	return err;
}

subsys_initcall(thp_pebs_shrinker_init);

// --- SYSFS END -----
