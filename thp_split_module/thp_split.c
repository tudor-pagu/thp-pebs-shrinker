#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>      // struct page, mm_struct
#include <linux/proc_fs.h> // proc interface
#include <linux/uaccess.h> // copy_to_user, copy_from_user
#include <linux/sched.h>   // pid_task, find_get_task
#include <linux/pid.h>     // pid structures
#include <linux/pagemap.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mohamed Taha GUELZIM");
MODULE_DESCRIPTION("Wrapper for split_huge_page with user-space VA and PID");

static struct proc_dir_entry *proc_entry;

// Helper function: split a huge page given a user VA and mm_struct
static int my_split_hugepage_pid(struct mm_struct *mm, unsigned long uaddr)
{
    struct page *page = NULL;
    long ret;
    int locked = 1;

    mmap_read_lock(mm);

    ret = get_user_pages_remote(mm, uaddr, 1,
                                FOLL_WRITE | FOLL_GET, &page, &locked);
    if (ret <= 0)
    {
        printk(KERN_ERR "thp_split: get_user_pages_remote failed for VA 0x%lx, ret=%ld\n",
               uaddr, ret);
        mmap_read_unlock(mm);
        return (int)ret;
    }

    if (!PageTransHuge(page))
    {
        printk(KERN_ERR "thp_split: page at VA 0x%lx is not THP\n", uaddr);
        put_page(page);
        mmap_read_unlock(mm);
        return -EINVAL;
    }

    lock_page(page);
    ret = split_huge_page(page);
    unlock_page(page);

    if (ret == 0)
        printk(KERN_INFO "thp_split: split_huge_page successful for VA 0x%lx\n", uaddr);
    else
        printk(KERN_ERR "thp_split: split_huge_page failed, ret=%ld\n", ret);

    put_page(page);
    mmap_read_unlock(mm);
    return (int)ret;
}

// /proc write handler
static ssize_t proc_write(struct file *file,
                          const char __user *buf,
                          size_t count,
                          loff_t *ppos)
{
    char kbuf[64];
    pid_t pid;
    unsigned long uaddr;
    struct pid *pid_struct;
    struct task_struct *task;
    struct mm_struct *mm;

    if (count >= sizeof(kbuf))
        return -EINVAL;

    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    if (sscanf(kbuf, "%d %lx", &pid, &uaddr) != 2)
        return -EINVAL;

    pid_struct = find_get_pid(pid);
    if (!pid_struct)
    {
        printk(KERN_ERR "thp_split: PID %d not found\n", pid);
        return -ESRCH;
    }

    task = pid_task(pid_struct, PIDTYPE_PID);
    if (!task)
    {
        printk(KERN_ERR "thp_split: task_struct for PID %d not found\n", pid);
        return -ESRCH;
    }

    mm = get_task_mm(task);
    if (!mm)
    {
        printk(KERN_ERR "thp_split: mm_struct for PID %d not found\n", pid);
        return -EINVAL;
    }

    my_split_hugepage_pid(mm, uaddr);

    mmput(mm);
    return count;
}

// /proc read handler (simple message)
static ssize_t proc_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    const char *msg = "THP split module loaded\n";
    return simple_read_from_buffer(buf, count, ppos, msg, strlen(msg));
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init thp_split_init(void)
{
    printk(KERN_INFO "THP split module loaded\n");
    proc_entry = proc_create("thp_split", 0666, NULL, &proc_fops);
    if (!proc_entry)
        return -ENOMEM;
    return 0;
}

static void __exit thp_split_exit(void)
{
    printk(KERN_INFO "THP split module unloaded\n");
    if (proc_entry)
        proc_remove(proc_entry);
}

module_init(thp_split_init);
module_exit(thp_split_exit);
