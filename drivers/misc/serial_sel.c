
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#include <mach/gpio.h>

#define IOCTL_MAGIC		'S'
#define SERIAL_SEL_GPS		_IO(IOCTL_MAGIC, 1)
#define SERIAL_SEL_SCAN		_IO(IOCTL_MAGIC, 2)
#define SERIAL_SEL_EXT_PRINTER		_IO(IOCTL_MAGIC, 3)
#define POWER_GPS		_IOW(IOCTL_MAGIC, 4, unsigned long)
#define POWER_SCAN			_IOW(IOCTL_MAGIC, 5, unsigned long)
#define POWER_EXT_PRINTER		_IOW(IOCTL_MAGIC, 6, unsigned long)

#if 1
	#define DEBUG	printk
#else
	#define DEBUG(...)
#endif

enum Serial_Sel_GPIOS{
	UART_SEL_S0 = 0,
	UART_SEL_S1,
	GPIO_GPS_POWER,
	GPIO_SCAN_WAKEUP,
	ALL_GPIO,
};

struct serial_sel_gpios_t{
	unsigned int gpio;
	unsigned char *name;
	int direction;
	int default_val;
};

//UART_SEL_S0		UART_SEL_S1		device
//	1				0				gps
//	0				1				scan
static struct serial_sel_gpios_t serial_sel_gpios[] = {
	{S5PV210_MP06(5), "UART_SEL_S0", S3C_GPIO_OUTPUT, 0},
	{S5PV210_MP07(2), "UART_SEL_S1", S3C_GPIO_OUTPUT, 0},
	{S5PV210_GPC0(3), "GPIO_GPS_POWER", S3C_GPIO_OUTPUT, 0},
	{S5PV210_GPJ1(0), "GPIO_SCAN_WAKEUP", S3C_GPIO_INPUT, 1},
};

static int open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	return ret;
}

static long ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	int ret = 0;

	switch (cmd) {
		case SERIAL_SEL_GPS:
			gpio_set_value(serial_sel_gpios[UART_SEL_S1].gpio, 0);
			gpio_set_value(serial_sel_gpios[UART_SEL_S0].gpio, 1);
			DEBUG("SERIAL_SEL_GPS\n");
			break;

		case SERIAL_SEL_SCAN:
			gpio_set_value(serial_sel_gpios[UART_SEL_S1].gpio, 1);
			gpio_set_value(serial_sel_gpios[UART_SEL_S0].gpio, 0);
			DEBUG("SERIAL_SEL_SCAN,\n");
			break;

		case SERIAL_SEL_EXT_PRINTER:
			gpio_set_value(serial_sel_gpios[UART_SEL_S1].gpio, 1);
			gpio_set_value(serial_sel_gpios[UART_SEL_S0].gpio, 1);
			DEBUG("SERIAL_SEL_EXT_PRINTER\n");
			break;

		case POWER_GPS:
			if(args)
				gpio_set_value(serial_sel_gpios[GPIO_GPS_POWER].gpio, 1);
			else
				gpio_set_value(serial_sel_gpios[GPIO_GPS_POWER].gpio, 0);
			DEBUG("POWER_GPS, args=%d\n", (int)args);
			break;

		case POWER_SCAN:
			DEBUG("POWER_SCAN, args=%d\n", (int)args);
			break;

		case POWER_EXT_PRINTER:
			DEBUG("POWER_EXT_PRINTER, args=%d\n", (int)args);
			break;

		default:
			printk("invalid cmd:%d\n",cmd);
			DEBUG("default, args=%d\n", (int)args);
			break;
	}

	return ret;
}

static int release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	return ret;
}

struct file_operations serial_sel_ops = {
	.open       = open,
	.unlocked_ioctl      = ioctl,
	.release    = release,
};

struct miscdevice serial_sel = {
	.name = "serial_sel",
	.minor= MISC_DYNAMIC_MINOR,
	.fops = &serial_sel_ops,
};

int serial_sel_gpio_init(void)
{
	int i, err;

	for(i = 0; i < ARRAY_SIZE(serial_sel_gpios); i++){
		err = gpio_request(serial_sel_gpios[i].gpio, serial_sel_gpios[i].name);
		if (err){
			printk(KERN_ERR "line:%d. fun:%sfailed to request %s\n"
				,__LINE__,  __func__, serial_sel_gpios[i].name);
			gpio_free(serial_sel_gpios[i].gpio);
			return err;
		}
		
		if(S3C_GPIO_OUTPUT == serial_sel_gpios[i].direction){
			s3c_gpio_setpull(serial_sel_gpios[i].gpio, S3C_GPIO_PULL_NONE);
			s3c_gpio_cfgpin(serial_sel_gpios[i].gpio, S3C_GPIO_OUTPUT);
			gpio_set_value(serial_sel_gpios[i].gpio, serial_sel_gpios[i].default_val);
		}
		else{
			s3c_gpio_cfgpin(serial_sel_gpios[i].gpio, S3C_GPIO_INPUT);
			s3c_gpio_setpull(serial_sel_gpios[i].gpio, S3C_GPIO_PULL_UP);
		}
	}

	return 0;
}

static int __init serial_sel_init(void)
{
	int ret;

	serial_sel_gpio_init();
	ret = misc_register(&serial_sel);

	return ret;
}

module_init(serial_sel_init);

MODULE_LICENSE("GPL");
