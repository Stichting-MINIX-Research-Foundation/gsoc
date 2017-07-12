#ifndef _PL011_SERIAL_H
#define _PL011_SERIAL_H

/* UART register map */
#define PL011_UART0_BASE 0x3f201000	/* UART0 physical address */

/* UART registers */
#define PL011_DR		0x000	/* Data register, */
#define PL011_SR_CR		0x004	/* Receive status register/error clear register */
#define PL011_FR		0x018	/* Flag register, */
#define PL011_ILPR		0x020	/* IrDA low-power counter register */
#define PL011_IBRD		0x024	/* Integer baud rate register */
#define PL011_FBRD		0x028	/* Fractional baud rate register */
#define PL011_LCR_H		0x02C	/* Line control register, */
#define PL011_CR		0x030	/* control register */
#define PL011_IFLS		0x034	/* Interrupt FIFO level select register */
#define PL011_IMSC		0x038	/* Interrupt mask set/clear register */
#define PL011_RIS		0x03C	/* Raw interrupt status register */
#define PL011_MIS		0x040	/* Masked interrupt status register */
#define PL011_ICR		0x044	/* Interrupt clear register */
#define PL011_DMACR		0x048	/* DMA control register */

/* Raw interrupt status register */
#define PL011_RIS_RXRIS	0x10 	/* receiver interrupt */
#define PL011_RIS_TXRIS	0x20 	/* transmiter interrupt */
#define PL011_RIS_RTRIS 0x40 	/* timeout interrupt */

/* Flag Register bits */
#define PL011_FR_RXFE	0x10 	/* receive fifo is empty */
#define PL011_FR_TXFF	0x20 	/* transmit fifo is full */
#define PL011_FR_RXFF  	0x40 	/* receive fifo is full */
#define PL011_FR_TXFE	0x80 	/* transmit fifo is empty */
#define PL011_FR_CTS 	0x00 	/* clear to send */

/* Line Control Register bits */
#define PL011_LCR_BRK		0x01 	/* Send break */
#define PL011_LCR_EPS 		0x04 	/* Even parity select */
#define PL011_LCR_SPS 		0x80 	/* Stick parity select */
#define PL011_LCR_FEN		0x10 	/* Enable FIFO */
#define PL011_LCR_PARITY	0x02	/* Enable parity */
#define PL011_LCR_STP2		0x08	/* Stop bits; 0=1 bit, 1=2 bits */
#define PL011_LCR_WLEN5		0x00	/* Wordlength 5 bits */
#define PL011_LCR_WLEN6		0x20	/* Wordlength 6 bits */
#define PL011_LCR_WLEN7		0x40	/* Wordlength 7 bits */
#define PL011_LCR_WLEN8		0x60	/* Wordlength 8 bits */

/* Interrupt FIFO Level Select */
#define PL011_IFLS_RXIFLSEL18 0x00 	/* receive fifo become 1/8 full */
#define PL011_IFLS_RXIFLSEL14 0x08 	/* receive fifo become 1/4 full */
#define PL011_IFLS_RXIFLSEL12 0x10 	/* receive fifo become 1/2 full */
#define PL011_IFLS_RXIFLSEL34 0x18 	/* receive fifo become 3/4 full */
#define PL011_IFLS_RXIFLSEL78 0x20 	/* receive fifo become 7/8 full */
#define PL011_IFLS_TXIFLSEL18 0x00 	/* receive fifo become 1/8 full */
#define PL011_IFLS_TXIFLSEL14 0x01 	/* receive fifo become 1/4 full */
#define PL011_IFLS_TXIFLSEL12 0x02 	/* receive fifo become 1/2 full */
#define PL011_IFLS_TXIFLSEL34 0x03 	/* receive fifo become 3/4 full */
#define PL011_IFLS_TXIFLSEL78 0x04 	/* receive fifo become 7/8 full */

#endif /* _PL011_SERIAL_H */
