#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/rwsem.h>

static DECLARE_RWSEM(my_rwsem);
static int shared_data = 0;

static struct task_struct *reader1;
static struct task_struct *reader2;
static struct task_struct *writer;

/* ---------------- READER THREAD ---------------- */
static int reader_thread(void *data)
{
    int id = *(int *)data;

    while (!kthread_should_stop()) {

        down_read(&my_rwsem);
        pr_info("Reader %d: read shared_data = %d\n", id, shared_data);

        /* SLEEP allowed with rw_semaphore */
        msleep(500);

        up_read(&my_rwsem);

        msleep(1000);
    }
    return 0;
}

/* ---------------- WRITER THREAD ---------------- */
static int writer_thread(void *data)
{
    while (!kthread_should_stop()) {

        down_write(&my_rwsem);
        shared_data++;
        pr_info("Writer: updated shared_data = %d\n", shared_data);

        /* SLEEP allowed with rw_semaphore */
        msleep(1000);

        up_write(&my_rwsem);

        msleep(3000);
    }
    return 0;
}

/* ---------------- MODULE INIT ---------------- */
static int __init rwsem_demo_init(void)
{
    static int id1 = 1, id2 = 2;

    reader1 = kthread_run(reader_thread, &id1, "rwsem_reader1");
    reader2 = kthread_run(reader_thread, &id2, "rwsem_reader2");
    writer  = kthread_run(writer_thread, NULL, "rwsem_writer");

    pr_info("RW_SEMAPHORE demo module loaded\n");
    return 0;
}

/* ---------------- MODULE EXIT ---------------- */
static void __exit rwsem_demo_exit(void)
{
    kthread_stop(reader1);
    kthread_stop(reader2);
    kthread_stop(writer);

    pr_info("RW_SEMAPHORE demo module unloaded\n");
}

module_init(rwsem_demo_init);
module_exit(rwsem_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anis");
MODULE_DESCRIPTION("Kernel rw_semaphore (rw_semlock) example");

