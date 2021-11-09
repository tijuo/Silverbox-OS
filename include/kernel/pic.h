#ifndef KERNEL_PIC_H
#define KERNEL_PIC_H

#define PIC1_PORT           0x20u
#define PIC2_PORT           0xA0u

#define SLAVE_IRQ           0x02u

#define PIC_READ_ISR        0x0Bu

// Non-specific EOI
#define PIC_NON_SPEC_EOI    0x20u

#define PIC_EOI             0x60u

#define PIC2_IRQ_START      8u

void sendAutoEOI( void );
void sendEOI( unsigned int irq );
void enableIRQ( unsigned int irq );
void disableIRQ( unsigned int irq );
int getInServiceIRQ( void );
bool isIRQInService(unsigned int irq);

#endif /* KERNEL_PIC_H */
