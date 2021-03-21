APP = i2c-utils
APPSOURCES= alpaca_i2c_utils.c
OUTS = /tftpboot/nfs/rootfs/home/casper/alpaca_i2c_utils
SRCS = alpaca_i2c_utils.c idt8a34001_regs.c
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
