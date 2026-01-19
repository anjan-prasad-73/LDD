#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

static int irq = 1;   // Keyboard IRQ (test only)

/* ---------- WORKQUEUE FUNCTION ---------- */
static void my_work_fn(struct work_struct *work)
{
    pr_info("Workqueue running in process context (CPU %d)\n",
            smp_processor_id());

    /* Allowed to sleep */
    msleep(100);

    pr_info("Workqueue work completed\n");
}

/* Declare work */
static DECLARE_WORK(my_work, my_work_fn);

/* ---------- IRQ HANDLER ---------- */
static irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    pr_info("IRQ %d: top half executed\n", irq);

    /* Schedule workqueue */
    schedule_work(&my_work);

    return IRQ_HANDLED;
}

/* ---------- INIT ---------- */
static int __init my_init(void)
{
    int ret;

    ret = request_irq(
        irq,
        my_irq_handler,
        IRQF_SHARED,
        "irq_workqueue_demo",
        &irq);

    if (ret) {
        pr_err("Failed to request IRQ %d\n", irq);
        return ret;
    }

    pr_info("IRQ → Workqueue module loaded\n");
    return 0;
}

/* ---------- EXIT ---------- */
static void __exit my_exit(void)
{
    /* Flush pending work */
    flush_work(&my_work);

    free_irq(irq, &irq);
    pr_info("IRQ → Workqueue module unloaded\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan");
MODULE_DESCRIPTION("IRQ to Workqueue example (Linux 6.x)");

