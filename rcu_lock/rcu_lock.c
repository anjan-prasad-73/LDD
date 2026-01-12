#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>

struct my_data {
    int value;
};

static struct my_data __rcu *rcu_ptr;

static struct task_struct *reader1;
static struct task_struct *reader2;
static struct task_struct *writer;

/* ---------------- READER THREAD ---------------- */
static int reader_thread(void *data)
{
    int id = *(int *)data;
    struct my_data *p;

    while (!kthread_should_stop()) {

        rcu_read_lock();
        p = rcu_dereference(rcu_ptr);
        if (p)
            pr_info("Reader %d: read value = %d\n", id, p->value);
        rcu_read_unlock();

        msleep(1000);
    }
    return 0;
}

/* ---------------- WRITER THREAD ---------------- */
static int writer_thread(void *data)
{
    struct my_data *new, *old;

    while (!kthread_should_stop()) {

        new = kmalloc(sizeof(*new), GFP_KERNEL);
        new->value = (rcu_ptr ? rcu_dereference(rcu_ptr)->value : 0) + 1;

        old = rcu_replace_pointer(rcu_ptr, new, true);

        pr_info("Writer: updated value = %d\n", new->value);

        /* Wait for all readers to finish */
        synchronize_rcu();

        kfree(old);

        msleep(3000);
    }
    return 0;
}

/* ---------------- MODULE INIT ---------------- */
static int __init rcu_demo_init(void)
{
    static int id1 = 1, id2 = 2;

    rcu_ptr = kmalloc(sizeof(*rcu_ptr), GFP_KERNEL);
    rcu_ptr->value = 0;

    reader1 = kthread_run(reader_thread, &id1, "rcu_reader1");
    reader2 = kthread_run(reader_thread, &id2, "rcu_reader2");
    writer  = kthread_run(writer_thread, NULL, "rcu_writer");

    pr_info("RCU demo module loaded\n");
    return 0;
}

/* ---------------- MODULE EXIT ---------------- */
static void __exit rcu_demo_exit(void)
{
    struct my_data *p;

    kthread_stop(reader1);
    kthread_stop(reader2);
    kthread_stop(writer);

    p = rcu_dereference(rcu_ptr);
    synchronize_rcu();
    kfree(p);

    pr_info("RCU demo module unloaded\n");
}

module_init(rcu_demo_init);
module_exit(rcu_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan");
MODULE_DESCRIPTION("Kernel RCU example");

