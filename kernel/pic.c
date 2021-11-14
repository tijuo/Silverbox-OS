#include <os/io.h>
#include <stdbool.h>
#include <kernel/pic.h>
#include <kernel/bits.h>
#include <kernel/error.h>
#include <kernel/debug.h>

/* TODO: Maybe I should use APIC? */

/// Send a non-specific EOI to the master and slave PICs
void sendAutoEOI(void) {
  // Send OCW2 (non-specific EOI)
  outPort8(PIC1_PORT, PIC_NON_SPEC_EOI);
  outPort8(PIC2_PORT, PIC_NON_SPEC_EOI);
}

/** Send a specific EOI to the PIC(s)
 @param irq An IRQ number from 0 to 15.

 */

void sendEOI(unsigned int irq) {
  assert(irq < 16);

  if(irq >= PIC2_IRQ_START) {
    outPort8(PIC1_PORT, PIC_EOI | SLAVE_IRQ);
    outPort8(PIC2_PORT, PIC_EOI | (irq - PIC2_IRQ_START));
  }
  else
    outPort8(PIC1_PORT, PIC_EOI | irq);
}

/**
 Unmasks an IRQ (enables an IRQ)

 @param irq An IRQ number from 0 to 15.
 */

void enableIRQ(unsigned int irq) {
  assert(irq < 16);

  // Send OCW1 (set IRQ mask)

  if(irq < PIC2_IRQ_START)
    outPort8(PIC1_PORT | 0x01,
             inPort8(PIC1_PORT | 0x01) & (uint8_t)~FROM_FLAG_BIT(irq));
  else
    outPort8(
        PIC2_PORT | 0x01,
        inPort8(PIC2_PORT | 0x01) & (uint8_t)~FROM_FLAG_BIT(irq-PIC2_IRQ_START));
}

/**
 Masks an IRQ (disables an IRQ)

 @param irq An IRQ number from 0 to 15.
 */

void disableIRQ(unsigned int irq) {
  assert(irq < 16);

  // Send OCW1 (set IRQ mask)

  if(irq < PIC2_IRQ_START)
    outPort8( PIC1_PORT | 0x01,
             inPort8(PIC1_PORT | 0x01) | (uint8_t)FROM_FLAG_BIT(irq));
  else
    outPort8(
        PIC2_PORT | 0x01,
        inPort8(PIC2_PORT | 0x01) | (uint8_t)FROM_FLAG_BIT(irq-PIC2_IRQ_START));
}

/**
 * Retrieves the IRQ that's currently being handled.
 *
 * @return The current IRQ in service. E_FAIL, if no IRQs are being handled.
 */

int getInServiceIRQ(void) {
  uint8_t pic1Byte;
  unsigned int pic1Bit;

  outPort8(PIC1_PORT, PIC_READ_ISR);
  pic1Byte = inPort8(PIC1_PORT);

  if(pic1Byte == 0)
    return E_FAIL;
  else {
    pic1Bit = _bit_scan_forward(pic1Byte);

    //__asm__("bsf %1, %0" : "=r"(pic1Bit) : "r"((unsigned int)pic1Byte));

    if(pic1Bit == SLAVE_IRQ) {
      uint8_t pic2Byte;
      unsigned int pic2Bit;

      outPort8(PIC2_PORT, PIC_READ_ISR);
      pic2Byte = inPort8(PIC2_PORT);

      if(pic2Byte == 0)
        return E_FAIL;
      else {
        pic2Bit = _bit_scan_forward(pic2Byte);
        //asm("bsf %1, %0" : "=r"(pic2Bit) : "r"((unsigned int)pic2Byte));
        return PIC2_IRQ_START + pic2Bit;
      }
    }
    else
      return pic1Bit;
  }
}

bool isIRQInService(unsigned int irq) {
  outPort8(PIC1_PORT, PIC_READ_ISR);
  uint8_t pic1Byte = inPort8(PIC1_PORT);

  if(irq >= PIC2_IRQ_START) {
    outPort8(PIC2_PORT, PIC_READ_ISR);
    uint8_t pic2Byte = inPort8(PIC2_PORT);

    return IS_FLAG_SET(pic1Byte, (1 << SLAVE_IRQ))
        && IS_FLAG_SET(pic2Byte, (1 << (irq - PIC2_IRQ_START)));
  }
  else
    return IS_FLAG_SET(pic1Byte, (1 << irq));
}
