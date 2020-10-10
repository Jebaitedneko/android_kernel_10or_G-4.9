/*
 * driver definition for sensor driver
 *
 * Coypright (c) 2017 Goodix
 */

#ifndef __GF_SPI_H
#define __GF_SPI_H

#include <linux/types.h>

#define GF_IOC_MAGIC            'g'
#define GF_IOC_INIT             _IOR(GF_IOC_MAGIC, 0, uint8_t)
#define GF_IOC_EXIT             _IO(GF_IOC_MAGIC, 1)
#define GF_IOC_RESET            _IO(GF_IOC_MAGIC, 2)
#define GF_IOC_ENABLE_IRQ       _IO(GF_IOC_MAGIC, 3)
#define GF_IOC_DISABLE_IRQ      _IO(GF_IOC_MAGIC, 4)
#define GF_IOC_ENABLE_POWER     _IO(GF_IOC_MAGIC, 7)
#define GF_IOC_DISABLE_POWER    _IO(GF_IOC_MAGIC, 8)
#define GF_IOC_REMOVE           _IO(GF_IOC_MAGIC, 12)

#define GF_NET_EVENT_IRQ		1
#define NETLINK_TEST			25

struct gf_dev {
	dev_t devt;
	struct list_head device_entry;
	struct platform_device *spi;
	struct input_dev *input;
	unsigned users;
	signed irq_gpio;
	signed reset_gpio;
#ifdef CONFIG_MACH_TENOR_E
	signed pwr_gpio;
	signed id_gpio;
#endif
	int irq;
	int irq_enabled;
};

void sendnlmsg(char *message);
void netlink_init(void);
void netlink_exit(void);

#endif /*__GF_SPI_H*/
