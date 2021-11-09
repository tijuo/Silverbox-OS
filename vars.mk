.c.o:
	$(CC) $(OPT_FLG) -c $(CFLAGS) $< -o $@

.s.o:
	$(AS) $(AFLAGS) $< -o $@

.cc.o:
	$(CPP) $(OPT_FLG) -c $(CPPFLAGS) $< -o $@

.cpp.o:
	$(CPP) $(OPT_FLG) -c $(CFFFLAGS) $< -o $@

$(SB_PREFIX)/lib/libc/libc.a:
	make -C $(SB_PREFIX)/lib/libc libc.a

$(SB_PREFIX)/lib/libos/libos.a:
	make -C $(SB_PREFIX)/lib/libos libos.a

dep:
	make depend
