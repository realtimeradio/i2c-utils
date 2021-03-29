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

#define I2C_DEVICES_MAP \
  X(I2C_DEV_EEPROM,  DEVICE_STRUCT("/dev/i2c-6" , 0x74, (1 << 0), 0x54, -1 )) \
  X(I2C_DEV_8A34001, DEVICE_STRUCT("/dev/i2c-10", 0x74, (1 << 4), 0x5b, -1 ))

#define X(name, dev) name,
typedef enum dev { I2C_DEVICES_MAP } I2CDev;
#undef X

int init_i2c_bus();
int init_i2c_dev(I2CDev dev);
int i2c_write(I2CDev dev, uint8_t *buf, uint16_t len);
int i2c_read(I2CDev dev, uint8_t *buf, uint16_t len);
int i2c_read_regs(I2CDev dev, uint8_t *offset, uint16_t olen, uint8_t *buf, uint16_t len);
#endif /* ALPACA_I2C_UTILS_H_ */
