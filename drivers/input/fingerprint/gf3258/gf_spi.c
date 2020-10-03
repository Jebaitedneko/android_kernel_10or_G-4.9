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

#define pr_fmt(fmt)		KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/mdss_io_util.h>
#include <linux/proc_fs.h>
#include "gf_spi.h"
#include <linux/platform_device.h>

#define VER_MAJOR   1
#define VER_MINOR   2
#define PATCH_LEVEL 11

#define	WAKELOCK_HOLD_TIME	100 /* in ms */
#define	GF_SPIDEV_NAME		"goodix,fingerprint"
#define	GF_DEV_NAME			"goodix_fp"
#define	GF_INPUT_NAME		"qwerty"
#define	CHRD_DRIVER_NAME	"goodix_fp_spi"
#define	CLASS_NAME		    "goodix_fp"
#define	PROC_NAME			"hwinfo"
#define	N_SPI_MINORS		32	/* ... up to 256 */

static int SPIDEV_MAJOR;

static DECLARE_BITMAP(minors, N_SPI_MINORS);
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static struct wakeup_source fp_wakelock;
static struct gf_dev gf;
static struct proc_dir_entry *proc_entry;

static void gf_enable_irq(struct gf_dev *gf_dev)
{
	if (gf_dev->irq_enabled) {
		pr_warn("IRQ has been enabled.\n");
	} else {
		enable_irq_wake(gf_dev->irq);
		gf_dev->irq_enabled = 1;
	}
}

static void gf_disable_irq(struct gf_dev *gf_dev)
{
	if (gf_dev->irq_enabled) {
		gf_dev->irq_enabled = 0;
		disable_irq_wake(gf_dev->irq);
	} else {
		pr_warn("IRQ has been disabled.\n");
	}
}

static irqreturn_t gf_irq(int irq, void *handle)
{

	char msg = GF_NET_EVENT_IRQ;
	struct gf_dev *gf_dev = &gf;
	__pm_wakeup_event(&fp_wakelock, WAKELOCK_HOLD_TIME);
	sendnlmsg(&msg);
	if ((gf_dev->wait_finger_down == true) && (gf_dev->device_available == 1) && (gf_dev->fb_black == 1)) {
		printk("%s:shedule_work\n",__func__);
		gf_dev->wait_finger_down = false;
		schedule_work(&gf_dev->work);
	}

	return IRQ_HANDLED;
}

static int irq_setup(struct gf_dev *gf_dev)
{
	int status;

	gf_dev->irq = gf_irq_num(gf_dev);
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
	disable_irq(gf_dev->irq);
	disable_irq_wake(gf_dev->irq);
	free_irq(gf_dev->irq, gf_dev);
}

static long gf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gf_dev *gf_dev = &gf;
	int retval = 0;
	u8 netlink_route = NETLINK_TEST;
	struct gf_ioc_chip_info info;

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
		gf_disable_irq(gf_dev);
		break;
	case GF_IOC_ENABLE_IRQ:
		pr_debug("%s GF_IOC_ENABLE_IRQ\n", __func__);
		gf_enable_irq(gf_dev);
		break;
	case GF_IOC_RESET:
		pr_debug("%s GF_IOC_RESET\n", __func__);
		gf_hw_reset(gf_dev, 3);
		break;
	case GF_IOC_ENABLE_POWER:
		pr_debug("%s GF_IOC_ENABLE_POWER\n", __func__);
			gf_power_on(gf_dev);
		break;
	case GF_IOC_DISABLE_POWER:
		pr_debug("%s GF_IOC_DISABLE_POWER\n", __func__);
			gf_power_off(gf_dev);
		break;
	case GF_IOC_CHIP_INFO:
		pr_debug("%s GF_IOC_CHIP_INFO\n", __func__);
		if (copy_from_user(&info, (void __user *)arg, sizeof(struct gf_ioc_chip_info))) {
			retval = -EFAULT;
			break;
		}
		pr_info("vendor_id : 0x%x\n", info.vendor_id);
		pr_info("mode : 0x%x\n", info.mode);
		pr_info("operation: 0x%x\n", info.operation);
		break;
	default:
		break;
	}

	return retval;
}

static long gf_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return gf_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
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
					goto err_parse_dt;

				status = irq_setup(gf_dev);
				if (status)
					goto err_irq;
			}
			gf_hw_reset(gf_dev, 3);
			gf_dev->device_available = 1;
		}
	} else {
		pr_info("No device for minor %d\n", iminor(inode));
	}
	mutex_unlock(&device_list_lock);

	return status;
err_irq:
	gf_cleanup(gf_dev);
err_parse_dt:
	return status;
}

static int proc_show_ver(struct seq_file *file,void *v)
{
	seq_printf(file,"Fingerprint: Goodix\n");
	return 0;
}

static int proc_open(struct inode *inode,struct file *file)
{
	printk("gf3258 proc_open\n");
	single_open(file,proc_show_ver,NULL);
	return 0;
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
		/*power off the sensor*/
		gf_dev->device_available = 0;
		gf_power_off(gf_dev);
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
	.compat_ioctl = gf_compat_ioctl,
	.open = gf_open,
	.release = gf_release,
};

static const struct file_operations proc_file_ops = {
	.owner = THIS_MODULE,
	.open = proc_open,
	.read = seq_read,
	.release = single_release,
};

static int goodix_fb_state_chg_callback(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct gf_dev *gf_dev;
	struct fb_event *evdata = data;
	unsigned int blank;
	char msg = 0;

	if (val != FB_EVENT_BLANK)
		return 0;
	pr_info("[info] %s go to the goodix_fb_state_chg_callback value = %d\n",
			__func__, (int)val);
	gf_dev = container_of(nb, struct gf_dev, notifier);
	if (evdata && evdata->data && val == FB_EVENT_BLANK && gf_dev) {
		blank = *(int *)(evdata->data);
		switch (blank) {
		case FB_BLANK_POWERDOWN:
			if (gf_dev->device_available == 1) {
				gf_dev->fb_black = 1;
				gf_dev->wait_finger_down = true;
				msg = GF_NET_EVENT_FB_BLACK;
				sendnlmsg(&msg);
			}
			break;
		case FB_BLANK_UNBLANK:
			if (gf_dev->device_available == 1) {
				gf_dev->fb_black = 0;
				msg = GF_NET_EVENT_FB_UNBLACK;
				sendnlmsg(&msg);
			}
			break;
		default:
			pr_info("%s defalut\n", __func__);
			break;
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block goodix_noti_block = {
	.notifier_call = goodix_fb_state_chg_callback,
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
	gf_dev->pwr_gpio = -EINVAL;
	gf_dev->device_available = 0;
	gf_dev->fb_black = 0;
	gf_dev->wait_finger_down = false;
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
		goto error_hw;
	}

	if (status == 0) {
		set_bit(minor, minors);
		list_add(&gf_dev->device_entry, &device_list);
	} else {
		gf_dev->devt = 0;
		goto error_hw;
	}
	mutex_unlock(&device_list_lock);

		/*input device subsystem */
		gf_dev->input = input_allocate_device();
		if (gf_dev->input == NULL) {
			pr_err("%s, failed to allocate input device\n", __func__);
			status = -ENOMEM;
			goto error_dev;
		}

		gf_dev->input->name = GF_INPUT_NAME;
		status = input_register_device(gf_dev->input);
		if (status) {
			pr_err("failed to register input device\n");
			goto error_input;
		}

	gf_dev->notifier = goodix_noti_block;
	fb_register_client(&gf_dev->notifier);


	wakeup_source_init(&fp_wakelock, "fp_wakelock");

	proc_entry = proc_create(PROC_NAME, 0644, NULL, &proc_file_ops);
	if (NULL == proc_entry) {
		printk("gf3258 Couldn't create proc entry!");
		return -ENOMEM;
	} else {
		printk("gf3258 Create proc entry success!");
	}

	pr_info("version V%d.%d.%02d\n", VER_MAJOR, VER_MINOR, PATCH_LEVEL);

	return status;

error_input:
	if (gf_dev->input != NULL)
		input_free_device(gf_dev->input);
error_dev:
	if (gf_dev->devt != 0) {
		pr_info("Err: status = %d\n", status);
		mutex_lock(&device_list_lock);
		list_del(&gf_dev->device_entry);
		device_destroy(gf_class, gf_dev->devt);
		clear_bit(MINOR(gf_dev->devt), minors);
		mutex_unlock(&device_list_lock);
	}
error_hw:
	gf_dev->device_available = 0;

	return status;
}

static int gf_remove(struct platform_device *pdev)
{
	struct gf_dev *gf_dev = &gf;

	wakeup_source_trash(&fp_wakelock);
	fb_unregister_client(&gf_dev->notifier);
	if (gf_dev->input)
		input_unregister_device(gf_dev->input);
	input_free_device(gf_dev->input);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&gf_dev->device_entry);
	device_destroy(gf_class, gf_dev->devt);
	clear_bit(MINOR(gf_dev->devt), minors);
	remove_proc_entry(PROC_NAME,NULL);

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
