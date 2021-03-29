#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // usleep

#include "alpaca_i2c_utils.h"
#include "alpaca_rfclks.h"

// currently htg specifc settings, need to incorporate alpaca/zcu216/zcu111
#include "LMK_LMX_config.h"

// htg and zcu111 use io expanders, zcu216/208 also do, but must be a fabric controlled gpio
// making the extendability across platforms tricky
#define IOX_CONF_REG 0x03
#define IOX_GPIO_REG 0x01
#define IOX_MUX_SEL0 1
#define IOX_MUX_SEL1 (1 << 1)

// TODO: also different across platforms
#define SELECT_SPI_SDO(X) (1 << X)
#define LMK_SDO_SS        0 // SELECT_SPI_SDO(0)
#define LMX_SDO_SS224_225 0 // SELECT_SPI_SDO(0)
#define LMK_SDO_SS226_227 1 // SELECT_SPI_SDO(1)

// TODO: LMK/LMX on htg, add part number for reference, different parts between
// all boards, but all LMK/LMX which have almost identical programming schemes.
#define LMK_REG_CNT 125
#define LMX_REG_CNT 116 // (apply rst, remove rst, prgm 113 registers, program R0 a second time)

void format_rfclk_pkt(uint8_t sdoselect, uint32_t d, uint8_t* buffer) {

  buffer[0] = SELECT_SPI_SDO(sdoselect);
  buffer[1] = (d >> 16) & 0xff;
  buffer[2] = (d >> 8)  & 0xff;
  buffer[3] =        d  & 0xff;
}

int main() {
  printf("Starting clock configuration...\n");
  // init i2c
  init_i2c_bus();

  init_i2c_dev(I2C_DEV_LMK_SPI_BRIDGE);
  init_i2c_dev(I2C_DEV_LMX_SPI_BRIDGE);
  init_i2c_dev(I2C_DEV_IOX);

  // configure spi devices
  uint8_t spi_config[2] = {0xf0, 0x03}; // spi bridge configuration packet
  i2c_write(I2C_DEV_LMK_SPI_BRIDGE, spi_config, 2);
  i2c_write(I2C_DEV_LMX_SPI_BRIDGE, spi_config, 2);

  // configure io expander
  // TODO: explain more consice about what is going on here with IOX_MUX_SEL#,
  // to remind myself it is just a verbose way to set both bottom two bits of
  // the iox to set both of those gpio as outputs and leaving the rest as
  // 1 which are actually device power-on defaults (results in 0xfc, 0b1111_1100, see iox datasheet)
  uint8_t iox_config[2] = {IOX_CONF_REG, (0xff & ~(IOX_MUX_SEL0 | IOX_MUX_SEL1))};
  i2c_write(I2C_DEV_IOX, iox_config, 2);

  // write to the GPIO reg to lower the outputs since the default is high (can
  // use the same data from iox_config[1] packet to do this)
  uint8_t iox_gpio[2] = {IOX_GPIO_REG, iox_config[1]};
  i2c_write(I2C_DEV_IOX, iox_config, 2);

  // configure lmk
  uint8_t rfclk_pkt_buffer[4];
  for (int i=0; i<LMK_REG_CNT; i++) {
    format_rfclk_pkt(LMK_SDO_SS, LMK_ARRAY[i], rfclk_pkt_buffer);
    i2c_write(I2C_DEV_LMK_SPI_BRIDGE, rfclk_pkt_buffer, 4);
  }

  close_i2c_dev(I2C_DEV_LMK_SPI_BRIDGE);
  close_i2c_dev(I2C_DEV_LMX_SPI_BRIDGE);
  close_i2c_dev(I2C_DEV_IOX);

  return 0;
}
