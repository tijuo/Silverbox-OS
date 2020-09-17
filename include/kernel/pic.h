#ifndef KERNEL_PIC_H
#define KERNEL_PIC_H

#define PIC1_CMD_PORT       0x20
#define PIC1_DATA_PORT      0x21
#define PIC2_CMD_PORT       0xA0
#define PIC2_DATA_PORT      0xA1

#define PIC_EOI             0x20

#define PIC2_IRQ_START      8

void sendEOI( void );
void enableIRQ( unsigned int irq );
void disableIRQ( unsigned int irq );

#endif /* KERNEL_PIC_H */
