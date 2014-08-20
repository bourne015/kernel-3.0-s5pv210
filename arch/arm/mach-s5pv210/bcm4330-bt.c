/*
 * Copyright (C) 2010 Samsung Electronics Co., Ltd.
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Modified for Crespo on August, 2010 By Samsung Electronics Co.
 * This is modified operate according to each status.
 *
 */

/* Control bluetooth power for Crespo platform */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/wakelock.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <plat/irqs.h>
#include <mach/gpio-smdkc110.h>

#ifdef pr_debug
#undef pr_debug
#define pr_debug printk
#endif

#ifndef	GPIO_LEVEL_LOW
#define GPIO_LEVEL_LOW		0
#endif

#ifndef	GPIO_LEVEL_HIGH
#define GPIO_LEVEL_HIGH		1
#endif

static struct rfkill *bt_rfk;
static const char bt_name[] = "bcm4330";

static int bluetooth_set_power(void *data, enum rfkill_user_states state)
{
	int ret = 0;

	switch (state) {

	case RFKILL_USER_STATE_UNBLOCKED:
		pr_debug("[BT] Device Powering ON\n");
		//uart cfg
		s3c_gpio_cfgpin(GPIO_BT_RXD, S3C_GPIO_SFN(GPIO_BT_RXD_AF));
		s3c_gpio_setpull(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_TXD, S3C_GPIO_SFN(GPIO_BT_TXD_AF));
		s3c_gpio_setpull(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_CTS, S3C_GPIO_SFN(GPIO_BT_CTS_AF));
		s3c_gpio_setpull(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_RTS, S3C_GPIO_SFN(GPIO_BT_RTS_AF));
		s3c_gpio_setpull(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
/*
		s3c_gpio_slp_cfgpin(GPIO_BT_RXD, S3C_GPIO_SLP_INPUT);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RXD, S3C_GPIO_PULL_UP);
		s3c_gpio_slp_cfgpin(GPIO_BT_TXD, S3C_GPIO_SLP_INPUT);
		s3c_gpio_slp_setpull_updown(GPIO_BT_TXD, S3C_GPIO_PULL_UP);
		s3c_gpio_slp_cfgpin(GPIO_BT_CTS, S3C_GPIO_SLP_OUT1);
		s3c_gpio_slp_setpull_updown(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_RTS, S3C_GPIO_SLP_OUT1);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
*/
		s3c_gpio_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_OUTPUT);
              s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
	       gpio_set_value(GPIO_WLAN_BT_EN, 1);
//		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
//		s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN,
//					S3C_GPIO_PULL_NONE);
		msleep(1);
		
		if (gpio_is_valid(GPIO_BT_EN))
			gpio_direction_output(GPIO_BT_EN, GPIO_LEVEL_HIGH);
		s3c_gpio_setpull(GPIO_BT_EN, S3C_GPIO_PULL_NONE);

		/* Set GPIO_BT_WLAN_REG_ON high */
		//s3c_gpio_slp_cfgpin(GPIO_BT_EN, S3C_GPIO_SLP_OUT1);
		//s3c_gpio_slp_setpull_updown(GPIO_BT_EN,
		//		S3C_GPIO_PULL_NONE);

		pr_debug("[BT] GPIO_BT_EN = %d\n",
				gpio_get_value(GPIO_BT_EN));
				
		if (gpio_is_valid(GPIO_BT_nRST))
			gpio_direction_output(GPIO_BT_nRST, GPIO_LEVEL_LOW);

		pr_debug("[BT] GPIO_BT_nRST = %d\n",
				gpio_get_value(GPIO_BT_nRST));
		/*
		 * FIXME sleep should be enabled disabled since the device is
		 * not booting if its enabled
		 */
		/*
		 * 100msec, delay between reg_on & rst.
		 * (bcm4329 powerup sequence)
		 */
		msleep(50);

		/* Set GPIO_BT_nRST high */
		s3c_gpio_setpull(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_HIGH);

//		s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT1);
//		s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_NONE);

		pr_debug("[BT] GPIO_BT_nRST = %d\n",
				gpio_get_value(GPIO_BT_nRST));

		/*
		 * 50msec, delay after bt rst
		 * (bcm4329 powerup sequence)
		 */
		msleep(50);

		s3c_gpio_cfgpin(GPIO_HOST_WAKE_BT, S3C_GPIO_OUTPUT); 
		s3c_gpio_setpull(GPIO_HOST_WAKE_BT, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_HOST_WAKE_BT, 1); 

		break;

	case RFKILL_USER_STATE_SOFT_BLOCKED:
		pr_debug("[BT] Device Powering OFF\n");

		s3c_gpio_cfgpin(GPIO_HOST_WAKE_BT, S3C_GPIO_OUTPUT); 
		s3c_gpio_setpull(GPIO_HOST_WAKE_BT, S3C_GPIO_PULL_DOWN);
		gpio_set_value(GPIO_HOST_WAKE_BT, 0);  
		
		s3c_gpio_setpull(GPIO_BT_nRST, S3C_GPIO_PULL_DOWN);
		gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_LOW);

		//s3c_gpio_slp_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT0);
		//s3c_gpio_slp_setpull_updown(GPIO_BT_nRST, S3C_GPIO_PULL_DOWN);

		pr_debug("[BT] GPIO_BT_nRST = %d\n",
				gpio_get_value(GPIO_BT_nRST));
		
		s3c_gpio_setpull(GPIO_BT_EN, S3C_GPIO_PULL_DOWN);
		gpio_set_value(GPIO_BT_EN, GPIO_LEVEL_LOW);

		if (gpio_get_value(GPIO_WLAN_EN) == 0) {
			s3c_gpio_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_OUTPUT);
			gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);

			//s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT0);
			//s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN,
			//		S3C_GPIO_PULL_DOWN);

			pr_debug("[BT] GPIO_WLAN_BT_EN = %d\n",
					gpio_get_value(GPIO_WLAN_BT_EN));
		}

		//s3c_gpio_slp_cfgpin(GPIO_BT_EN, S3C_GPIO_SLP_OUT0);
		//s3c_gpio_slp_setpull_updown(GPIO_BT_EN,
		//		S3C_GPIO_PULL_DOWN);

		pr_debug("[BT] GPIO_BT_EN = %d\n",
				gpio_get_value(GPIO_BT_EN));
		
		//uart, input pull down
		s3c_gpio_setpull(GPIO_BT_RXD, S3C_GPIO_PULL_DOWN);	
		s3c_gpio_cfgpin(GPIO_BT_RXD, S3C_GPIO_INPUT);	
		s3c_gpio_setpull(GPIO_BT_TXD, S3C_GPIO_PULL_DOWN);	
		s3c_gpio_cfgpin(GPIO_BT_TXD, S3C_GPIO_INPUT);	
		s3c_gpio_setpull(GPIO_BT_CTS, S3C_GPIO_PULL_DOWN);	
		s3c_gpio_cfgpin(GPIO_BT_CTS, S3C_GPIO_INPUT);	
		s3c_gpio_setpull(GPIO_BT_RTS, S3C_GPIO_PULL_DOWN);	 
		s3c_gpio_cfgpin(GPIO_BT_RTS, S3C_GPIO_INPUT); 
/*		
		s3c_gpio_slp_cfgpin(GPIO_BT_RXD, S3C_GPIO_SLP_INPUT);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RXD, S3C_GPIO_PULL_DOWN);
		s3c_gpio_slp_cfgpin(GPIO_BT_TXD, S3C_GPIO_SLP_INPUT);
		s3c_gpio_slp_setpull_updown(GPIO_BT_TXD, S3C_GPIO_PULL_DOWN);
		s3c_gpio_slp_cfgpin(GPIO_BT_CTS, S3C_GPIO_SLP_INPUT);
		s3c_gpio_slp_setpull_updown(GPIO_BT_CTS, S3C_GPIO_PULL_DOWN);
		s3c_gpio_slp_cfgpin(GPIO_BT_RTS, S3C_GPIO_SLP_INPUT);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);//when wifi open ,the pin will became high 
*/
		break;

	default:
		pr_err("[BT] Bad bluetooth rfkill state %d\n", state);
	}

	return 0;

}

static int bt_rfkill_set_block(void *data, bool blocked)
{
	unsigned int ret = 0;

	ret = bluetooth_set_power(data, blocked ?
			RFKILL_USER_STATE_SOFT_BLOCKED :
			RFKILL_USER_STATE_UNBLOCKED);

	return ret;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_rfkill_set_block,
};

static int __init bt_rfkill_probe(struct platform_device *pdev)
{
	int irq;
	int ret;

	ret = gpio_request(GPIO_BT_EN, "BT_REG_ON");
	if (ret < 0) {
		pr_err("[BT] Failed to request GPIO_BT_EN!\n");
		goto err_req_gpio_bt_en;
	}

	ret = gpio_request(GPIO_BT_nRST, "BT_RST_N");
	if (ret < 0) {
		pr_err("[BT] Failed to request GPIO_BT_nRST!\n");
		goto err_req_gpio_bt_nrst;
	}
	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			&bt_rfkill_ops, NULL);

	if (!bt_rfk) {
		pr_err("[BT] bt_rfk : rfkill_alloc is failed\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	rfkill_init_sw_state(bt_rfk, 0);

	pr_debug("[BT] rfkill_register(bt_rfk)\n");

	ret = rfkill_register(bt_rfk);
	if (ret) {
		pr_debug("********ERROR IN REGISTERING THE RFKILL********\n");
		goto err_register;
	}

	rfkill_set_sw_state(bt_rfk, 1);
	bluetooth_set_power(NULL, RFKILL_USER_STATE_SOFT_BLOCKED);
	printk("%s\n", __func__);
	return ret;

 err_register:
	rfkill_destroy(bt_rfk);
 err_alloc:
	gpio_free(GPIO_BT_nRST);

 err_req_gpio_bt_nrst:
	gpio_free(GPIO_BT_EN);

 err_req_gpio_bt_en:
	return ret;
}

static struct platform_driver bt_device_rfkill = {
	.probe = bt_rfkill_probe,
	.driver = {
		.name = "bt_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init bt_rfkill_init(void)
{
	int rc = 0;
	rc = platform_driver_register(&bt_device_rfkill);

	return rc;
}

module_init(bt_rfkill_init);
MODULE_DESCRIPTION("bt rfkill");
MODULE_LICENSE("GPL");
