#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/delay.h>

static struct task_struct *producer;
static struct task_struct *consumer;

static struct semaphore sem_empty;
static struct semaphore sem_full;

static int data = 0;

/* Producer thread */
static int producer_fn(void *arg)
{
    while (!kthread_should_stop()) {

        /* Wait until buffer is empty */
        if (down_interruptible(&sem_empty))
            break;

        data++;
        pr_info("Producer produced data = %d\n", data);

        /* Signal consumer */
        up(&sem_full);

        msleep(1000);
    }
    return 0;
}

/* Consumer thread */
static int consumer_fn(void *arg)
{
    while (!kthread_should_stop()) {

        /* Wait until data is available */
        if (down_interruptible(&sem_full))
            break;

        pr_info("Consumer consumed data = %d\n", data);

        /* Signal producer */
        up(&sem_empty);

        msleep(1500);
    }
    return 0;
}

static int __init sem_module_init(void)
{
    pr_info("Semaphore module loaded\n");

    /* Initialize semaphores */
    sema_init(&sem_empty, 1); // buffer empty
    sema_init(&sem_full, 0);  // no data initially

    producer = kthread_run(producer_fn, NULL, "producer_kthread");
    consumer = kthread_run(consumer_fn, NULL, "consumer_kthread");

    if (IS_ERR(producer) || IS_ERR(consumer)) {
        pr_err("Failed to create threads\n");
        return -ENOMEM;
    }

    return 0;
}

static void __exit sem_module_exit(void)
{
    pr_info("Semaphore module unloading\n");

    if (producer)
        kthread_stop(producer);

    if (consumer)
        kthread_stop(consumer);
}

module_init(sem_module_init);
module_exit(sem_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan");
MODULE_DESCRIPTION("Kernel Semaphore Producer-Consumer Example");

