/*
 * s5p-lcd.c
 *
 *  Created on: 2011-12-1
 *      Author: sawyer
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/consumer.h>
#if defined(CONFIG_SMDKC110_REV03) || defined(CONFIG_SMDKV210_REV02)
#include <linux/mfd/max8998.h>
#else
#include <linux/mfd/max8698.h>
#endif
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/pwm_backlight.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/clk.h>
#include <linux/usb/ch9.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/skbuff.h>
#include <linux/console.h>
#include <linux/gpio_keys.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/system.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
//#include <mach/regs-mem.h>
#include <mach/gpio.h>
#include <mach/gpio-smdkc110.h>
#include <mach/regs-gpio.h>
//#include <mach/ts-s3c.h>
//#include <mach/adc.h>
//#include <mach/param.h>
//#include <mach/system.h>

#ifdef CONFIG_S3C64XX_DEV_SPI
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>
#endif

#include <linux/usb/gadget.h>

#include <plat/media.h>
#include <mach/media.h>

#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#include <plat/media.h>
#include <mach/media.h>
#endif

#ifdef CONFIG_S5PV210_POWER_DOMAIN
#include <mach/power-domain.h>
#endif

#ifdef CONFIG_VIDEO_S5K4BA
#include <media/s5k4ba_platform.h>
#undef	CAM_ITU_CH_A
#define	CAM_ITU_CH_B
#endif

//#include <media/ov3640_platform.h>
#include <media/tvp5150.h>

#ifdef CONFIG_VIDEO_S5K4EA
#include <media/s5k4ea_platform.h>
#endif

#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>
#include <plat/mfc.h>
#include <plat/iic.h>
#include <plat/pm.h>

#include <plat/sdhci.h>
#include <plat/fimc.h>
#include <plat/csis.h>
#include <plat/jpeg.h>
#include <plat/clock.h>
#include <plat/regs-otg.h>
#include <../../../drivers/video/samsung/s3cfb.h>
#include <mach/s5p-lcd.h>
#include "linux/mfd/wm831x/pdata.h"

#ifdef CONFIG_FB_S3C_LTE480WV
static struct s3cfb_lcd lte480wv = {
	.width	= 800,
	.height	= 480,
	.bpp	= 16,
	.freq	= 70,

	.timing = {
		.h_fp	= 210,
		.h_bp	= 16,
		.h_sw	= 30,
		.v_fp	= 22,
		.v_fpe	= 1,
		.v_bp	= 10,
		.v_bpe	= 1,
		.v_sw	= 13,
	},

	.polarity = {
		.rise_vclk	= 0,
		.inv_hsync	= 1,
		.inv_vsync	= 1,
		.inv_vden	= 0,
	},
};

static void lte480wv_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}

static int lte480wv_backlight_on(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PV210_GPD0(0), "GPD0");

	if (err) {
		printk(KERN_ERR "failed to request GPD0 for "
			"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(0), 1);

	s3c_gpio_cfgpin(S5PV210_GPD0(0), S5PV210_GPD_0_0_TOUT_0);

	gpio_free(S5PV210_GPD0(0));

	return 0;
}

static int lte480wv_backlight_off(struct platform_device *pdev, int onoff)
{
	int err;

	err = gpio_request(S5PV210_GPD0(0), "GPD0");
	if (err) {
		printk(KERN_ERR "failed to request GPD0 for "
				"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(0), 0);

	gpio_free(S5PV210_GPD0(0));
	return 0;
}

static int lte480wv_reset_lcd(struct platform_device *pdev)
{
/*
	int err;

	err = gpio_request(S5PV210_GPH0(6), "GPH0");
	if (err) {
		printk(KERN_ERR "failed to request GPH0 for "
				"lcd reset control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPH0(6), 1);
	mdelay(100);

	gpio_set_value(S5PV210_GPH0(6), 0);
	mdelay(10);

	gpio_set_value(S5PV210_GPH0(6), 1);
	mdelay(10);

	gpio_free(S5PV210_GPH0(6));
*/
	return 0;
}

struct s3c_platform_fb lcd_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,

	.lcd = &lte480wv,
	.cfg_gpio	= lte480wv_cfg_gpio,
	.backlight_on	= lte480wv_backlight_on,
	.backlight_onoff    = lte480wv_backlight_off,
	.reset_lcd	= lte480wv_reset_lcd,
};
#endif

#ifdef CONFIG_FB_S3C_AT070TNA2
static struct s3cfb_lcd lcd = {
	.width	= 1024,
	.height	= 600,
	.bpp	= 24,
	.freq	= 60,

	.timing = {
		.h_fp	= 150,
		.h_bp	= 150,
		.h_sw	= 20,
		.v_fp	= 20,
		.v_fpe	= 1,
		.v_bp	= 10,
		.v_bpe	= 1,
		.v_sw	= 5,
	},

	.polarity = {
		.rise_vclk	= 0,
		.inv_hsync	= 1,
		.inv_vsync	= 1,
		.inv_vden	= 0,
	},
};

static void lcd_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}


static int lcd_backlight_on(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PV210_GPD0(1), "GPD01");

	if (err) {
		printk(KERN_ERR "failed to request GPD01 for "
			"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(1), 1);
	s3c_gpio_setpull(S5PV210_GPD0(1), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S5PV210_GPD0(1), S5PV210_GPD_0_1_TOUT_1);

	gpio_free(S5PV210_GPD0(1));

	return 0;
}

static int lcd_backlight_off(struct platform_device *pdev, int onoff)
{
	int err;

	err = gpio_request(S5PV210_GPD0(1), "GPD0");
	if (err) {
		printk(KERN_ERR "failed to request GPD0 for "
				"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(1), 0);

	gpio_free(S5PV210_GPD0(1));
	return 0;
}

static int lcd_reset_lcd(struct platform_device *pdev)
{
/*
	int err;

	err = gpio_request(S5PV210_GPH0(6), "GPH0");
	if (err) {
		printk(KERN_ERR "failed to request GPH0 for "
				"lcd reset control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPH0(6), 1);
	mdelay(100);

	gpio_set_value(S5PV210_GPH0(6), 0);
	mdelay(10);

	gpio_set_value(S5PV210_GPH0(6), 1);
	mdelay(10);

	gpio_free(S5PV210_GPH0(6));
*/
	return 0;
}

struct s3c_platform_fb lcd_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,

	.lcd = &lcd,
	.cfg_gpio	= lcd_cfg_gpio,
	.backlight_on	= lcd_backlight_on,
	.backlight_onoff    = lcd_backlight_off,
	.reset_lcd	= lcd_reset_lcd,
};
#endif

#ifdef CONFIG_FB_S3C_070TN83
/*Note: may also need to modify ts-s3c for the touch screen*/
static struct s3cfb_lcd at070tn83 = {
	.width	= 800,
	.height	= 480,
	.bpp	= 16,
	.freq	= 60,

	.timing = {
		.h_fp = 40,
		.h_bp = 40,
		.h_sw = 48,
		.v_fp = 13,
		.v_fpe = 1,
		.v_bp = 29,
		.v_bpe = 1,
		.v_sw = 3,
	},

	.polarity = {
		.rise_vclk	= 0,
		.inv_hsync	= 1,
		.inv_vsync	= 1,
		.inv_vden	= 0,
	},
};

static void at070tn83_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}


static int at070tn83_backlight_on(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PV210_GPD0(0), "GPD0");

	if (err) {
		printk(KERN_ERR "failed to request GPD0 for "
			"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(0), 1);

	s3c_gpio_cfgpin(S5PV210_GPD0(0), S5PV210_GPD_0_0_TOUT_0);

	gpio_free(S5PV210_GPD0(0));

	return 0;
}

static int at070tn83_backlight_off(struct platform_device *pdev, int onoff)
{
	int err;

	err = gpio_request(S5PV210_GPD0(0), "GPD0");
	if (err) {
		printk(KERN_ERR "failed to request GPD0 for "
				"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(0), 0);

	gpio_free(S5PV210_GPD0(0));
	return 0;
}

static int at070tn83_reset_lcd(struct platform_device *pdev)
{
/*
	int err;

	err = gpio_request(S5PV210_GPH0(6), "GPH0");
	if (err) {
		printk(KERN_ERR "failed to request GPH0 for "
				"lcd reset control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPH0(6), 1);
	mdelay(100);

	gpio_set_value(S5PV210_GPH0(6), 0);
	mdelay(10);

	gpio_set_value(S5PV210_GPH0(6), 1);
	mdelay(10);

	gpio_free(S5PV210_GPH0(6));
*/
	return 0;
}

struct s3c_platform_fb lcd_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,

	.lcd = &at070tn83,
	.cfg_gpio	= at070tn83_cfg_gpio,
	.backlight_on	= at070tn83_backlight_on,
	.backlight_onoff    = at070tn83_backlight_off,
	.reset_lcd	= at070tn83_reset_lcd,
};
#endif

#ifdef CONFIG_REAL210_VGA
static struct s3cfb_lcd vga = {
	.width	= 800,
	.height	= 600,
	.bpp	= 24,
	.freq	= 60,

	.timing = {
		.h_fp	= 10,
		.h_bp	= 20,
		.h_sw	= 10,
		.v_fp	= 10,
		.v_fpe	= 1,
		.v_bp	= 20,
		.v_bpe	= 1,
		.v_sw	= 10,
	},

	.polarity = {
		.rise_vclk	= 1,
		.inv_hsync	= 1,
		.inv_vsync	= 1,
		.inv_vden	= 0,
	},
};

static void vga_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}


static int vga_backlight_on(struct platform_device *pdev)
{
	return 0;
}

static int vga_backlight_off(struct platform_device *pdev, int onoff)
{

}

static int vga_reset_lcd(struct platform_device *pdev)
{

	return 0;
}

struct s3c_platform_fb lcd_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,

	.lcd = &vga,
	.cfg_gpio	= vga_cfg_gpio,
	.backlight_on	= vga_backlight_on,
	.backlight_onoff    = vga_backlight_off,
	.reset_lcd	= vga_reset_lcd,
};
#endif

#ifdef CONFIG_FB_S3C_ILI9806E
#define LCD_SPI_DELAY 1 
#define S5P_FB_SPI_CLK S5PV210_MP01(7)
#define S5P_FB_SPI_SDO S5PV210_MP01(6)
#define S5P_FB_SPI_SDI S5PV210_MP01(5)
#define S5P_FB_SPI_CS S5PV210_MP02(0)
#define S5P_FB_SPI_RESET S5PV210_GPJ3(7)
//#define S5P_FB_SPI_nPWREN S5PV210_MP07(6)
#define S5P_FB_SPI_nPWREN S5PV210_GPH2(7)

extern unsigned int v70_hw_ver;
static int s3cfb_lq_gpio_request(void)
{

	if(gpio_request(S5P_FB_SPI_CLK, "S5P_FB_SPI_CLK")) {
		printk("spi clk request error!\n");
		return  -1;
	}
	
	if(gpio_request(S5P_FB_SPI_SDO, "S5P_FB_SPI_SDO")) {
		printk("spi mosi request error!\n");
		goto spi_sdo_err;
	}
	
	if(gpio_request(S5P_FB_SPI_SDI, "S5P_FB_SPI_SDI")) {
		printk("spi miso request error!\n");
		goto spi_sdi_err;
	}
	
	if(gpio_request(S5P_FB_SPI_CS, "S5P_FB_SPI_CS")) {
		printk("spi cs request error!\n");
		goto spi_cs_err;
	}
	
	if(gpio_request(S5P_FB_SPI_RESET, "S5P_FB_SPI_RESET")) {
		printk("spi reset request error!\n");
		goto spi_reset_err;
	}

	if(gpio_request(S5P_FB_SPI_nPWREN, "S5P_FB_SPI_nPWREN")) {
		printk("spi pwren request error!\n");
		goto  spi_pwren_err;
	}
	return 0;
	
spi_pwren_err:
	gpio_free(S5P_FB_SPI_nPWREN);
spi_reset_err:
	gpio_free(S5P_FB_SPI_RESET);
spi_cs_err:
	gpio_free(S5P_FB_SPI_CS);
spi_sdi_err:
	gpio_free(S5P_FB_SPI_SDO);
spi_sdo_err:
	gpio_free(S5P_FB_SPI_CLK);
	return  -1;
}

static void s3cfb_free_gpio_lq(void) 
{
	gpio_free(S5P_FB_SPI_CLK);
	gpio_free(S5P_FB_SPI_SDO);
	gpio_free(S5P_FB_SPI_SDI);
	gpio_free(S5P_FB_SPI_CS);
	gpio_free(S5P_FB_SPI_RESET);
	gpio_free(S5P_FB_SPI_nPWREN);
}

static void s3cfb_set_spi_gpio_shutdown(void)  
{
	s3cfb_lq_gpio_request();
	
	gpio_direction_input(S5P_FB_SPI_RESET);//reset
	gpio_direction_input(S5P_FB_SPI_CLK);  //spi clk
	gpio_direction_input(S5P_FB_SPI_SDO);  //spi sdo
	gpio_direction_input(S5P_FB_SPI_SDI);  
	gpio_direction_input(S5P_FB_SPI_CS);   //spi cs
//	gpio_direction_output(S5P_FB_SPI_nPWREN, 1);   //spi pwr

	s3c_gpio_setpull(S5P_FB_SPI_RESET, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(S5P_FB_SPI_CLK, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(S5P_FB_SPI_SDO, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(S5P_FB_SPI_SDI, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(S5P_FB_SPI_CS, S3C_GPIO_PULL_DOWN);
//	s3c_gpio_setpull(S5P_FB_SPI_nPWREN, S3C_GPIO_PULL_NONE);
	s3cfb_free_gpio_lq();
}

static void s3cfb_set_gpio_lq(void)
{
	
	gpio_direction_output(S5P_FB_SPI_RESET, 1);//reset
	gpio_direction_output(S5P_FB_SPI_CLK, 1);  //spi clk
	gpio_direction_output(S5P_FB_SPI_SDO, 1);  //spi sdo
	gpio_direction_input(S5P_FB_SPI_SDI);  //spi sdi
	gpio_direction_output(S5P_FB_SPI_CS, 1);   //spi cs
	if (v70_hw_ver == 30) 
		gpio_direction_output(S5P_FB_SPI_nPWREN, 0);
	else
		gpio_direction_output(S5P_FB_SPI_nPWREN, 1);   //spi pwr

	s3c_gpio_setpull(S5P_FB_SPI_RESET, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(S5P_FB_SPI_CLK, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(S5P_FB_SPI_SDO, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(S5P_FB_SPI_SDI, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(S5P_FB_SPI_CS, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(S5P_FB_SPI_nPWREN, S3C_GPIO_PULL_NONE);
}

inline void lq_spi_lcd_pwren(int value)
{
	if (v70_hw_ver == 30)
		value = !value;

	if(value)
		gpio_set_value(S5P_FB_SPI_nPWREN,1);
	else
		gpio_set_value(S5P_FB_SPI_nPWREN,0);
}

inline void lq_spi_lcd_reset(int value)
{
	gpio_set_value(S5P_FB_SPI_RESET,value);
}

inline void lq_spi_lcd_clk(int value)
{
        gpio_set_value(S5P_FB_SPI_CLK, value);
}

inline void lq_spi_lcd_data(int value)
{
        gpio_set_value(S5P_FB_SPI_SDO, value);
}

inline void lq_spi_lcd_cs(int value)
{
        gpio_set_value(S5P_FB_SPI_CS, value);
}

//************** write parameter
static  void write_lcdcom(unsigned char index)
  {
	 //unsigned char HX_WR_COM=0x74;//74
	 unsigned char  i;
	 
	 lq_spi_lcd_cs(1);   //SET_HX_CS;
	 lq_spi_lcd_clk(1);  //SET_HX_CLK;
	 lq_spi_lcd_data(1); //SET_HX_SDO;
	
	 lq_spi_lcd_cs(0);   // output "L" set CS
	 udelay(LCD_SPI_DELAY);
	 
	   lq_spi_lcd_clk(0); 
	  // udelay(LCD_SPI_DELAY);
   
	   lq_spi_lcd_data(0);//SDO transfer "0"(command)
       udelay(LCD_SPI_DELAY); 
        
	   lq_spi_lcd_clk(1);
	   udelay(LCD_SPI_DELAY); 

	   for(i=0;i<8;i++) // 8 Data
	   {
		    lq_spi_lcd_clk(0); //CLK "L"

		   if ( index& 0x80)
		    	lq_spi_lcd_data(1);
		   else
		    	lq_spi_lcd_data(0);

		    index<<= 1;
		    udelay(LCD_SPI_DELAY); 
		    
		    lq_spi_lcd_clk(1); //CLK "H"
		    udelay(LCD_SPI_DELAY); 
	   }
	   
	   //lq_spi_lcd_clk( 1);
	   //udelay(LCD_SPI_DELAY); 

	   lq_spi_lcd_cs(1); //output "H" release cs
       udelay(LCD_SPI_DELAY);
  }
/************************ write register ***************************/
static  void write_lcdregister(unsigned char data)
{
	   //unsigned char HX_WR_COM=0x76;//76
	 unsigned char i;
	   
	 lq_spi_lcd_cs(1);   //SET_HX_CS;
	 lq_spi_lcd_clk(1);  //SET_HX_CLK;
	 lq_spi_lcd_data(1); //SET_HX_SDO;
	
	 lq_spi_lcd_cs(0);   // output "L" set CS
	 udelay(LCD_SPI_DELAY); 

	  lq_spi_lcd_clk(0);  

	  lq_spi_lcd_data(1); //SDO transfer "1"(data)
	    
	  udelay(LCD_SPI_DELAY); 

	  lq_spi_lcd_clk(1);
	  udelay(LCD_SPI_DELAY); 

	  for(i=0;i<8;i++) // 8 Data
	  {
		  lq_spi_lcd_clk(0);

		  if ( data& 0x80)
	   			lq_spi_lcd_data(1);
	   		else
	    		lq_spi_lcd_data(0);
	    		data<<= 1;
	    	udelay(LCD_SPI_DELAY);

            //CLK from "L" to "H"
	    	lq_spi_lcd_clk(1);
		  	udelay(LCD_SPI_DELAY);
	    }
	    
	   	//lq_spi_lcd_clk( 1);
	  	udelay(LCD_SPI_DELAY);  

	   	//SET_HX_CS;
		lq_spi_lcd_cs(1);   //SET_HX_CS;
	   	udelay(LCD_SPI_DELAY);
  }

//power on

static void ili9806e_reset_lcd(void)
{
	lq_spi_lcd_pwren(1);
	msleep(1); 
	lq_spi_lcd_reset(0);
	msleep(1);  
	lq_spi_lcd_reset(1);
	msleep(120);  
}

static void lq_init_ldi(void)  
{
	printk("Lcd init DDI\n");
	s3cfb_lq_gpio_request();
	s3cfb_set_gpio_lq();
	ili9806e_reset_lcd();
	
	
	 // ****************************** EXTC Command Set enable register ******************************//
	write_lcdcom(0xFF);
	write_lcdregister(0xFF);
	write_lcdregister(0x98);
	write_lcdregister(0x06);
	write_lcdregister(0x04);
	write_lcdregister(0x01);

	write_lcdregister(0x08);
	write_lcdregister(0x10);

	write_lcdregister(0x21);
	write_lcdregister(0x01);

	write_lcdregister(0x30);
	write_lcdregister(0x02);

	write_lcdregister(0x31);
	write_lcdregister(0x02);

	write_lcdregister(0x40);
	write_lcdregister(0x15);
		msleep(1);
	write_lcdregister(0x41);
	write_lcdregister(0x33);
		msleep(1);
	write_lcdregister(0x42);
	write_lcdregister(0x01);
		msleep(1);
	write_lcdregister(0x43);
	write_lcdregister(0x02);
		msleep(1);
	write_lcdregister(0x44);
	write_lcdregister(0x02);
		msleep(1);
	write_lcdregister(0x60);
	write_lcdregister(0x03);

	write_lcdregister(0x61);
	write_lcdregister(0x00);

	write_lcdregister(0x62);
	write_lcdregister(0x08);

	write_lcdregister(0x63);
	write_lcdregister(0x04);

	write_lcdregister(0x52);
	write_lcdregister(0x00);

	write_lcdregister(0x53);
	write_lcdregister(0x6e);
	//****************************** Gamma Setting ******************************//
	write_lcdcom(0xA0);
	write_lcdregister(0x00);

	write_lcdcom(0xA1);
	write_lcdregister(0x03);

	write_lcdcom(0xA2);
	write_lcdregister(0x0a);

	write_lcdcom(0xA3);
	write_lcdregister(0x0f);

	write_lcdcom(0xA4);
	write_lcdregister(0x09);

	write_lcdcom(0xA5);
	write_lcdregister(0x18);

	write_lcdcom(0xA6);
	write_lcdregister(0x0A);

	write_lcdcom(0xA7);
	write_lcdregister(0x09);

	write_lcdcom(0xA8);
	write_lcdregister(0x03);

	write_lcdcom(0xA9);
	write_lcdregister(0x07);

	write_lcdcom(0xAA);
	write_lcdregister(0x05);

	write_lcdcom(0xAB);
	write_lcdregister(0x03);

	write_lcdcom(0xAC);
	write_lcdregister(0x0b);

	write_lcdcom(0xAD);
	write_lcdregister(0x2e);

	write_lcdcom(0xAE);
	write_lcdregister(0x28);

	write_lcdcom(0xAF);
	write_lcdregister(0x00);
	//******************************Nagitive ******************************//
	write_lcdcom(0xC0);
	write_lcdregister(0x00);

	write_lcdcom(0xC1);
	write_lcdregister(0x02);

	write_lcdcom(0xC2);
	write_lcdregister(0x09);

	write_lcdcom(0xC3);
	write_lcdregister(0x0f);

	write_lcdcom(0xC4);
	write_lcdregister(0x08);

	write_lcdcom(0xC5);
	write_lcdregister(0x17);

	write_lcdcom(0xC6);
	write_lcdregister(0x0b);

	write_lcdcom(0xC7);
	write_lcdregister(0x08);

	write_lcdcom(0xC8);
	write_lcdregister(0x03);

	write_lcdcom(0xC9);
	write_lcdregister(0x08);

	write_lcdcom(0xCA);
	write_lcdregister(0x06);

	write_lcdcom(0xCB);
	write_lcdregister(0x04);

	write_lcdcom(0xCC);
	write_lcdregister(0x0a);

	write_lcdcom(0xCD);
	write_lcdregister(0x2d);

	write_lcdcom(0xCE);
	write_lcdregister(0x29);

	write_lcdcom(0xCF);
	write_lcdregister(0x00);
	//****************************** Page 6 Command ******************************//
	write_lcdcom(0xFF);
	write_lcdregister(0xFF);
	write_lcdregister(0x98);
	write_lcdregister(0x06);
	write_lcdregister(0x04);
	write_lcdregister(0x06);

	write_lcdcom(0x00);
	write_lcdregister(0x21);

	write_lcdcom(0x01);
	write_lcdregister(0x06);

	write_lcdcom(0x02);
	write_lcdregister(0x20);

	write_lcdcom(0x03);
	write_lcdregister(0x00);

	write_lcdcom(0x04);
	write_lcdregister(0x01);

	write_lcdcom(0x05);
	write_lcdregister(0x01);

	write_lcdcom(0x06);
	write_lcdregister(0x80);

	write_lcdcom(0x07);
	write_lcdregister(0x02);

	write_lcdcom(0x08);
	write_lcdregister(0x05);

	write_lcdcom(0x09);
	write_lcdregister(0x00);

	write_lcdcom(0x0A);
	write_lcdregister(0x00);

	write_lcdcom(0x0B);
	write_lcdregister(0x00);

	write_lcdcom(0x0C);
	write_lcdregister(0x01);

	write_lcdcom(0x0D);
	write_lcdregister(0x01);

	write_lcdcom(0x0E);
	write_lcdregister(0x00);

	write_lcdcom(0x0F);
	write_lcdregister(0x00);

	write_lcdcom(0x10);
	write_lcdregister(0xF0);

	write_lcdcom(0x11);
	write_lcdregister(0xF4);

	write_lcdcom(0x12);
	write_lcdregister(0x00);

	write_lcdcom(0x13);
	write_lcdregister(0x00);

	write_lcdcom(0x14);
	write_lcdregister(0x00);

	write_lcdcom(0x15);
	write_lcdregister(0xC0);

	write_lcdcom(0x16);
	write_lcdregister(0x08);

	write_lcdcom(0x17);
	write_lcdregister(0x00);

	write_lcdcom(0x18);
	write_lcdregister(0x00);

	write_lcdcom(0x19);
	write_lcdregister(0x00);

	write_lcdcom(0x1A);
	write_lcdregister(0x00);

	write_lcdcom(0x1B);
	write_lcdregister(0x00);

	write_lcdcom(0x1C);
	write_lcdregister(0x00);

	write_lcdcom(0x1D);
	write_lcdregister(0x00);

	write_lcdcom(0x20);
	write_lcdregister(0x02);

	write_lcdcom(0x21);
	write_lcdregister(0x23);

	write_lcdcom(0x22);
	write_lcdregister(0x45);

	write_lcdcom(0x23);
	write_lcdregister(0x67);

	write_lcdcom(0x24);
	write_lcdregister(0x01);

	write_lcdcom(0x25);
	write_lcdregister(0x23);

	write_lcdcom(0x26);
	write_lcdregister(0x45);

	write_lcdcom(0x27);
	write_lcdregister(0x67);

	write_lcdcom(0x30);
	write_lcdregister(0x13);

	write_lcdcom(0x31);
	write_lcdregister(0x22);

	write_lcdcom(0x32);
	write_lcdregister(0x22);

	write_lcdcom(0x33);
	write_lcdregister(0x22);

	write_lcdcom(0x34);
	write_lcdregister(0x22);

	write_lcdcom(0x35);
	write_lcdregister(0xbb);

	write_lcdcom(0x36);
	write_lcdregister(0xaa);

	write_lcdcom(0x37);
	write_lcdregister(0xdd);

	write_lcdcom(0x38);
	write_lcdregister(0xcc);

	write_lcdcom(0x39);
	write_lcdregister(0x66);

	write_lcdcom(0x3A);
	write_lcdregister(0x77);

	write_lcdcom(0x3B);
	write_lcdregister(0x22);

	write_lcdcom(0x3C);
	write_lcdregister(0x22);

	write_lcdcom(0x3D);
	write_lcdregister(0x22);

	write_lcdcom(0x3E);
	write_lcdregister(0x22);

	write_lcdcom(0x3F);
	write_lcdregister(0x22);

	write_lcdcom(0x40);
	write_lcdregister(0x22);
	//****************************** Page 0 Command ******************************//
	write_lcdcom(0xFF);
	write_lcdregister(0xFF);
	write_lcdregister(0x98);
	write_lcdregister(0x06);
	write_lcdregister(0x04);
	write_lcdregister(0x00);

	write_lcdcom(0x3a);
	write_lcdregister(0x77);

	write_lcdcom(0x11);
	msleep(220);
	write_lcdcom(0x29);
	s3cfb_free_gpio_lq();
	printk("LCD Init End\n");	
}

static struct s3cfb_lcd ili9806e = {
	.width = 480,
	.height = 800,
	.bpp = 32,
        .freq   = 66,

	.timing = {
		.h_fp = 15,
		.h_bp = 13,
		.h_sw = 8, 
		.v_fp = 5,
		.v_fpe = 1,
		.v_bp = 7,
		.v_bpe = 1,
		.v_sw = 1,
	},
	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
        .init_ldi = lq_init_ldi,
};

static void ili9806e_shutdown_gpio(struct platform_device *pdev) 
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_INPUT);
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_DOWN); 
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_INPUT);
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_DOWN); 
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_INPUT);
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_DOWN); 
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_INPUT);
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_DOWN); 
	}
	
	s3cfb_set_spi_gpio_shutdown();

}

static void ili9806e_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}

static int ili9806e_backlight_on(struct platform_device *pdev)
{

	int err;

	err = gpio_request(S5PV210_GPD0(1), "GPD01");

	if (err) {
		printk(KERN_ERR "failed to request GPD01 for "
			"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(1), 1);
	s3c_gpio_setpull(S5PV210_GPD0(1), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S5PV210_GPD0(1), S5PV210_GPD_0_1_TOUT_1);

	gpio_free(S5PV210_GPD0(1));

	return 0;
}

static int ili9806e_backlight_off(struct platform_device *pdev, int onoff)
{
	int err;

	err = gpio_request(S5PV210_GPD0(1), "GPD01");
	if (err) {
		printk(KERN_ERR "failed to request GPD01 for "
				"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPD0(1), 0);

	gpio_free(S5PV210_GPD0(1));
	return 0;
}

struct s3c_platform_fb lcd_fb_data __initdata = {
	.hw_ver	= 0x62,
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,

	.lcd = &ili9806e,

	.cfg_gpio	= ili9806e_cfg_gpio,
	.shutdown_gpio = ili9806e_shutdown_gpio, 
	.backlight_on	= ili9806e_backlight_on,
	.backlight_onoff    = ili9806e_backlight_off,
};
#endif
