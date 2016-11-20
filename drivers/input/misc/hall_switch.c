/*
 *  Hall switch sensor driver
 *
 * Copyright (c) 2016, Michael Sky <electrydev@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/input.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct hall_switch_data {
	struct input_dev *input_dev;
	struct delayed_work hall_work;
	struct workqueue_struct *hall_workqueue;
	int hall_gpio;
	int hall_irq;
};

static irqreturn_t misc_hall_irq(int irq, void *data)
{
	struct hall_switch_data *hall_data = data;
	int gpio_value;

	if (hall_data == NULL)
		return 0;

	disable_irq_nosync(hall_data->hall_irq);

	gpio_value = gpio_get_value(hall_data->hall_gpio);
	if (gpio_value) {
		// Far
		input_event(hall_data->input_dev, EV_KEY, KEY_FAR, 1);
		input_sync(hall_data->input_dev);
		input_event(hall_data->input_dev, EV_KEY, KEY_FAR, 0);
		input_sync(hall_data->input_dev);
	} else {
		// Near
		input_event(hall_data->input_dev, EV_KEY, KEY_NEAR, 1);
		input_sync(hall_data->input_dev);
		input_event(hall_data->input_dev, EV_KEY, KEY_NEAR, 0);
		input_sync(hall_data->input_dev);
	}

	enable_irq(hall_data->hall_irq);

	return IRQ_HANDLED;
}

static int __devinit hall_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct device_node *np = pdev->dev.of_node;
	struct hall_switch_data *hall_data;

	hall_data = kzalloc(sizeof(struct hall_switch_data), GFP_KERNEL);
	if (!hall_data) {
		dev_err(&pdev->dev, "Failed allocate memory for hall_data\n");
		retval = -ENOMEM;
		goto exit;
	}

	retval = of_get_named_gpio(np, "irq-gpio", 0);
	if (retval < 0) {
		dev_err(&pdev->dev, "Failed to get irq gpio, ret=%d\n",
			retval);
		goto exit_kfree;
	}
	hall_data->hall_gpio = retval;

	hall_data->input_dev = input_allocate_device();
	if (hall_data->input_dev == NULL) {
		dev_err(&pdev->dev, "Failed to allocate input device\n");
		retval = -ENOMEM;
		goto exit_kfree;
	}

	hall_data->input_dev->name = "hall-switch";

	set_bit(EV_KEY, hall_data->input_dev->evbit);
	set_bit(KEY_NEAR, hall_data->input_dev->keybit);
	set_bit(KEY_FAR, hall_data->input_dev->keybit);
	input_set_capability(hall_data->input_dev, EV_KEY, KEY_NEAR);
	input_set_capability(hall_data->input_dev, EV_KEY, KEY_FAR);

	retval = input_register_device(hall_data->input_dev);
	if (retval) {
		dev_err(&pdev->dev, "Failed to register input device\n");
		goto exit_register_input;
	}

	retval = gpio_request(hall_data->hall_gpio, "hall_gpio");
	if (retval) {
		dev_err(&pdev->dev, "irq gpio [%d], request failed\n",
			hall_data->hall_gpio);
		goto exit_enable_irq;
	}
	retval = gpio_direction_input(hall_data->hall_gpio);
	if (retval) {
		dev_err(&pdev->dev, "irq gpio [%d], direction set failed\n",
			hall_data->hall_gpio);
		goto exit_free_gpio;
	}

	hall_data->hall_irq = gpio_to_irq(hall_data->hall_gpio);

	retval = request_threaded_irq(hall_data->hall_irq, NULL,
			misc_hall_irq, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"misc_hall_irq", hall_data);
	if (retval < 0) {
		dev_err(&pdev->dev,
			"Failed to create hall irq thread for gpio [%d]\n",
			hall_data->hall_gpio);
		goto exit_free_gpio;
	}

	enable_irq_wake(hall_data->hall_irq);

	return retval;

exit_free_gpio:
	gpio_free(hall_data->hall_gpio);

exit_enable_irq:
	input_unregister_device(hall_data->input_dev);

exit_register_input:
	input_free_device(hall_data->input_dev);
	hall_data->input_dev = NULL;

exit_kfree:
	kfree(hall_data);

exit:
	return retval;
}


#ifdef CONFIG_OF
static struct of_device_id hall_match_table[] = {
	{ .compatible = "hall-switch",},
	{ },
};
#else
#define hall_match_table NULL
#endif

static struct platform_driver hall_driver = {
	.probe = hall_probe,
	.driver = {
		.name = "hall_switch",
		.owner = THIS_MODULE,
		.of_match_table = hall_match_table,
	},
};

static int __init hall_init(void)
{
	return platform_driver_register(&hall_driver);
}

module_init(hall_init);
MODULE_DESCRIPTION("Hall switch sensor driver");
MODULE_AUTHOR("Michael Sky <electrydev@gmail.com>");
MODULE_LICENSE("GPL");
