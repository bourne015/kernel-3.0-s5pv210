// Sample test program for I2C/SPI UARTs, adapted from DOS test program for 8-bit UARTs

#include "117x.h"
#include "xr20m1170.h"
#include "gpio_i2c.h"
#include <linux/interrupt.h>

#define intvect  0x0d		//default interrupt vector for IRQ5
#define SIZE     9999
#define INTR 	0x03


int baseaddr = 0x2f8;
long int bufferin0, bufferout0;
char buffer0[SIZE];
int r_trg = 56;  //trigger levels
int t_trg = 8;
//int txd = 64 - t_trg; //no. of characters to be loaded into the TX FIFO
int txd = 56; //no. of characters to be loaded into the TX FIFO
char data0 [0x100]; // data to be transmitted
int databuf0;

/* oldisr's are the existing interrupt service routines */

//void interrupt (*oldportisr) (...);

/* myisr()'s are the new interrupt service routines
	 which will be used for this program */

/* Read and Write functions*/

unsigned char read (unsigned char reg)
{
	unsigned char read;

	read = gpio_i2c_read(0x6A, reg << 3);

	return read;
}

void write (unsigned char reg, unsigned char value)
{
	gpio_i2c_write(0x6A, reg << 3, value);
}

static irqreturn_t myisr(int irq, void *dev_id)
{
	 int int_val0;
	 int i;

	 write (IER, 0x00);
	 int_val0 = read (ISR);
	printk("line:%d. fun:%s, int_val0:%#x\n", __LINE__, __func__, int_val0);

	switch (int_val0)	{
		case 0xc4	:	for (i = 0; i < r_trg; i++) {
							printk("line:%d. fun:%s\n", __LINE__, __func__);
							buffer0[bufferin0] = read (RHR);
							bufferin0++;
							if (bufferin0 == SIZE) bufferin0 = 0;
						}
						break;
		case 0xcc	:	while ( (read(LSR) & 0x01 )  ) {
							printk("line:%d. fun:%s\n", __LINE__, __func__);
							buffer0[bufferin0] = read (RHR);
							bufferin0++;
							if (bufferin0 == SIZE) bufferin0 = 0;
						}
						break;
		case 0xc2	:	for (i = 0; i < txd; i++) {
							printk("line:%d. fun:%s\n", __LINE__, __func__);
							write (THR, data0[databuf0]);
							databuf0++;
							if (databuf0 == 0x100) databuf0 = 0;
						}
						break;
		default		:	;
	}

	write (IER, INTR);
	return IRQ_HANDLED;
}

int go_main (void)
{
  int i, flowcontrol, flag, char0, dt0, error, err;
  long int cnt = 0;
	unsigned char dld_reg;
	unsigned char prescale;
	unsigned char samplemode;
	unsigned short required_diviser;
	unsigned long required_diviser2;
  int xon1 = 0x13;
  int xon2 = 0x14;
  int xoff1 = 0x11;
  int xoff2 = 0x12;

  dt0 = 0;
  error = 0;
  bufferin0 = 0;
  bufferout0 = 0;
  databuf0 = 0;

	prescale = 4;		//divide by 1,   (MCR bit 7);
	samplemode = 4;		//4x,            (DLD bit 5:4);

	required_diviser2 = (14745600 * 16) / (prescale * samplemode * 9600);
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

//  buffer0 = new char [SIZE];

	for (i = 0; i < 0x100; i++) {
		switch (i) {
			case 0x11 : case 0x13 :
			case 0x12 : case 0x14 :
									data0[i] = i + 0x80;
									break;
			default	:				data0[i] = i;
		}
	}

/* Test to see if the UART is there */
	write (SPR, 0x55);
	if (read (SPR) != 0x55) {
		printk("\n UART Not detected\n");
		return -1;
	}

	err = request_irq(M1170_IRQ_NUM, myisr,
			     IRQF_TRIGGER_FALLING | IRQF_DISABLED,
			     "1170_isr", NULL);
	if (err){
		printk("line:%d. fun:%s request_irq fail. err:%d\n"
			, __LINE__, __func__, err);
		return err;
	}
	disable_irq(M1170_IRQ_NUM);

/* Obtain flow control selection from user */
//	cout << "\nEnter flow control parameters from the following options\n";
//	cout << "\t0) No Flow Control\n";
//	cout << "\t1) Hardware Flow Control (Auto-RTS & Auto-CTS)\n";
//	cout << "\t2) Software Flow Control, with XON1/XOFF1\n";
//	cout << "\t3) Software Flow Control, with XON2/XOFF2\n";
//	cout << "\t4) Software Flow Control, with XON1 & XON2/XOFF1 & XOFF2\n";

//	cin >> flag;
	flag = 0;
	switch (flag)	{
		case 0  :	flowcontrol = 0x00; break;
		case 1	:	flowcontrol = 0xc0;	break;
		case 2	:	flowcontrol = 0x0a;	break;
		case 3	:	flowcontrol = 0x05;	break;
		case 4	:	flowcontrol = 0x0f;	break;
		default	:	flowcontrol = 0x00;	break;
	}

/* Initialization */
  write (LCR, 0xBF);	// enable access to enhanced registers
  write (XOFF1, xoff1);
  write (XON1, xon1);
  write (XOFF2, xoff2);
  write (XON2, xon2);
  write (EFR, 0x10 | flowcontrol);	// enable shaded bits and access to DLD, and flow control	
  write (LCR, 0x80);	// enable access to divisors
  write (DLM, required_diviser >> 8);
  write (DLL, required_diviser & 0xff);	//enter value for divisors
  write (DLD, dld_reg);	//accessible only if EFR bit-4 = 1
  write (LCR, 0x03);	// 8N1 data format
  write (FCR, 0x87);	// RX trg level = 56; TX trg level = 8, enable and reset FIFOs
  write (IER, 0x00);

/* If hardware flow control is used, enable RTS initially */
	if (flag == 1) write (MCR, 0xa);	// assert RTS and enable interrupt output
	else write (MCR, 0x8);	// only enable interrupt output

/* Wait till a key is pressed...*/
//	cout << "\nDevice Initialized. Press ENTER to continue..";
//	while (! kbhit() );
//	char dummy = getchar();

//  clrscr();
//  cout << "\nFull-duplex TX/RX in one channel in progress using ";
//  switch (flag) {
//	case 0	:	cout << "\nNo Flow Control\n";
//				break;
//	case 1	:	cout << "\nHardware Flow Control\n";
//				break;
//	case 2	:	cout << "\nSoftware Flow Control, with XON1/XOFF1\n";
//				break;
//	case 3	:	cout << "\nSoftware Flow Control, with XON2/XOFF2\n";
//				break;
//	case 4	:	cout << "\nSoftware Flow Control, with XON1 & XON2/XOFF1 & XOFF2\n";
//				break;
//  }

/* enable IRQ5 in the PIC & enable UART interrupts */
//	outportb (0x21, ( inportb(0x21) & 0xdf) );
	write (IER, INTR);

	printk("\nNo. of packets received:\n");
	disable_irq(M1170_IRQ_NUM);

	while ( 1 ) {printk("line:%d. fun:%s\n", __LINE__, __func__);
		if (bufferin0 != bufferout0) {printk("line:%d. fun:%s\n", __LINE__, __func__);
			char0 = ( (int) buffer0[bufferout0]) & 0xff;
			if (buffer0[bufferout0] != data0[dt0]) {
				printk("\nError: Expected %#x; Received %#x\n", ((int)data0[dt0] & 0xff), char0);
				error++;
			}

			bufferout0++;
			if (bufferout0 == SIZE) {
				bufferout0 = 0;
			}
			dt0++;
			if (dt0 == 0x100) {
			  cnt++;
			  dt0 = 0;
			  printk("\ncnt: %ld\n",cnt);
			}
		}

		if (error > 5) {
			write (MCR, 0x01);
			break;
		}
  }

   printk("\nTotal number of errors during receive .error = 6. ");
   write (IER, 0x00);
return 0;
}
