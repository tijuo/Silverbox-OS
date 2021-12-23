#include <kernel/kernel.h>
#include <kernel/interrupt.h>
#include <kernel/io.h>
#include <kernel/dma.h>

void dma_calc_blk(struct DMA_blk *blk, void *data, word len)
{
  blk->page = ((addr_t)data >> 16);
  blk->offset = ((addr_t)data & 0xFFFF);
  blk->length = len - 1; // DMA quirk: a length of 0 sends 1 byte, length of 255 sends 256, etc.
}

void dma_enable_channel(byte channel)
{
//  channel &= 0x07;

  if(channel < 4)
    outportb(DMA_8BIT_MASK, channel);
  else
    outportb(DMA_16BIT_MASK, channel);
}

void dma_disable_channel(byte channel)
{
//  channel &= 0x07;

  if(channel < 4)
    outportb(DMA_8BIT_MASK, channel | 0x04);
  else
    outportb(DMA_16BIT_MASK, channel | 0x04);
}

void dma_clear_ff(byte channel)
{
  if(channel < 4)
    outportb(DMA_8BIT_CLRFF, 0x00);
  else
    outportb(DMA_16BIT_CLRFF, 0x00);
}

void dma_set_count(byte channel, word length)
{
  int cnt_port;

  switch(channel) {
    case 0:
      cnt_port = DMA0_COUNT;
      break;
    case 1:
      cnt_port = DMA1_COUNT;
      break;
    case 2:
      cnt_port = DMA2_COUNT;
      break;
    case 3:
      cnt_port = DMA3_COUNT;
      break;
    case 4:
      cnt_port = DMA4_COUNT;
      break;
    case 5:
      cnt_port = DMA5_COUNT;
      break;
    case 6:
      cnt_port = DMA6_COUNT;
      break;
    case 7:
      cnt_port = DMA7_COUNT;
      break;
    default:
      return;
  }
//  outportb(cnt_port, 0xFF);
//  outportb(cnt_port, 0x01);
  outportb(cnt_port, length & 0xFF);
  outportb(cnt_port, length >> 8);
}

void dma_set_addr(byte channel, word offset)
{
  int addr_port;

  switch(channel) {
    case 0:
      addr_port = DMA0_ADDR;
      break;
    case 1:
      addr_port = DMA1_ADDR;
      break;
    case 2:
      addr_port = DMA2_ADDR;
      break;
    case 3:
      addr_port = DMA3_ADDR;
      break;
    case 4:
      addr_port = DMA4_ADDR;
      break;
    case 5:
      addr_port = DMA5_ADDR;
      break;
    case 6:
      addr_port = DMA6_ADDR;
      break;
    case 7:
      addr_port = DMA7_ADDR;
      break;
    default:
      return;
  }
  outportb(addr_port, offset & 0xFF);
  outportb(addr_port, offset >> 8);
}

void dma_set_page(byte channel, int page)
{
  int page_port;

  switch(channel) {
    case 0:
      page_port = DMA0_PAGE;
      break;
    case 1:
      page_port = DMA1_PAGE;
      break;
    case 2:
      page_port = DMA2_PAGE;
      break;
    case 3:
      page_port = DMA3_PAGE;
      break;
    case 4:
      page_port = DMA4_PAGE;
      break;
    case 5:
      page_port = DMA5_PAGE;
      break;
    case 6:
      page_port = DMA6_PAGE;
      break;
    case 7:
      page_port = DMA7_PAGE;
      break;
    default:
      return;
  }
  outportb(page_port, page);
}

void dma_set_mode(byte channel, byte mode)
{
  if(channel < 4)
    outportb(DMA_8BIT_MODE, mode);
  else
    outportb(DMA_16BIT_MODE, mode);
}

void start_dma(byte channel, addr_t *address, int len, byte mode)
{
  mode |= channel;

  struct DMA_blk blk;

  dma_calc_blk(&blk, (void *)address, len);

  disable_int();
  dma_disable_channel(channel);
  dma_clear_ff(channel);
  dma_set_mode(channel, mode);
  dma_set_addr(channel, blk.offset);
  dma_set_page(channel, blk.page);
  dma_set_count(channel, blk.length);
  dma_enable_channel(channel);
  enable_int();
}
