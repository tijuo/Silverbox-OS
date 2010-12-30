.c.o:
	$(CC) $(OPT_FLG) -c $(CFLAGS) -I$(INC_DIR) $< -o $@

.s.o:
	$(AS) $(AFLAGS) $< -o $@

$(PREFIX)/lib/libc.a:
	make -C $(PREFIX)/lib/ libc.a

$(PREFIX)/lib/libos.a:
	make -C $(PREFIX)/lib/ libos.a

dep:
	make depend
