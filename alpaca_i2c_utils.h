#ifndef ALPACA_I2C_UTILS_H_
#define ALPACA_I2C_UTILS_H_

typedef enum  dev {
  I2C_DEV_EEPROM,
  I2C_DEV_8A34001,
  I2C_DEV_SFP0,
  I2C_DEV_SFP0_MOD
} I2CDev;

typedef struct i2c_slave {
  const char* dev_path;
  uint8_t mux_addr;
  uint8_t mux_sel;
  uint8_t slave_addr;
  int fd;
} I2CSlave;

int init_i2c_bus();
int init_i2c_dev(I2CDev dev);
int i2c_write(I2CDev dev, uint8_t *buf, uint16_t len);
int i2c_read(I2CDev dev, uint8_t *buf, uint16_t len);
int i2c_read_regs(I2CDev dev, uint8_t *offset, uint16_t olen, uint8_t *buf, uint16_t len);
#endif /* ALPACA_I2C_UTILS_H_ */
