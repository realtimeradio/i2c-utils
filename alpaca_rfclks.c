#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h>

#include "alpaca_i2c_utils.h"
#include "HTG_LMK_LMX_config.h"

// htg and zcu111 use hardware gpio expanders, zcu216/208 use a fabric controlled gpio
#define IOX_CONF_REG 0x03
#define IOX_GPIO_REG 0x01
#define IOX_MUX_SEL0 1       // port 0 of expander
#define IOX_MUX_SEL1 (1 << 1)// port 1 of expander

#define SELECT_SPI_SDO(X) (1 << X)
#define LMK_SDO_SS        0 /* LMK04832 PLL on its own bridge connected to SS0, nothing else on that bridge is connected */
#define LMX_SDO_SS224_225 0 /* ADC LMX2594 RFPLL for tiles 224/225, SS0 on bridge, I0A on mux */
#define LMX_SDO_SS226_227 1 /* ADC LMX2594 RFPLL for tiles 226/227, SS1 on bridge, I1A on mux */

#define LMK04832_REG_CNT 125
#define LMX2594_REG_CNT 116    // (apply rst, remove rst, program 113 registers, program R0 a second time)

#define REG_RW_BIT 0x80        // the 8th bit of the address section of the LMK/LMX indicates Read/Write to the register
#define LMX_MUXOUT_LD_SEL 0x4  // bit mask in R0 to toggle MUXOUT_LD_SEL for SPI readback on LMK

void format_rfclk_pkt(uint8_t sdoselect, uint32_t d, uint8_t* buffer) {
  // TODO this needs to be moved to a common method as it works between lmk/lmx chips
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
  // iox configuration packet, power-on defaults are all `1` setting the iox
  // configured as inputs. Here, set IOX_MUX_SEL bits to 0 configuring the outputs
  uint8_t iox_config[2] = {IOX_CONF_REG, (0xff & ~(IOX_MUX_SEL0 | IOX_MUX_SEL1))}; // iox configuration packet
  i2c_write(I2C_DEV_IOX, iox_config, 2);

  // write to the GPIO reg to lower the outputs since power-on default is high
  // (use the same data from iox_config[1] packet to do this since already zeros)
  uint8_t iox_gpio[2] = {IOX_GPIO_REG, iox_config[1]};
  i2c_write(I2C_DEV_IOX, iox_gpio, 2);

  // configure lmk
  uint8_t rfclk_pkt_buffer[4];
  for (int i=0; i<LMK04832_REG_CNT; i++) {
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
    // the read is implmented in two stages using these base methods instead of
    // the i2c_read_regs() method `I think` because we are working with a SPI to
    // I2C translation and so we use the bridge to write something out and then
    // read from the buffer of the device. The device doesn't understand a
    // repeated start
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
