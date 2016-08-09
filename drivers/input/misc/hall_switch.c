#include <linux/module.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/bitops.h>
#include <linux/leds.h>

#include <linux/platform_device.h>
#include <linux/sysdev.h>

#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/kernel.h>


static volatile int key_debug = 5;
#define HALL_GPIO 80

struct hall_switch_data{
    struct input_dev *input_dev;
    struct delayed_work hall_work;
    struct workqueue_struct *hall_workqueue;
    int hall_irq;
    int hall_gpio_val;
};
static irqreturn_t misc_hall_irq(int irq, void *data)
{
    struct hall_switch_data *hall_data = data;
    int gpio_value;

    if (hall_data == NULL)
        return 0;

	disable_irq_nosync(hall_data->hall_irq);
	gpio_value = gpio_get_value(HALL_GPIO);
    if (gpio_value) {
        /*----hall far----*/
        if (key_debug == 5)
            printk("hall-switch %d,report: far!!!\n", HALL_GPIO);

        input_event(hall_data->input_dev, EV_KEY, KEY_SHOP, 1);
        input_sync(hall_data->input_dev);
        input_event(hall_data->input_dev, EV_KEY, KEY_SHOP, 0);
        input_sync(hall_data->input_dev);
    } else {
        /*----hall near----*/
        if (key_debug == 5)
            printk("hall-switch %d,report: near!!!\n", HALL_GPIO);

        input_event(hall_data->input_dev, EV_KEY, KEY_SPORT, 1);
        input_sync(hall_data->input_dev);
        input_event(hall_data->input_dev, EV_KEY, KEY_SPORT, 0);
        input_sync(hall_data->input_dev);
    }

	enable_irq(hall_data->hall_irq);
    return IRQ_HANDLED;
}

//gpio value: /sys/devices/system/hall-switch/hall_int_gpio
static ssize_t hall_gpio80_show(struct sysdev_class *class, struct sysdev_class_attribute * attr, char *buf)
{
	int tmp = gpio_get_value(HALL_GPIO);
        return sprintf(buf, "%s\n", tmp == 0 ? "0" : "1");
}

static SYSDEV_CLASS_ATTR(hall_int_gpio, 0444, hall_gpio80_show, NULL);

static struct sysdev_class_attribute *mhall_int_attributes[] = {
        &attr_hall_int_gpio,
        NULL
};

static struct sysdev_class module_hall_class = {
        .name = "hall-switch",
};

static int __devinit hall_probe(struct platform_device *pdev)
{
        int retval = 0;

        int err = 0;
        struct hall_switch_data *hall_data;
        hall_data = kzalloc(sizeof(struct hall_switch_data), GFP_KERNEL);
        if (!hall_data){
            err = -ENOMEM;
            goto exit;
        }

        /*----Register to Input Device----*/
        hall_data->input_dev = input_allocate_device();
        if (hall_data->input_dev == NULL){
            err = -ENOMEM;
            printk("hall-switch: Failed to allocate input device!!!\n");
            goto exit_kfree;
        }

        hall_data->input_dev->name = "hall-switch";

        set_bit(EV_SYN, hall_data->input_dev->evbit);
        set_bit(EV_KEY, hall_data->input_dev->evbit);
        set_bit(EV_ABS, hall_data->input_dev->evbit);

        set_bit(KEY_SPORT, hall_data->input_dev->keybit);
        input_set_capability(hall_data->input_dev, EV_KEY, KEY_SPORT);
        set_bit(KEY_SHOP, hall_data->input_dev->keybit);
        input_set_capability(hall_data->input_dev, EV_KEY, KEY_SHOP);

        set_bit(KEY_SPORT_B, hall_data->input_dev->keybit);
        input_set_capability(hall_data->input_dev, EV_KEY, KEY_SPORT_B);
        set_bit(KEY_SHOP_B, hall_data->input_dev->keybit);
        input_set_capability(hall_data->input_dev, EV_KEY, KEY_SHOP_B);

        retval = input_register_device(hall_data->input_dev);
        if(retval){
            printk("hall-switch: Failed to register input device!!!\n");
            goto exit_register_input;
        }

		retval = gpio_request(HALL_GPIO, "hall_gpio");
		if (retval) {
			printk("hall-switch: irq gpio %d,request failed\n",HALL_GPIO);
			goto exit_enable_irq;
		}
		retval = gpio_direction_input(HALL_GPIO);
		if (retval) {
			printk("hall-switch: irq gpio %d,direction set failed\n",HALL_GPIO);
			goto exit_free_gpio;
		}
        /*----hall irq request----*/
        hall_data->hall_irq = gpio_to_irq(HALL_GPIO);

        retval = request_threaded_irq(hall_data->hall_irq, NULL, misc_hall_irq, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "misc_hall_irq", hall_data);
        if(retval < 0){
            printk("hall-switch: Failed to create hall irq thread!!!i%d\n",HALL_GPIO);
            goto exit_free_gpio;
        }
        enable_irq_wake(hall_data->hall_irq);

        return retval;
exit_free_gpio:
		gpio_free(HALL_GPIO);

exit_enable_irq:
     input_unregister_device(hall_data->input_dev);

exit_register_input:
     input_free_device(hall_data->input_dev);
     hall_data->input_dev = NULL;

exit_kfree:
     kfree(hall_data);
exit:
     return err;
}


static struct platform_driver msm_hall_driver = {
	.probe = hall_probe,
	.driver = {
		.name = "msm_hall_switch",
		.owner = THIS_MODULE,
	},
};

static int __init hall_init(void)
{
        struct sysdev_class_attribute **attr;
        int res;

        res = sysdev_class_register(&module_hall_class);
        if (unlikely(res)) {
                return res;
        }

        for (attr = mhall_int_attributes; *attr; attr++) {
                res = sysdev_class_create_file(&module_hall_class, *attr);
                if (res)
                        goto out_unreg;
        }

	return platform_driver_register(&msm_hall_driver);

out_unreg:
        for (; attr >= mhall_int_attributes; attr--)
                sysdev_class_remove_file(&module_hall_class, *attr);
        sysdev_class_unregister(&module_hall_class);

        return res;


}

module_init(hall_init);
MODULE_DESCRIPTION("Hall switch sensor driver");
MODULE_LICENSE("GPL");