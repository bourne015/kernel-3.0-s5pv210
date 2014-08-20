#ifndef __XR20m1170_H__
#define __XR20m1170_H__

/*********************************/
//UART_SEL
//driver/misc/serial_sel.c
/*********************************/

#define M1170_RST	S5PV210_GPJ1(1)
#define M1170_IRQ	S5PV210_GPH3(5)
#define M1170_IRQ_NUM	IRQ_EINT(29)
#define GPIO_IIC_SCL	S5PV210_GPJ2(4)
#define GPIO_IIC_SDA	S5PV210_GPJ2(5)

#define DEBUG_MSG_XR20M1170	0
#if DEBUG_MSG_XR20M1170
#define DMSG(format, args...) printk("%s: " format "\n", __func__, ##args)
#else
#define DMSG(stuff...)          do{}while(0)
#endif

#define XR20M1170_I2C_CHECK	0

#define        POWER_RUN            0
#define        POWER_PRE_SUSPEND    1
#define        POWER_SUSPEND        2
#define        POWER_PRE_RESUME     3

#define TRUNC(a)                        ((unsigned int)(a))
#define ROUND(a)                        ((unsigned int)(a+0.5))

#define BIT0                            0x1
#define BIT1                            0x2
#define BIT2                            0x4
#define BIT3                            0x8
#define BIT4                            0x10
#define BIT5                            0x20
#define BIT6                            0x40
#define BIT7                            0x80

#define XR20M1170REG_RHR            0
#define XR20M1170REG_THR            1
#define XR20M1170REG_DLL            2
#define XR20M1170REG_DLM            3
#define XR20M1170REG_DLD            4
#define XR20M1170REG_IER            5
#define XR20M1170REG_ISR            6
#define XR20M1170REG_FCR            7
#define XR20M1170REG_LCR            8
#define XR20M1170REG_MCR            9
#define XR20M1170REG_LSR            10
#define XR20M1170REG_MSR            11
#define XR20M1170REG_SPR            12
#define XR20M1170REG_TCR            13
#define XR20M1170REG_TLR            14
#define XR20M1170REG_TXLVL          15
#define XR20M1170REG_RXLVL          16
#define XR20M1170REG_IODIR          17
#define XR20M1170REG_IOSTATE        18
#define XR20M1170REG_IOINTENA       19
#define XR20M1170REG_IOCONTROL      20
#define XR20M1170REG_EFCR           21
#define XR20M1170REG_EFR            22
#define XR20M1170REG_XON1           23
#define XR20M1170REG_XON2           24
#define XR20M1170REG_XOFF1          25
#define XR20M1170REG_XOFF2          26

#endif
