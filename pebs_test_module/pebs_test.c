#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <linux/mm.h>


static struct perf_event *pebs_events[2] = {NULL, NULL};

static int tracked_pid = -1;
module_param(tracked_pid, int, 0);

static DEFINE_XARRAY(thp_xarray);

struct huge_page_info {
    unsigned long sub_pages_bitmap[BITS_TO_LONGS(HPAGE_PMD_NR)];
};

static void pebs_callback(struct perf_event *event,
                          struct perf_sample_data *data,
                          struct pt_regs *regs)
{
    if (!event->ctx->task) {
        pr_err("pebs_callback: do not have the task\n");
        return;
    }

    if (data->addr >= TASK_SIZE_MAX) {
        // access a kernel address
        return;
    }

    struct task_struct *task = event->ctx->task;
    unsigned long uaddr = data->addr;

    // int cpu = smp_processor_id();
    // pr_info("PEBS sample: cpu=%d pid=%d addr=%px ip=%px\n",
    //         cpu,
    //         task->pid,
    //         (void *)uaddr,
    //         (void *)instruction_pointer(regs));


    // find the page
    
    struct page *page = NULL;
    long ret;
    int locked = 1;

    mmap_read_lock(task->mm);

    ret = get_user_pages_remote(task->mm, uaddr, 1, FOLL_WRITE | FOLL_GET, &page, &locked);

    if (ret <= 0) {
        pr_err("pebs_callback: get_user_pages_remote failed for VA 0x%lx, ret=%ld\n", uaddr, ret);
        mmap_read_unlock(task->mm);
        return;
    }

    struct page *head = compound_head(page);

    if (!PageTransHuge(head)) {
        // pr_info("pebs_callback: address %p not from a huge page\n", (void *)data->addr);
        put_page(page);
        mmap_read_unlock(task->mm);
        return;
    }

    // update the page info
    unsigned long thp_index = page_to_pfn(head);
    unsigned long subpage_idx = page_to_pfn(page) - thp_index;

    put_page(page);
    mmap_read_unlock(task->mm);

    struct huge_page_info *info = xa_load(&thp_xarray, thp_index);
    if (!info) {
        info = kzalloc(sizeof(*info), GFP_KERNEL);
        if (!info) {
            pr_err("pebs_callback: failed to allocate bitmap for THP at pfn 0x%lx\n", thp_index);
            return;
        }
        if (xa_store(&thp_xarray, thp_index, info, GFP_KERNEL) != NULL) {
            kfree(info);
            info = xa_load(&thp_xarray, thp_index);
        }
    }
    set_bit(subpage_idx, info->sub_pages_bitmap);

    // page processing finished

    // pr_info("pebs_callback: info update - thp %lu, sub-page %lu\n", thp_index, subpage_idx);
}

static int __init pebs_test_init(void) {
    pr_info("PEBS test loaded.\n");

    struct task_struct *task = NULL;
    if (tracked_pid != -1) {
        task = pid_task(find_vpid(tracked_pid), PIDTYPE_PID);
    }

    if (!task) {
        pr_err("pebs_test_init: could not find the task\n");
        return 0;
    }

    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));

    attr.type           = PERF_TYPE_RAW;
    attr.config         = 0x81d0; // 0x81d0 - ALL_LOADS, 0x82d0 - ALL_STORES
    attr.sample_period  = 10000;
    attr.sample_type    = PERF_SAMPLE_IP | PERF_SAMPLE_ADDR;
    attr.precise_ip     = 2; // enable PEBS
    attr.size           = sizeof(attr);
    attr.wakeup_events  = 1;
    attr.exclude_kernel = 1;

    pebs_events[0] = perf_event_create_kernel_counter(&attr, -1, task, pebs_callback, NULL);

    attr.config = 0x82d0;
    pebs_events[1] = perf_event_create_kernel_counter(&attr, -1, task, pebs_callback, NULL);

    pr_info("PEBS sampler loaded\n");

    return 0;
}   

static void __exit pebs_test_exit(void) {
    pr_info("Huge pages info:\n");

    unsigned long thp_index;
    struct huge_page_info *info;
    
    xa_for_each(&thp_xarray, thp_index, info) {
        if (info) {
            unsigned int set_bits = bitmap_weight(info->sub_pages_bitmap, HPAGE_PMD_NR);
            pr_info("THP at pfn 0x%lx - sub-pages used %u\n",  thp_index, set_bits);
        }
    }

    xa_for_each(&thp_xarray, thp_index, info) {
        kfree(info);
    }
    xa_destroy(&thp_xarray);

    for (int i = 0; i < 2; ++i) {
        if (pebs_events[i]) {
            perf_event_release_kernel(pebs_events[i]);    
        }
    }
    
    pr_info("PEBS sampler unloaded.\n");
}


module_init(pebs_test_init);
module_exit(pebs_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("EPFL CS477");
MODULE_DESCRIPTION("A simple PEBS sampler LKM");
MODULE_VERSION("1.0");
