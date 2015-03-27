/* linux/arch/arm/plat-samsung/pm-proc.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2004-2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * S3C common power management (suspend to ram) support.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/serial_core.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <mach/hardware.h>

#include <mach/gpio.h>  
#include <mach/map.h>

#include <plat/regs-serial.h>
#include <mach/regs-clock.h>
#include <mach/regs-irq.h>
#include <asm/fiq_glue.h>
#include <asm/irq.h>
#include <mach/regs-gpio.h>

#include <plat/pm.h>
//#include <plat/irq-eint-group.h>
#include <mach/pm-core.h>
#include <plat/gpio-cfg.h> 
/* for external use */
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <mach/gpio-smdkc110.h> 

#define S3C_GPIO_SETPIN_ZERO         0
#define S3C_GPIO_SETPIN_ONE          1
#define S3C_GPIO_SETPIN_NONE	     2

struct gpio_init_data {
	uint num;
	uint cfg;
	uint val;
	uint pud;
	uint drv;
};
static struct gpio_init_data sleep_gph_table[] = {
/*
	{
		.num	= S5PV210_GPH0(0),//HS_HP_DET -> PS_HOLD
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH0(1),//HS_KEY_DET//rear
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(2),//PMIC_IRQ
		.cfg	= S3C_GPIO_SFN(0xf),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(3),//V_EXT_DET //rear
		.cfg	= S3C_GPIO_SFN(0xf),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH0(4),//SCAN_PWRDWN -> PRN_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH0(5),//PRINT_DRV_nDEFAULT -> WL_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(6),//RF_IRQ 
		.cfg	= S3C_GPIO_SFN(0xf),//rising
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(7),//MSR_INT
		.cfg	= S3C_GPIO_SFN(0xf),//rising
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH1(0),//PWR_STATE
		.cfg	=S3C_GPIO_SFN(0xf),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH1(1),//TP_INT
		.cfg	= S3C_GPIO_SFN(0xf),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH1(2),//ICC_INT
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH1(3),//SAM_IO
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN, 
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, */{  
		.num	= S5PV210_GPH1(4),//BT_WAKE_HOST
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},/* {  
		.num	= S5PV210_GPH1(5),//K21_TAMPER_OUT L:active
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{ 
		.num	= S5PV210_GPH1(6),//SAM_RST
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH1(7),//WLAN_WAKEUP_HOST
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},*/{
		.num	= S5PV210_GPH2(0),//KEY
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(1),//KEY
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(2),//KEY
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(3),//KEY
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},/* {
		.num	= S5PV210_GPH2(4),//WL_WAKE_HOST
		.cfg	= S3C_GPIO_SFN(0xf),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH2(5),//NC-> WIFI_REG_ON
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(6),//NC->BT_REG_ON
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},*/ {
		.num	= S5PV210_GPH2(7),//PRN_LAT~ -> LCD_VDDEN/TP_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH3(0),//KEY
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(1),//KEY
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(2),//KEY
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(3),//KEY
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},/* {
		.num	= S5PV210_GPH3(4),//CHG_GOOD
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, 
	{
		.num	= S5PV210_GPH3(4),//CHG_GOOD
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(5),//1170_IRQ
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{  
		.num	= S5PV210_GPH3(6),//SERIAL_SEL1 -> TP_PWR_EN but no use
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, */{
		.num	= S5PV210_GPH3(7),//SERIAL_SEL0 -> SPK_PA_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}
};

static struct gpio_init_data init_gpios[] = {
	{
		.num	= S5PV210_GPB(0),//SPI0_CLK_PRN
		.cfg	= S3C_GPIO_SPECIAL(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPB(1),//NC
		.cfg	= S3C_GPIO_INPUT,//SPI0_CS
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,

	}, {
		.num	= S5PV210_GPB(2),//NC
		.cfg	= S3C_GPIO_INPUT,//SPI0_MISO
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPB(3),//SPI0_MOSI_PRN
		.cfg	= S3C_GPIO_SPECIAL(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPB(4),//NFC_SPI1_CLK
		.cfg	= S3C_GPIO_SPECIAL(2),
		.val	= S3C_GPIO_PULL_DOWN,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPB(5),//NFC_SPI1_CS
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPB(6),//NFC_SPI1_MISO
		.cfg	= S3C_GPIO_SPECIAL(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPB(7),//NFC_SPI1_MOSI
		.cfg	= S3C_GPIO_SPECIAL(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPC0(0),//AUD_RST
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPC0(1),//FM15160_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPC0(2),// 2D_SCAN_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPC0(3),//GPS_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPC0(4),//MSR_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPC1(0),//WIFI_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPC1(1),//KEY_LED_CTL
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPC1(2),//CAM_BL
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPC1(3),//EXT_ILLUM_EN:no use
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPC1(4),//CAM_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPD0(0), //pwm0
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPD0(1),//pwm1
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPD0(2),//pwm2
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPD0(3),//pwm3
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPD1(0),//i2c_sda0
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPD1(1),//i2c_scl0
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPD1(2),//i2c_sda1
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPD1(3),//i2c_scl1
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPD1(4),//i2c_sda2
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPD1(5),//i2c_scl2
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},
/*camera data IO ingore */
	{
		.num	= S5PV210_GPE1(4),//CAM_RST
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},
/*...... lcd data Io ingore*/
	{
		.num	= S5PV210_GPF3(4),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPF3(5),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPG0(0),//Xmmc0CLK
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG0(1),//Xmmc0CMD
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG0(2),//Xmmc0CDn:NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG0(3),//Xmmc0DATA0
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG0(4),//Xmmc0DATA1
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG0(5),//Xmmc0DATA2
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG0(6),//Xmmc0DATA3
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPG1(0),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG1(1),//Xmmc0RSTn
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG1(2),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG1(3),//Xmmc0DATA4
		.cfg	= S3C_GPIO_SFN(3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG1(4),//Xmmc0DATA5
		.cfg	= S3C_GPIO_SFN(3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG1(5),//Xmmc0DATA6
		.cfg	= S3C_GPIO_SFN(3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG1(6),//Xmmc0DATA7
		.cfg	= S3C_GPIO_SFN(3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPG2(0),//Xmmc2CLK
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG2(1),//Xmmc2CMD 
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG2(2),//Xmmc2CDn -> NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG2(3),//Xmmc2Data0
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG2(4),//Xmmc2Data1
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG2(5),//Xmmc2Data2
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG2(6),//Xmmc2Data3
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPG3(0),//Xmmc3CLK
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG3(1),//Xmmc3CMD
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG3(2),//Xmmc3CDn -> NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG3(3),//Xmmc3Data0
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG3(4),//Xmmc3Data1
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG3(5),//Xmmc3Data2
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPG3(6),//Xmmc3Data3
		.cfg	= S3C_GPIO_SFN(2),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH0(0),//HS_HP_DET -> PS_HOLD
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(1),//HS_KEY_DET
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(2),//PMIC_IRQ
		.cfg	= S3C_GPIO_SFN(0xf),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(3),//V_EXT_DET
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(4),//SCAN_PWRDWN -> PRN_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(5),//PRINT_DRV_nDEFAULT -> WL_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(6),//RF_IRQ 
		.cfg	= S3C_GPIO_INPUT,//rising
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH0(7),//MSR_INT
		.cfg	= S3C_GPIO_INPUT,//rising
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH1(0),//PWR_STATE
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH1(1),//TP_INT
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_UP,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH1(2),//ICC_INT
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH1(3),//SAM_IO
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {  
		.num	= S5PV210_GPH1(4),//BT_WAKE_HOST
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {  
		.num	= S5PV210_GPH1(5),//K21_TAMPER_OUT, L:tamper
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, { 
		.num	= S5PV210_GPH1(6),//SAM_RST
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH1(7),//WLAN_WAKEUP_HOST
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH2(0),//KEY
		.cfg	= S3C_GPIO_SFN(0x3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(1),//KEY
		.cfg	= S3C_GPIO_SFN(0x3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(2),//KEY
		.cfg	= S3C_GPIO_SFN(0x3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(3),//KEY
		.cfg	= S3C_GPIO_SFN(0x3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(4),//WL_WAKE_HOST
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_UP,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(5),//NC-> WIFI_REG_ON
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(6),//NC->BT_REG_ON
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH2(7),//PRN_LAT~ -> LCD_VDDEN/TP_PWR_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPH3(0),//KEY
		.cfg	= S3C_GPIO_SFN(0x3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(1),//KEY
		.cfg	= S3C_GPIO_SFN(0x3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(2),//KEY
		.cfg	= S3C_GPIO_SFN(0x3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(3),//KEY
		.cfg	= S3C_GPIO_SFN(0x3),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(4),//CHG_GOOD
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(5),//1170_IRQ
		.cfg	= S3C_GPIO_SFN(0xF),
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_UP,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {  
		.num	= S5PV210_GPH3(6),//SERIAL_SEL1 -> TP_PWR_EN but no use
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPH3(7),//SERIAL_SEL0 -> SPK_PA_EN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPI(0),//AUD_I2S_SCLK
		.cfg	= S3C_GPIO_INPUT,//if S3C_GPIO_PULL_NONE, codec output H
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPI(1),//AUD_I2S_CDCLK
		.cfg	= S3C_GPIO_INPUT,//if S3C_GPIO_PULL_NONE, codec output H
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPI(2),//AUD_I2S_LRCK //if pull it down,leakage will happen in the sleep  
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPI(3),//AUD_I2S_SDI
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPI(4),//AUD_I2S_SDO
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPI(5),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPI(6),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPJ0(0),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ0(1),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ0(2),//DVS1
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ0(3),//DVS2
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ0(4),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ0(5),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ0(6),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ0(7),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPJ1(0),//scan_aim_wake, it no use
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ1(1),//M1170_RST
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X, 
	}, {
		.num	= S5PV210_GPJ1(2),//V210_WAKE_UP_K21 (L wakeup)
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ1(3),//SAM_S0
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ1(4),//SAM_S1
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ1(5),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPJ2(0),//MCR_GPIO_PIN_MOSI
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},	{
		.num	= S5PV210_GPJ2(1),//MCR_GPIO_PIN_MISO
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ2(2),//MCR_GPIO_PIN_CLK
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ2(3),//MCR_GPIO_PIN_CS
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ2(4),//M1170_GPIO_SCL
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPJ2(5),//M1170_GPIO_SDA
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ2(6),//SYS_PWR_V210->HS_HP_DET
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,//if pull it down, the irq will happen when power up
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ2(7),//BT_RST_N
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPJ3(0),//LED_BLUE
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ3(1),//LED_YELLOW
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ3(2),//LED_GREEN
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ3(3),//LED_RED
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ3(4),//TP_RST
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ3(5),//PRN_POUT
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ3(6),//HOST_WAKEUP_WL
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPJ3(7),//LCD_RST
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_GPJ4(0),//WL_RST:no use
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ4(1),//WL_ON~
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ4(2),//HOST_WAKE_BT
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ4(3),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_GPJ4(4),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP01(0),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, 
#if 0
	{

		.num	= S5PV210_MP01(1),//SPI2_CS_FM15160
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP01(2),//SPI2_CLK_FM15160
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP01(3),//SPI2_MOSI_FM15160
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP01(4),//SPI2_MISO_FM15160
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, 
#endif
	{
		.num	= S5PV210_MP01(5),//LCD_SDO
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP01(6),//LCD_SDI
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP01(7),//LCD_SCK
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP02(0),//LCD_CS
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP02(1),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP02(2), //vdd_io pullup
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP02(3),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP03(0),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP03(1),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP03(2),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP03(3),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP03(4),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP03(5),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP03(6),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP03(7),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP04(0),//NC
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP04(1),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP04(2),//PRN_MTA
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP04(3),//PRN_MTA~
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP04(4),//PRN_MTB
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, { 
		.num	= S5PV210_MP04(5),//PRN_MTB~
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {  
		.num	= S5PV210_MP04(6),//PRN_STB
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP04(7),//NC
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP05(0),//ICC_RST
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP05(1),//ICC_CMD, L:active
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP05(2),//ICC_IO
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP05(3),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP05(4),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP05(5),//ICC_VSEL1
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP05(6),//ICC_SEL0
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP05(7),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP06(0),//SAM_PWR
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP06(1),// NC
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ONE,
		.pud	= S3C_GPIO_PULL_NONE,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP06(2),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP06(3),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP06(4),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP06(5),//SPK_PA_EN->Serial_sel0
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP06(6),//NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP06(7),// NC
		.cfg	= S3C_GPIO_INPUT,
		.val	= S3C_GPIO_SETPIN_NONE,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP07(0),// NC
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP07(1),// NC
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP07(2),//TP_PWR_EN->Serial_sel1
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP07(3),// NC
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP07(4),// NC
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	}, {
		.num	= S5PV210_MP07(5),// NC
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP07(6),//LCDVDD_EN -> PNT_LAT~
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},{
		.num	= S5PV210_MP07(7),// NC
		.cfg	= S3C_GPIO_OUTPUT,
		.val	= S3C_GPIO_SETPIN_ZERO,
		.pud	= S3C_GPIO_PULL_DOWN,
		.drv	= S3C_GPIO_DRVSTR_1X,
	},
};

static unsigned int sleep_gpio_table[][3] = {
#if 1
//0	
	{ S5PV210_GPA0(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*WL_UART0_RXD*/
	{ S5PV210_GPA0(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*WL_UART0_TXD*/
	{ S5PV210_GPA0(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},//float
	{ S5PV210_GPA0(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},//float
	{ S5PV210_GPA0(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},	 /* GPIO_BT RXD*/  
	{ S5PV210_GPA0(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /* GPIO_BT TXD*/ 
	{ S5PV210_GPA0(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, 	 /* GPIO_BT_CTS*/  
	{ S5PV210_GPA0(7), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /* GPIO_BT_RTS */  
//7 
//8	
	{ S5PV210_GPA1(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},  //RX2 DEBUG
	{ S5PV210_GPA1(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  //TX2 DEBUG
	{ S5PV210_GPA1(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},  //RX3 k21
	{ S5PV210_GPA1(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_UP}, //TX3 K21
//11	
//12	
	{ S5PV210_GPB(0),  S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},/*print clk*/
	{ S5PV210_GPB(1),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*NC*/
	{ S5PV210_GPB(2),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*NC*/
	{ S5PV210_GPB(3),  S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},/*print mosi*/
	{ S5PV210_GPB(4),  S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},//rf clk
	{ S5PV210_GPB(5),  S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},//rf cs 
	{ S5PV210_GPB(6),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},//rf miso
	{ S5PV210_GPB(7),  S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},//rf mosi
	
//20
	{ S5PV210_GPC0(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},   /*AUD_RST*/
	{ S5PV210_GPC0(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC -> FM15160_PWR_EN*/
	{ S5PV210_GPC0(2), S3C_GPIO_SLP_OUT1,	S3C_GPIO_PULL_NONE},  /*NC -> 2D_SCAN_PWR_EN*/
	{ S5PV210_GPC0(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},   /*NC-> GPS_PWR_EN*/
	{ S5PV210_GPC0(4), S3C_GPIO_SLP_OUT1,	S3C_GPIO_PULL_NONE},  /*NC->MCR_PWR_EN*/
//24
//25
	{ S5PV210_GPC1(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*1D_SCAN_PWR_EN->WIFI_PWR_EN*/
	{ S5PV210_GPC1(1), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},  /*NC->KEY_LED_CTL*/
	{ S5PV210_GPC1(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC ->CAM_BL*/
	{ S5PV210_GPC1(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC->EXT_ILLUM_EN*/
	{ S5PV210_GPC1(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC->CAM_PWR_EN*/
	
//30
	{ S5PV210_GPD0(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*SAM_CLK*/
	{ S5PV210_GPD0(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD_BL*/
	{ S5PV210_GPD0(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*IC_CLK*/
	{ S5PV210_GPD0(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*NC*/
//33
//34	
	{ S5PV210_GPD1(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XI2CSDA0*///ADUIO,CAM,2D
	{ S5PV210_GPD1(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*XI2CCLK0*///ADUIO,CAM,2D
	{ S5PV210_GPD1(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*XI2CSDA1*///TP
	{ S5PV210_GPD1(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*XI2CCLK1*/ //TP
	{ S5PV210_GPD1(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XI2CSDA2*///PMIC
	{ S5PV210_GPD1(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*XI2CCLK2*///PMIC
//39
//40	
	{ S5PV210_GPE0(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*Camera, XCIPCLK*/
	{ S5PV210_GPE0(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
	{ S5PV210_GPE0(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
	{ S5PV210_GPE0(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
	{ S5PV210_GPE0(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
	{ S5PV210_GPE0(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
	{ S5PV210_GPE0(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
	{ S5PV210_GPE0(7), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
//47
//48	
	{ S5PV210_GPE1(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
	{ S5PV210_GPE1(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
	{ S5PV210_GPE1(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Camera*/
	{ S5PV210_GPE1(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*Camera, XCIMCLK*/
	{ S5PV210_GPE1(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*CAM_RST*/
//52
//53	
	{ S5PV210_GPF0(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD XVHSYNC*/
	{ S5PV210_GPF0(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD XVVSYNC*/
	{ S5PV210_GPF0(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD XVVDEN*/
	{ S5PV210_GPF0(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD XVVCLK*/
	{ S5PV210_GPF0(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD XVVD1*/
	{ S5PV210_GPF0(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD XVVD2*/ 
	{ S5PV210_GPF0(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD3*/  
	{ S5PV210_GPF0(7), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD4*/
//60
//61	
	{ S5PV210_GPF1(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD5*/
	{ S5PV210_GPF1(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD6*/
	{ S5PV210_GPF1(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD7*/
	{ S5PV210_GPF1(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD8*/
	{ S5PV210_GPF1(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD XVVD9*/ 
	{ S5PV210_GPF1(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD10*/ 
	{ S5PV210_GPF1(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD11*/
	{ S5PV210_GPF1(7), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD12*/
//68
//69	
	{ S5PV210_GPF2(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD13*/
	{ S5PV210_GPF2(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD14*/
	{ S5PV210_GPF2(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD15*/
	{ S5PV210_GPF2(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD16*/
	{ S5PV210_GPF2(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD17*/
	{ S5PV210_GPF2(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD XVVD18*/
	{ S5PV210_GPF2(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*LCD XVVD19*/
	{ S5PV210_GPF2(7), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD20*/
//76
//77	
	{ S5PV210_GPF3(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD21*/
	{ S5PV210_GPF3(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD22*/
	{ S5PV210_GPF3(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*LCD XVVD23*/
	{ S5PV210_GPF3(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD XVVD24*/
	{ S5PV210_GPF3(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_GPF3(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
//82
//83	
	{ S5PV210_GPG0(0), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_NONE}, /* XMMC0CLK*/
	{ S5PV210_GPG0(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /* XMMC0CMD*/
	{ S5PV210_GPG0(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /* XMMC0CDN:NC*/
	{ S5PV210_GPG0(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC0DATA0*/
	{ S5PV210_GPG0(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC0DATA1*/
	{ S5PV210_GPG0(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC0DATA2*/
	{ S5PV210_GPG0(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC0DATA3*/
//89
//90	 
	{ S5PV210_GPG1(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_GPG1(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*Xmmc0RSTn*/
	{ S5PV210_GPG1(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_GPG1(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC0DATA4 */
	{ S5PV210_GPG1(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC0DATA5 */
	{ S5PV210_GPG1(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC0DATA6 */
	{ S5PV210_GPG1(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC0DATA7 */
//96
//97
	{ S5PV210_GPG2(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC2CLK*/
	{ S5PV210_GPG2(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC2CMD*/
	{ S5PV210_GPG2(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*XMMC2CDN:NC*/
	{ S5PV210_GPG2(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC2DATA0*/
	{ S5PV210_GPG2(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC2DATA1*/
	{ S5PV210_GPG2(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC2DATA2*/
	{ S5PV210_GPG2(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC2DATA3*/
//103
//104
	{ S5PV210_GPG3(0), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},  /*XMMC3CLK*/
	{ S5PV210_GPG3(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE}, /*XMMC3CMD*/
	{ S5PV210_GPG3(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*Xmmc3CDn:NC*/
	{ S5PV210_GPG3(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*XMMC3DATA0*/
	{ S5PV210_GPG3(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*XMMC3DATA1*/
	{ S5PV210_GPG3(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*XMMC3DATA2*/
	{ S5PV210_GPG3(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*XMMC3DATA3*/
//110
//111	/* Alive part ending and off part start*/
	{ S5PV210_GPI(0),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*XI2SCLK0*/
	{ S5PV210_GPI(1),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*XI2SCDCLK0*/
	{ S5PV210_GPI(2),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*XI2SLRCLK0*/
	{ S5PV210_GPI(3),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*XI2SSDI0*/
	{ S5PV210_GPI(4),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*XI2SSDO0_0*/
	{ S5PV210_GPI(5),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_GPI(6),  S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
//117	
//118	
	{ S5PV210_GPJ0(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC*/
	{ S5PV210_GPJ0(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC*/
	{ S5PV210_GPJ0(2), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},  //dvs0
	{ S5PV210_GPJ0(3), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},  //dvs1
	{ S5PV210_GPJ0(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC*/
	{ S5PV210_GPJ0(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC*/
	{ S5PV210_GPJ0(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC*/
	{ S5PV210_GPJ0(7), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
//125
//126	
	{ S5PV210_GPJ1(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*SCAN_AIM_WAKE no USE*/
	{ S5PV210_GPJ1(1), S3C_GPIO_SLP_OUT1,	S3C_GPIO_PULL_NONE}, /*M1170_RST*/
	{ S5PV210_GPJ1(2), S3C_GPIO_SLP_OUT1,	S3C_GPIO_PULL_NONE}, /*V210_WAKE_UP_K21: L wakeup*/
	{ S5PV210_GPJ1(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*SAM_S0*/
	{ S5PV210_GPJ1(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*SAM_S1*/
	{ S5PV210_GPJ1(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
//131
//132
	{ S5PV210_GPJ2(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*MCR_GPIO_PIN_MOSI*/
	{ S5PV210_GPJ2(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*MCR_GPIO_PIN_MISO*/
	{ S5PV210_GPJ2(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*MCR_GPIO_PIN_CLK*/
	{ S5PV210_GPJ2(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*MCR_GPIO_PIN_CS*/
	{ S5PV210_GPJ2(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*M1170_GPIO_SCL*/
	{ S5PV210_GPJ2(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*M1170_GPIO_SDA*/
	{ S5PV210_GPJ2(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*HS_HP_DET*/
	{ S5PV210_GPJ2(7), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*BT_RST_N*/
//139
//140	
	{ S5PV210_GPJ3(0), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},/*LED*/
	{ S5PV210_GPJ3(1), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},/*LED*/
	{ S5PV210_GPJ3(2), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},/*LED*/
	{ S5PV210_GPJ3(3), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},/*LED*/	
	{ S5PV210_GPJ3(4), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},/*TP_RST*/
	{ S5PV210_GPJ3(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*PRN_POUT*/
	{ S5PV210_GPJ3(6), S3C_GPIO_SLP_OUT1,	S3C_GPIO_PULL_NONE},/*HOST_WAKE_WL*/
	{ S5PV210_GPJ3(7), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},/*LCD_RST*/
//147
//148	
	{ S5PV210_GPJ4(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*WL_RST~ : no use*/
	{ S5PV210_GPJ4(1), S3C_GPIO_SLP_PREV,	S3C_GPIO_PULL_DOWN},/*WL_ON~*/
	{ S5PV210_GPJ4(2), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN},/*HOST_WAKE_BT*/
	{ S5PV210_GPJ4(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*NC*/
	{ S5PV210_GPJ4(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*NC*/
//152
//153
	/* memory part */
	{ S5PV210_MP01(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*NC*/
	{ S5PV210_MP01(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*SPI2_CS_FM15160*/
	{ S5PV210_MP01(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*SPI2_CLK_FM15160*/
	{ S5PV210_MP01(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*SPI2_MOSI_FM15160*/
	{ S5PV210_MP01(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*SPI2_MISO_FM15160*/
	{ S5PV210_MP01(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*LCD_SDO*/
	{ S5PV210_MP01(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*LCD_SDI*/
	{ S5PV210_MP01(7), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},  /*LCD_SCLK*/
//160
//161
	{ S5PV210_MP02(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*LCD_CS*/
	{ S5PV210_MP02(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP02(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_NONE},/*external pull up*/
	{ S5PV210_MP02(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
//164	
//165
	{ S5PV210_MP03(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP03(1), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP03(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP03(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP03(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP03(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP03(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*NC*/
	{ S5PV210_MP03(7), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN},/*NC*/
//172	
//173
	{ S5PV210_MP04(0), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP04(1), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /* NC*/
	{ S5PV210_MP04(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*PRN_MTA*/
	{ S5PV210_MP04(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*PRN_MTA~*/
	{ S5PV210_MP04(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*PRN_MTB*/
	{ S5PV210_MP04(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*PRN_MTB~*/
	{ S5PV210_MP04(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*PRN_STB*/
	{ S5PV210_MP04(7), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /* NC*/
//180 
//181
	{ S5PV210_MP05(0), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*ICC_RST*/
	{ S5PV210_MP05(1), S3C_GPIO_SLP_OUT1,	S3C_GPIO_PULL_NONE}, /*ICC_CMD*/
	{ S5PV210_MP05(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*ICC_IO*/
	{ S5PV210_MP05(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP05(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP05(5), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*ICC_VSEL1*/
	{ S5PV210_MP05(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*ICC_VSEL0*/
	{ S5PV210_MP05(7), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /* NC*/
//188
//189
	{ S5PV210_MP06(0), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /*SAM_PWR*/
	{ S5PV210_MP06(1), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /* NC*/
	{ S5PV210_MP06(2), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP06(3), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP06(4), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP06(5), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /*SPK_PA_EN->SERIAL_S0*/
	{ S5PV210_MP06(6), S3C_GPIO_SLP_INPUT,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP06(7), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /*NC*/
//196	
//197
	{ S5PV210_MP07(0), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP07(1), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /*NC*/
	{ S5PV210_MP07(2), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /*TP_PWR_EN-> SERIAL_S1*/
	{ S5PV210_MP07(3), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /* NC*/
	{ S5PV210_MP07(4), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /* NC*/
	{ S5PV210_MP07(5), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /* NC*/
	{ S5PV210_MP07(6), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /*LCDVDD_EN->PRN_LAT~*/
	{ S5PV210_MP07(7), S3C_GPIO_SLP_OUT0,	S3C_GPIO_PULL_DOWN}, /* NC*/
//204
	/* Memory part ending and off part ending */
#endif
};

void s3c_config_gpio_table(void)
{
	u32 i, gpio;

	for (i = 0; i < ARRAY_SIZE(init_gpios); i++) {
		gpio = init_gpios[i].num;
		if (gpio <= S5PV210_MP07(7)) {
			s3c_gpio_cfgpin(gpio, init_gpios[i].cfg);
			s3c_gpio_setpull(gpio, init_gpios[i].pud);

			if (init_gpios[i].val != S3C_GPIO_SETPIN_NONE)
				gpio_set_value(gpio, init_gpios[i].val);

			s3c_gpio_set_drvstrength(gpio, init_gpios[i].drv);
		}
	}
}
void s3c_print_gpio_table(void)
{
	u32 i, gpio, cfgpin, updown,val=2;

	for (i = 0; i < ARRAY_SIZE(init_gpios); i++) {
		gpio = init_gpios[i].num;
		if (gpio <= S5PV210_MP07(7)) {
			cfgpin = s3c_gpio_getcfg(gpio);
			updown = s3c_gpio_getpull(gpio);
			if (init_gpios[i].val != S3C_GPIO_SETPIN_NONE)
				val = gpio_get_value(gpio);
			printk("%s i=%d : gpio = %d, cfgpin = 0x%x, updown = 0x%x, val = 0x%x\n",__func__,i,gpio,cfgpin,updown, val);
		}
	}
}

void s3c_print_sleep_gph_table(void){
/*
	u32 i, gpio, cfgpin, updown,val;

	for (i = 0; i < ARRAY_SIZE(sleep_gph_table); i++) {
		gpio = sleep_gph_table[i].num;
		if (gpio <= S5PV210_GPH3(7)) {
			cfgpin = s3c_gpio_getcfg(gpio);
			updown = s3c_gpio_getpull(gpio);
			if (sleep_gph_table[i].val != S3C_GPIO_SETPIN_NONE){
				val = gpio_get_value(gpio);
				printk("%s i=%d : gpio = %d, cfgpin = 0x%x, updown = 0x%x, val = 0x%x\n",__func__,i,gpio,cfgpin,updown, val);}
			else{
				printk("%s i=%d : gpio = %d, cfgpin = 0x%x, updown = 0x%x, val = 0x%x\n",__func__,i,gpio,cfgpin,updown, sleep_gph_table[i].val);
			}
		}
	}*/
}
void s3c_config_sleep_gph_table(void)
{
	u32 i, gpio;
	for (i =  0; i < ARRAY_SIZE(sleep_gph_table); i++) {
		gpio = sleep_gph_table[i].num;
		if (gpio <= S5PV210_GPH3(7)) {
			s3c_gpio_cfgpin(gpio, sleep_gph_table[i].cfg);
			s3c_gpio_setpull(gpio, sleep_gph_table[i].pud);

			if (sleep_gph_table[i].val != S3C_GPIO_SETPIN_NONE)
				gpio_set_value(gpio, sleep_gph_table[i].val);

			s3c_gpio_set_drvstrength(gpio, sleep_gph_table[i].drv);
		}
	}
}	
void s3c_config_sleep_gpio_table(int array_size, unsigned int (*gpio_table)[3])
{
	u32 i, gpio;
	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_slp_cfgpin(gpio, gpio_table[i][1]);
		s3c_gpio_slp_setpull_updown(gpio, gpio_table[i][2]);
	}
}
/*
void s3c_print_sleep_gpio_table(int array_size, unsigned int (*gpio_table)[3])
{
	u32 i, gpio, cfgpin, updown;
	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		cfgpin = s3c_gpio_get_slp_cfgpin(gpio);
		updown = s3c_gpio_slp_getpull_updown(gpio);
		printk("%s i=%d : gpio = %d, cfgpin = 0x%x, updown = 0x%x\n",__func__,i,gpio,cfgpin,updown);
		
	}
	printk("%s i=%d\n",__func__,i);
}
*/
void s5pv210_init_gpio(void)
{
	s3c_config_gpio_table();
//	s3c_print_gpio_table();
	s3c_config_sleep_gpio_table(ARRAY_SIZE(sleep_gpio_table),sleep_gpio_table);
}


EXPORT_SYMBOL(s5pv210_init_gpio);

void s5pc110_platform_proc(void)
{
//	s3c_print_sleep_gpio_table(ARRAY_SIZE(sleep_gpio_table),sleep_gpio_table);
	return;
}
