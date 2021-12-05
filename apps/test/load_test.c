int main(void) {
  asm("xchg %bx, %bx\n");

  while(1);

  return -1;
}
