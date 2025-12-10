#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>      // for struct page, etc
#include <linux/proc_fs.h> // for proc interface
#include <linux/uaccess.h> // for copy_to_user, copy_from_user

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mohamed Taha GUELZIM");
MODULE_DESCRIPTION("Wrapper for split_huge_page");

static struct proc_dir_entry *proc_entry;

static int my_split_hugepage(struct page *page)
{
    printk(KERN_INFO "Splitting hugepage at phys addr %p\n", page);
    int ret = split_huge_page(page); // kernel function
    if (ret == 0)
        printk(KERN_INFO "Split successful!\n");
    else
        printk(KERN_INFO "Split failed: %d\n", ret);
    return ret;
}

// tempo: simple /proc read example
static ssize_t proc_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    const char *msg = "THP split module loaded\n";
    return simple_read_from_buffer(buf, count, ppos, msg, strlen(msg));
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
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
