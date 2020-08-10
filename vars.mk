.c.o:
	$(CC) $(OPT_FLG) -c $(CFLAGS) -I$(INC_DIR) $< -o $@

.s.o:
	$(AS) $(AFLAGS) $< -o $@

$(SB_PREFIX)/lib/libc.a:
	make -C $(SB_PREFIX)/lib/ libc.a

$(SB_PREFIX)/libos.a:
	make -C $(SB_PREFIX)/lib/ libos.a

dep:
	make depend
