#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/smp.h>

#define PROC_NAME "percpu_hits"

/* Per-CPU variable */
DEFINE_PER_CPU(unsigned long, hit_count);

/* /proc show function */
static int percpu_proc_show(struct seq_file *m, void *v)
{
    int cpu;

    /* Increment counter on the current CPU */
    this_cpu_inc(hit_count);

    seq_puts(m, "Per-CPU hit counters:\n");

    for_each_online_cpu(cpu) {
        seq_printf(m, "CPU %d : %lu\n",
                   cpu, per_cpu(hit_count, cpu));
    }

    return 0;
}

static int percpu_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, percpu_proc_show, NULL);
}

/* proc_ops is REQUIRED for kernel 6.x */
static const struct proc_ops percpu_proc_ops = {
    .proc_open    = percpu_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init percpu_init(void)
{
    int cpu;

    for_each_possible_cpu(cpu)
        per_cpu(hit_count, cpu) = 0;

    if (!proc_create(PROC_NAME, 0444, NULL, &percpu_proc_ops)) {
        pr_err("Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    pr_info("percpu_proc module loaded\n");
    return 0;
}

static void __exit percpu_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
    pr_info("percpu_proc module unloaded\n");
}

module_init(percpu_init);
module_exit(percpu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan");
MODULE_DESCRIPTION("Per-CPU /proc example for Linux 6.8");
