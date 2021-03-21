#ifndef ALPACA_I2C_UTILS_H_
#define ALPACA_I2C_UTILS_H_

typedef struct i2c_slave {
  const char* dev_path;
  uint8_t mux_addr;
  uint8_t mux_sel;
  uint8_t slave_addr;
  int fd;
} I2CSlave;

typedef enum  dev {
  I2C_DEV_EEPROM,
  I2C_DEV_8A34001,
  I2C_DEV_SFP0,
  I2C_DEV_SFP0_MOD
} I2CDev;
  

#endif /* ALPACA_I2C_UTILS_H_ */
