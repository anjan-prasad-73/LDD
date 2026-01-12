#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/rwlock.h>

static rwlock_t my_rwlock;
static int shared_data = 0;

static struct task_struct *reader1;
static struct task_struct *reader2;
static struct task_struct *writer;

/* ---------------- READER THREAD ---------------- */
static int reader_thread(void *data)
{
    int id = *(int *)data;

    while (!kthread_should_stop()) {

        read_lock(&my_rwlock);
        pr_info("Reader %d: read shared_data = %d\n", id, shared_data);
        read_unlock(&my_rwlock);

        msleep(1000);
    }
    return 0;
}

/* ---------------- WRITER THREAD ---------------- */
static int writer_thread(void *data)
{
    while (!kthread_should_stop()) {

        write_lock(&my_rwlock);
        shared_data++;
        pr_info("Writer: updated shared_data = %d\n", shared_data);
        write_unlock(&my_rwlock);

        msleep(3000);
    }
    return 0;
}

/* ---------------- MODULE INIT ---------------- */
static int __init rwlock_demo_init(void)
{
    static int id1 = 1, id2 = 2;

    rwlock_init(&my_rwlock);

    reader1 = kthread_run(reader_thread, &id1, "reader1");
    reader2 = kthread_run(reader_thread, &id2, "reader2");
    writer  = kthread_run(writer_thread, NULL, "writer");

    pr_info("RWLOCK demo module loaded\n");
    return 0;
}

/* ---------------- MODULE EXIT ---------------- */
static void __exit rwlock_demo_exit(void)
{
    kthread_stop(reader1);
    kthread_stop(reader2);
    kthread_stop(writer);

    pr_info("RWLOCK demo module unloaded\n");
}

module_init(rwlock_demo_init);
module_exit(rwlock_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan");
MODULE_DESCRIPTION("Kernel RWLOCK Example");

