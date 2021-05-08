#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t

#include "alpaca_i2c_utils.h"
#include "alpaca_rfclks.h"
#include "HTG_LMK_LMX_config.h"

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
  // iox configuration packet, power-on defaults are all `1` setting the iox
  // configured as inputs. Here, set IOX_MUX_SEL bits to 0 configuring as outputs
  // just using the lmx tile 224/225 macro since I know it is zero
  uint8_t iox_config[2] = {IOX_CONF_REG, (0xff & LMX_ADC_MUX_SEL_224_225)}; // iox configuration packet
  i2c_write(I2C_DEV_IOX, iox_config, 2);

  // write to the GPIO reg to lower the outputs since power-on default is high
  // (use the same data from iox_config[1] packet to do this since already zeros)
  uint8_t iox_gpio[2] = {IOX_GPIO_REG, iox_config[1]};
  i2c_write(I2C_DEV_IOX, iox_gpio, 2);

  // configure lmk
  uint8_t rfclk_pkt_buffer[4];
  for (int i=0; i<LMK_REG_CNT; i++) {
    printf("writing %x to the LMK...\n", LMK_ARRAY[i]);
    format_rfclk_pkt(LMK_SDO_SS, LMK_ARRAY[i], rfclk_pkt_buffer);
    i2c_write(I2C_DEV_LMK_SPI_BRIDGE, rfclk_pkt_buffer, 4);
  }

  // configure lmx for rf tile 224/225
  for (int i=0; i<LMX2594_REG_CNT; i++) {
    //printf("Writing %x to LMX for tiles 224/225..\n", LMX_ARRAY[i]);
    format_rfclk_pkt(LMX_SDO_SS224_225, LMX_ARRAY[i], rfclk_pkt_buffer);
    i2c_write(I2C_DEV_LMX_SPI_BRIDGE, rfclk_pkt_buffer, 4);
  }

  // configure lmx for rf tile 226/227
  for (int i=0; i<LMX2594_REG_CNT; i++) {
    //printf("Writing %x to LMX for tiles 226/227...\n", LMX_ARRAY[i]);
    format_rfclk_pkt(LMX_SDO_SS226_227, LMX_ARRAY[i], rfclk_pkt_buffer);
    i2c_write(I2C_DEV_LMX_SPI_BRIDGE, rfclk_pkt_buffer, 4);
  }

  // read back configuration from lmx
  // in general, the iox would need to be set to the chip we wanted to read, but
  // since we initialized it above to 0b00 we are set up to select lmx tile 224/225.
  // (SDO_SS lines up with mux on htg)

  printf("Reading LMX register config for tile 224/225\n");
  // set MUX_OUT_LD_SEL of lmx register R0 for readback
  uint8_t R0[4];
  // hardcoded R0 from LMX config array determined as (R0 & ~LMX_MUXOUT_LD_SEL)
  format_rfclk_pkt(LMX_SDO_SS224_225, 0x00002418, R0);
  if(0==i2c_write(I2C_DEV_LMX_SPI_BRIDGE, R0, 4)) {
    printf("error setting R0 register for readback\n");
    return 0;
  }

  uint32_t lmx_config_data[256];
  uint32_t* lmx_cd = lmx_config_data;

  uint8_t reg_read[3] = {0x0, 0x0, 0x0};
  uint8_t tx_read[4] = {0x0, 0x0, 0x0, 0x0};
  for (int i=0; i<113; i++, lmx_cd++) { // 113 program registers in LMX
    tx_read[0] = SELECT_SPI_SDO(LMX_SDO_SS224_225);
    tx_read[1] = (i | REG_RW_BIT);

    reg_read[0] = 0x0;
    reg_read[1] = 0x0;
    reg_read[2] = 0x0;

    if (0==i2c_write(I2C_DEV_LMX_SPI_BRIDGE, tx_read, 4)) {
      printf("error writing reg to read\n");
      return 0;
    }
    if (0==i2c_read(I2C_DEV_LMX_SPI_BRIDGE, reg_read, 3)) {
      printf("error reading target reg\n");
      return 0;
    }
    *lmx_cd = ((uint8_t)i << 16) + (reg_read[1] << 8) + reg_read[2];
  }

  // revert the MUX_OUT_LD_SEL bit
  // hardcoded R0 from LMX config array
  format_rfclk_pkt(LMX_SDO_SS224_225, 0x0000241C, R0);
  i2c_write(I2C_DEV_LMX_SPI_BRIDGE, R0, 4);

  // display lmx config info
  printf("LMX for tile 224/225 config data are:\n");
  for (int i=112, j=0; i>=0; i--, j++) {
    if (j%9==8) {
      printf("0x%06x,\n", lmx_config_data[i]);
    } else {
      printf("0x%06x, ", lmx_config_data[i]);
    }
  }

  close_i2c_dev(I2C_DEV_LMK_SPI_BRIDGE);
  close_i2c_dev(I2C_DEV_LMX_SPI_BRIDGE);
  close_i2c_dev(I2C_DEV_IOX);
  close_i2c_bus();

  printf("\n\nRFPLL clock chips programmed!\n");

  return 0;
}
