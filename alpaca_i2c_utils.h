#ifndef ALPACA_I2C_UTILS_H_
#define ALPACA_I2C_UTILS_H_

#define DEVICE_STRUCT(dp, ma, ms, sa, fd) {dp, ma, ms, sa, fd}
typedef struct i2c_slave {
  const char* dev_path;
  uint8_t mux_addr;
  uint8_t mux_sel;
  uint8_t slave_addr;
  int fd;
} I2CSlave;

#define PLATFORM ZCU216

#define ZCU216  0
#define HTG49DR 1

#if PLATFORM == ZCU216
  #define PLATFORM_I2C_DEVICES \
    X(I2C_DEV_EEPROM,  DEVICE_STRUCT("/dev/i2c-6" , 0x74, (1 << 0), 0x54, -1 )) /* Device EEPROM */ \
    X(I2C_DEV_8A34001, DEVICE_STRUCT("/dev/i2c-10", 0x74, (1 << 4), 0x5b, -1 )) /* IDT 8A34001 Transceiver clock chip */ \
    X(I2C_DEV_SFP0,    DEVICE_STRUCT("/dev/i2c-20", 0x75, (1 << 7), 0x50, -1 )) /* SFP0 Socket, A0h SFF-8472 memory space */ \
    X(I2C_DEV_SFP0_MOD,DEVICE_STRUCT("/dev/i2c-20", 0x75, (1 << 7), 0x51, -1 )) /* SFP0 Module, A2h SFF-8472 memory space */ \

#elif PLATFORM == HTG49DR
  #define PLATFORM_I2C_DEVICES \
    X(I2C_DEV_LMK_SPI_BRIDGE, DEVICE_STRUCT("/dev/i2c-2", 0x71, (1 << 0), 0x2e, -1)) \
    X(I2C_DEV_LMX_SPI_BRIDGE, DEVICE_STRUCT("/dev/i2c-6", 0x71, (1 << 4), 0x2a, -1)) \
    X(I2C_DEV_IOX,            DEVICE_STRUCT("/dev/i2c-5", 0x71, (1 << 3), 0x20, -1)) \

#endif

#define I2C_DEVICES_MAP PLATFORM_I2C_DEVICES

#define X(name, dev) name,
typedef enum dev { I2C_DEVICES_MAP } I2CDev;
#undef X

int init_i2c_bus();
int init_i2c_dev(I2CDev dev);
int close_i2c_dev(I2CDev dev);
int i2c_write(I2CDev dev, uint8_t *buf, uint16_t len);
int i2c_read(I2CDev dev, uint8_t *buf, uint16_t len);
int i2c_read_regs(I2CDev dev, uint8_t *offset, uint16_t olen, uint8_t *buf, uint16_t len);
#endif /* ALPACA_I2C_UTILS_H_ */
