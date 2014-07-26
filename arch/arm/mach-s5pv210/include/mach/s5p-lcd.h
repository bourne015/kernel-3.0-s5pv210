/*
 * s5p-lcd.h
 *
 *  Created on: 2011-12-1
 *      Author: sawyer
 */

#ifndef S5P_LCD_H_
#define S5P_LCD_H_

#define S5PV210_GPD_0_0_TOUT_0  (0x2)
#define S5PV210_GPD_0_1_TOUT_1  (0x2 << 4)
#define S5PV210_GPD_0_2_TOUT_2  (0x2 << 8)
#define S5PV210_GPD_0_3_TOUT_3  (0x2 << 12)

extern struct s3c_platform_fb lcd_fb_data;

#ifdef CONFIG_DIRECT_VGA_OUTPUT
extern int resolution;
#endif

struct resolution_info{
	char	label[16];
	int		width;
	int 	height;
	int		freq;
};

void populate_lcd(void);

#endif /* S5P_LCD_H_ */
