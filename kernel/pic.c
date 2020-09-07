#include <kernel/io.h>
#include <kernel/pic.h>
#include <kernel/bits.h>

HOT(void sendEOI(void));
COLD(void enableIRQ( unsigned int irq ));
COLD(void disableIRQ( unsigned int irq ));

/* TODO: Maybe I should use APIC? */

/// Send an EOI to the master and slave PICs

void sendEOI( void )
{
  outByte(PIC1_CMD_PORT, PIC_EOI);
  outByte(PIC2_CMD_PORT, PIC_EOI);
  ioWait();
}

/**
    Unmasks an IRQ (enables an IRQ)

    @param irq An IRQ number from 0 to 15.
*/

void enableIRQ(unsigned int irq)
{
  if(irq < PIC2_IRQ_START)
    outByte(PIC1_DATA_PORT, inByte(PIC1_DATA_PORT) & (byte)~FROM_FLAG_BIT(irq));
  else
    outByte(PIC2_DATA_PORT, inByte(PIC2_DATA_PORT) & (byte)~FROM_FLAG_BIT(irq-PIC2_IRQ_START));

  ioWait();
}

/**
    Masks an IRQ (disables an IRQ)

    @param irq An IRQ number from 0 to 15.
*/

void disableIRQ(unsigned int irq)
{
  if(irq < PIC2_IRQ_START)
    outByte( PIC1_DATA_PORT, inByte(PIC1_DATA_PORT) | (byte)FROM_FLAG_BIT(irq));
  else
    outByte(PIC2_DATA_PORT, inByte(PIC2_DATA_PORT) | (byte)FROM_FLAG_BIT(irq-PIC2_IRQ_START));

  ioWait();
}
