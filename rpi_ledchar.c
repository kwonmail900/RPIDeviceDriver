// SPDX-License-Identifier: GPL-2.0
/*
 * Educational Raspberry Pi LED character driver (legacy integer GPIO API).
 * No custom Device Tree overlay is needed.
 *
 * IMPORTANT: gpio_num is a Linux GLOBAL GPIO number, NOT the BCM offset.
 * For GPIO17 on a recent kernel, gpio_num may be e.g. 529 (512 + 17),
 * depending on the GPIO controller's dynamic base number.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/err.h>

#define DEVICE_NAME "rpi_ledchar"
#define CLASS_NAME  "rpi_led_class"
#define INPUT_MAX   16

static int gpio_num = -1;
module_param(gpio_num, int, 0444);
MODULE_PARM_DESC(gpio_num, "Linux global GPIO number for LED (required)");

static int major;
static struct class *led_class;
static struct device *led_device;
static DEFINE_MUTEX(led_mutex);

static ssize_t led_read(struct file *file, char __user *buffer,
                        size_t count, loff_t *ppos)
{
    char state[3];
    int value;
    int ret;

    ret = mutex_lock_interruptible(&led_mutex);
    if (ret)
        return ret;

    value = gpio_get_value_cansleep(gpio_num);
    mutex_unlock(&led_mutex);
    if (value < 0)
        return value;

    state[0] = value ? '1' : '0';
    state[1] = '\n';
    state[2] = '\0';
    return simple_read_from_buffer(buffer, count, ppos, state, 2);
}

static ssize_t led_write(struct file *file, const char __user *buffer,
                         size_t count, loff_t *ppos)
{
    char input[INPUT_MAX];
    int value;
    int ret;

    if (count == 0)
        return 0;
    if (count >= sizeof(input))
        return -EINVAL;
    if (copy_from_user(input, buffer, count))
        return -EFAULT;
    input[count] = '\0';

    ret = kstrtoint(strim(input), 10, &value);
    if (ret || (value != 0 && value != 1))
        return -EINVAL;

    ret = mutex_lock_interruptible(&led_mutex);
    if (ret)
        return ret;
    gpio_set_value_cansleep(gpio_num, value);
    mutex_unlock(&led_mutex);

    pr_info(DEVICE_NAME ": GPIO %d -> LED %s\n",
            gpio_num, value ? "ON" : "OFF");
    return count;
}

static int led_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static int led_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations led_fops = {
    .owner   = THIS_MODULE,
    .open    = led_open,
    .read    = led_read,
    .write   = led_write,
    .release = led_release,
};

static int __init led_init(void)
{
    int ret;

    if (!gpio_is_valid(gpio_num)) {
        pr_err(DEVICE_NAME ": supply a valid gpio_num=<global number>\n");
        return -EINVAL;
    }

    ret = gpio_request(gpio_num, DEVICE_NAME);
    if (ret) {
        pr_err(DEVICE_NAME ": gpio_request(%d) failed: %d\n", gpio_num, ret);
        return ret;
    }

    ret = gpio_direction_output(gpio_num, 0);
    if (ret) {
        pr_err(DEVICE_NAME ": gpio_direction_output failed: %d\n", ret);
        goto err_gpio_free;
    }

    major = register_chrdev(0, DEVICE_NAME, &led_fops);
    if (major < 0) {
        ret = major;
        goto err_gpio_off;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    led_class = class_create(CLASS_NAME);
#else
    led_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(led_class)) {
        ret = PTR_ERR(led_class);
        goto err_unregister;
    }

    led_device = device_create(led_class, NULL,
                               MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(led_device)) {
        ret = PTR_ERR(led_device);
        goto err_class_destroy;
    }

    pr_info(DEVICE_NAME ": loaded (major=%d, gpio=%d)\n", major, gpio_num);
    return 0;

err_class_destroy:
    class_destroy(led_class);
err_unregister:
    unregister_chrdev(major, DEVICE_NAME);
err_gpio_off:
    gpio_set_value_cansleep(gpio_num, 0);
err_gpio_free:
    gpio_free(gpio_num);
    return ret;
}

static void __exit led_exit(void)
{
    device_destroy(led_class, MKDEV(major, 0));
    class_destroy(led_class);
    unregister_chrdev(major, DEVICE_NAME);
    gpio_set_value_cansleep(gpio_num, 0);
    gpio_free(gpio_num);
    pr_info(DEVICE_NAME ": unloaded\n");
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("Raspberry Pi LED char driver using legacy integer GPIO API");
