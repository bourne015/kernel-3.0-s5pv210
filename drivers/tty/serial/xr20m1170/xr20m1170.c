/*
 * Based on linux/drivers/serial/pxa.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/serial_reg.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <mach/gpio.h>
#include "xr20m1170.h"
#include "gpio_i2c.h"
#include "117x.h"

static struct i2c_client *i2c_xr20m1170;

static u8 cached_lcr[2];
static u8 cached_efr[2];
static u8 cached_mcr[2];

#define SPIN_LOCK_UNLOCKED __SPIN_LOCK_UNLOCKED(old_style_spin_init)
static spinlock_t xr20m1170_lock = SPIN_LOCK_UNLOCKED;
static unsigned long xr20m1170_flags;

struct xr20m1170_port {
	struct uart_port port;
	unsigned devid;
	unsigned ier;
	unsigned char lcr;
	unsigned char mcr;
	char *name;

#ifdef    CONFIG_PM
	unsigned int power_mode;
#endif
};

/* 
 * meaning of the pair:
 * first: the subaddress (physical offset<<3) of the register
 * second: the access constraint:
 * 10: no constraint
 * 20: lcr[7] == 0
 * 30: lcr == 0xbf
 * 40: lcr != 0xbf
 * 50: lcr[7] == 1 , lcr != 0xbf, efr[4] = 1
 * 60: lcr[7] == 1 , lcr != 0xbf,
 * 70: lcr != 0xbf,  and (efr[4] == 0 or efr[4] =1, mcr[2] = 0)
 * 80: lcr != 0xbf,  and (efr[4] = 1, mcr[2] = 1)
 * 90: lcr[7] == 0, efr[4] =1 
 * 100: lcr!= 0xbf, efr[4] =1
 * third:  1: readonly
 * 2: writeonly
 * 3: read/write
 */
static const int reg_info[27][3] = {
	{0x0, 20, 1},		//RHR
	{0x0, 20, 2},		//THR
	{0x0, 60, 3},		//DLL
	{0x8, 60, 3},		//DLM
	{0x10, 50, 3},		//DLD
	{0x8, 20, 3},		//IER:bit[4-7] needs efr[4] ==1,but we dont' access them now
	{0x10, 20, 1},		//ISR:bit[4/5] needs efr[4] ==1,but we dont' access them now
	{0x10, 20, 2},		//FCR :bit[4/5] needs efr[4] ==1,but we dont' access them now
	{0x18, 10, 3},		//LCR
	{0x20, 40, 3},		//MCR :bit[2/5/6] needs efr[4] ==1,but we dont' access them now
	{0x28, 40, 1},		//LSR
	{0x30, 70, 1},		//MSR
	{0x38, 70, 3},		//SPR
	{0x30, 80, 3},		//TCR
	{0x38, 80, 3},		//TLR
	{0x40, 20, 1},		//TXLVL
	{0x48, 20, 1},		//RXLVL
	{0x50, 20, 3},		//IODir
	{0x58, 20, 3},		//IOState
	{0x60, 20, 3},		//IOIntEna
	{0x70, 20, 3},		//IOControl
	{0x78, 20, 3},		//EFCR
	{0x10, 30, 3},		//EFR
	{0x20, 30, 3},		//Xon1
	{0x28, 30, 3},		//Xon2
	{0x30, 30, 3},		//Xoff1
	{0x38, 30, 3},		//Xoff2
};

static DECLARE_MUTEX(xr20m1170_i2c_mutex);

static unsigned char xr20m1170_i2c_read(unsigned char devid,
	unsigned char reg)
{
#if 1
	unsigned char read;
	read = gpio_i2c_read((i2c_xr20m1170->addr)<<1, reg);
//	printk("xr20m1170_i2c_read:%#x\n", read);
	return read;
#else
	int err;
	unsigned char msgbuf[2];
	struct i2c_msg msg[2] = {
	{i2c_xr20m1170->addr, 0, 1, &msgbuf[0] }, 
	{i2c_xr20m1170->addr, I2C_M_RD, 1, &msgbuf[1] }//|I2C_M_NOSTART
	};

	msgbuf[0] =  reg|devid;       
	msg[0].len = 1;
	msg[1].len = 1;

//	down(&xr20m1170_i2c_mutex);
	err = i2c_transfer(i2c_xr20m1170->adapter, msg, 2);
	if(err < 0) {
		printk("i2c transfer fail in i2c read.err:%d.reg:%#x\n", err, msgbuf[0]);
//		up(&xr20m1170_i2c_mutex);
		return -EBUSY;
	}

//	up(&xr20m1170_i2c_mutex);
	return msgbuf[1];
#endif
}

static unsigned char xr20m1170_i2c_write(unsigned char devid,
	unsigned char reg, unsigned char value)
{
#if 1
//	printk("xr20m1170_i2c_write:%#x\n", value);
	gpio_i2c_write((i2c_xr20m1170->addr)<<1, reg, value);
#else
	int err;
	unsigned char msgbuf[2];        
	struct i2c_msg msg = {i2c_xr20m1170->addr, 0,1, msgbuf};

	msgbuf[0] = reg|devid;
	msgbuf[1] = value;
	msg.len = 2;

//	down(&xr20m1170_i2c_mutex);
	err = i2c_transfer(i2c_xr20m1170->adapter, &msg, 1);
	if (err < 0){
		printk("i2c transfer fail in i2c write.err:%d.reg:%#x.value:%#x\n"
			, err, msgbuf[0], msgbuf[1]);
//		up(&xr20m1170_i2c_mutex);
		return -EBUSY;
	}

//	up(&xr20m1170_i2c_mutex);
#endif
	return 0;        
}

static int xr20m1170_check_chip(void)
{
	unsigned char reg, save;

	DMSG("+");
	reg = xr20m1170_i2c_read(0, reg_info[XR20M1170REG_LCR][0]);
	save = reg;
	reg = 0x03;
	xr20m1170_i2c_write(0, reg_info[XR20M1170REG_LCR][0], reg);

	reg = xr20m1170_i2c_read(0, reg_info[XR20M1170REG_LCR][0]);
	if(reg == 0x03){
		printk("line:%d. fun:%s OK\n", __LINE__, __func__);
	}else{
		printk("line:%d. fun:%s Fail.>>>>>>...<<<<<<reg:%#x\n"
			, __LINE__, __func__, reg);
	}

	xr20m1170_i2c_write(0, reg_info[XR20M1170REG_LCR][0], save);
	DMSG("-");
	return 0;
}

static void EnterConstraint(unsigned char devid, unsigned char regaddr)
{
	switch (reg_info[regaddr][1]) {
		//10: no contraint
	case 20:		//20: lcr[7] == 0
		if (cached_lcr[devid] & BIT7)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid] & ~BIT7);
		break;
	case 30:		//30: lcr == 0xbf
		if (cached_lcr[devid] != 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0xbf);
		break;
	case 40:		//40: lcr != 0xbf
		if (cached_lcr[devid] == 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0x3f);
		break;
	case 50:		//50: lcr[7] == 1 , lcr != 0xbf, efr[4] = 1
		if (!(cached_efr[devid] & BIT4)) {
			if (cached_lcr[devid] != 0xbf)
				xr20m1170_i2c_write(devid,
					     reg_info[XR20M1170REG_LCR][0],
					     0xbf);
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_EFR][0],
				     cached_efr[devid] | BIT4);
		}
		if ((cached_lcr[devid] == 0xbf)
		    || (!(cached_lcr[devid] & BIT7)))
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     (cached_lcr[devid] | BIT7) & ~BIT0);
		break;
	case 60:		//60: lcr[7] == 1 , lcr != 0xbf,
		if ((cached_lcr[devid] == 0xbf)
		    || (!(cached_lcr[devid] & BIT7)))
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     (cached_lcr[devid] | BIT7) & ~BIT0);
		break;
	case 70:		//lcr != 0xbf,  and (efr[4] == 0 or efr[4] =1, mcr[2] = 0)
		if (cached_lcr[devid] == 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0x3f);
		if ((cached_efr[devid] & BIT4) && (cached_mcr[devid] & BIT2))
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_MCR][0],
				     cached_mcr[devid] & ~BIT2);
		break;
	case 80:		//lcr != 0xbf,  and (efr[4] = 1, mcr[2] = 1)
		if (cached_lcr[devid] != 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0xbf);
		if (!(cached_efr[devid] & BIT4))
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_EFR][0],
				     cached_efr[devid] | BIT4);
		xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
			     cached_lcr[devid] & ~BIT7);
		if (!(cached_mcr[devid] & BIT2))
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_MCR][0],
				     cached_mcr[devid] | BIT2);
		break;
	case 90:		//90: lcr[7] == 0, efr[4] =1 
		if (!(cached_efr[devid] & BIT4)) {
			if (cached_lcr[devid] != 0xbf)
				xr20m1170_i2c_write(devid,
					     reg_info[XR20M1170REG_LCR][0],
					     0xbf);
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_EFR][0],
				     cached_efr[devid] | BIT4);
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid] & ~BIT7);
		} else if (cached_lcr[devid] & BIT7)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid] & ~BIT7);
		break;
	case 100:		//100: lcr!= 0xbf, efr[4] =1
		if (!(cached_efr[devid] & BIT4)) {
			if (cached_lcr[devid] != 0xbf)
				xr20m1170_i2c_write(devid,
					     reg_info[XR20M1170REG_LCR][0],
					     0xbf);
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_EFR][0],
				     cached_efr[devid] | BIT4);
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0x3f);
		} else if (cached_lcr[devid] == 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0x3f);
		break;
	}
}

static void ExitConstraint(unsigned char devid, unsigned char regaddr)
{
	//restore
	switch (reg_info[regaddr][1]) {
		//10: no contraint
	case 20:		//20: lcr[7] == 0
		if (cached_lcr[devid] & BIT7)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid]);
		break;
	case 30:		//30: lcr == 0xbf
		if (cached_lcr[devid] != 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid]);
		break;
	case 40:		//40: lcr != 0xbf
		if (cached_lcr[devid] == 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0xbf);
		break;
	case 50:		//50: lcr[7] == 1 , lcr != 0xbf, efr[4] = 1
		if ((cached_efr[devid] & BIT4) == 0) {
			if (cached_lcr[devid] != 0xbf)
				xr20m1170_i2c_write(devid,
					     reg_info[XR20M1170REG_LCR][0],
					     0xbf);
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_EFR][0],
				     cached_efr[devid]);
		}
		xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
			     cached_lcr[devid]);
		break;
	case 60:		//60: lcr[7] == 1 , lcr != 0xbf,
		if ((cached_lcr[devid] == 0xbf)
		    || (!(cached_lcr[devid] & BIT7)))
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid]);
		break;
	case 70:		//lcr != 0xbf,  and (efr[4] == 0 or efr[4] =1, mcr[2] = 0)
		if ((cached_efr[devid] & BIT4) && (cached_mcr[devid] & BIT2))
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_MCR][0],
				     cached_mcr[devid]);
		if (cached_lcr[devid] == 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid]);
		break;
	case 80:		//lcr != 0xbf,  and (efr[4] = 1, mcr[2] = 1)
		if (!(cached_mcr[devid] & BIT2))
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_MCR][0],
				     cached_mcr[devid]);
		xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0], 0xbf);
		if (!(cached_efr[devid] & BIT4))
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_EFR][0],
				     cached_efr[devid]);
		if (cached_lcr[devid] != 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid]);
		break;
	case 90:		//90: lcr[7] == 0, efr[4] =1 (for ier bit 4-7)
		if (!(cached_efr[devid] & BIT4)) {
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0xbf);
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_EFR][0],
				     cached_efr[devid]);
			if (cached_lcr[devid] != 0xbf)
				xr20m1170_i2c_write(devid,
					     reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid]);
		} else if (cached_lcr[devid] & BIT7)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     cached_lcr[devid]);
		break;
	case 100:		//100: lcr!= 0xbf, efr[4] =1
		if (!(cached_efr[devid] & BIT4)) {
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0xbf);
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_EFR][0],
				     cached_efr[devid]);
			if (cached_lcr[devid] != 0xbf)
				xr20m1170_i2c_write(devid,
					     reg_info[XR20M1170REG_LCR][0],
					     cached_lcr[devid]);
		} else if (cached_lcr[devid] == 0xbf)
			xr20m1170_i2c_write(devid, reg_info[XR20M1170REG_LCR][0],
				     0xbf);
		break;
	}
}

static void serial_out(unsigned char devid, unsigned char regaddr,
		       unsigned char data)
{
	if (!(reg_info[regaddr][2] & 0x2)) {
		printk("Reg not writeable\n");
		return;
	}

	DMSG("%d, value:0x%x", regaddr, data);
	switch (regaddr) {
	case XR20M1170REG_LCR:
		if (data == cached_lcr[devid])
			return;
		cached_lcr[devid] = data;
		break;
	case XR20M1170REG_EFR:
		if (data == cached_efr[devid])
			return;
		cached_efr[devid] = data;
		break;
	case XR20M1170REG_MCR:
		if (data == cached_mcr[devid])
			return;
		cached_mcr[devid] = data;
		break;
	}

	spin_lock_irqsave(&xr20m1170_lock, xr20m1170_flags);
	EnterConstraint(devid, regaddr);
	xr20m1170_i2c_write(devid, reg_info[regaddr][0], data);

#if XR20M1170_I2C_CHECK
	if (!(reg_info[regaddr][2] & 0x1)) {
		printk("Reg not readable, don't need to check!!!\n");
	}
	else{
		unsigned char read;
		read = xr20m1170_i2c_read(devid, reg_info[regaddr][0]);
		if(read != data){
			printk("*************devid:%d, value:%#x, but read:%#x\n", devid, data, read);
		}
		else{
			printk("############devid:%d, value:%#x, and read:%#x, OK\n", devid, data, read);
		}
	}
#endif

	ExitConstraint(devid, regaddr);
	spin_unlock_irqrestore(&xr20m1170_lock, xr20m1170_flags);
}

static unsigned char serial_in(unsigned char devid, unsigned char regaddr)
{
	unsigned char ret;

	if (!(reg_info[regaddr][2] & 0x1)) {
		printk("Reg not readable\n");
		return 0;
	}

	switch (regaddr) {
	case XR20M1170REG_LCR:
		ret = cached_lcr[devid];
		break;
	case XR20M1170REG_EFR:
		ret = cached_efr[devid];
		break;
	case XR20M1170REG_MCR:
		ret = cached_mcr[devid];
		break;
	default:
		spin_lock_irqsave(&xr20m1170_lock, xr20m1170_flags);
		EnterConstraint(devid, regaddr);
		ret = xr20m1170_i2c_read(devid, reg_info[regaddr][0]);
		ExitConstraint(devid, regaddr);
		spin_unlock_irqrestore(&xr20m1170_lock, xr20m1170_flags);
	}

	DMSG("%d, value:0x%x", regaddr, ret);
	return ret;
}

static void serial_xr20m1170_enable_ms(struct uart_port *port)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;

	DMSG("+");
	up->ier |= UART_IER_MSI;
	serial_out(up->devid, XR20M1170REG_IER, up->ier);
	DMSG("-");
}

static void serial_xr20m1170_stop_tx(struct uart_port *port)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	DMSG("+");
	if (up->ier & UART_IER_THRI) {
		up->ier &= ~UART_IER_THRI;
		serial_out(up->devid, XR20M1170REG_IER, up->ier);
	}
	DMSG("-");
}

static void serial_xr20m1170_stop_rx(struct uart_port *port)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;

	DMSG("+");
	up->ier &= ~UART_IER_RLSI;
	up->port.read_status_mask &= ~UART_LSR_DR;
	serial_out(up->devid, XR20M1170REG_IER, up->ier);
	DMSG("-");
}

static void pio_receive_chars(struct xr20m1170_port *up, int *status)
{
	struct tty_struct *tty = up->port.state->port.tty;
	unsigned int ch, flag;
	int max_count = 256;

	DMSG("+");
	do {
		ch = serial_in(up->devid, XR20M1170REG_RHR);
//		printk("%#x\n", ch);
		flag = TTY_NORMAL;
		up->port.icount.rx++;

		if (unlikely(*status & (UART_LSR_BI | UART_LSR_PE |
					UART_LSR_FE | UART_LSR_OE))) {
			/* For statistics only */
			if (*status & UART_LSR_BI) {
				*status &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(&up->port))
					goto ignore_char;
			} else if (*status & UART_LSR_PE)
				up->port.icount.parity++;
			else if (*status & UART_LSR_FE)
				up->port.icount.frame++;
			if (*status & UART_LSR_OE)
				up->port.icount.overrun++;

			/* Mask off conditions which should be ignored. */
			*status &= up->port.read_status_mask;

			if (*status & UART_LSR_BI) {
				flag = TTY_BREAK;
			} else if (*status & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (*status & UART_LSR_FE)
				flag = TTY_FRAME;
		}

		uart_insert_char(&up->port, *status, UART_LSR_OE, ch, flag);

	      ignore_char:
		*status = serial_in(up->devid, XR20M1170REG_LSR);
	} while ((*status & UART_LSR_DR) && (max_count-- > 0));

	tty_flip_buffer_push(tty);
	DMSG("-");
}

static void pio_transmit_chars(struct xr20m1170_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	int count;

	DMSG("+");
	if (up->port.x_char) {
		serial_out(up->devid, XR20M1170REG_THR, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		serial_xr20m1170_stop_tx(&up->port);
		return;
	}

	count = up->port.fifosize / 2;
	do {
//		unsigned char efr = 0;
//		efr = serial_in(up->devid, XR20M1170REG_EFR);
//		printk("###cgh:%s,line:%d, efr=%d\n", __func__, __LINE__, efr);
		serial_out(up->devid, XR20M1170REG_THR, xmit->buf[xmit->tail]);
//		mdelay(100);
//		efr = serial_in(up->devid, XR20M1170REG_EFR);
//		printk("###cgh:%s,line:%d, efr=%d\n", __func__, __LINE__, efr);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		up->port.icount.tx++;
//		mdelay(100);
//		rrxx = serial_in(up->devid, XR20M1170REG_RHR);
//		printk("###cgh:%s,line:%d, rrxx=%d\n", __func__, __LINE__, rrxx);
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS){
		uart_write_wakeup(&up->port);
	}
	if (uart_circ_empty(xmit)){
		serial_xr20m1170_stop_tx(&up->port);}
	DMSG("-");
}

static void serial_xr20m1170_start_tx(struct uart_port *port)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;

	DMSG("+");
	if (!(up->ier & UART_IER_THRI)) {
		up->ier |= UART_IER_THRI;
		serial_out(up->devid, XR20M1170REG_IER, up->ier);
	}
	DMSG("-");
}

static void check_modem_status(struct xr20m1170_port *up)
{
	int status;

	DMSG("+");
	status = serial_in(up->devid, XR20M1170REG_MSR);

	if (!(status & UART_MSR_ANY_DELTA))
		return;

	if (status & UART_MSR_TERI)
		up->port.icount.rng++;
	if (status & UART_MSR_DDSR)
		up->port.icount.dsr++;
	if (status & UART_MSR_DDCD)
		uart_handle_dcd_change(&up->port, status & UART_MSR_DCD);
	if (status & UART_MSR_DCTS)
		uart_handle_cts_change(&up->port, status & UART_MSR_CTS);

	wake_up_interruptible(&up->port.state->port.delta_msr_wait);
	DMSG("-");
}

/* This handles the interrupt from one port.  */
static irqreturn_t serial_xr20m1170_irq(int irq, void *dev_id)
{
	struct xr20m1170_port *up = dev_id;
	unsigned int isr, lsr;

	DMSG("+");
	isr = serial_in(up->devid, XR20M1170REG_ISR);
	if (isr & UART_IIR_NO_INT) {
		/* FIXME, should return IRQ_NONE normally, but it would report
		 * unknown irq in PIO mode */
		return IRQ_HANDLED;
	}
	lsr = serial_in(up->devid, XR20M1170REG_LSR);
	if (lsr & UART_LSR_DR) {
		pio_receive_chars(up, &lsr);
#ifdef CONFIG_IPM
		ipm_event_notify(IPM_EVENT_DEVICE, IPM_EVENT_DEVICE_OUTD0CS,
				 NULL, 0);
#endif
	}
	check_modem_status(up);
	if (lsr & UART_LSR_THRE)
		pio_transmit_chars(up);
	DMSG("-");
	return IRQ_HANDLED;
}

static unsigned int serial_xr20m1170_tx_empty(struct uart_port *port)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	unsigned long flags;
	unsigned int ret;

	DMSG("+");
	spin_lock_irqsave(&up->port.lock, flags);
	ret =
	    serial_in(up->devid,
		      XR20M1170REG_LSR) & UART_LSR_TEMT ? TIOCSER_TEMT : 0;
	spin_unlock_irqrestore(&up->port.lock, flags);
	DMSG("-");

	return ret;
}

static unsigned int serial_xr20m1170_get_mctrl(struct uart_port *port)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	unsigned char status;
	unsigned int ret;

	DMSG("+");
	status = serial_in(up->devid, XR20M1170REG_MSR);

	ret = 0;
	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;
	DMSG("-");
	return ret;
}

static void serial_xr20m1170_set_mctrl(struct uart_port *port,
				       unsigned int mctrl)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	unsigned char mcr = 0;

	DMSG("+");

	if (mctrl & TIOCM_RTS)
		mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		mcr |= UART_MCR_LOOP;

	mcr |= up->mcr;

	serial_out(up->devid, XR20M1170REG_MCR, mcr);

	DMSG("-mcr:%x", mcr);
}

static void serial_xr20m1170_break_ctl(struct uart_port *port,
	int break_state)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	unsigned long flags;

	DMSG("+");
	spin_lock_irqsave(&up->port.lock, flags);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_out(up->devid, XR20M1170REG_LCR, up->lcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	DMSG("-");
}

static int serial_xr20m1170_startup(struct uart_port *port)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	unsigned long flags;
	int retval;

	DMSG("+");
//{
//	int err;
//	err = gpio_request(M1170_RST, "M1170_RST");
//	if (err) {
//		printk(KERN_ERR "line:%d. fun:%s failed to request M1170_RST\n",
//			__LINE__, __func__);
//		gpio_free(M1170_RST);
//		return err;
//	}
	gpio_direction_output(M1170_RST, 0);
	msleep(3);
	gpio_direction_output(M1170_RST, 1);
//}
//	xr20m1170_check_chip();
	up->mcr = 0;
	if (up->port.irq) {
		retval = request_irq(up->port.irq, serial_xr20m1170_irq,
				     IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				     up->name, (void *)up);
		if (retval){
			printk("line:%d. fun:%s request_irq fail. err:%d, irq:%d\n"
				, __LINE__, __func__, retval, up->port.irq);
			return retval;
		}
	}

	/*
	 * Clear the FIFO buffers and disable them.
	 * (they will be reenabled in set_termios())
	 */
	serial_out(up->devid, XR20M1170REG_FCR, UART_FCR_ENABLE_FIFO);
	serial_out(up->devid, XR20M1170REG_FCR,
		   UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR |
		   UART_FCR_CLEAR_XMIT);
	serial_out(up->devid, XR20M1170REG_FCR, 0);

	/* Clear the interrupt registers.  */
	(void)serial_in(up->devid, XR20M1170REG_LSR);
	(void)serial_in(up->devid, XR20M1170REG_RHR);
	(void)serial_in(up->devid, XR20M1170REG_ISR);
	(void)serial_in(up->devid, XR20M1170REG_MSR);

	/* Now, initialize the UART */
	serial_out(up->devid, XR20M1170REG_LCR, UART_LCR_WLEN8);

	spin_lock_irqsave(&up->port.lock, flags);
	up->port.mctrl |= TIOCM_OUT2;
	serial_xr20m1170_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Finally, enable interrupts.  Note: Modem status interrupts
	 * are set via set_termios(), which will be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	/*RTOIE is 0x10, and should be Sleep Mode Enable */
	//up->ier = UART_IER_RLSI | UART_IER_RDI | UART_IER_RTOIE;
	up->ier = UART_IER_RLSI | UART_IER_RDI;
	serial_out(up->devid, XR20M1170REG_IER, up->ier);

	/* And clear the interrupt registers again for luck.  */
	(void)serial_in(up->devid, XR20M1170REG_LSR);
	(void)serial_in(up->devid, XR20M1170REG_RHR);
	(void)serial_in(up->devid, XR20M1170REG_ISR);
	(void)serial_in(up->devid, XR20M1170REG_MSR);
#if 0
#ifdef CONFIG_PM
	up->power_mode = POWER_RUN;
#endif
#endif

	DMSG("-");
	return 0;
}

static void serial_xr20m1170_shutdown(struct uart_port *port)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	unsigned long flags;

	DMSG("+");
//	gpio_free(M1170_RST);
	free_irq(up->port.irq, up);

	/* Disable interrupts from this port */
	up->ier = 0;
	serial_out(up->devid, XR20M1170REG_IER, 0);

	spin_lock_irqsave(&up->port.lock, flags);
	up->port.mctrl &= ~TIOCM_OUT2;
	up->port.mctrl &= ~TIOCM_RTS;
	up->port.mctrl &= ~TIOCM_CTS;
	serial_xr20m1170_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/* Disable break condition and FIFOs */
	serial_out(up->devid, XR20M1170REG_LCR,
		   serial_in(up->devid, XR20M1170REG_LCR) & ~UART_LCR_SBC);
	serial_out(up->devid, XR20M1170REG_FCR,
		   UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR |
		   UART_FCR_CLEAR_XMIT);
	serial_out(up->devid, XR20M1170REG_FCR, 0);
	DMSG("-");
}

static void serial_xr20m1170_set_termios(struct uart_port *port,
	struct ktermios *termios, struct ktermios *old)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	unsigned char cval, fcr = 0;
	unsigned char efr;
	unsigned long flags;
	unsigned int baud;
	unsigned char dld_reg;
	unsigned char prescale;
	unsigned char samplemode;
	unsigned short required_diviser;
	unsigned long required_diviser2;

	DMSG("+");

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = UART_LCR_WLEN5;
		break;
	case CS6:
		cval = UART_LCR_WLEN6;
		break;
	case CS7:
		cval = UART_LCR_WLEN7;
		break;
	case CS8:
	default:
		cval = UART_LCR_WLEN8;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		cval |= UART_LCR_STOP;
	if (termios->c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;
	if (termios->c_cflag & CMSPAR)
		cval |= UART_LCR_SPAR;

	/* Ask the core to calculate the divisor for us.  */
	baud = uart_get_baud_rate(port, termios, old, 0, 4000000);

#ifdef CONFIG_DVFM
	up->baud = baud;	/* save for DVFM scale callback */
#endif

#if 0
	if (baud < 230400)
		fcr = UART_FCR_ENABLE_FIFO;	//8 bytes
	else
		fcr = UART_FCR_ENABLE_FIFO | 0x80;	// 56 bytes
#else
	if (baud < 230400)
		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_11 | 
			UART_FCR_T_TRIG_11;	//FIFO:TX 56 bytes, RX 60 bytes
	else
		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_11 | 
			UART_FCR_T_TRIG_11;//FIFO:TX 56 bytes, RX 60 bytes
#endif

	prescale = 1;		//divide by 1,   (MCR bit 7);
	samplemode = 4;		//4x,            (DLD bit 5:4);

	required_diviser2 = (14745600 * 16) / (prescale * samplemode * baud);
	required_diviser = required_diviser2 / 16;
	dld_reg = required_diviser2 - required_diviser * 16;

	switch (samplemode) {
	case 16:
		break;
	case 8:
		dld_reg |= 0x10;
		break;
	case 4:
		dld_reg |= 0x20;
		break;
	default:
		printk(KERN_ERR "should not be here!\n");
	}

	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&up->port.lock, flags);

	/* Update the per-port timeout.  */
	uart_update_timeout(port, termios->c_cflag, baud);

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	/* Characters to ignore */
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	/* ignore all characters if CREAD is not set */
	if (!(termios->c_cflag & CREAD))
		up->port.ignore_status_mask |= UART_LSR_DR;

	/* CTS flow control flag and modem status interrupts */
	if (UART_ENABLE_MS(&up->port, termios->c_cflag))
		up->ier |= UART_IER_MSI;
	else
		up->ier &= ~UART_IER_MSI;
	serial_out(up->devid, XR20M1170REG_IER, up->ier);

	serial_out(up->devid, XR20M1170REG_DLM, required_diviser >> 8);
	serial_out(up->devid, XR20M1170REG_DLL, required_diviser & 0xff);
	serial_out(up->devid, XR20M1170REG_DLD, dld_reg);
	up->lcr = cval;		/* Save LCR */
	serial_out(up->devid, XR20M1170REG_LCR, cval);	/* reset DLAB */
	serial_xr20m1170_set_mctrl(&up->port, up->port.mctrl);
	serial_out(up->devid, XR20M1170REG_FCR, fcr);

	if (termios->c_cflag & CRTSCTS) {
		efr = serial_in(up->devid, XR20M1170REG_EFR);
		efr = efr | UART_EFR_RTS | UART_EFR_CTS;
		serial_out(up->devid, XR20M1170REG_EFR, efr);
	}

	spin_unlock_irqrestore(&up->port.lock, flags);
	DMSG("-");
}

static void serial_xr20m1170_pm(struct uart_port *port, unsigned int state,
				unsigned int oldstate)
{
	//struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	//pxa_set_cken(up->cken, !state);
	if (!state)
		udelay(1);
}

static void serial_xr20m1170_release_port(struct uart_port *port)
{
}

static int serial_xr20m1170_request_port(struct uart_port *port)
{
	return 0;
}

static void serial_xr20m1170_config_port(struct uart_port *port, int flags)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	up->port.type = PORT_PXA;
}

static int serial_xr20m1170_verify_port(struct uart_port *port,
					struct serial_struct *ser)
{
	/* we don't want the core code to modify any port params */
	return -EINVAL;
}

static const char *serial_xr20m1170_type(struct uart_port *port)
{
	struct xr20m1170_port *up = (struct xr20m1170_port *)port;
	return up->name;
}

struct uart_ops serial_xr20m1170_pops = {
	.tx_empty = serial_xr20m1170_tx_empty,
	.set_mctrl = serial_xr20m1170_set_mctrl,
	.get_mctrl = serial_xr20m1170_get_mctrl,
	.stop_tx = serial_xr20m1170_stop_tx,
	.start_tx = serial_xr20m1170_start_tx,
	.stop_rx = serial_xr20m1170_stop_rx,
	.enable_ms = serial_xr20m1170_enable_ms,
	.break_ctl = serial_xr20m1170_break_ctl,
	.startup = serial_xr20m1170_startup,
	.shutdown = serial_xr20m1170_shutdown,
	.set_termios = serial_xr20m1170_set_termios,
	.pm = serial_xr20m1170_pm,
	.type = serial_xr20m1170_type,
	.release_port = serial_xr20m1170_release_port,
	.request_port = serial_xr20m1170_request_port,
	.config_port = serial_xr20m1170_config_port,
	.verify_port = serial_xr20m1170_verify_port,
};

static struct xr20m1170_port serial_xr20m1170_ports[] = {
	{
	 .name = "XMUART0",
	 .devid = 0,
	 .port = {
		  .type = PORT_PXA,
		  .iotype = UPIO_MEM,
		  .membase = 0,
		  .mapbase = 0,
		  .irq = M1170_IRQ_NUM,
		  .uartclk = 24000000,
		  .fifosize = 64,
		  .ops = &serial_xr20m1170_pops,
		  .line = 0,
		  },
	 },
};

static struct uart_driver serial_xr20m1170_reg = {
	.owner = THIS_MODULE,
	.driver_name = "xm20m1170",
	.dev_name = "ttyXM",
	.major = TTY_MAJOR,
	.minor = 70,
	.nr = ARRAY_SIZE(serial_xr20m1170_ports),
	.cons = NULL,
};

#ifdef CONFIG_PM
static int serial_xr20m1170_suspend(struct platform_device *dev,
				    pm_message_t state)
{
	struct xr20m1170_port *sport = &serial_xr20m1170_ports;//platform_get_drvdata(dev);

	DMSG("+");
	if (sport) {
		if (!(sport->ier & UART_IERX_SLEEP)) {
			sport->ier |= UART_IERX_SLEEP;
			serial_out(sport->devid, XR20M1170REG_IER, sport->ier);
		}

		sport->power_mode = POWER_PRE_SUSPEND;
		uart_suspend_port(&serial_xr20m1170_reg, &sport->port);
		sport->power_mode = POWER_SUSPEND;
	}
	DMSG("-");
	return 0;
}

static int serial_xr20m1170_resume(struct platform_device *dev)
{
	struct xr20m1170_port *sport = &serial_xr20m1170_ports;//platform_get_drvdata(dev);

	DMSG("+");
	if (sport) {
		sport->power_mode = POWER_PRE_RESUME;
		uart_resume_port(&serial_xr20m1170_reg, &sport->port);
		sport->power_mode = POWER_RUN;

		if ((sport->ier & UART_IERX_SLEEP)) {
			sport->ier &= ~UART_IERX_SLEEP;
			serial_out(sport->devid, XR20M1170REG_IER, sport->ier);
		}
	}

	DMSG("-");
	return 0;
}
#else
#define serial_xr20m1170_suspend    NULL
#define serial_xr20m1170_resume    NULL
#endif

static int serial_xr20m1170_probe(struct platform_device *dev)
{
	serial_xr20m1170_ports[dev->id].port.dev = &dev->dev;
	uart_add_one_port(&serial_xr20m1170_reg,
			  &serial_xr20m1170_ports[dev->id].port);
	platform_set_drvdata(dev, &serial_xr20m1170_ports[dev->id]);
#if 0
#ifdef CONFIG_PM
	serial_xr20m1170_ports[dev->id].power_mode = POWER_RUN;
#endif
#endif

	return 0;
}

static int serial_xr20m1170_remove(struct platform_device *dev)
{
	struct xr20m1170_port *sport = platform_get_drvdata(dev);
	platform_set_drvdata(dev, NULL);

	if (sport)
		uart_remove_one_port(&serial_xr20m1170_reg, &sport->port);

	return 0;
}

static struct platform_driver serial_xr20m1170_driver = {
	.probe = serial_xr20m1170_probe,
	.remove = serial_xr20m1170_remove,
	.suspend = serial_xr20m1170_suspend,
	.resume = serial_xr20m1170_resume,
	.driver = {
		   .name = "xr20m1170-uart",
		   },
};

static int xr20m1170_gpio_init(void)
{
	int err;

	DMSG("+");
	err = gpio_request(M1170_RST, "M1170_RST");
	if (err) {
		printk(KERN_ERR "line:%d. fun:%s failed to request M1170_RST\n",
			__LINE__, __func__);
		gpio_free(M1170_RST);
		return err;
	}
	s3c_gpio_setpull(M1170_RST, S3C_GPIO_PULL_NONE);
	gpio_direction_output(M1170_RST, 0);
	msleep(3);
	gpio_direction_output(M1170_RST, 1);

	err = gpio_request(M1170_IRQ, "M1170_IRQ");
	if(err) {
		printk(KERN_ERR "line:%d. fun:%sfailed to request M1170_IRQ\n"
			,__LINE__, __func__);
		gpio_free(M1170_IRQ);
		return err;
       }
	s3c_gpio_cfgpin(M1170_IRQ, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(M1170_IRQ, S3C_GPIO_PULL_UP);

	err = gpio_request(GPIO_IIC_SCL, "GPIO_IIC_SCL");
	if (err){
		printk(KERN_ERR "line:%d. fun:%sfailed to request GPIO_IIC_SCL\n"
			,__LINE__, __func__);
		gpio_free(GPIO_IIC_SCL);
		return err;
	}
	s3c_gpio_setpull(GPIO_IIC_SCL, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_IIC_SCL, 1);

	err = gpio_request(GPIO_IIC_SDA, "GPIO_IIC_SDA");
	if (err){
		printk(KERN_ERR "line:%d. fun:%sfailed to request GPIO_IIC_SDA\n"
			,__LINE__, __func__);
		gpio_free(GPIO_IIC_SDA);
		return err;
	}
	s3c_gpio_setpull(GPIO_IIC_SDA, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_IIC_SDA, 1);

	printk("line:%d. fun:%s ok\n", __LINE__, __func__);
	DMSG("-");

	return 0;
}

static int __devinit i2c_xr20m1170_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{

	DMSG("+");

	i2c_xr20m1170 = i2c;
	xr20m1170_gpio_init();
	xr20m1170_check_chip();

	cached_lcr[0] = xr20m1170_i2c_read(0, reg_info[XR20M1170REG_LCR][0]);
//	cached_efr[0] = xr20m1170_i2c_read(0, reg_info[XR20M1170REG_EFR][0]);
	cached_efr[0] = 0x00;//disable software flow control,default 0x01
	cached_mcr[0] = xr20m1170_i2c_read(0, reg_info[XR20M1170REG_MCR][0]);

//	go_main();

	DMSG("-");
	return 0;
}

static int __devexit i2c_xr20m1170_remove(struct i2c_client *i2c)
{
	return 0;
}

static const struct i2c_device_id i2c_xr20m1170_id[] = {
	{
	.name = "i2c_xr20m1170",
	.driver_data = 0,
	},
};

static struct i2c_driver i2c_xr20m1170_driver = {
	.driver = {
		.name		= "i2c_xr20m1170",
		.owner		= THIS_MODULE,
	},
	.id_table = i2c_xr20m1170_id,
	.probe		= i2c_xr20m1170_probe,
	.remove		= __devexit_p(i2c_xr20m1170_remove),
};

int __init serial_xr20m1170_init(void)
{
	int ret;

	DMSG("+");
	i2c_add_driver(&i2c_xr20m1170_driver);

	ret = uart_register_driver(&serial_xr20m1170_reg);
	if (ret != 0)
		return ret;

	ret = platform_driver_register(&serial_xr20m1170_driver);
	if (ret != 0)
		uart_unregister_driver(&serial_xr20m1170_reg);

	DMSG("-");
	return ret;
}

void __exit serial_xr20m1170_exit(void)
{
	DMSG("+");
	platform_driver_unregister(&serial_xr20m1170_driver);
	uart_unregister_driver(&serial_xr20m1170_reg);
	i2c_del_driver(&i2c_xr20m1170_driver);
	DMSG("-");
}

module_init(serial_xr20m1170_init);
module_exit(serial_xr20m1170_exit);
MODULE_LICENSE("GPL");
