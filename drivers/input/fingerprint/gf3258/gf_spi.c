/*
 * TEE driver for goodix fingerprint sensor
 * Copyright (C) 2016 Goodix
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include "gf_spi.h"

#define	WAKELOCK_HOLD_TIME	25 /* in ms */
#define	GF_SPIDEV_NAME		"goodix,fingerprint"
#define	GF_DEV_NAME			"goodix_fp"
#define	GF_INPUT_NAME		"gf3258"
#define	CHRD_DRIVER_NAME	"goodix_fp_spi"
#define	CLASS_NAME		    "goodix_fp"
#define	N_SPI_MINORS		32	/* ... up to 256 */

static int SPIDEV_MAJOR;

static DECLARE_BITMAP(minors, N_SPI_MINORS);
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static struct wakeup_source fp_wakelock;
static struct gf_dev gf;

int gf_parse_dts(struct gf_dev* gf_dev)
{
	int rc = 0;

	gf_dev->reset_gpio = of_get_named_gpio(gf_dev->spi->dev.of_node,"goodix,gpio_reset",0);
	pr_info("gf::reset_gpio:%d\n", gf_dev->reset_gpio);
	gpio_request(gf_dev->reset_gpio, "goodix_reset");
	gpio_direction_output(gf_dev->reset_gpio, 1);

	gf_dev->irq_gpio = of_get_named_gpio(gf_dev->spi->dev.of_node,"goodix,gpio_irq",0);
	pr_info("gf::irq_gpio:%d\n", gf_dev->irq_gpio);
	gpio_request(gf_dev->irq_gpio, "goodix_irq");
	gpio_direction_input(gf_dev->irq_gpio);

	return 0;
}

void gf_cleanup(struct gf_dev	* gf_dev)
{
	pr_info("[info] %s\n",__func__);
	if (gpio_is_valid(gf_dev->irq_gpio))
	{
		gpio_free(gf_dev->irq_gpio);
		pr_info("remove irq_gpio success\n");
	}
	if (gpio_is_valid(gf_dev->reset_gpio))
	{
		gpio_free(gf_dev->reset_gpio);
		pr_info("remove reset_gpio success\n");
	}
#ifdef CONFIG_MACH_TENOR_E
	if (gpio_is_valid(gf_dev->pwr_gpio))
	{
		gpio_free(gf_dev->pwr_gpio);
		pr_info("remove pwr_gpio success\n");
	}
	if (gpio_is_valid(gf_dev->id_gpio))
	{
		gpio_free(gf_dev->id_gpio);
		pr_info("remove id_gpio success\n");
	}
#endif
}

static irqreturn_t gf_irq(int irq, void *handle)
{
	char msg[2] =  { 0x0 };
	msg[0] = GF_NET_EVENT_IRQ;
	__pm_wakeup_event(&fp_wakelock, WAKELOCK_HOLD_TIME);
	sendnlmsg(msg);
	return IRQ_HANDLED;
}

static int irq_setup(struct gf_dev *gf_dev)
{
	int status;

	gf_dev->irq = gpio_to_irq(gf_dev->irq_gpio);
	status = request_threaded_irq(gf_dev->irq, NULL, gf_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"gf", gf_dev);

	if (status) {
		pr_err("failed to request IRQ:%d\n", gf_dev->irq);
		return status;
	}
	enable_irq_wake(gf_dev->irq);
	gf_dev->irq_enabled = 1;

	return status;
}

static void irq_cleanup(struct gf_dev *gf_dev)
{
	gf_dev->irq_enabled = 0;
	disable_irq_wake(gf_dev->irq);
	free_irq(gf_dev->irq, gf_dev);
}

static long gf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gf_dev *gf_dev = &gf;
	int retval = 0;
	u8 netlink_route = NETLINK_TEST;

	if (_IOC_TYPE(cmd) != GF_IOC_MAGIC)
		return -ENODEV;

	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		retval = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (retval)
		return -EFAULT;

	switch (cmd) {
	case GF_IOC_INIT:
		pr_debug("%s GF_IOC_INIT\n", __func__);
		if (copy_to_user((void __user *)arg, (void *)&netlink_route, sizeof(u8))) {
			pr_err("GF_IOC_INIT failed\n");
			retval = -EFAULT;
			break;
		}
		break;
	case GF_IOC_EXIT:
		pr_debug("%s GF_IOC_EXIT\n", __func__);
		break;
	case GF_IOC_DISABLE_IRQ:
		pr_debug("%s GF_IOC_DISABEL_IRQ\n", __func__);
			gf_dev->irq_enabled = 0;
			disable_irq_wake(gf_dev->irq);
		break;
	case GF_IOC_ENABLE_IRQ:
		pr_debug("%s GF_IOC_ENABLE_IRQ\n", __func__);
			enable_irq_wake(gf_dev->irq);
			gf_dev->irq_enabled = 1;
		break;
	case GF_IOC_RESET:
		pr_debug("%s GF_IOC_RESET\n", __func__);
			gpio_direction_output(gf_dev->reset_gpio, 1);
			gpio_set_value(gf_dev->reset_gpio, 0);
			mdelay(3);
			gpio_set_value(gf_dev->reset_gpio, 1);
			mdelay(3);
		break;
	case GF_IOC_ENABLE_POWER:
		pr_debug("%s GF_IOC_ENABLE_POWER\n", __func__);
			pr_info("---- power on ----\n");
		break;
	case GF_IOC_DISABLE_POWER:
		pr_debug("%s GF_IOC_DISABLE_POWER\n", __func__);
			pr_info("---- power off ----\n");
		break;
	default:
		break;
	}

	return retval;
}

static int gf_open(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev = &gf;
	int status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(gf_dev, &device_list, device_entry) {
		if (gf_dev->devt == inode->i_rdev) {
			pr_info("Found\n");
			status = 0;
			break;
		}
	}

	if (status == 0) {
		if (status == 0) {
			gf_dev->users++;
			filp->private_data = gf_dev;
			nonseekable_open(inode, filp);
			pr_info("Succeed to open device. irq = %d\n",
					gf_dev->irq);
			if (gf_dev->users == 1) {
				status = gf_parse_dts(gf_dev);
				if (status)
					return status;

				status = irq_setup(gf_dev);
				if (status)
					gf_cleanup(gf_dev);
			}
			gpio_direction_output(gf_dev->reset_gpio, 1);
			gpio_set_value(gf_dev->reset_gpio, 0);
			mdelay(3);
			gpio_set_value(gf_dev->reset_gpio, 1);
			mdelay(3);
		}
	} else {
		pr_info("No device for minor %d\n", iminor(inode));
	}
	mutex_unlock(&device_list_lock);

	return status;
}

static int gf_release(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev = &gf;
	int status = 0;

	mutex_lock(&device_list_lock);
	gf_dev = filp->private_data;
	filp->private_data = NULL;

	/*last close?? */
	gf_dev->users--;
	if (!gf_dev->users) {

		pr_info("disble_irq. irq = %d\n", gf_dev->irq);

		irq_cleanup(gf_dev);
		gf_cleanup(gf_dev);
		pr_info("---- power off ----\n");
	}
	mutex_unlock(&device_list_lock);
	return status;
}

static const struct file_operations gf_fops = {
	.owner = THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.unlocked_ioctl = gf_ioctl,
	.open = gf_open,
	.release = gf_release,
};

static struct class *gf_class;
static int gf_probe(struct platform_device *pdev)

{
	struct gf_dev *gf_dev = &gf;
	int status = -EINVAL;
	unsigned long minor;
	int i;

	/* Initialize the driver data */
	INIT_LIST_HEAD(&gf_dev->device_entry);
	gf_dev->spi = pdev;
	gf_dev->irq_gpio = -EINVAL;
	gf_dev->reset_gpio = -EINVAL;
#ifdef CONFIG_MACH_TENOR_E
	gf_dev->pwr_gpio = -EINVAL;
	gf_dev->id_gpio = -EINVAL;
#endif
	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		gf_dev->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(gf_class, &gf_dev->spi->dev, gf_dev->devt,
				gf_dev, GF_DEV_NAME);
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&gf_dev->spi->dev, "no minor number available!\n");
		status = -ENODEV;
		mutex_unlock(&device_list_lock);
		return status;
	}

	if (status == 0) {
		set_bit(minor, minors);
		list_add(&gf_dev->device_entry, &device_list);
	} else {
		gf_dev->devt = 0;
		return status;
	}
	mutex_unlock(&device_list_lock);

		/*input device subsystem */
		gf_dev->input = input_allocate_device();
		if (gf_dev->input == NULL) {
			pr_err("%s, failed to allocate input device\n", __func__);
			status = -ENOMEM;
			if (gf_dev->input != NULL)
				input_free_device(gf_dev->input);
		}

		gf_dev->input->name = GF_INPUT_NAME;
		status = input_register_device(gf_dev->input);
		if (status) {
			pr_err("failed to register input device\n");
			if (gf_dev->devt != 0) {
				pr_info("Err: status = %d\n", status);
				mutex_lock(&device_list_lock);
				list_del(&gf_dev->device_entry);
				device_destroy(gf_class, gf_dev->devt);
				clear_bit(MINOR(gf_dev->devt), minors);
				mutex_unlock(&device_list_lock);
			}
		}

	wakeup_source_init(&fp_wakelock, "fp_wakelock");

	return status;
}

static int gf_remove(struct platform_device *pdev)
{
	struct gf_dev *gf_dev = &gf;

	wakeup_source_trash(&fp_wakelock);
	if (gf_dev->input)
		input_unregister_device(gf_dev->input);
	input_free_device(gf_dev->input);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&gf_dev->device_entry);
	device_destroy(gf_class, gf_dev->devt);
	clear_bit(MINOR(gf_dev->devt), minors);
	mutex_unlock(&device_list_lock);

	return 0;
}

static const struct of_device_id gx_match_table[] = {
	{ .compatible = GF_SPIDEV_NAME },
	{},
};

static struct platform_driver gf_driver = {
	.driver = {
		.name = GF_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = gx_match_table,
	},
	.probe = gf_probe,
	.remove = gf_remove,
};

static int __init gf_init(void)
{
	int status;

#ifdef CONFIG_MACH_TENOR_E
	struct gf_dev *gf_dev = &gf;
	gf_dev->pwr_gpio = of_get_named_gpio(gf_dev->spi->dev.of_node, "goodix,gpio_pwr",0);
	gpio_request(gf_dev->pwr_gpio, "goodix_pwr");
	gpio_direction_output(gf_dev->pwr_gpio, 1);
	gpio_set_value(gf_dev->pwr_gpio, 0);
	mdelay(10);
	gpio_set_value(gf_dev->pwr_gpio, 1);
	mdelay(10);

	gf_dev->id_gpio = of_get_named_gpio(gf_dev->spi->dev.of_node, "goodix,gpio_id",0);
	gpio_request(gf_dev->id_gpio, "goodix_id");
	gpio_direction_input(gf_dev->id_gpio);
#endif

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */

	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(SPIDEV_MAJOR, CHRD_DRIVER_NAME, &gf_fops);
	if (status < 0) {
		pr_warn("Failed to register char device!\n");
		return status;
	}
	SPIDEV_MAJOR = status;
	gf_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(gf_class)) {
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		pr_warn("Failed to create class.\n");
		return PTR_ERR(gf_class);
	}
	status = platform_driver_register(&gf_driver);
	if (status < 0) {
		class_destroy(gf_class);
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		pr_warn("Failed to register SPI driver.\n");
	}

	netlink_init();

	pr_info("status = 0x%x\n", status);
	return 0;
}
module_init(gf_init);

static void __exit gf_exit(void)
{
	netlink_exit();
	platform_driver_unregister(&gf_driver);
	class_destroy(gf_class);
	unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
}
module_exit(gf_exit);

MODULE_AUTHOR("Jiangtao Yi, <yijiangtao@goodix.com>");
MODULE_AUTHOR("Jandy Gou, <gouqingsong@goodix.com>");
MODULE_DESCRIPTION("goodix fingerprint sensor device driver");
MODULE_LICENSE("GPL");
