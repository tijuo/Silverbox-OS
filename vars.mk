.c.o:
	$(CC) $(OPT_FLG) -c $(CFLAGS) -I$(INC_DIR) $< -o $@

.s.o:
	$(AS) $(AFLAGS) $< -o $@

.cc.o:
	$(CPP) $(OPT_FLG) -c $(CPPFLAGS) -I$(INC_DIR) $< -o $@

$(SB_PREFIX)/lib/libc/libc.a:
	make -C $(SB_PREFIX)/lib/libc libc.a

$(SB_PREFIX)/lib/libos/libos.a:
	make -C $(SB_PREFIX)/lib/libos libos.a

dep:
	make depend
