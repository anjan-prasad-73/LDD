#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

static int irq = 1;   // Example IRQ number (change as per hardware)

/* ---------- TOP HALF ---------- */
static irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    pr_info("IRQ %d: top half executed\n", irq);

    /* Wake up threaded handler */
    return IRQ_WAKE_THREAD;
}

/* ---------- THREADED HANDLER ---------- */
static irqreturn_t my_irq_thread(int irq, void *dev_id)
{
    pr_info("IRQ %d: threaded handler start\n", irq);

    /* Allowed to sleep */
    msleep(100);

    pr_info("IRQ %d: threaded handler end\n", irq);
    return IRQ_HANDLED;
}

/* ---------- MODULE INIT ---------- */
static int __init my_init(void)
{
    int ret;

    ret = request_threaded_irq(
            irq,
            my_irq_handler,     // top-half
            my_irq_thread,      // threaded handler
            IRQF_SHARED,
            "my_threaded_irq",
            (void *)my_init);

    if (ret) {
        pr_err("Failed to request IRQ %d\n", irq);
        return ret;
    }

    pr_info("Threaded IRQ registered on IRQ %d\n", irq);
    return 0;
}

/* ---------- MODULE EXIT ---------- */
static void __exit my_exit(void)
{
    free_irq(irq, (void *)my_init);
    pr_info("Threaded IRQ freed\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan");
MODULE_DESCRIPTION("Simple Threaded IRQ Example");

