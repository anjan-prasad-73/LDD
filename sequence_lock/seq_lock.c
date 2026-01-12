#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/seqlock.h>

static seqlock_t my_seqlock;
static int shared_data = 0;

static struct task_struct *reader1;
static struct task_struct *reader2;
static struct task_struct *writer;

/* ---------------- READER THREAD ---------------- */
static int reader_thread(void *data)
{
    int id = *(int *)data;
    unsigned int seq;
    int value;

    while (!kthread_should_stop()) {

        do {
            seq = read_seqbegin(&my_seqlock);
            value = shared_data;
        } while (read_seqretry(&my_seqlock, seq));

        pr_info("Reader %d: read shared_data = %d\n", id, value);

        msleep(1000);
    }
    return 0;
}

/* ---------------- WRITER THREAD ---------------- */
static int writer_thread(void *data)
{
    while (!kthread_should_stop()) {

        write_seqlock(&my_seqlock);
        shared_data++;
        pr_info("Writer: updated shared_data = %d\n", shared_data);
        write_sequnlock(&my_seqlock);

        msleep(3000);
    }
    return 0;
}

/* ---------------- MODULE INIT ---------------- */
static int __init seqlock_demo_init(void)
{
    static int id1 = 1, id2 = 2;

    seqlock_init(&my_seqlock);

    reader1 = kthread_run(reader_thread, &id1, "seqlock_reader1");
    reader2 = kthread_run(reader_thread, &id2, "seqlock_reader2");
    writer  = kthread_run(writer_thread, NULL, "seqlock_writer");

    pr_info("SEQLOCK demo module loaded\n");
    return 0;
}

/* ---------------- MODULE EXIT ---------------- */
static void __exit seqlock_demo_exit(void)
{
    kthread_stop(reader1);
    kthread_stop(reader2);
    kthread_stop(writer);

    pr_info("SEQLOCK demo module unloaded\n");
}

module_init(seqlock_demo_init);
module_exit(seqlock_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anis");
MODULE_DESCRIPTION("Kernel seqlock example");

