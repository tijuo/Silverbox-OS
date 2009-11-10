#include <os/io.h>
#include <kernel/pic.h>

HOT(void sendEOI(void));
COLD(void enableIRQ( int irq ));
COLD(void disableIRQ( int irq ));
COLD(void enableAllIRQ(void));
COLD(void disableAllIRQ(void));

/* TODO: Maybe I should use APIC? */

/// Send an EOI to the master and slave PICs

void sendEOI( void )
{
  outByte( (word)0x20, (byte)0x20 );
  ioWait();
  outByte( (word)0xA0, (byte)0x20 );
  ioWait();
}

/** 
    Unmasks an IRQ (enables an IRQ)

    @param irq An IRQ number from 0 to 15.
*/

void enableIRQ( int irq )
{
  volatile byte data;

  if( irq < 0 || irq > 15 )
    return;

  if( irq < 8 )
  {
    data = inByte( (word)0x21 );
    ioWait();
    outByte( (word)0x21, data & (byte)~(1 << (byte)irq));
    ioWait();
  }
  else
  {
    data = inByte( (word)0xA1 );
    ioWait();
    outByte( (word)0xA1, data & (byte)~(1 << ((byte)irq-8)));
    ioWait();
  }
}

/** 
    Masks an IRQ (disables an IRQ)

    @param irq An IRQ number from 0 to 15.
*/

void disableIRQ( int irq )
{
  volatile byte data;

  if( irq < 0 || irq > 15 )
    return;

  if( irq < 8 )
  {
    data = inByte( (word)0x21 );
    ioWait();
    outByte( (word)0x21, data | (byte)(1 << (byte)irq));
    ioWait();
  }
  else
  {
    data = inByte( (word)0xA1 );
    ioWait();
    outByte( (word)0xA1, data | (byte)(1 << ((byte)irq-8)));
    ioWait();
  }
}

/// Unmask all IRQs (enables all IRQs)

void enableAllIRQ( void )
{
  outByte( (word)0x21, 0 );
  ioWait();
  outByte( (word)0xA1, 0 );
  ioWait();
}

/// Mask all IRQs (disables all IRQs)

void disableAllIRQ( void )
{
  outByte( (word)0x21, 0xFF );
  ioWait();
  outByte( (word)0xA1, 0xFF );
  ioWait();
}
