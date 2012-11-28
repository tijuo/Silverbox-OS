#ifndef PIC_H
#define PIC_H

void sendEOI( void );
void enableIRQ( unsigned int irq );
void disableIRQ( unsigned int irq );
void enableAllIRQ( void );
void disableAllIRQ( void );

#endif /* PIC_H */
