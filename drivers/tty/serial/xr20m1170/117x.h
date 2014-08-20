#define	RHR		0x0
#define	THR		0x0
#define	IER		0x1
#define	FCR		0x2
#define	ISR		0x2
#define	LCR		0x3
#define	MCR		0x4
#define	LSR		0x5
#define	MSR		0x6
#define	TCR		0x6
#define	SPR		0x7
#define	TLR		0x07
#define	TXLVL		0x08
#define	RXLVL		0x09
#define	IODIR		0x0A
#define	IOSTATE	0x0B
#define	IOINT		0x0C
#define	IOCONT	0x0E
#define	EFCR		0x0F

/* LCR bit-7 needs to be '1'	*/

#define	DLL		0
#define	DLM		1
#define	DLD		2

/* LCR needs to be 0xbf	*/

#define  EFR		2
#define	XON1		4
#define	XON2		5
#define	XOFF1		6
#define	XOFF2		7

int go_main (void);

