#include <kernel/io.h>
#include <kernel/pic.h>

HOT(void sendEOI(void));
COLD(void enableIRQ( unsigned int irq ));
COLD(void disableIRQ( unsigned int irq ));

/* TODO: Maybe I should use APIC? */

/// Send an EOI to the master and slave PICs

void sendEOI( void )
{
  outByte( (word)0x20u, (byte)0x20u );
  ioWait();
  outByte( (word)0xA0u, (byte)0x20u );
  ioWait();
}

/**
    Unmasks an IRQ (enables an IRQ)

    @param irq An IRQ number from 0 to 15.
*/

void enableIRQ( unsigned int irq )
{
  if( irq < 8 )
  {
    outByte( (word)0x21u, inByte((word)0x21u) & (byte)~(1u << irq));
    ioWait();
  }
  else
  {
    outByte( (word)0xA1u, inByte((word)0xA1u) & (byte)~(1u << (irq-8)));
    ioWait();
  }
}

/**
    Masks an IRQ (disables an IRQ)

    @param irq An IRQ number from 0 to 15.
*/

void disableIRQ( unsigned int irq )
{
  if( irq < 8 )
  {
    outByte( (word)0x21, inByte((word)0x21u) | (byte)(1u << irq));
    ioWait();
  }
  else
  {
    outByte( (word)0xA1, inByte((word)0xA1) | (byte)(1u << (irq-8)));
    ioWait();
  }
}
