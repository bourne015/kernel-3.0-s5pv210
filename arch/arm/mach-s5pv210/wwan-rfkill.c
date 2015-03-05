#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <linux/rfkill.h>

#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <mach/gpio-smdkc110.h>

static void wwan_enable(bool on)
{
//	printk(KERN_ALERT "wwan : wwan_enable on=%d +\n", on);
	if (on) {
	 	//gpio_set_value(WWAN_RESET, 0);
		gpio_set_value(WWAN_PWR_ON, 0);
		gpio_set_value(WWAN_4V_ON, 1);
		msleep(100);
		gpio_set_value(WWAN_PWR_ON, 1);	
		msleep(900);
		gpio_set_value(WWAN_PWR_ON, 0);
	} else {
		//gpio_set_value(WWAN_RESET, 1);
		gpio_set_value(WWAN_PWR_ON, 1);
		gpio_set_value(WWAN_4V_ON, 0);
		msleep(10);
	}
//	printk(KERN_ALERT "wwan : wwan_enable -\n");
}

static int wwan_set_block(void *data, bool blocked)
{
	wwan_enable(!blocked);
	return 0;
}

static const struct rfkill_ops wwan_rfkill_ops = {
	.set_block = wwan_set_block,
};


static int wwan_probe(struct platform_device *pdev)
{
#if 1
	struct rfkill *rfk;
	int ret;
//	ret = gpio_request(WWAN_RESET, "WL Reset");
//	if (ret) {
//		printk("request WL Reset failue\n");
//		return ret;
//	}
	
	ret = gpio_request(WWAN_PWR_ON, "WL on/off");
	if (ret) {
		printk("request WL on/off failue\n");
		goto error_wwanreset;
	}
	
	ret = gpio_request(WWAN_4V_ON, "VBat 4.0V on");
	if (ret) {
		printk("request VBat 4.0V on failue\n");
		goto error_pwron;
	}
	
//	s3c_gpio_cfgpin(WWAN_RESET, S3C_GPIO_SFN(1));
//	s3c_gpio_setpull(WWAN_RESET, S3C_GPIO_PULL_NONE);	
	s3c_gpio_cfgpin(WWAN_PWR_ON, S3C_GPIO_SFN(1));
	s3c_gpio_setpull(WWAN_PWR_ON, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(WWAN_4V_ON, S3C_GPIO_SFN(1));
	s3c_gpio_setpull(WWAN_4V_ON, S3C_GPIO_PULL_NONE);
	
	rfk = rfkill_alloc("wwan_switch", &pdev->dev, RFKILL_TYPE_WWAN,
			&wwan_rfkill_ops, NULL);
	if (!rfk) {
		goto err_rfk_alloc;
	}

	rfkill_init_sw_state(rfk, 1);

	ret = rfkill_register(rfk);
	if (ret) {
		goto err_rfkill;
	}

	platform_set_drvdata(pdev, rfk);

	return 0;

err_rfkill:
	rfkill_destroy(rfk);
err_rfk_alloc:
	gpio_free(WWAN_4V_ON);
error_pwron:
	gpio_free(WWAN_PWR_ON);
error_wwanreset:
//	gpio_free(WWAN_RESET);
	return ret;
#endif
return 0;
}

static int wwan_remove(struct platform_device *pdev)
{
	struct rfkill *rfk = platform_get_drvdata(pdev);
		
	platform_set_drvdata(pdev, NULL);

	if (rfk) {
		rfkill_unregister(rfk);
		rfkill_destroy(rfk);
	}

	wwan_enable(0);

	gpio_free(WWAN_4V_ON);
	gpio_free(WWAN_PWR_ON);
//	gpio_free(WWAN_RESET);

	return 0;
}


static struct platform_driver wwan_driver = {
	.driver		= {
		.name	= "wwan_power_switch",
	},
	.probe		= wwan_probe,
	.remove		= wwan_remove,
};

static struct platform_device wwan_device = {
	.name = "wwan_power_switch",
	.id = -1,	
};
static int __init wwan_init(void)
{
	int ret;
	ret = platform_device_register(&wwan_device);
	if(ret)
		return ret;
	
	ret = platform_driver_register(&wwan_driver);
	if(ret)
	{
		platform_device_unregister(&wwan_device);
	}
	
	return ret;
}

static void __exit wwan_exit(void)
{
	platform_device_unregister(&wwan_device);
	platform_driver_unregister(&wwan_driver);
}

module_init(wwan_init);
module_exit(wwan_exit);

MODULE_AUTHOR("Zheng Hongjian<hj.zheng@jcfmobileworks.com>");
MODULE_DESCRIPTION("Driver for power switch of wwan");
MODULE_LICENSE("GPL");
