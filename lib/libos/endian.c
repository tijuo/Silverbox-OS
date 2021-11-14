uint16_t endianSwap16(uint16_t i) {
  return (i >> 8) | (i << 8);
}

uint32_t endianSwap32(uint32_t i) {
  return (i >> 24) | ((i >> 8) & 0xFF00) | ((i << 8) & 0xFF0000) | (i << 24);
}

uint64_t endianSwap64(uint64_t i) {
  return (i >> 56) | ((i << 40) & 0xFF000000000000)
         | ((i << 24) & 0xFF0000000000) | ((i << 8) & 0xFF00000000)
         | ((i >> 8) & 0xFF000000) | ((i >> 24) & 0xFF0000)
         | ((i >> 40) & 0xFF00) | (i << 56);
}
