#ifndef ALPACA_I2C_UTILS_H_
#define ALPACA_I2C_UTILS_H_

#define DEVICE_STRUCT(dp, ma, ms, sa, fd, pfd) {dp, ma, ms, sa, fd, pfd}
typedef struct i2c_slave {
  const char* dev_path;      // linux device file path
  uint8_t mux_addr;          // i2c address of the mux
  uint8_t mux_sel;           // mux configuration packet to enable mux channel to the device
  uint8_t slave_addr;        // i2c address of the slave device
  int fd;                    // file descriptor of the child device
  int* parent_fd;            // parent i2c bus that the mux-ed slave lives on, fd_i2c0 or fd_i2c1
} I2CSlave;

#define PLATFORM ZCU216

#define ZCU216  0
#define HTG49DR 1

#if PLATFORM == ZCU216
  #define PLATFORM_I2C_DEVICES \
    X(I2C_DEV_EEPROM,   DEVICE_STRUCT("/dev/i2c-6" , 0x74, (1 << 0), 0x54, -1, &fd_i2c1)) /* Device EEPROM */ \
    X(I2C_DEV_S15341,   DEVICE_STRUCT("/dev/i2c-7" , 0x74, (1 << 1), 0x76, -1, &fd_i2c1)) /* si5341 clock */ \
    X(I2C_DEV_S1570,    DEVICE_STRUCT("/dev/i2c-8" , 0x74, (1 << 2), 0x5d, -1, &fd_i2c1)) /* user si570 clock */ \
    X(I2C_DEV_MGT_S1570,DEVICE_STRUCT("/dev/i2c-9" , 0x74, (1 << 3), 0x5d, -1, &fd_i2c1)) /* user MGT si570 clock */ \
    X(I2C_DEV_CLK104,   DEVICE_STRUCT("/dev/i2c-11", 0x74, (1 << 5), 0x2f, -1, &fd_i2c1)) /* CLK104 */ \
    X(I2C_DEV_8A34001,  DEVICE_STRUCT("/dev/i2c-10", 0x74, (1 << 4), 0x5b, -1, &fd_i2c1)) /* IDT 8A34001 Transceiver clock chip */ \
    \
    X(I2C_DEV_SFP0,     DEVICE_STRUCT("/dev/i2c-21", 0x75, (1 << 7), 0x50, -1, &fd_i2c1)) /* SFP0 Socket, A0h SFF-8472 memory space */ \
    X(I2C_DEV_SFP0_MOD, DEVICE_STRUCT("/dev/i2c-21", 0x75, (1 << 7), 0x51, -1, &fd_i2c1)) /* SFP0 Module, A2h SFF-8472 memory space */ \
    X(I2C_DEV_SFP1    , DEVICE_STRUCT("/dev/i2c-20", 0x75, (1 << 6), 0x50, -1, &fd_i2c1)) /* SFP1 Module, A0h SFF-8472 memory space */ \
    X(I2C_DEV_SFP1_MOD, DEVICE_STRUCT("/dev/i2c-20", 0x75, (1 << 6), 0x51, -1, &fd_i2c1)) /* SFP1 Module, A2h SFF-8472 memory space */ \
    X(I2C_DEV_SFP2    , DEVICE_STRUCT("/dev/i2c-19", 0x75, (1 << 5), 0x50, -1, &fd_i2c1)) /* SFP2 Module, A0h SFF-8472 memory space */ \
    X(I2C_DEV_SFP2_MOD, DEVICE_STRUCT("/dev/i2c-19", 0x75, (1 << 5), 0x51, -1, &fd_i2c1)) /* SFP2 Module, A2h SFF-8472 memory space */ \
    X(I2C_DEV_SFP3    , DEVICE_STRUCT("/dev/i2c-18", 0x75, (1 << 4), 0x50, -1, &fd_i2c1)) /* SFP3 Module, A0h SFF-8472 memory space */ \
    X(I2C_DEV_SFP3_MOD, DEVICE_STRUCT("/dev/i2c-18", 0x75, (1 << 4), 0x51, -1, &fd_i2c1)) /* SFP3 Module, A2h SFF-8472 memory space */ \

#elif PLATFORM == HTG49DR
  #define PLATFORM_I2C_DEVICES \
    X(I2C_DEV_LMK_SPI_BRIDGE, DEVICE_STRUCT("/dev/i2c-2", 0x71, (1 << 0), 0x2e, -1, &fd_i2c0)) /* SC18IS602 i2c to spi bridge for LMK04832 */ \
    X(I2C_DEV_LMX_SPI_BRIDGE, DEVICE_STRUCT("/dev/i2c-6", 0x71, (1 << 4), 0x2a, -1, &fd_i2c0)) /* SC18IS602 i2c to spi bridge for ADC/DAC LMX2594 */ \
    X(I2C_DEV_IOX,            DEVICE_STRUCT("/dev/i2c-5", 0x71, (1 << 3), 0x20, -1, &fd_i2c0)) /* TCA6408 io expander for mux SPI SDO readback of LMK */ \
    X(I2C_DEV_QSFP28_A,       DEVICE_STRUCT("/dev/i2c-3", 0x71, (1 << 1), 0x50, -1, &fd_i2c0)) /* QSFP28 A, A0h SFF-8472 memory space */ \
    X(I2C_DEV_QSFP28_A_MOD,   DEVICE_STRUCT("/dev/i2c-3", 0x71, (1 << 1), 0x51, -1, &fd_i2c0)) /* QSFP28 A, A2h SFF-8472 memory space */ \
    X(I2C_DEV_QSFP28_B,       DEVICE_STRUCT("/dev/i2c-7", 0x71, (1 << 5), 0x50, -1, &fd_i2c0)) /* QSFP28 B, A0h SFF-8472 memory space */ \
    X(I2C_DEV_QSFP28_B_MOD,   DEVICE_STRUCT("/dev/i2c-7", 0x71, (1 << 5), 0x51, -1, &fd_i2c0)) /* QSFP28 B, A2h SFF-8472 memory space */ \
    X(I2C_DEV_SI571,          DEVICE_STRUCT("/dev/i2c-8", 0x71, (1 << 6), 0xFF, -1, &fd_i2c0)) /* TODO: get device description and slave address */ \
    X(I2C_DEV_DDR4_SODIMM,    DEVICE_STRUCT("/dev/i2c-9", 0x71, (1 << 7), 0xFF, -1, &fd_i2c0)) /* TODO: get device description and slave address */
#endif

#define I2C_DEVICES_MAP PLATFORM_I2C_DEVICES

#define X(name, dev) name,
typedef enum dev { I2C_DEVICES_MAP } I2CDev;
#undef X

int init_i2c_bus();
int close_i2c_bus();
int init_i2c_dev(I2CDev dev);
int close_i2c_dev(I2CDev dev);
int i2c_write(I2CDev dev, uint8_t *buf, uint16_t len);
int i2c_read(I2CDev dev, uint8_t *buf, uint16_t len);
int i2c_read_regs(I2CDev dev, uint8_t *offset, uint16_t olen, uint8_t *buf, uint16_t len);
#endif /* ALPACA_I2C_UTILS_H_ */
