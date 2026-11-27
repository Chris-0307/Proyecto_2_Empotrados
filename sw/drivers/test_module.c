#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init iniciar(void)
{
    printk(KERN_INFO "Modulo de prueba iniciado\n");
    return 0;
}

static void __exit terminar(void)
{
    printk(KERN_INFO "Modulo de prueba eliminado\n");
}

module_init(iniciar);
module_exit(terminar);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modulo de prueba DE1-SoC");