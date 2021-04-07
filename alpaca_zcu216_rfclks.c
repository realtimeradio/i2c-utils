#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // write

#include <errno.h>
#include <assert.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "alpaca_i2c_utils.h"
#include "ZCU216_LMK_LMX_config.h"

#define SELECT_SPI_SDO(X) (1 << X)
#define LMK_SDO_SS     1 /* LMK PLL,   SS1 on bridge, I2A on mux */
#define LMX_ADC_SDO_SS 3 /* ADC RFPLL, SS3 on bridge, I0A on mux */
#define LMX_DAC_SDO_SS 2 /* DAC RFPLL, SS2 on bridge, I1A on mux */

// For the SDIO mux the it is a two bit mux selecting the SDO line to go back
// through the SPI bridge so setting S0,S1 to 0b11 (decimal 3) results in
// selecting the LMK
#define LMK_MUX_SEL 2          /* LMK04828 */
#define LMX_ADC_MUX_SEL 0      /* ADC LMX2594 PLL */
#define LMX_DAC_MUX_SEL 1      /* DAC LMX2594 PLL */
char CLK104_GPIO_MUX_SEL0[4];
char CLK104_GPIO_MUX_SEL1[4];
// zcu216 does not have a connection on SS0 and I3A on mux

#define LMK04828_REG_CNT 128
#define LMX2594_REG_CNT 116       /* (apply rst, remove rst, program 113 registers, program R0 a second time) */

#define REG_RW_BIT 0x80           /* the 8th bit of the address section of the LMK/LMX indicates Read/Write to the register */
#define LMK_MUXOUT_REG_ADDR 0X15F /* LMK MUXOUT reg. address */
#define LMK_MUXOUT_REG_VAL 0X3B   /* LMK MUXOUT reg. value */

#define SUCCESS 1
#define FAILURE 0

void format_rfclk_pkt(uint8_t sdoselect, uint32_t d, uint8_t* buffer) {
  // TODO this needs to be moved to a common method if it works between lmk/lmx chips
  buffer[0] = SELECT_SPI_SDO(sdoselect);
  buffer[1] = (d >> 16) & 0xff;
  buffer[2] = (d >> 8)  & 0xff;
  buffer[3] =        d  & 0xff;
}

int set_sdo_mux(int mux_sel) {
  // TODO: axi gpio driver support reading if wanted to read before doing anything
  int fd_value;
  char gpio_path_value[64];

  sprintf(gpio_path_value, "/sys/class/gpio/gpio%s/value", CLK104_GPIO_MUX_SEL0);
  fd_value = open(gpio_path_value, O_RDWR);
  if (fd_value < 0) {
    printf("ERROR: could not open MUX_SEL0 (bit 0)\n");
  }

  if (mux_sel & 0x1) {
    printf("bit 0=1\n");
    write(fd_value, "1", 2); // toggle hi,  "echo 1 > /sys/class/gpio/gpio510/value"
  } else {
    printf("bit 0=0\n");
    write(fd_value, "0", 2); // toggle low, "echo 0 > /sys/class/gpio/gpio510/value"
  }
  close(fd_value);

  sprintf(gpio_path_value, "/sys/class/gpio/gpio%s/value", CLK104_GPIO_MUX_SEL1);
  fd_value = open(gpio_path_value, O_RDWR);
  if (fd_value < 0) {
    printf("ERROR: could not open value for MUX_SEL1 (bit 1)\n");
  }

  if (mux_sel & (0x1 << 1)) {
    printf("bit 1=1\n");
    write(fd_value, "1", 2); //toggle hi
  } else {
    printf("bit 1=0\n");
    write(fd_value, "0", 2); //toggle low
  }
  close(fd_value);

  return SUCCESS;
}

int init_clk104_gpio(int gpio_id) {
  // Init fabric gpio
  sprintf(CLK104_GPIO_MUX_SEL0, "%d", gpio_id); // reported gpio id by the kernel from boot messages
  sprintf(CLK104_GPIO_MUX_SEL1, "%d", gpio_id+1);

  int fd_export;
  int fd_direction;
  char gpio_path_direction[64];
  char gpio_path_value[64];

  fd_export = open("/sys/class/gpio/export", O_WRONLY);
  if (fd_export < 0) {
    printf("ERROR: could not open gpio device to prepare for export\n");
    return FAILURE;
  }

  write(fd_export, CLK104_GPIO_MUX_SEL0, 4); //"echo 310 > /sys/class/export/gpio"
  write(fd_export, CLK104_GPIO_MUX_SEL1, 4);

  // set the direction of the GPIOs to outputs
  sprintf(gpio_path_direction, "/sys/class/gpio/gpio%s/direction", CLK104_GPIO_MUX_SEL0);
  fd_direction = open(gpio_path_direction, O_RDWR);
  if (fd_direction < 0) {
    close(fd_export);
    printf("ERROR: could not open first exported gpio to set output direction\n");
    return FAILURE;
  }

  write(fd_direction, "out", 4); // "echo out > /sys/class/gpio/gpio510/direction"
  close(fd_direction);

  // repeat for second gpio
  sprintf(gpio_path_direction, "/sys/class/gpio/gpio%s/direction", CLK104_GPIO_MUX_SEL1);
  fd_direction = open(gpio_path_direction, O_RDWR);
  if (fd_direction < 0) {
    close(fd_export);
    printf("ERROR: could not open second exported gpio to set output direction\n");
    return FAILURE;
  }

  write(fd_direction, "out", 4);
  close(fd_direction);

  close(fd_export);
  return SUCCESS;
}

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

  // configure lmk (has optional input to tile 226)
  uint8_t rfclk_pkt_buffer[4];
  for (int i=0; i<LMK04828_REG_CNT; i++) {
    //printf("writing %x to the LMK...\n", LMK_ARRAY[i]);
    format_rfclk_pkt(LMK_SDO_SS, LMK_ARRAY[i], rfclk_pkt_buffer);
    i2c_write(I2C_DEV_CLK104, rfclk_pkt_buffer, 4);
  }

  // configure clk104 adc lmx2594 to tile 225
  for (int i=0; i<LMX2594_REG_CNT; i++) {
    //printf("Writing %x to ADC LMX..\n", LMX_ARRAY[i]);
    format_rfclk_pkt(LMX_ADC_SDO_SS, LMX_ARRAY[i], rfclk_pkt_buffer);
    i2c_write(I2C_DEV_CLK104, rfclk_pkt_buffer, 4);
  }

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
  for (int i=0; i<LMK04828_REG_CNT; i++, lmk_cd++) {
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
  for (int i=0; i<128; i++) {
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
    tx_read[1] = (i | LMX_RW_BIT);

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
