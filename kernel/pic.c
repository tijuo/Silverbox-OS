#include <kernel/io.h>
#include <kernel/pic.h>

HOT(void sendEOI(void));
COLD(void enableIRQ( unsigned int irq ));
COLD(void disableIRQ( unsigned int irq ));
COLD(void enableAllIRQ(void));
COLD(void disableAllIRQ(void));

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
  byte data;

  if( irq > 15 )
    return;

  if( irq < 8 )
  {
    data = inByte( (word)0x21u );
    ioWait();
    outByte( (word)0x21u, data & (byte)~(1u << irq));
    ioWait();
  }
  else
  {
    data = inByte( (word)0xA1u );
    ioWait();
    outByte( (word)0xA1u, data & (byte)~(1u << (irq-8)));
    ioWait();
  }
}

/**
    Masks an IRQ (disables an IRQ)

    @param irq An IRQ number from 0 to 15.
*/

void disableIRQ( unsigned int irq )
{
  byte data;

  if( irq > 15 )
    return;

  if( irq < 8 )
  {
    data = inByte( (word)0x21 );
    ioWait();
    outByte( (word)0x21, data | (byte)(1u << irq));
    ioWait();
  }
  else
  {
    data = inByte( (word)0xA1 );
    ioWait();
    outByte( (word)0xA1, data | (byte)(1u << (irq-8)));
    ioWait();
  }
}

/// Unmask all IRQs (enables all IRQs)

void enableAllIRQ( void )
{
  outByte( (word)0x21u, 0 );
  ioWait();
  outByte( (word)0xA1u, 0 );
  ioWait();
}

/// Mask all IRQs (disables all IRQs)

void disableAllIRQ( void )
{
  outByte( (word)0x21u, 0xFFu );
  ioWait();
  outByte( (word)0xA1u, 0xFFu );
  ioWait();
}
