APP = i2c-utils
APPSOURCES= alpaca_i2c_utils.c zcu216_test_i2c.c
OUTS = /tftpboot/nfs/rootfs/home/casper/zcu216_i2c_utils
SRCS = alpaca_i2c_utils.c idt8a34001_regs.c zcu216_test_i2c.c
INCLUDES =
LIBDIR =
BOARD_FLAG =
OBJS =

%.o: %.c
	$(CC) ${LDFLAGS} ${BOARD_FLAG} $(INCLUDES) ${CFLAGS} -c $(APPSOURCES)

all: $(OBJS)
	$(CC) ${LDFLAGS} $(INCLUDES) $(LIBDIR) $(OBJS) $(SRCS) -o $(OUTS)

clean:
	rm -rf $(OUTS) *.o
