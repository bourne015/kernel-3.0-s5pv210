/* linux/drivers/media/video/hm5065.c
 *
 *   www.xcembed.com
 *
 * Driver for hm5065 (QSXGA camera) from Samsung Electronics
 * 1/6" 3.2Mp CMOS Image Sensor SoC with an Embedded Image Processor
 * supporting MIPI CSI-2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-i2c-drv.h>
#include <media/hm5065_platform.h>
#include <linux/gpio.h>


#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "hm5065.h"
#include <mach/gpio-smdkc110.h>

#define HM5065_DRIVER_NAME	"HM5065"
#if 0
#define debug_hm5065	printk("###cgh:%s,line:%d\n", __func__, __LINE__);
#else
#define debug_hm5065	do{}while(0);
#endif

/* Default resolution & pixelformat. plz ref hm5065_platform.h */
#define DEFAULT_RES		WVGA	/* Index of resoultion */
#define DEFAUT_FPS_INDEX	S5K4EA_15FPS
#define DEFAULT_FMT		V4L2_PIX_FMT_YUYV	/* YUV422 */


static struct timer_list	flash_timer;


int initialised=0;
int flash_mode=1;


/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

static int hm5065_init(struct v4l2_subdev *sd, u32 val);

#define  HM5065_R(waddr,val)	do{hm5065_i2c_read(waddr, &val); }while(0)
#define  HM5065_W(waddr,val)	do{hm5065_i2c_write(waddr, val); }while(0)
#define  HM5065_SLEEP(ms)		do{mdelay(ms);}while(0)


/* Camera functional setting values configured by user concept */
struct hm5065_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_CAMERA_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CAMERA_CONTRAST */
	unsigned int saturation;/* V4L2_CID_CAMERA_SATURATION */
	unsigned int sharpness;	/* V4L2_CID_CAMERA_SHARPNESS */
	unsigned int glamour;
};

struct hm5065_state {
	struct hm5065_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct hm5065_userset userset;
	int framesize_index;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
	int check_previewdata;
};

enum {
	HM5065_PREVIEW_VGA,
};

struct hm5065_enum_framesize {
	unsigned int index;
	unsigned int width;
	unsigned int height;
};

struct hm5065_enum_framesize hm5065_framesize_list[] = {
	/* { S5K4EA_PREVIEW_WVGA,  800,  480 }, */
	{ HM5065_PREVIEW_VGA, 640, 480 }
};

static inline struct hm5065_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct hm5065_state, sd);
}

/*
 * S5K4EA register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
static inline int hm5065_write(struct v4l2_subdev *sd, u8 addr, u8 val, u8 length)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[1];
	unsigned char reg[4];
	int err = 0;
	int retry = 0;


	if (!client->adapter)
		return -ENODEV;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 4;
	msg->buf = reg;

	reg[0] = addr >> 8;
	reg[1] = addr & 0xff;
	reg[2] = val & 0xff;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return err;	/* Returns here on success */

	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%02x%02x, "
			"value: 0x%02x%02x\n", __func__,
			reg[0], reg[1], reg[2], reg[3]);
		retry++;
		goto again;
	}

	return err;
}

/*********************************************************************************/

#if 0
static int hm5065_i2c_rxdata(struct i2c_client *client, unsigned short saddr, unsigned char *rxdata,
		int length)
{
	struct i2c_msg msg[] = {
		{
			.addr	= saddr,
			.flags	= 0,
			.len	= 2,
			.buf	= rxdata,
		},
		{
			.addr	= saddr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	if (i2c_transfer(client->adapter, msg, 2) < 0) {
		printk("hm5065_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int hm5065_i2c_read(struct v4l2_subdev *sd, unsigned short  saddr, unsigned int raddr, unsigned int *rdata)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int rc = 0;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = hm5065_i2c_rxdata(client, saddr, buf, 1);
	if (rc < 0) {
		printk("hm5065_i2c_read_byte failed!\n");
		return rc;
	}

	*rdata = buf[0];

	return rc;
}
#endif

static inline int hm5065_read(struct v4l2_subdev *sd, u16 addr, u8 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[2];
	unsigned char reg[10];
	int err = 0;
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

again:
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = &reg[0];
	reg[0] = addr >> 8;
	reg[1] = addr & 0xff;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &reg[2];	

	err = i2c_transfer(client->adapter, msg, 2);
	if (err >= 0)
	{
	    *val = reg[2];	
		return err;	/* Returns here on success */
	}
	/* abnormal case: retry 5 times */
	if (retry < 3) {
		printk("%s: address: 0x%02x%02x, " \
			" err:%d\n", __func__, \
			reg[0], reg[1], err);
		retry++;
		goto again;
	}


	printk("%s: address: 0x%02x%02x, " \
			" err:%d\n", __func__, \
			reg[0], reg[1], err);

	return err;
}

static int hm5065_i2c_write(struct v4l2_subdev *sd, unsigned char addr1, unsigned char addr2, u8 i2c_data, u8 length)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	unsigned char msgbuf[3];
	struct i2c_msg msg = {
		client->addr,
		0,
		sizeof(addr1)+sizeof(addr2)+length,
		msgbuf
	};

	msgbuf[0] = addr1;
	msgbuf[1] = addr2;
	msgbuf[2] = i2c_data & 0xFF;
//	memcpy(msgbuf[sizeof(addr)], i2c_data, length);

	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

static int hm5065_i2c_write_serials(struct v4l2_subdev *sd, struct hm5065_reg *serial, uint32_t array_length, char *pdesc)
{
#if 0
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned int rdata;
#endif

	int32_t  rc = 0;
	int32_t i;

	for (i=0; i<array_length; i++) {
		if ((serial[i].addr1 == REG_DELAY) &&(serial[i].addr2 == REG_DELAY)){
			mdelay(200);
		}
		else {
			rc = hm5065_i2c_write(sd, serial[i].addr1, serial[i].addr2, serial[i].val, 1);
			if (rc < 0) {
				printk(KERN_INFO "[xxm]: %s write failed. rc = %d\n", __func__, rc);
				goto init_probe_fail;
			}
#if 0
			//printk("hm5065_i2c_write, reg:%#x, val:%#x\n", serial[i].addr, serial[i].val);
			
			rc = hm5065_i2c_read(sd, client->addr, serial[i].addr, &rdata);
			if (rc < 0) {
				printk(KERN_INFO "[xxm]: %s hm5065_i2c_read failed. rc = %d\n", __func__, rc);
				goto init_probe_fail;
			}
			if((short)rdata != serial[i].val)
				printk("hm5065_i2c_read , reg:%#x, val:%#x, but read:%#x\n", serial[i].addr, serial[i].val, rdata);
#endif
		}
	}
#if 0
	for (i=0x3808; i<0x380c; i++) {
		rc = hm5065_i2c_read(sd, client->addr, i, &rdata);
		if (rc < 0) {
			printk(KERN_INFO "[xxm]: %s hm5065_i2c_read failed. rc = %d\n", __func__, rc);
			goto init_probe_fail;
		}
		printk("hm5065_i2c_read , reg:%#x, val:%#x\n", i, rdata);
	}
#endif
	printk(KERN_INFO  "[xxm]: %s %s serials write suceesfully.\n", __func__, (pdesc ? pdesc : (" ")) );
	return rc;	

init_probe_fail:
	return rc;
}

/*********************************************************************************/



static const char *hm5065_querymenu_wb_preset[] = {
	"WB sunny",
	"WB cloudy",
	"WB Tungsten",
	"WB Fluorescent",
	NULL
};

static const char *hm5065_querymenu_effect_mode[] = {
	"Effect Normal",
	"Effect Monochrome",
	"Effect Sepia",
	"Effect Negative",
	"Effect Aqua",
	"Effect Sketch",
	NULL
};

static const char *hm5065_querymenu_ev_bias_mode[] = {
	"-3EV",	"-2,1/2EV", "-2EV", "-1,1/2EV",
	"-1EV", "-1/2EV", "0", "1/2EV",
	"1EV", "1,1/2EV", "2EV", "2,1/2EV",
	"3EV", NULL
};

static struct v4l2_queryctrl hm5065_controls[] = {
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 0,
		.maximum = 10000,
		.step = 1,
		.default_value = 0,	/* FIXME */
	},
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(hm5065_querymenu_wb_preset) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CAMERA_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto white balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Exposure bias",
		.minimum = 0,
		.maximum = ARRAY_SIZE(hm5065_querymenu_ev_bias_mode) - 2,
		.step = 1,
		.default_value = (ARRAY_SIZE(hm5065_querymenu_ev_bias_mode)
				- 2) / 2,	/* 0 EV */
	},
	{
		.id = V4L2_CID_CAMERA_EFFECT,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Image Effect",
		.minimum = 0,
		.maximum = ARRAY_SIZE(hm5065_querymenu_effect_mode) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CAMERA_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_CAMERA_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_CAMERA_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
};

static int hm5065_reset(struct v4l2_subdev *sd)
{
	return hm5065_init(sd, 0);
}



static int hm5065_flash_on(void)
{
	s3c_gpio_setpull(CAM_FLASH, S3C_GPIO_PULL_NONE);
	gpio_direction_output(CAM_FLASH, 1);	
	//printk("%s:value=%d\n",__func__,gpio_get_value(CAM_FLASH));
	return 0;
}

static int hm5065_flash_off(void)
{
	s3c_gpio_setpull(CAM_FLASH, S3C_GPIO_PULL_DOWN);
	gpio_direction_output(CAM_FLASH, 0);	
	//printk("%s:value=%d\n",__func__,gpio_get_value(CAM_FLASH));

	return 0;
}


static void flash_timer_fn(unsigned long arg)
{
	int rv;
	hm5065_flash_off();	
	del_timer(&flash_timer);
}	
	

int flash_timer_init(void)
{
	hm5065_flash_on();
	init_timer(&flash_timer);
	flash_timer.function = flash_timer_fn;
	mod_timer(&flash_timer, jiffies + 300);
	return 0;
}



const char **hm5065_ctrl_get_menu(u32 id)
{
	switch (id) {
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return hm5065_querymenu_wb_preset;

	case V4L2_CID_CAMERA_EFFECT:
		return hm5065_querymenu_effect_mode;

	case V4L2_CID_EXPOSURE:
		return hm5065_querymenu_ev_bias_mode;

	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *hm5065_find_qctrl(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hm5065_controls); i++)
		if (hm5065_controls[i].id == id)
			return &hm5065_controls[i];

	return NULL;
}

static int hm5065_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hm5065_controls); i++) {
		if (hm5065_controls[i].id == qc->id) {
			memcpy(qc, &hm5065_controls[i],
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int hm5065_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;

	qctrl.id = qm->id;
	hm5065_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, hm5065_ctrl_get_menu(qm->id));
}


/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * freq : in Hz
 * flag : not supported for now
 */
static int hm5065_s_crystal_freq(struct v4l2_subdev *sd, u32  freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int hm5065_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}




static int hm5065_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;
	int array_length;

#if 1
		struct hm5065_state *state = to_state(sd);
		struct i2c_client *client = v4l2_get_subdevdata(sd);
	
		state->pix.pixelformat = fmt->fmt.pix.pixelformat;
		state->pix.width=fmt->fmt.pix.width;
		state->pix.height=fmt->fmt.pix.height;
	
		printk("fmt->fmt: width=%d, height=%d\n",fmt->fmt.pix.width,fmt->fmt.pix.height);
		printk("state: width=%d, height=%d\n",state->pix.width,state->pix.height);
#endif


#if 1
	switch(fmt->fmt.pix.pixelformat)
	{
		case V4L2_PIX_FMT_NV21:
			printk("V4L2_PIX_FMT_NV21\n");	
		case V4L2_PIX_FMT_YUV420:
			printk("V4L2_PIX_FMT_YUV420\n");	
		case V4L2_PIX_FMT_NV12:
			printk("V4L2_PIX_FMT_NV12\n");
		case V4L2_PIX_FMT_YUYV:	//FIXME
			//hm5065_flash_off();
			if(initialised){
				array_length = ARRAY_SIZE(hm5065_setting_30fps_VGA_640_480);
				err = hm5065_i2c_write_serials(sd, hm5065_setting_30fps_VGA_640_480, array_length, "hm5605 preview param");
			}
			break;
		//case V4L2_PIX_FMT_YUYV:
		//	printk("V4L2_PIX_FMT_YUYV\n");
		//	if(flash_mode!=FLASH_MODE_OFF)
		//		flash_timer_init();
		//	array_length = ARRAY_SIZE(hm5065_setting_15fps_QSXGA_2592_1944);
		//	err = hm5065_i2c_write_serials(sd, hm5065_setting_15fps_QSXGA_2592_1944, array_length, "hm5605 capture param");
		//	mdelay(100);
		//	break;
		default:
			printk("unknow pixel format\n");
			break;	
	}
#endif
	return err;
}

static int hm5065_enum_framesizes(struct v4l2_subdev *sd,
					struct v4l2_frmsizeenum *fsize)
{
#if 0
	struct hm5065_state *state = to_state(sd);
	int num_entries = sizeof(hm5065_framesize_list) /
				sizeof(struct hm5065_enum_framesize);
	struct hm5065_enum_framesize *elem;
	int index = 0;
	int i = 0;

	/* The camera interface should read this value, this is the resolution
	 * at which the sensor would provide framedata to the camera i/f
	 *
	 * In case of image capture,
	 * this returns the default camera resolution (WVGA)
	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	index = state->framesize_index;

	for (i = 0; i < num_entries; i++) {
		elem = &hm5065_framesize_list[i];
		if (elem->index == index) {
			fsize->discrete.width =
			    hm5065_framesize_list[index].width;
			fsize->discrete.height =
			    hm5065_framesize_list[index].height;
			return 0;
		}
	}
	return -EINVAL;
#else
	struct hm5065_state *state = to_state(sd);
	struct hm5065_enum_framesize *elem;
	int index = 0;
	int i = 0;
	int array_length,err=0;

	/* The camera interface should read this value, this is the resolution
	 * at which the sensor would provide framedata to the camera i/f
	 *
	 * In case of image capture,
	 * this returns the default camera resolution (WVGA)
	 */
#if 1	 
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	index = state->framesize_index;
	printk("state: width=%d, height=%d\n",state->pix.width,state->pix.height);

	fsize->discrete.width =	state->pix.width;
	fsize->discrete.height = state->pix.height;
#endif

#endif
}

static int hm5065_enum_frameintervals(struct v4l2_subdev *sd,
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int hm5065_enum_fmt(struct v4l2_subdev *sd,
				struct v4l2_fmtdesc *fmtdesc)
{
	int err = 0;

	return err;
}

static int hm5065_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}

static int hm5065_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	int err = 0;

	return err;
}

static int hm5065_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	int err = 0;

	return err;
}

static int hm5065_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hm5065_state *state = to_state(sd);
	struct hm5065_userset userset = state->userset;
	int err = 0;

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		ctrl->value = userset.auto_wb;
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		ctrl->value = userset.manual_wb;
		break;
	case V4L2_CID_CAMERA_EFFECT:
		ctrl->value = userset.effect;
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		ctrl->value = userset.contrast;
		break;
	case V4L2_CID_CAMERA_SATURATION:
		ctrl->value = userset.saturation;
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		ctrl->value = userset.saturation;
		break;
	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
	case V4L2_CID_CAM_JPEG_MEMSIZE:
	case V4L2_CID_CAM_JPEG_QUALITY:
	//case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT: //fixme, from kernel2.6
	case V4L2_CID_CAM_DATE_INFO_YEAR:
	case V4L2_CID_CAM_DATE_INFO_MONTH:
	case V4L2_CID_CAM_DATE_INFO_DATE:
	case V4L2_CID_CAM_SENSOR_VER:
	case V4L2_CID_CAM_FW_MINOR_VER:
	case V4L2_CID_CAM_FW_MAJOR_VER:
	case V4L2_CID_CAM_PRM_MINOR_VER:
	case V4L2_CID_CAM_PRM_MAJOR_VER:
	case V4L2_CID_ESD_INT:
	case V4L2_CID_CAMERA_GET_ISO:
	case V4L2_CID_CAMERA_GET_SHT_TIME:
	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
	case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
		ctrl->value = 0;
		break;
	case V4L2_CID_EXPOSURE:
		ctrl->value = userset.exposure_bias;
		break;
	default:
		dev_err(&client->dev, "%s: no such ctrl\n", __func__);
		/* err = -EINVAL; */
		break;
	}

	return err;
}

static int hm5065_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hm5065_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;
#if 1
	switch (ctrl->id) {
	case V4L2_CID_CAMERA_FLASH_MODE:
		printk("%s: V4L2_CID_CAMERA_FLASH_MODE %d \n",__func__,ctrl->value);
		flash_mode=ctrl->value;
		break;
	case V4L2_CID_CAMERA_BRIGHTNESS:
		break;
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_WHITE_BALANCE\n",__func__);		
		if (value <= WHITE_BALANCE_AUTO) {
		/*	err = hm5065_i2c_write_serials(sd,
				(unsigned char *)hm5065_regs_awb_enable[value],
				sizeof(hm5065_regs_awb_enable[value]));*/
		} else {
		/*	err = hm5065_i2c_write_serials(sd,
				(unsigned char *)hm5065_regs_wb_preset[value-2],
				sizeof(hm5065_regs_wb_preset[value-2]));*/
		}
		break;
	case V4L2_CID_CAMERA_EFFECT:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_EFFECT\n", __func__);
		/*err = hm5065_i2c_write_serials(sd,
			(unsigned char *)hm5065_regs_color_effect[value-1],
			sizeof(hm5065_regs_color_effect[value-1]));*/
		break;
	case V4L2_CID_CAMERA_ISO:
	case V4L2_CID_CAMERA_METERING:
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_CONTRAST\n", __func__);
		/*err = hm5065_i2c_write_serials(sd,
			(unsigned char *)hm5065_regs_contrast_bias[value],
			sizeof(hm5065_regs_contrast_bias[value]));*/
		break;
	case V4L2_CID_CAMERA_SATURATION:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_SATURATION\n", __func__);
		/*err = hm5065_i2c_write_serials(sd,
			(unsigned char *)hm5065_regs_saturation_bias[value],
			sizeof(hm5065_regs_saturation_bias[value]));*/
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_SHARPNESS\n", __func__);
		/*err = hm5065_i2c_write_serials(sd,
			(unsigned char *)hm5065_regs_sharpness_bias[value],
			sizeof(hm5065_regs_sharpness_bias[value]));*/
		break;
	case V4L2_CID_CAMERA_WDR:
	case V4L2_CID_CAMERA_FACE_DETECTION:
	case V4L2_CID_CAMERA_FOCUS_MODE:
	case V4L2_CID_CAM_JPEG_QUALITY:
	case V4L2_CID_CAMERA_SCENE_MODE:
	case V4L2_CID_CAMERA_GPS_LATITUDE:
	case V4L2_CID_CAMERA_GPS_LONGITUDE:
	case V4L2_CID_CAMERA_GPS_TIMESTAMP:
	case V4L2_CID_CAMERA_GPS_ALTITUDE:
	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
	case V4L2_CID_CAMERA_FRAME_RATE:
		break;
	case V4L2_CID_CAM_PREVIEW_ONOFF:
		if (state->check_previewdata == 0)
			err = 0;
		else
			err = -EIO;
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE:
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		break;
	case V4L2_CID_CAMERA_RESET:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_RESET\n", __func__);
		err = hm5065_reset(sd);
		break;
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "%s: V4L2_CID_EXPOSURE\n",
			__func__);
		/*err = hm5065_i2c_write_serials(sd,
			(unsigned char *)hm5065_regs_ev_bias[value],
			sizeof(hm5065_regs_ev_bias[value]));*/
		break;
	default:
		dev_err(&client->dev, "%s: no such control: %0x\n", __func__, ctrl->id);
		/* err = -EINVAL; */
		break;
	}

	if (err < 0)
		dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed\n", __func__);
#endif
	return err;

}

static int hm5065_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hm5065_state *state = to_state(sd);
	int err = -EINVAL;
	int array_length;
	initialised=1;

    flash_mode=FLASH_MODE_OFF;

	v4l_info(client, "%s: camera initialization start\n", __func__);

//	unsigned char test = 3;
//	hm5065_read(sd, 0x0002, &test);
//	printk("***test id: %0x\n", test);
	
	array_length = ARRAY_SIZE(hm5065_init_reg);
//	while(1)
	err = hm5065_i2c_write_serials(sd, hm5065_init_reg, array_length, "hm5065 init param");

	//err = hm5065_i2c_write_serials(sd, hm5065_init_iq_reg, array_length, "hm5065 init iq param");

	msleep(10);

	if (err != 0) {
		/* This is preview fail */
		state->check_previewdata = 100;
		v4l_err(client, "%s: camera initialization failed\n",
			__func__);
		//return -EIO;
	}

	/* This is preview success */
	state->check_previewdata = 0;

	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time
 * therefor,it is not necessary to be initialized on probe time.
 * except for version checking
 * NOTE: version checking is optional
 */
static int hm5065_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hm5065_state *state = to_state(sd);
	struct hm5065_platform_data *pdata;

	dev_info(&client->dev, "fetching platform data\n");

	pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (!(pdata->default_width && pdata->default_height)) {
		/* TODO: assign driver default resolution */
	} else {
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;
	}

	if (!pdata->pixelformat)
		state->pix.pixelformat = DEFAULT_FMT;
	else
		state->pix.pixelformat = pdata->pixelformat;

	if (!pdata->freq)
		state->freq = 48000000;	/* 48MHz default */
	else
		state->freq = pdata->freq;

	if (!pdata->is_mipi) {
		state->is_mipi = 0;
		dev_info(&client->dev, "parallel mode\n");
	} else
		state->is_mipi = pdata->is_mipi;

	return 0;
}

static int hm5065_sleep(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
#if 0
	int err = -EINVAL, i;
#endif

	v4l_info(client, "%s: sleep mode\n", __func__);

#if 0
	for (i = 0; i < HM5065_SLEEP_REGS; i++) {
		if (hm5065_sleep_reg[i][0] == REG_DELAY) {
			mdelay(hm5065_sleep_reg[i][1]);
			err = 0;
		} else {
			err = hm5065_write(sd, hm5065_sleep_reg[i][0],
				hm5065_sleep_reg[i][1]);
		}

		if (err < 0)
			v4l_info(client, "%s: register set failed\n", __func__);
	}

	if (err < 0) {
		v4l_err(client, "%s: sleep failed\n", __func__);
		return -EIO;
	}
#endif
	return 0;
}

static int hm5065_wakeup(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
#if 0
	int err = -EINVAL, i;
#endif

	v4l_info(client, "%s: wakeup mode\n", __func__);
#if 0
	for (i = 0; i < HM5065_WAKEUP_REGS; i++) {
		if (hm5065_wakeup_reg[i][0] == REG_DELAY) {
			mdelay(hm5065_wakeup_reg[i][1]);
			err = 0;
		} else {
			err = hm5065_write(sd, hm5065_wakeup_reg[i][0],
				hm5065_wakeup_reg[i][1]);
		}

		if (err < 0)
			v4l_info(client, "%s: register set failed\n", __func__);
	}

	if (err < 0) {
		v4l_err(client, "%s: wake up failed\n", __func__);
		return -EIO;
	}
#endif
	return 0;
}

static int hm5065_s_stream(struct v4l2_subdev *sd, int enable)
{
	return enable ? hm5065_wakeup(sd) : hm5065_sleep(sd);
}

static const struct v4l2_subdev_core_ops hm5065_core_ops = {
	.init = hm5065_init,	/* initializing API */
	.s_config = hm5065_s_config,	/* Fetch platform data */
	.queryctrl = hm5065_queryctrl,
	.querymenu = hm5065_querymenu,
	.g_ctrl = hm5065_g_ctrl,
	.s_ctrl = hm5065_s_ctrl,
};

static const struct v4l2_subdev_video_ops hm5065_video_ops = {
	.s_crystal_freq = hm5065_s_crystal_freq,
	.g_fmt = hm5065_g_fmt,
	.s_fmt = hm5065_s_fmt,
	.enum_framesizes = hm5065_enum_framesizes,
	.enum_frameintervals = hm5065_enum_frameintervals,
	.enum_fmt = hm5065_enum_fmt,
	.try_fmt = hm5065_try_fmt,
	.g_parm = hm5065_g_parm,
	.s_parm = hm5065_s_parm,
	.s_stream = hm5065_s_stream,
};

static const struct v4l2_subdev_ops hm5065_ops = {
	.core = &hm5065_core_ops,
	.video = &hm5065_video_ops,
};

/*
 * hm5065_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int hm5065_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct hm5065_state *state;
	struct v4l2_subdev *sd;

    int	err = gpio_request(CAM_FLASH, "CAM_FLASH");
	if (err)
	   printk(KERN_ERR "#### failed to request CAM_FLASH, flash disable\n");

	state = kzalloc(sizeof(struct hm5065_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, HM5065_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &hm5065_ops);

	dev_info(&client->dev, "hm5065 has been probed\n");
	return 0;
}

static int hm5065_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	initialised=0;

	v4l2_device_unregister_subdev(sd);

	gpio_free(CAM_FLASH);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id hm5065_id[] = {
	{ HM5065_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, hm5065_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = HM5065_DRIVER_NAME,
	.probe = hm5065_probe,
	.remove = hm5065_remove,
	.id_table = hm5065_id,
};

MODULE_DESCRIPTION("HM5065 QSXGA camera driver");
MODULE_LICENSE("GPL");

