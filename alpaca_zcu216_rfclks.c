#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h>

#include "alpaca_i2c_utils.h"
#include "alpaca_rfclks.h"
#include "ZCU216_LMK_LMX_config.h"

int main() {
  int ret;
  printf("Starting ZCU216 clock configuration...\n");
  // init i2c
  init_i2c_bus();
  // init spi bridge
  init_i2c_dev(I2C_DEV_CLK104);
  // init fabric gpio for SDIO readback (no IO Expander on zcu216/208)
  init_clk104_gpio(320);

  // set sdo mux to lmk
  ret = set_sdo_mux(LMK_MUX_SEL);
  usleep(0.5e6);
  if (ret == 0) {
    printf("gpio sdo mux not set correctly\n");
    return 0;
  }

  // configure spi device
  uint8_t spi_config[2] = {0xf0, 0x03}; // spi bridge configuration packet
  i2c_write(I2C_DEV_CLK104, spi_config, 2);

  // configure lmk (is an optional ref. clk input to tile 226)
  prog_pll(I2C_DEV_CLK104, LMK_SDO_SS, LMK_ARRAY, LMK_REG_CNT);

  // configure clk104 adc lmx2594 to tile 225
  prog_pll(I2C_DEV_CLK104, LMX_ADC_SDO_SS, LMX_ARRAY, LMX2594_REG_CNT);

  printf("Reading LMK04828 register config\n");
  // sdo mux alread set to lmk from above
  uint8_t R351[4];
  // hardcoded readback value computed from R531 to config spi readback
  format_rfclk_pkt(LMK_SDO_SS, 0x00015f3b, R351);
  if(0==i2c_write(I2C_DEV_CLK104, R351, 4)) {
    printf("error setting R351 for readback on LMK04828\n");
    return 0;
  }

  uint32_t lmk_config_data[256];
  uint32_t* lmk_cd = lmk_config_data;

  uint8_t lmk_reg_read[3] = {0x0, 0x0, 0x0};
  uint8_t lmk_tx_read[4] = {0x0, 0x0, 0x0, 0x0};
  for (int i=0; i<LMK_REG_CNT; i++, lmk_cd++) {
    // LMK address to read do not simply increment as with the LMX so we pull
    // out of valid addresses from the LMK and use those, end up double counting
    // registers that are part of the reset sequence

    lmk_tx_read[0] = SELECT_SPI_SDO(LMK_SDO_SS);
    lmk_tx_read[1] = (0xff & (LMK_ARRAY[i] >> 16)) | REG_RW_BIT;
    lmk_tx_read[2] =  0xff & (LMK_ARRAY[i] >> 8);

    lmk_reg_read[0] = 0x0;
    lmk_reg_read[1] = 0x0;
    lmk_reg_read[2] = 0x0;

    if (0==i2c_write(I2C_DEV_CLK104, lmk_tx_read, 4)) {
      printf("error writing reg to read\n");
      return 0;
    }
    if (0==i2c_read(I2C_DEV_CLK104, lmk_reg_read, 3)) {
      printf("error reading target reg\n");
      return 0;
    }
    *lmk_cd = (LMK_ARRAY[i] & 0xffff00) + lmk_reg_read[2];
  }

  // revert PLL1_LD_MUX/PLL1_LD_TYPE reg back (register 0x15F, R351)
  format_rfclk_pkt(LMK_SDO_SS, 0x00015f3e, R351);
  if(0==i2c_write(I2C_DEV_CLK104, R351, 4)) {
    printf("error setting R351 for readback on LMK04828\n");
    return 0;
  } 

  // display lmk config info
  printf("LMK04828 config data are:\n");
  for (int i=0; i<LMK_REG_CNT; i++) {
    if (i%9==8) {
      printf("0x%06x,\n", lmk_config_data[i]);
    } else {
      printf("0x%06x, ", lmk_config_data[i]);
    }
  }
  printf("\n");
 /**********************************************************************/
  printf("\nReading LMX2594 register config\n");
  ret = set_sdo_mux(LMX_ADC_MUX_SEL);
  if (ret == 0) {
    printf("gpio sdo mux not set correctly\n");
    return 0;
  }

  // set MUX_OUT_LD_SEL of lmx register R0 for readback
  uint8_t R0[4];
  format_rfclk_pkt(LMX_ADC_SDO_SS, 0x00002418, R0);
  if(0==i2c_write(I2C_DEV_CLK104, R0, 4)) {
    printf("error setting R0 register for readback\n");
    return 0;
  }

  uint32_t lmx_config_data[256];
  uint32_t* lmx_cd = lmx_config_data;

  uint8_t reg_read[3] = {0x0, 0x0, 0x0};
  uint8_t tx_read[4] = {0x0, 0x0, 0x0, 0x0};
  for (int i=0; i<113; i++, lmx_cd++) { //113 registers in LMX
    tx_read[0] = SELECT_SPI_SDO(LMX_ADC_SDO_SS);
    tx_read[1] = (i | REG_RW_BIT);

    reg_read[0] = 0x0;
    reg_read[1] = 0x0;
    reg_read[2] = 0x0;
    // the read is implmented in two stages using these base methods instead of
    // the i2c_read_regs() method `I think` because we are working with a SPI to
    // I2C translation and so we use the bridge to write something out and then
    // read from the buffer of the device. The device doesn't understand a
    // repeated start
    if (0==i2c_write(I2C_DEV_CLK104, tx_read, 4)) {
      printf("error writing reg to read\n");
      return 0;
    }
    if (0==i2c_read(I2C_DEV_CLK104, reg_read, 3)) {
      printf("error reading target reg\n");
      return 0;
    }
    *lmx_cd = ((uint8_t)i << 16) + (reg_read[1] << 8) + reg_read[2];
  }

  // revert the MUX_OUT_LD_SEL bit
  format_rfclk_pkt(LMX_ADC_SDO_SS, 0x0000241C, R0);
  i2c_write(I2C_DEV_CLK104, R0, 4);

  // display lmx config info
  printf("LMX2594 config data are:\n");
  for (int i=112, j=0; i>=0; i--, j++) {
    if (j%9==8) {
      printf("0x%06x,\n", lmx_config_data[i]);
    } else {
      printf("0x%06x, ", lmx_config_data[i]);
    }
  }
 
  // return sdo mux to lmk
  ret = set_sdo_mux(LMK_MUX_SEL);
  if (ret == 0) {
    printf("gpio sdo mux not set correctly\n");
    return 0;
  }

  close_i2c_dev(I2C_DEV_CLK104);
  close_i2c_bus();

  printf("\n\nRFPLL clock chips programmed! Hopefully...\n");

  return 0;
}
