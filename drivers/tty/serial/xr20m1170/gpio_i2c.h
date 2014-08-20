#ifndef __GPIO_I2C_H__
#define __GPIO_I2C_H__

unsigned char gpio_i2c_read(unsigned char addr, unsigned char reg);
unsigned char gpio_i2c_write(unsigned char addr, unsigned char reg, unsigned char value);
#endif
