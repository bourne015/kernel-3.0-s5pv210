#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include <mach/regs-gpio.h>
#include <mach/hardware.h>

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <asm/uaccess.h>

static  int wakeup_main_irq;
static wait_queue_head_t s_read_queue;
static atomic_t s_read_value;


#define WWAN_WAKEUP_MAIN_GPIO  S5PV210_GPH2(4)
#define HOST_WAKEUP_WL_GPIO S5PV210_GPJ3(6)
static struct work_struct wakeup_main_work;
static struct wake_lock wakeup_main_lock;
extern unsigned int v70_hw_ver;

static void wwan_pm_wakeup_main_work(struct work_struct *work)
{
	wake_lock_timeout(&wakeup_main_lock, HZ * 6);
}

static irqreturn_t wwan_pm_wakeup_main_isr(int irq, void *dev_id)
{
	printk(KERN_ALERT "wwan_pm : 3G wwan wakeup~~~~~~~~\n");
	if (!work_pending(&wakeup_main_work))
		schedule_work(&wakeup_main_work);
	return IRQ_HANDLED;
}

int wwan_pm_probe(struct platform_device * pdev)
{
	int irq, err;
	int ret = -1;
	
	wake_lock_init(&wakeup_main_lock, WAKE_LOCK_SUSPEND, "wwan_wakeup");
	err = gpio_request(WWAN_WAKEUP_MAIN_GPIO, "wwan_wakeup_main");
	if (err) {
		goto error_request_wakeup_main;
	}
	
	s3c_gpio_setpull(WWAN_WAKEUP_MAIN_GPIO, S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(WWAN_WAKEUP_MAIN_GPIO, S3C_GPIO_SFN(0xf));

	s3c_gpio_setpull(HOST_WAKEUP_WL_GPIO, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(HOST_WAKEUP_WL_GPIO, S3C_GPIO_OUTPUT);
	if (v70_hw_ver == 30)
		gpio_set_value(HOST_WAKEUP_WL_GPIO, 1);
	else
		gpio_set_value(HOST_WAKEUP_WL_GPIO, 0);
	INIT_WORK(&wakeup_main_work, wwan_pm_wakeup_main_work);

	wakeup_main_irq = gpio_to_irq(WWAN_WAKEUP_MAIN_GPIO);
	if (wakeup_main_irq <= 0) {
		goto error_request_wakeup_main_irq;
	}
	#if 1
	err = request_irq(wakeup_main_irq, wwan_pm_wakeup_main_isr, IORESOURCE_IRQ_LOWEDGE | IORESOURCE_IRQ_SHAREABLE, "wwan_wakeup", NULL);
	if (err) {
		goto error_request_wakeup_main_irq;
	}
	
	disable_irq(wakeup_main_irq);
	#endif
	device_init_wakeup(&pdev->dev, 1);

	return 0;	

error_request_wakeup_main_irq:	
	wake_lock_destroy(&wakeup_main_lock);
	gpio_free(WWAN_WAKEUP_MAIN_GPIO);
error_request_wakeup_main:
	return ret;
}

int wwan_pm_remove(struct platform_device * pdev)
{
	int irq;
	irq = gpio_to_irq(WWAN_WAKEUP_MAIN_GPIO);
	if (irq > 0) {
		disable_irq(irq);
	}
	
	cancel_work_sync(&wakeup_main_work);
	wake_lock_destroy(&wakeup_main_lock);

	gpio_free(WWAN_WAKEUP_MAIN_GPIO);	
	return 0;
}

int wwan_pm_suspend(struct platform_device * pdev, pm_message_t state)
{
	int err = 0;
//	printk(KERN_ALERT "wwan_pm : wwan_pm_suspend\n");

	if (v70_hw_ver != 30)
		gpio_set_value(HOST_WAKEUP_WL_GPIO, 1);
	if (device_may_wakeup(&pdev->dev)) {
		#if 0
		err = request_irq(wakeup_main_irq, wwan_pm_wakeup_main_isr, IORESOURCE_IRQ_LOWEDGE | IORESOURCE_IRQ_SHAREABLE, "wwan_wakeup", NULL);
		if (err) {
			return err;
		}
		#else
		enable_irq(wakeup_main_irq);
		#endif
		
		enable_irq_wake(wakeup_main_irq);
	}
//	printk(KERN_ALERT "wwan_pm : wwan_pm_suspend end\n");
	
	return 0;
}

int wwan_pm_resume(struct platform_device * pdev)
{
//printk(KERN_ALERT "wwan_pm : wwan_pm_resume\n");

	if (v70_hw_ver != 30)
		gpio_set_value(HOST_WAKEUP_WL_GPIO, 0);
	if (device_may_wakeup(&pdev->dev)) {
		disable_irq_wake(wakeup_main_irq);
		#if 0
		free_irq(wakeup_main_irq, NULL);
		#else
		disable_irq(wakeup_main_irq);
		#endif
	}
	atomic_inc(&s_read_value);
	wake_up_interruptible(&s_read_queue);
//	printk(KERN_ALERT "wwan_pm : wwan_pm_resume end\n");
	return 0;
}


int wwan_pm_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}

int wwan_pm_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t wwan_pm_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	char out[64];
	int len, value;
	
	if (wait_event_interruptible(s_read_queue, (atomic_read(&s_read_value) > 0))) {
		return -ERESTARTSYS;
	}

	value = atomic_read(&s_read_value);
	atomic_set(&s_read_value, 0);
	len = sprintf(out, "%d", value);	
	count = min(count, (size_t)len);
	
	if (count > 0) {
		if (copy_to_user(buf, out, count)) {
			return -EFAULT;
		}
	}

	return count;
}

static unsigned int wwan_pm_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &s_read_queue,  wait);
	if (atomic_read(&s_read_value) > 0)
		mask |= POLLIN | POLLRDNORM;	/* readable */
	return mask;
}

struct file_operations wwan_pm_misc_fops = 
{
	.owner = THIS_MODULE,
	.llseek =	no_llseek,
	.read = 	wwan_pm_read,
	.poll = 	wwan_pm_poll,
	.open = 	wwan_pm_open,
	.release =	wwan_pm_release,
};

struct miscdevice wwan_pm_misc_dev = 
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = "wwan-pm",
	.fops = &wwan_pm_misc_fops,
};

struct platform_driver wwan_pm_drv = 
{
	.probe = wwan_pm_probe,
	.remove = wwan_pm_remove,
	.suspend = wwan_pm_suspend,
	.resume = wwan_pm_resume,
	.driver ={
		.name		= "wwan-pm",
		.owner		= THIS_MODULE,
	},
};

int wwan_pm_init(void)
{
	int ret;
	
	ret = platform_driver_register(&wwan_pm_drv);
	if (ret < 0) {
		goto error;
	}

	ret = misc_register(&wwan_pm_misc_dev);
	if (ret < 0) {
		platform_driver_unregister(&wwan_pm_drv);
		goto error;
	}

	init_waitqueue_head(&s_read_queue);
	atomic_set(&s_read_value, 0);

	ret = 0;
error:
	return ret;
}

void wwan_pm_clean(void)
{
    misc_deregister(&wwan_pm_misc_dev);
	platform_driver_unregister(&wwan_pm_drv);
}

module_init(wwan_pm_init);
module_exit(wwan_pm_clean);
