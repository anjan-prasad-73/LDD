#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/atomic.h>

static atomic_t my_counter = ATOMIC_INIT(0);

static int __init atomic_test_init(void)
{
    int value;

    pr_info("Atomic module loaded\n");

    atomic_inc(&my_counter);
    atomic_inc(&my_counter);

    value = atomic_read(&my_counter);
    pr_info("Counter value after increment = %d\n", value);

    return 0;
}

static void __exit atomic_test_exit(void)
{
    int value;

    atomic_dec(&my_counter);
    value = atomic_read(&my_counter);

    pr_info("Atomic module unloaded\n");
    pr_info("Counter value after decrement = %d\n", value);
}

module_init(atomic_test_init);
module_exit(atomic_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anjan");
MODULE_DESCRIPTION("Simple Atomic Variable Test Module");

