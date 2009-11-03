#include <os/io.h>
#include <kernel/pic.h>

HOT(void sendEOI(void));
COLD(void enableIRQ( int irq ));
COLD(void disableIRQ( int irq ));
COLD(void enableAllIRQ(void));
COLD(void disableAllIRQ(void));

/* XXX: Maybe I should use APIC? */

void sendEOI( void )
{
  outByte( (word)0x20, (byte)0x20 );
  ioWait();
  outByte( (word)0xA0, (byte)0x20 );
  ioWait();
}

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

void enableAllIRQ( void )
{
  outByte( (word)0x21, 0 );
  ioWait();
  outByte( (word)0xA1, 0 );
  ioWait();
}

void disableAllIRQ( void )
{
  outByte( (word)0x21, 0xFF );
  ioWait();
  outByte( (word)0xA1, 0xFF );
  ioWait();
}
