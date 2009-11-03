#ifndef PIC_H
#define PIC_H

void sendEOI( void );
void enableIRQ( int irq );
void disableIRQ( int irq );
void enableAllIRQ( void );
void disableAllIRQ( void );

#endif /* PIC_H */
