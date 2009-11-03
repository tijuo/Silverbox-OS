signed char mouse_buffer[3];

void enable_mouse(void) {
  unsigned char command;

  wait_kb_input();
  outportb(0x64, 0xA8);
  wait_kb_input();
  outportb(0x64, 0x20);

  command = inportb(0x60) | 3;
  wait_kb_input();
  outportb(0x64, 0x60);
  wait_kb_input();
  outportb(0x60, command);

  send_mouse(0xE8);
  send_mouse(0x03);
  send_mouse(0xE7);
  send_mouse(0xF4);
}

void mouse_handler(task_t *current)
{
  print("Mouse Interrupt.\n");

  mouse_buffer[0] = inportb(0x60);
  mouse_buffer[1] = inportb(0x60);
  mouse_buffer[2] = inportb(0x60);

  endIRQ( IRQ12 );
}

int mouse_read(struct file *file, void *buffer, unsigned count)
{
  if(buffer == NULL)
    return -1;
  else if(count < 3)
    return -1;

  memcpy(buffer, mouse_buffer, sizeof(signed char) * 3);
  return 3;
}

void mod_main(void)
{
  registerInt(IRQ12);
  enable_mouse();
}
