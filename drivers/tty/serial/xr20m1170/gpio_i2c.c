#include <mach/gpio.h>
#include "gpio_i2c.h"
#include "xr20m1170.h"

#define SCL_H	{gpio_set_value(GPIO_IIC_SCL, 1);}
#define SCL_L	{gpio_set_value(GPIO_IIC_SCL, 0);}

#define SDA_H	{gpio_set_value(GPIO_IIC_SDA, 1);}
#define SDA_L	{gpio_set_value(GPIO_IIC_SDA, 0);}

#define SDA_IN	{s3c_gpio_cfgpin(GPIO_IIC_SDA, S3C_GPIO_INPUT);}
#define SDA_OUT	{s3c_gpio_cfgpin(GPIO_IIC_SDA, S3C_GPIO_OUTPUT);}

#define WHILE_SDA_HIGH (gpio_get_value(GPIO_IIC_SDA))

#if DEBUG_MSG_XR20M1170
unsigned int BitDelayTimeout = 0x100;
#else
unsigned int BitDelayTimeout = 0x25;
#endif
static void BitDelay(void)
{
	volatile unsigned int dwTimeout;
	dwTimeout = BitDelayTimeout;
	while(--dwTimeout){
		;
	}
}

static void I2C_Start(void)
{
	SDA_OUT;

	SDA_H;
	SCL_H;
	BitDelay();
	SDA_L;
	BitDelay();
}

static void I2C_Stop(void)
{
	SDA_OUT;

	SDA_L;
	SCL_H;
	BitDelay();
	SDA_H;
	BitDelay();
}

#if 0
static void I2C_Ack(void)
{
	SDA_OUT;

	SDA_L;
	SCL_H;
	BitDelay();
	SCL_L;
	BitDelay();

	SDA_IN;
}
#endif

static void I2C_Ack1(void)
{
	int i = 0;

	SDA_IN;
	SCL_H;
	BitDelay();
	while((WHILE_SDA_HIGH) && (i<255))
		i++;
	SCL_L;
	BitDelay();

	SDA_OUT; 
}

static void I2C_Nack(void)
{
	SDA_OUT;

	SDA_H;
	SCL_H;
	BitDelay();
	SCL_L;
	BitDelay();
}

static unsigned char Write_I2C_Byte(char byte)
{
	char i;

	SCL_L;
	BitDelay();

	for(i = 0; i < 8; i++){
		if((byte & 0x80) == 0x80){
			SDA_H;
		}
		else{
			SDA_L;
		}
		SCL_H;
		BitDelay();
		SCL_L;
		BitDelay();
		byte <<= 1;
	}
	return 0;
}

static unsigned char Read_I2C_Byte(void)
{
	char i, buff = 0;

	SDA_IN;

	SCL_L;
	BitDelay();

	for(i = 0; i < 8; i++){
		SCL_H;
		BitDelay();

		if(WHILE_SDA_HIGH){
			buff |= 0x01;
		}
		else{
			buff &=~0x01;
		}
		if(i < 7)
			buff <<= 1;
		SCL_L;
		BitDelay();
	}
	return buff;
}

unsigned char gpio_i2c_read(unsigned char addr, unsigned char reg)
{
	unsigned char data;
	
	I2C_Start();

	Write_I2C_Byte(addr);
	I2C_Ack1();

	Write_I2C_Byte(reg);
	I2C_Ack1();

	I2C_Stop();

	I2C_Start();

	Write_I2C_Byte(addr + 1);
	I2C_Ack1();

	data = Read_I2C_Byte();
	I2C_Nack();

	I2C_Stop();

	return data;
}

unsigned char gpio_i2c_write(unsigned char addr, unsigned char reg, unsigned char value)
{
	I2C_Start();

	Write_I2C_Byte(addr);
	I2C_Ack1();

	Write_I2C_Byte(reg);
	I2C_Ack1();

	Write_I2C_Byte(value);
	I2C_Ack1();

	I2C_Stop();

	return 0;
}
