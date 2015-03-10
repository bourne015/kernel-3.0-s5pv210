/*
 * Battery Charge driver for Android P14 platformwith one or two external
 * power supplies (AC/USB) connected to main battery. 
 *
 * Copyright@ 2011 Wang Tianxu <wtxarmux@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
//#define DEBUG
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/pda_power.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <mach/map.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-cfg.h>
#include <plat/adc.h>
#include <asm/io.h>
#include <linux/irq.h>
#include <linux/sort.h>  
#include <mach/gpio-smdkc110.h>

#include <linux/slab.h>  
#define GPIO_MP26123_ACOK   S5PV210_GPH0(3)
#define GPIO_MP26123_CHGGOOD   S5PV210_GPH3(4)
//#define CONFIG_CHARGE_DEBUG
#ifdef CONFIG_CHARGE_DEBUG
#define chgdbg(x...) printk(x)
#else
#define chgdbg(x...)
#endif
//#define DEBUG  
//extern int s3c_adc_get_adc_data(int channel); 
extern int s3c_adc_get_value(int channel, int prescaler, int delay);

#define AD_FILTER_LENGTH	10
#define AD_CLOSE_ENOUGH	10
#define AD_THRESHOLD	8
typedef enum {
    CHARGER_BATTERY = 0,
    CHARGER_USB,
    CHARGER_AC,
    CHARGER_DISCHARGE
} charger_type_t;

struct ad_value_filter{
	int N;	/* How many samples we have. */
	int samples[AD_FILTER_LENGTH];	/* The samples: our input. */
	int group_size[AD_FILTER_LENGTH];		/* Used for temporal computations. */
	int sorted_samples[AD_FILTER_LENGTH];	/* Used for temporal computations. */

	int range_max;	/* Max. computed ranges. */
	int range_min;	/* Min. computed ranges. */

	int ready;		/* If we are ready to deliver samples. */
	int result;		/* Index of the point being returned. */	
};

struct battery_info {
	u32 batt_id;		/* Battery ID from ADC */
	u32 batt_vol;		/* Battery voltage from ADC */
	u32 batt_vol_adc;	/* Battery ADC value */
	u32 batt_vol_adc_cal;	/* Battery ADC value (calibrated)*/
	u32 batt_temp;		/* Battery Temperature (C) from ADC */
	u32 batt_temp_adc;	/* Battery Temperature ADC value */
	u32 batt_temp_adc_cal;	/* Battery Temperature ADC value (calibrated) */
	u32 batt_current;	/* Battery current from ADC */
	u32 level;		/* formula */
	u32 charging_source;	/* 0: no cable, 1:usb, 2:AC */
	u32 charging_enabled;	/* 0: Disable, 1: Enable */
	u32 batt_health;	/* Battery Health (Authority) */
	u32 batt_is_full;       /* 0 : Not full 1: Full */
};

/* lock to protect the battery info */

struct s3c_battery_info {
	int present;
	int polling;
	unsigned long polling_interval;

	struct battery_info bat_info;
};

struct v70_bat_exist {
	unsigned int delay_cnt;
	unsigned int filt_y;
	unsigned int filt_n;
	int bat_old;
	int bat_new;
	unsigned int bat_exist_val;
	unsigned int bat_exist_val_old;
};

static struct s3c_battery_info s3c_bat_info;

static struct ad_value_filter ad_val_filter;

static struct wake_lock vbus_wake_lock;

//static struct work_struct power_changed_wq;

static int usb_flag = 0;

static struct v70_bat_exist bat_exist;

enum {
	PDA_PSY_OFFLINE = 0,
	PDA_PSY_ONLINE = 1,
};

unsigned int bat_maxvolt = 4300;  /*unit of mV*/
unsigned int bat_minvolt = 3300;   /*unit of mV*/
unsigned int wait_for_status = 500;   /*unit of ms*/
unsigned int wait_for_charger = 500; /*unit of ms*/
unsigned int polling_interval = 2000;  /*unit of ms*/

struct mp26123_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);

};

struct mp26123_chip {
	struct delayed_work		work; 
	struct work_struct               power_changed_work;
	unsigned int debounce_val;
	int debounce_interval;
	struct mp26123_platform_data	*pdata;
	struct timer_list timer;
	/* State Of Connect */
	int online;
	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
};


int g_charger_current = 0;
int g_charger_voltage = 0;

/**************japy add battery charge,11.05.27,macro mark*************/

static int s3c_battery_initial = 0;

static enum power_supply_property mp26123_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY, /* in percents! */
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property s3c_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

int s3c_adc_voltage_source = CHARGER_BATTERY;	// yntan added
int s3c_adc_voltage_value  = 8336;		// yntan added
int s3c_adc_voltage_level  = 0;			// yntan added

EXPORT_SYMBOL(s3c_adc_voltage_source);		// yntan added
EXPORT_SYMBOL(s3c_adc_voltage_value);		// yntan added
EXPORT_SYMBOL(s3c_adc_voltage_level);		// yntan added

static ssize_t v70_bat_exist_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (bat_exist.bat_exist_val)  {
		s3c_adc_voltage_source = CHARGER_BATTERY;
		return sprintf(buf, "Yes\n");
	} else {
		s3c_adc_voltage_source = CHARGER_DISCHARGE;
		return sprintf(buf, "No\n");
	}
}

static int s3c_power_get_property(struct power_supply *bat_ps, 
		enum power_supply_property psp, 
		union power_supply_propval *val)
{
	charger_type_t charger;
	
	dev_dbg(bat_ps->dev, "%s : psp = %d\n", __func__, psp);

	charger = s3c_bat_info.bat_info.charging_source;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (bat_ps->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = (charger == CHARGER_AC ? 1 : 0);
		else if (bat_ps->type == POWER_SUPPLY_TYPE_USB)
			val->intval = (charger == CHARGER_USB ? 1 : 0);
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

static int bat_get_charging_status(void)
{
	charger_type_t charger = CHARGER_BATTERY; 
	int ret = 0;
        
	charger = s3c_bat_info.bat_info.charging_source;
        
	switch (charger) {
	case CHARGER_BATTERY:
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case CHARGER_USB:
	case CHARGER_AC:
		if (s3c_bat_info.bat_info.batt_is_full) {
			ret = POWER_SUPPLY_STATUS_FULL;
		} else {
			ret = POWER_SUPPLY_STATUS_CHARGING;
		}
		break;
	case CHARGER_DISCHARGE:
		ret = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
	}

	return ret;
}

static int mp26123_bat_get_property(struct power_supply *bat_ps, 
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	dev_dbg(bat_ps->dev, "%s : psp = %d\n", __func__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bat_get_charging_status();
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s3c_bat_info.bat_info.batt_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s3c_bat_info.present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY: /* in percents! */
		val->intval = s3c_bat_info.bat_info.level;	
		dev_dbg(bat_ps->dev, "%s : level = %d\n", __func__, 
				val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = s3c_bat_info.bat_info.batt_vol * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = 302;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static char *supply_list[] = {
	"battery",
};

static struct power_supply s3c_power_supplies[] = {
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = mp26123_battery_props,
		.num_properties = ARRAY_SIZE(mp26123_battery_props),
		.get_property = mp26123_bat_get_property,
	},
	{
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = s3c_power_properties,
		.num_properties = ARRAY_SIZE(s3c_power_properties),
		.get_property = s3c_power_get_property,
	},
	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = supply_list,
		.num_supplicants = ARRAY_SIZE(supply_list),
		.properties = s3c_power_properties,
		.num_properties = ARRAY_SIZE(s3c_power_properties),
		.get_property = s3c_power_get_property,
	},
};

static int s3c_cable_status_update(int status)
{
	int ret = 0;
	charger_type_t source = CHARGER_BATTERY;

	if(!s3c_battery_initial)
		return -EPERM;

	switch(status) {
	case CHARGER_BATTERY:
		s3c_bat_info.bat_info.charging_source = CHARGER_BATTERY;
#if defined DEBUG
		printk("s3c_cable_status_update: CHARGER_BATTERY\n");
#endif
		s3c_bat_info.bat_info.batt_temp = 1;
		if(s3c_bat_info.bat_info.level == 0)s3c_bat_info.bat_info.level == 1;
		break;
	case CHARGER_USB:
		s3c_bat_info.bat_info.charging_source = CHARGER_USB;
#if defined DEBUG
		printk("s3c_cable_status_update: CHARGER_USB\n");
#endif		
		s3c_bat_info.bat_info.batt_temp = 1;
		break;
	case CHARGER_AC:
#if defined DEBUG
		printk("s3c_cable_status_update: CHARGER_AC\n");
#endif		
		s3c_bat_info.bat_info.charging_source = CHARGER_AC;
		s3c_bat_info.bat_info.batt_temp = 1;
		break;
	case CHARGER_DISCHARGE:
		s3c_bat_info.bat_info.charging_source = CHARGER_DISCHARGE;
		break;
	default:
		ret = -EINVAL;
	}
	source = s3c_bat_info.bat_info.charging_source;

	//s3c_adc_voltage_source = s3c_bat_info.bat_info.charging_source;	// yntan DEBUG

	if (source == CHARGER_AC) {
		//wake_lock(&vbus_wake_lock);
	} else {
		/* give userspace some time to see the uevent and update
		 * LED state or whatnot...
		 */
		wake_lock_timeout(&vbus_wake_lock, HZ / 2);
	}

	/* if the power source changes, all power supplies may change state */
	power_supply_changed(&s3c_power_supplies[CHARGER_BATTERY]);
	return ret;
}


static int int_cmp(const void *_a, const void *_b)
{
	const int *a = _a;
	const int *b = _b;

	if (*a > *b)
		return 1;
	if (*a < *b)
		return -1;
	return 0;
}

static int ad_filter_process(int ad_val)
{
	int i, overage;
	int *v = ad_val_filter.sorted_samples;
	int ngroups = 0;
	int best_size;
	int best_idx = 0;
	int idx = 0;
	
	BUG_ON(ad_val_filter.N >= AD_FILTER_LENGTH);
	BUG_ON(ad_val_filter.ready);

	if(ad_val_filter.N >= AD_FILTER_LENGTH) ad_val_filter.N = 0;

	ad_val_filter.samples[ad_val_filter.N] = ad_val;

	if (++ad_val_filter.N < AD_FILTER_LENGTH)
		return 0;	/* We need more samples. */

	memcpy(v, ad_val_filter.samples, ad_val_filter.N * sizeof(int));

	sort(v, ad_val_filter.N, sizeof(int), int_cmp, NULL);

	ad_val_filter.group_size[0] = 1;
	for (i = 1; i < ad_val_filter.N; ++i) {
		if (v[i] - v[i - 1] <= AD_CLOSE_ENOUGH)
			ad_val_filter.group_size[ngroups]++;
		else
			ad_val_filter.group_size[++ngroups] = 1;
	}
	ngroups++;

	best_size = ad_val_filter.group_size[0];
	for (i = 1; i < ngroups; i++) {
		idx += ad_val_filter.group_size[i - 1];
		if (best_size < ad_val_filter.group_size[i]) {
			best_size = ad_val_filter.group_size[i];
			best_idx = idx;
		}
	}

	if (best_size < AD_THRESHOLD) {
		/* This set is not good enough for us. */
		ad_val_filter.N =0;
		ad_val_filter.ready = 0;
		//printk("we give up: error\n");//
		return 1;  
	}

	//ad_val_filter.range_min = v[best_idx];
	//ad_val_filter.range_max = v[best_idx + best_size - 1];
	//overage = (ad_val_filter.range_min + ad_val_filter.range_max)/2;
	ad_val_filter.result = 0;
	for(i=best_idx; i<best_idx + best_size; i++)
		ad_val_filter.result += v[i];
	ad_val_filter.result= 
		(ad_val_filter.result + best_size -1)/best_size;
#ifdef DEBUG
	printk("best size = %d, best_idx = %d\n",best_size,best_idx);
	printk("sort samples:  ");
	for(i=best_idx; i<best_idx + best_size; i++)
		printk("%d, ", v[i]);
	printk("\n");
#endif
	ad_val_filter.range_min = 	ad_val_filter.result;
	ad_val_filter.N =0;
	ad_val_filter.ready = 1;
	return 0;
}

#define VOL_TREND_NUM (37+4)
#define MAX_CHARGE_STEP (10+5)
static int charge_vol_trend[]=
{
	// first stage
	//5995, // 0%
	6817, //0%
	7145, // 1%
	7301, // 2%3
	7420, // 3%4
	7509, // 4%5
	7576, // 5%6
	7608, // 6%7
	7621, // 7%8
	7632, // 8%9
	7643, // 9%10
	// the second stage
	7654, // 10% 11
	7724, // 15%12
	7787, // 20%13
	7826, // 25%14
	7845, // 30%15
	7860, // 35%16
	7879, // 40%17
	7905, // 45%18
	7939, // 50%19
	7987, // 55%20
	8028, // 60%21
	8079, // 65%22
	8138, // 70%23
	8200, // 75%24
	8266, // 80%25
	8368, // 85%26
	// the third stage
	8, //86%
	8,//87%
	10,//88%
	10,//89%
	12,//90%
	48, // 91%
	15, // 92%
	15, // 93%
	18, // 94%
	21, // 95%
	27, // 96%
	33, // 97%
	42, // 98%
	60, // 99%
	120,// 100%
};

static int discharge_vol_trend[]=
{
	// first stage
	//5995, // 0%
	6972,//6300, //0%
	6992,//6546, // 1%
	7012,//6778, // 2%
	7032,//6930, // 3%
	7052, // 4%
	7128, // 5%
	7184, // 6%
	7220, // 7%
	7242, // 8%
	7256, // 9%
	// the second stage
	7268, // 10%
	7306, // 15%
	7354, // 20%
	7388, // 25%
	7414, // 30%
	7438, // 35%
	7466, // 40%
	7500, // 45%
	7536, // 50%
	7578, // 55%
	7624, // 60%
	7682, // 65%
	7750, // 70%
	7828, // 75%
	7906, // 80%
	8004, // 85%
	// the third stage
	8024,//86%
	8042,//87%
	8058,//88%
	8076,//89%
	8094, // 90%
	8114, // 91%
	8136, // 92%
	8156, // 93%
	8174, // 94%
	8194, // 95%
	8214, // 96%
	8234, // 97%
	8256, // 98%
	8282, // 99%
	8336 // 100%
};
static int charge_time=0, charge_step=0;
static void calculate_bat_level(void)
{	
	unsigned int vol,i;

	if(s3c_bat_info.bat_info.batt_temp){
		s3c_bat_info.bat_info.batt_temp --;
		return;
	}
	
	if(s3c_bat_info.bat_info.charging_source == CHARGER_BATTERY){ //discharge
		vol =(((ad_val_filter.range_min * 3300)/4095)*509)/200; 
		vol -= s3c_bat_info.bat_info.batt_vol_adc_cal;
		s3c_bat_info.bat_info.batt_vol = vol;
		if(vol<discharge_vol_trend[0]) 
			vol = discharge_vol_trend[0];
		else if(vol > discharge_vol_trend[VOL_TREND_NUM-1]) 
			vol = discharge_vol_trend[VOL_TREND_NUM-1];	
		
		for(i=1; discharge_vol_trend[i] < vol; i++);
		
		if (i < 11){ // caculate first stage battery level
			if (vol < (discharge_vol_trend[i-1] + discharge_vol_trend[i])/2)
				s3c_bat_info.bat_info.level = i-1;
			else 
				s3c_bat_info.bat_info.level = i;
		}else if(i < 26){ // caculate second stage battery level
			s3c_bat_info.bat_info.level = 5* (vol - discharge_vol_trend[i-1]) /
					(discharge_vol_trend[i] - discharge_vol_trend[i-1])
					+ (i-11)*5 + 10; 
		}else {// caculate third stage battery level
			if (vol < (discharge_vol_trend[i-1] + discharge_vol_trend[i])/2)
				s3c_bat_info.bat_info.level = i-26 + 85;
			else 
				s3c_bat_info.bat_info.level = i-25+ 85;								
		}
		
		if(s3c_bat_info.bat_info.level > (100 - MAX_CHARGE_STEP)){
			charge_step = s3c_bat_info.bat_info.level - (100 - MAX_CHARGE_STEP);
			charge_time = charge_vol_trend[VOL_TREND_NUM - MAX_CHARGE_STEP - 1 + charge_step];
		}
		else {
			charge_step = 0;
			charge_time = 0;
		}
	}
	else {// battery charge
		vol =(((ad_val_filter.range_min * 3300)/4096)*509)/200;
		vol -= s3c_bat_info.bat_info.batt_vol_adc_cal;
		s3c_bat_info.bat_info.batt_vol = vol;
		
		if(vol<charge_vol_trend[0]) {
			vol = charge_vol_trend[0];
			i=1;
		}
		else if(vol > charge_vol_trend[VOL_TREND_NUM-MAX_CHARGE_STEP-1]) {
			vol = charge_vol_trend[VOL_TREND_NUM-MAX_CHARGE_STEP-1];
			i = 26;
		}
		else{
			for(i=1; charge_vol_trend[i] < vol; i++);
		}

		if (i < 11){ 
		//caculate first stage battery level
			if (vol < (charge_vol_trend[i-1] + charge_vol_trend[i])/2)
				s3c_bat_info.bat_info.level = i-1;
			else 
				s3c_bat_info.bat_info.level = i;
				
			charge_step = 0;
			charge_time = 0;
		}else if(i < 26){ 
		//caculate second stage battery level
			s3c_bat_info.bat_info.level = 5* (vol - charge_vol_trend[i-1]) /
					(charge_vol_trend[i] - charge_vol_trend[i-1])
					+ (i-11)*5 + 10;
	
			charge_step = 0;
			charge_time = 0;
			
		}else if(charge_step==0){	
			if(gpio_get_value(GPIO_MP26123_CHGGOOD))
			{
				s3c_bat_info.bat_info.level = 100;
				charge_step = MAX_CHARGE_STEP;
				charge_time = 0;
				goto cal_return;
			}
	
			s3c_bat_info.bat_info.level = 100 - MAX_CHARGE_STEP;
			charge_step = 1;
			charge_time = charge_vol_trend[VOL_TREND_NUM - MAX_CHARGE_STEP];	
		}else if(charge_time){
			if(gpio_get_value(GPIO_MP26123_CHGGOOD))
			{
				s3c_bat_info.bat_info.level = 100;
				charge_step = MAX_CHARGE_STEP;
				charge_time = 0;
				goto cal_return;
			}
			
			s3c_bat_info.bat_info.level = 100 - MAX_CHARGE_STEP + charge_step;
			charge_time --;
			if(charge_time == 0){
				if(charge_step < MAX_CHARGE_STEP) {
					charge_step++;
					charge_time = charge_vol_trend[VOL_TREND_NUM - MAX_CHARGE_STEP - 1 + charge_step];
				}else{
				
					if(gpio_get_value(GPIO_MP26123_CHGGOOD)){
						goto cal_return;
					}else {
						s3c_bat_info.bat_info.level--;
						charge_step--;
						charge_time = charge_vol_trend[VOL_TREND_NUM - MAX_CHARGE_STEP - 1 + charge_step];
					}
				}	
			}
		}		
	}
cal_return:
	s3c_adc_voltage_value = s3c_bat_info.bat_info.batt_vol;	// yntan added
	s3c_adc_voltage_level = s3c_bat_info.bat_info.level;	// yntan added

#if defined DEBUG	
	//printk("mp26123:adc= %d, battery_level = %d, s3c_bat_info.bat_info.batt_vol  = %d\n",ad_val_filter.range_min, s3c_bat_info.bat_info.level, s3c_bat_info.bat_info.batt_vol);
#endif
	//printk("mp26123:adc= %d, battery_level = %d, s3c_bat_info.bat_info.batt_vol  = %d\n",ad_val_filter.range_min, s3c_bat_info.bat_info.level, s3c_bat_info.bat_info.batt_vol);
}

static void power_changed_func(struct work_struct *work)
{
	struct mp26123_chip *chip = \
		container_of(work, struct mp26123_chip, power_changed_work);
	
   charger_type_t status = 0;
	//	printk("__func__ : %s \n", __func__);
	if(!gpio_get_value(GPIO_MP26123_ACOK) && !chip->debounce_val)
	{
#if 0
		if(usb_flag==0){
			status = CHARGER_AC;
			s3c_bat_info.bat_info.batt_vol_adc_cal = 110;
		}
		else{		
			status = CHARGER_USB;
			s3c_bat_info.bat_info.batt_vol_adc_cal = 128;
		}
#else
		status = CHARGER_AC;
		s3c_bat_info.bat_info.batt_vol_adc_cal = 0;
#endif
	
	}else if(gpio_get_value(GPIO_MP26123_ACOK) && chip->debounce_val){
		status = CHARGER_BATTERY;
		s3c_bat_info.bat_info.batt_vol_adc_cal = 0;
	}

	//update charger
	
	s3c_cable_status_update(status);
	
	enable_irq(IRQ_EINT(3));

}

static void charge_check_timer(unsigned long data)
{
	struct mp26123_chip *chip = data;
	schedule_work(&chip->power_changed_work);
}

static irqreturn_t power_changed_isr(int irq, void *data)
{
	struct mp26123_chip *chip = data;
	
	disable_irq_nosync(IRQ_EINT(3));
	
#if defined DEBUG	
	printk("power_changed_isr\n");
#endif
	
	if (chip->debounce_interval)
	{
		chip->debounce_val = gpio_get_value(GPIO_MP26123_ACOK);
		mod_timer(&chip->timer,
			jiffies + msecs_to_jiffies(chip->debounce_interval));
	}
	else
		schedule_work(&chip->power_changed_work);

	return IRQ_HANDLED;
}


static struct irqaction power_changed_irq = {
	.name		= "power changed",
	.flags		= IRQF_SHARED ,
	.handler	= power_changed_isr,
};

//static unsigned int delay_cnt = 0;
static void mp26123_work(struct work_struct *work)
{
	struct mp26123_chip *chip;
	int old_level;
	unsigned int adc_value, bat_value;
	
	old_level = s3c_bat_info.bat_info.level; 
	chip = container_of(work, struct mp26123_chip, work.work);
	
	//adc_value = s3c_adc_get_adc_data(1);
	adc_value = s3c_adc_get_value(1, 21, 16);
#if defined DEBUG
	printk("mp26123:adc_value = %d\n",adc_value);
#endif
	ad_filter_process(adc_value);
	if(ad_val_filter.ready){		
		 calculate_bat_level();
		//	s3c_bat_info.bat_info.level = vol_val_filter.range_max;
			if(s3c_bat_info.bat_info.level == 0)s3c_bat_info.bat_info.batt_vol_adc ++;
			else s3c_bat_info.bat_info.batt_vol_adc = 0;

			if(old_level != s3c_bat_info.bat_info.level){
#if defined DEBUG			
				printk("Batter State of charge: %d\n",s3c_bat_info.bat_info.level);
#endif
				if(s3c_bat_info.bat_info.level)
					power_supply_changed(&s3c_power_supplies[CHARGER_BATTERY]);
			}
			else if(s3c_bat_info.bat_info.batt_vol_adc > 2){
			/* battery level had come to 0 with 5 cycles, report battery level */	
#if defined DEBUG			
				printk("Batter State of charge: %d\n",s3c_bat_info.bat_info.level);
#endif		
				power_supply_changed(&s3c_power_supplies[CHARGER_BATTERY]);
			}

		ad_val_filter.ready = 0;
	}

	if (bat_exist.delay_cnt == 0) {
		bat_exist.bat_old = gpio_get_value(GPIO_MP26123_CHGGOOD);
		//printk("bat test bat_old:%d\n", bat_exist.bat_old);
	}
	bat_exist.delay_cnt++;
	if (bat_exist.delay_cnt == 2) {
		bat_exist.bat_new = gpio_get_value(GPIO_MP26123_CHGGOOD);
		//printk("bat test bat_new:%d\n", bat_exist.bat_new);
		if (bat_exist.bat_old != bat_exist.bat_new) {
			if (++bat_exist.filt_n == 2) {
				bat_exist.bat_exist_val = 0;
				bat_exist.filt_n = 0;
				//printk("no battery\n\n\n");
			}
			bat_exist.filt_y = 0;
		} else {
			if (++bat_exist.filt_y == 4) {
				bat_exist.bat_exist_val = 1;
				bat_exist.filt_y = 0;
				//printk("has battery\n\n\n");
			}
			bat_exist.filt_n = 0;
		}

		if (bat_exist.bat_exist_val != bat_exist.bat_exist_val_old) {
			bat_exist.bat_exist_val_old = bat_exist.bat_exist_val;
			if (bat_exist.bat_exist_val == 0)
				--s3c_bat_info.bat_info.level;
			else if (bat_exist.bat_exist_val == 1)
				++s3c_bat_info.bat_info.level;

			if (s3c_bat_info.bat_info.level < 0)
				s3c_bat_info.bat_info.level = 0;
			else if (s3c_bat_info.bat_info.level > 100)
				s3c_bat_info.bat_info.level = 100;
			power_supply_changed(&s3c_power_supplies[CHARGER_BATTERY]);
		}
		bat_exist.delay_cnt = 0;
	}

	schedule_delayed_work(&chip->work, 256);
}
#ifdef USB_CHARGE
void s3c_cable_check_status(int flag)
{
    charger_type_t status = 0;

    if (flag == 0)  // Battery
        status = CHARGER_BATTERY;
    else    // USB
        status = CHARGER_USB;
#if defined DEBUG
	printk("mp26123:%s,%s,status = %d\n",__FILE__,__func__,status);
#endif
       s3c_cable_status_update(status);
	usb_flag = flag;	
}
#else
void s3c_cable_check_status(int flag){
	if(!s3c_battery_initial)
		return;
	if (flag == 0)  // Battery
        	wake_lock_timeout(&vbus_wake_lock, HZ / 2);
       else    // USB
//    	wake_lock_timeout(&vbus_wake_lock, HZ / 2);
		wake_lock(&vbus_wake_lock);
		
//	printk("usb flag = %d\n", flag);
}
#endif
EXPORT_SYMBOL(s3c_cable_check_status);

static DEVICE_ATTR(bat_exist, 0644, v70_bat_exist_show, NULL);
static int __devinit mp26123_battery_probe(struct platform_device *pdev)
{
	struct mp26123_chip *chip;
	int ret;
	int i; 

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	
	chip->pdata = pdev->dev.platform_data;
	chip->debounce_interval = 5;
	platform_set_drvdata(pdev, chip);

	setup_timer(&chip->timer, charge_check_timer, (unsigned long)chip);
	
	s3c_gpio_setpull(GPIO_MP26123_CHGGOOD, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_MP26123_CHGGOOD, S3C_GPIO_INPUT);
	
	s3c_gpio_setpull(GPIO_MP26123_ACOK, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_MP26123_ACOK, S3C_GPIO_SFN(0xF));
	
	INIT_WORK(&chip->power_changed_work, power_changed_func);
	
	//set_irq_wake(IRQ_EINT(3), 1);
	
	ret = request_irq(IRQ_EINT(3), power_changed_isr, IRQF_TRIGGER_RISING|\
	IRQF_TRIGGER_FALLING | IRQF_SHARED, "power changed", chip);
	if(ret){
		dev_err(&pdev->dev, "Unable to claim irq %d; error %d\n", IRQ_EINT(3), ret);
		goto free_timer;
	}
	//device_init_wakeup(&pdev->dev, 1);
	
	s3c_bat_info.present = 1;

	s3c_bat_info.bat_info.batt_id = 0;
	s3c_bat_info.bat_info.batt_vol = 8395;
	s3c_bat_info.bat_info.batt_vol_adc = 0;
	s3c_bat_info.bat_info.batt_vol_adc_cal = 0;
	s3c_bat_info.bat_info.batt_temp = 1;
	s3c_bat_info.bat_info.batt_temp_adc = 0;
	s3c_bat_info.bat_info.batt_temp_adc_cal = 0;
	s3c_bat_info.bat_info.batt_current = 0;
	s3c_bat_info.bat_info.level = 50;
	s3c_bat_info.bat_info.charging_source = CHARGER_BATTERY;
	s3c_bat_info.bat_info.charging_enabled = 0;
	s3c_bat_info.bat_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;

	bat_exist.delay_cnt = 0;
	bat_exist.bat_exist_val = 1;
	bat_exist.bat_exist_val_old = 1;
	bat_exist.filt_y = 0;
	bat_exist.filt_n = 0;

	charge_step = 8;
	charge_time = charge_vol_trend[VOL_TREND_NUM - MAX_CHARGE_STEP - 1 + charge_step];
	
	ret = device_create_file(&pdev->dev, &dev_attr_bat_exist);
	if (ret) {
		printk("failed to create file for battery exist: %d\n", ret);
	}

	/* init power supplier framework */
	for (i = 0; i < ARRAY_SIZE(s3c_power_supplies); i++) {
		ret = power_supply_register(&pdev->dev, 
				&s3c_power_supplies[i]);
		if (ret) {
			dev_err(&pdev->dev, "Failed to register"
					"power supply %d,%d\n", i, ret);
			goto err_battery_failed;
		}
	}
	s3c_battery_initial = 1;
	power_supply_changed(&s3c_power_supplies[CHARGER_BATTERY]);

	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, mp26123_work);	
	schedule_delayed_work(&chip->work,256);
	
#if defined DEBUG
	printk("mp26123:%s,%s,%d\n",__FILE__,__func__,__LINE__);
#endif
	return 0;	
free_timer:
	del_timer_sync(&chip->timer);
err_battery_failed:
	dev_err(&pdev->dev, "failed: power supply register\n");
	kfree(chip);
	return ret;
}

static int __devexit mp26123_battery_remove(struct platform_device *pdev)
{
	struct mp26123_chip *chip = platform_get_drvdata(pdev);
	int i;	

	device_remove_file(&pdev->dev, &dev_attr_bat_exist);

	for (i = 0; i < ARRAY_SIZE(s3c_power_supplies); i++) {
		power_supply_unregister(&s3c_power_supplies[i]);
	}	
	kfree(chip);
	return 0;
}

static int mp26123_battery_suspend(struct platform_device *pdev, 
		pm_message_t state)
{
	struct mp26123_chip * chip =  platform_get_drvdata(pdev);
	cancel_delayed_work(&chip->work); 
	return 0;
}

static int mp26123_battery_resume(struct platform_device *pdev)
{ 
	struct mp26123_chip * chip =  platform_get_drvdata(pdev);
	power_supply_changed(&s3c_power_supplies[CHARGER_BATTERY]);  
	schedule_delayed_work(&chip->work, 512);
	return 0;
}


static const struct platform_device_id mp26123_id[] = {
	{ "v70-battery", 0 },
};
MODULE_DEVICE_TABLE(i2c, mp26123
);

static struct platform_driver  mp26123_i2c_driver= {
	.driver = {
		.name = "v70-battery",
	},
	.probe = mp26123_battery_probe,
	.remove = __devexit_p(mp26123_battery_remove)
,
	.suspend = mp26123_battery_suspend,
	.resume = mp26123_battery_resume,
	.id_table= mp26123_id,
};

static int __init mp26123_battery_init(void)
{
	wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present"); 
	return platform_driver_register(&mp26123_i2c_driver);
}

static void __exit mp26123_battery_exit(void)
{
	platform_driver_unregister(&mp26123_i2c_driver);
}

module_init(mp26123_battery_init);
module_exit(mp26123_battery_exit);
MODULE_AUTHOR("Wang Tianxu <wtxarmux@gmail.com>");
MODULE_DESCRIPTION("MP26123 Fuel Gauge");
MODULE_LICENSE("GPL");
