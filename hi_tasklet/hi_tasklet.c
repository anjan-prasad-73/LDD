#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>

static int irq = 1;   // Keyboard IRQ (test only)

/* ---------- HI TASKLET FUNCTION ---------- */
static void my_hi_tasklet_fn(struct tasklet_struct *t)
{
    pr_info("HI_TASKLET running on CPU %d\n", smp_processor_id());
}

/* Declare HIGH priority tasklet */
DECLARE_TASKLET(my_hi_tasklet, my_hi_tasklet_fn);

/* ---------- IRQ HANDLER (TOP HALF) ---------- */
static irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    pr_info("IRQ %d: top half executed\n", irq);

    /* Schedule HIGH priority tasklet */
    tasklet_hi_schedule(&my_hi_tasklet);

    return IRQ_HANDLED;
}

/* ---------- MODULE INIT ---------- */
static int __init my_init(void)
{
    int ret;

    ret = request_irq(
        irq,
        my_irq_handler,
        IRQF_SHARED,
        "hi_tasklet_demo",
        &irq);

    if (ret) {
        pr_err("Failed to request IRQ %d\n", irq);
        return ret;
    }

    pr_info("HI_TASKLET module loaded\n");
    return 0;
}

/* ---------- MODULE EXIT ---------- */
static void __exit my_exit(void)
{
    tasklet_kill(&my_hi_tasklet);
    free_irq(irq, &irq);
    pr_info("HI_TASKLET module unloaded\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan");
MODULE_DESCRIPTION("IRQ to HI_TASKLET example (Linux 6.x)");

