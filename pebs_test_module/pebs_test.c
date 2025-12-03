#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>


static struct perf_event **pebs_events;

static void pebs_callback(struct perf_event *event,
                          struct perf_sample_data *data,
                          struct pt_regs *regs)
{
    int cpu = smp_processor_id();
    // data->addr contains the memory address for MEM_INST_RETIRED loads
    pr_info("PEBS sample: cpu=%d addr=%px ip=%px\n",
            cpu,
            (void *)data->addr,
            (void *)instruction_pointer(regs));
}

static int __init pebs_test_init(void) {
    pr_info("PEBS test loaded.\n");

    struct perf_event_attr attr;

    memset(&attr, 0, sizeof(attr));

    attr.type           = PERF_TYPE_RAW;
    attr.config         = 0x81d0;         // MEM_INST_RETIRED.ALL_LOADS
    attr.sample_period  = 1000;           // PEBS sample every 1000 loads
    attr.sample_type    = PERF_SAMPLE_IP | PERF_SAMPLE_ADDR;
    attr.precise_ip     = 2;              // enable PEBS
    attr.size           = sizeof(attr);
    attr.wakeup_events  = 1;

    pebs_events = kcalloc(num_online_cpus(), sizeof(struct perf_event *), GFP_KERNEL);
    if (!pebs_events)
        return -ENOMEM;

    int cpu;
    for_each_online_cpu(cpu) {
        struct perf_event *event;

        event = perf_event_create_kernel_counter(&attr, cpu, NULL, pebs_callback, NULL);

        if (IS_ERR(event)) {
            pr_err("Failed on CPU %d: %ld\n", cpu, PTR_ERR(event));
            continue;
        }

        pebs_events[cpu] = event;
    }


    pr_info("PEBS ALL_LOADS sampler loaded on all CPUs\n");

    return 0;
}   

static void __exit pebs_test_exit(void) {
    int cpu;
    for_each_online_cpu(cpu) {
        if (pebs_events[cpu]) {
            perf_event_release_kernel(pebs_events[cpu]);
        }
    }

    kfree(pebs_events);

    pr_info("PEBS test unloaded.\n");
}


module_init(pebs_test_init);
module_exit(pebs_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("EPFL CS477");
MODULE_DESCRIPTION("A simple PEBS test LKM");
MODULE_VERSION("1.0");