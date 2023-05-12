#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>

#define DEBUG 1
#define IMX_GPIO_NR(bank, nr)           (((bank)-1)*32 + (nr))

int led[]={
	IMX_GPIO_NR(1, 16),
	IMX_GPIO_NR(1, 17),
	IMX_GPIO_NR(1, 18),
	IMX_GPIO_NR(1, 19),
};

static int led_write(void)
{
	int i;
	unsigned long data = 15;
	for (i = 0; i < ARRAY_SIZE(led); i++){
		gpio_direction_output(led[i],(data >> i) & 0x01);
	}
	printk("Hello, world \n");
	return 0;
}

static void led_exit(void)
{
	int i;
	unsigned long data = 0;
	for (i = 0; i < ARRAY_SIZE(led); i++){
		gpio_direction_output(led[i],(data >> i) & 0x01);
	}
	printk("Goodbye, world\n");
}

module_init(led_write);
module_exit(led_exit);

MODULE_LICENSE("Dual BSD/GPL");
