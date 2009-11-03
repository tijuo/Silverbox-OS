#ifndef DMA
#define DMA

struct DMA_blk {
  word page;
  word offset;
  word length;
};

#define DMA0_ADDR		        0x00
#define DMA0_COUNT		        0x01
#define DMA1_ADDR              	0x02
#define DMA1_COUNT             	0x03
#define DMA2_ADDR               0x04
#define DMA2_COUNT              0x05
#define DMA3_ADDR               0x06
#define DMA3_COUNT             	0x07
#define DMA4_ADDR               0xC0
#define DMA4_COUNT              0xC2
#define DMA5_ADDR               0xC4
#define DMA5_COUNT              0xC6
#define DMA6_ADDR               0xC8
#define DMA6_COUNT              0xCA
#define DMA7_ADDR               0xCC
#define DMA7_COUNT              0xCE

#define DMA0_PAGE		        0x87
#define DMA1_PAGE		        0x83
#define DMA2_PAGE               0x81
#define DMA3_PAGE               0x82
#define DMA4_PAGE               0x8F 	/* Unusable */
#define DMA5_PAGE               0x8B
#define DMA6_PAGE               0x89
#define DMA7_PAGE               0x8A

#define DMA_8BIT_MODE		0x0B
#define DMA_16BIT_MODE		0xD6

#define DMA_8BIT_MASK		0x0A
#define DMA_16BIT_MASK		0xD4

#define DMA_8BIT_CLRFF		0x0C
#define DMA_16BIT_CLRFF		0xD8

#define DMA_DEMAND_MODE		0x00
#define DMA_SINGLE_MODE		0x40
#define DMA_BLOCK_MODE		0x80
#define DMA_CASCADE_MODE	0xC0

#define DMA_ADDR_INCR		0x00
#define DMA_ADDR_DECR		0x20
#define DMA_SINGLE_CYCLE	0x00
#define DMA_AUTOINIT		0x10
#define DMA_VERIFY_TRANS	0x00
#define DMA_WRITE_TRANS		0x04
#define DMA_READ_TRANS		0x08

#define DMA0_MODE_SELECT	0x00
#define DMA1_MODE_SELECT	0x01
#define DMA2_MODE_SELECT	0x02
#define DMA3_MODE_SELECT	0x03
#define DMA4_MODE_SELECT	0x00
#define DMA5_MODE_SELECT	0x01
#define DMA6_MODE_SELECT	0x02
#define DMA7_MODE_SELECT	0x03

#define DMA0_MASK_SELECT        0x00
#define DMA1_MASK_SELECT        0x01
#define DMA2_MASK_SELECT        0x02
#define DMA3_MASK_SELECT        0x03
#define DMA4_MASK_SELECT        0x00
#define DMA5_MASK_SELECT        0x01
#define DMA6_MASK_SELECT        0x02
#define DMA7_MASK_SELECT        0x03

#define DMA_ENABLE_CHANNEL	0x00
#define DMA_DISABLE_CHANNEL	0x04

#define TYPE_16BIT		0x00
#define TYPE_8BIT		0x0F

void dma_calc_blk(struct DMA_blk *blk, void *data, word len);
void dma_enable_channel(byte channel);
void dma_disable_channel(byte channel);
void dma_set_count(byte channel, word length);
void dma_set_addr(byte channel, word offset);
void dma_set_page(byte channel, int page);
void dma_set_mode(byte channel, byte mode);
void dma_start_transfer(byte channel, struct DMA_blk *blk, byte mode);

#endif
