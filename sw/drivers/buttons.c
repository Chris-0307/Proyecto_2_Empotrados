#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

#define BUTTONS_BASE 0xFF200050
#define IRQ_MASK     0x08
#define EDGE_CAPTURE 0x0C

static void __iomem *buttons;
static unsigned int irq_number;
static unsigned int evento;

static DECLARE_WAIT_QUEUE_HEAD(cola);
static DEFINE_SPINLOCK(lock);

/* Funcion que se ejecuta al presionar un boton */
static irqreturn_t buttons_irq(int irq, void *dev_id)
{
    unsigned int valor;

    valor = readl(buttons + EDGE_CAPTURE) & 0xF;

    if (valor == 0)
        return IRQ_NONE;

    /* Limpiar la interrupcion */
    writel(valor, buttons + EDGE_CAPTURE);
    readl(buttons + EDGE_CAPTURE);

    /* Guardar los botones presionados */
    spin_lock(&lock);
    evento |= valor;
    spin_unlock(&lock);

    wake_up_interruptible(&cola);

    return IRQ_HANDLED;
}

/* Leer los botones desde un programa en C */
static ssize_t buttons_read(struct file *file, char __user *buf,
                            size_t count, loff_t *offset)
{
    unsigned char valor;
    unsigned long flags;
    int ret;

    if (count == 0)
        return 0;

    while (1) {
        spin_lock_irqsave(&lock, flags);

        valor = evento;
        evento = 0;

        spin_unlock_irqrestore(&lock, flags);

        if (valor != 0)
            break;

        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        ret = wait_event_interruptible(cola,
                                      ACCESS_ONCE(evento) != 0);

        if (ret)
            return ret;
    }

    if (copy_to_user(buf, &valor, 1))
        return -EFAULT;

    return 1;
}

static unsigned int buttons_poll(struct file *file,
                                 poll_table *wait)
{
    poll_wait(file, &cola, wait);

    if (ACCESS_ONCE(evento) != 0)
        return POLLIN | POLLRDNORM;

    return 0;
}

static const struct file_operations buttons_fops = {
    .owner = THIS_MODULE,
    .read = buttons_read,
    .poll = buttons_poll
};

static struct miscdevice buttons_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "fpga_buttons",
    .fops = &buttons_fops
};

static int __init buttons_init(void)
{
    struct device_node *gic;
    unsigned int irq_spec[3] = {0, 41, 4};
    int ret;

    buttons = ioremap(BUTTONS_BASE, 16);

    if (!buttons)
        return -ENOMEM;

    /* Deshabilitar interrupciones mientras configuramos */
    writel(0, buttons + IRQ_MASK);
    writel(0xF, buttons + EDGE_CAPTURE);

    /* Buscar el controlador de interrupciones del ARM */
    gic = of_find_compatible_node(NULL, NULL,
                                  "arm,cortex-a9-gic");

    if (!gic) {
        ret = -ENODEV;
        goto error_map;
    }

    irq_number = irq_create_of_mapping(gic, irq_spec, 3);
    of_node_put(gic);

    if (!irq_number) {
        ret = -EINVAL;
        goto error_map;
    }

    ret = request_irq(irq_number, buttons_irq,
                      IRQF_TRIGGER_HIGH,
                      "fpga_buttons", &buttons_dev);

    if (ret)
        goto error_map;

    ret = misc_register(&buttons_dev);

    if (ret)
        goto error_irq;

    /* Habilitar interrupciones de los cuatro botones */
    writel(0xF, buttons + IRQ_MASK);

    printk(KERN_INFO "Driver de botones iniciado, IRQ %u\n",
           irq_number);

    return 0;

error_irq:
    free_irq(irq_number, &buttons_dev);

error_map:
    iounmap(buttons);
    return ret;
}

static void __exit buttons_exit(void)
{
    writel(0, buttons + IRQ_MASK);
    writel(0xF, buttons + EDGE_CAPTURE);
    readl(buttons + EDGE_CAPTURE);

    free_irq(irq_number, &buttons_dev);
    misc_deregister(&buttons_dev);
    iounmap(buttons);

    printk(KERN_INFO "Driver de botones eliminado\n");
}

module_init(buttons_init);
module_exit(buttons_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver de interrupciones para botones DE1-SoC");