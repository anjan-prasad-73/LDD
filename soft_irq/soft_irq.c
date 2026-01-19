#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>

static int irq = 1;   // Keyboard IRQ for testing

/* ---------- TASKLET FUNCTION ---------- */
static void my_tasklet_fn(struct tasklet_struct *t)
{
    pr_info("Tasklet running on CPU %d\n", smp_processor_id());
}

/* Declare tasklet (NEW API) */
DECLARE_TASKLET(my_tasklet, my_tasklet_fn);

/* ---------- TOP HALF ---------- */
static irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    pr_info("IRQ %d: top half executed\n", irq);

    tasklet_schedule(&my_tasklet);
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
        "irq_tasklet_demo",
        &irq);   // stable dev_id

    if (ret) {
        pr_err("Failed to request IRQ %d\n", irq);
        return ret;
    }

    pr_info("IRQ → Tasklet module loaded\n");
    return 0;
}

/* ---------- EXIT ---------- */
static void __exit my_exit(void)
{
    tasklet_kill(&my_tasklet);
    free_irq(irq, &irq);
    pr_info("IRQ → Tasklet module unloaded\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan");
MODULE_DESCRIPTION("IRQ to Tasklet example (Linux 6.8 compatible)");
