#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // write

#include <errno.h>
#include <assert.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "alpaca_i2c_utils.h"
#include "alpaca_rfclks.h"

/*
 * Format i2c packet to write to rfpll
 *
 * sdoselect:
 *   slave select value for target pll on spi bridge.
 * d:
 *   register data value to write. LMK04208 registers are programmed using 32-bit
 *   values. having 8 address bits and 24 data bits. LMK04828B, LMK04832, LMX2594
 *   uss 24-bit values.
 * buffer:
 *   holds the single transaction i2c byte formatted data. The i2c packet is the
 *   sdo byte followed by the byte seperated data to forward to the PLL e.g.,
 *   LMK04208 = {sdo byte, 4 data bytes}, LMX2594={sdo byte, 3 data bytes}.
 * len:
 *   length of i2c tranaction buffer (including sdo select byte).
 */
void format_rfclk_pkt(uint8_t sdoselect, uint32_t d, uint8_t* buffer, uint8_t len) {

  // add spi select byte first
  buffer[0] = SELECT_SPI_SDO(sdoselect);

  // format the rest of the rfpll data packet e.g.,
  // buffer[1] = (d >> 16) & 0xff;
  // buffer[2] = (d >> 8)  & 0xff;
  // buffer[3] =        d  & 0xff;

  for (int i=1; i<len; i++) {
    buffer[i] = (d >> (len-i-1)*8) & 0xff;
  }

  return;
}

/*
 * Parse a TICS Pro clock txt formatted file
 */
uint32_t* readtcs(FILE* tcsfile, uint16_t len, uint8_t pll_type) {
  uint32_t* rp;
  rp = malloc(sizeof(uint32_t)*len);
  if (rp == NULL) {
    return rp;
  }

  char R[128];
  char ln[128];
  int n;
  int i;

  // lmk's=0, lmx2594=anything else
  // TICS raw register hex files for the different lmk parts contain the
  // complete programming sequence for rfsocs. For the lmx we must agument by
  // implementing the programming sequence as described in the data sheet to
  // apply rst, remove rst, program 113 registers, apply R0 again. LMX raw hex
  // text files start with the 113 program registers. Setting to 2 here leaves
  // room to fill in reset assert/de-assert
  i = (pll_type == 0) ? 0 : 2;

  // parsing pattern works for lmk04208, lmk04828b, lmk04832, lmx2594
  while (fgets(ln, sizeof(ln), tcsfile) != NULL) {
    if (i >= len) {
      free(rp);
      return NULL;
    }

    n = sscanf(ln, " %s %*s %x", R, &rp[i]);
    if (n != 3) {
      n = sscanf(ln, " %s %x", R, &rp[i]);
    }
    i++;
  }

  // prepare programming sequence for lmx2594 {apply rst, remove rst, 113 prgm registers, apply R0 again}
  if (pll_type == 1) {
    rp[0] = LMX2594_RST_VAL; // apply reset
    rp[1] = 0x000000;        // remove reset
    rp[len-1] = rp[len-2];   // apply R0 a second time
  }

  return rp;
}

/*
 * Program rfpll from a sequence of register data values
 *
 * dev:
 *   i2c device struct used to communicate with the rfpll (e.g., spi bridge)
 * buf:
 *   buffer containing rfpll register values
 * len:
 *   length of `buffer`
 * pkt_len:
 *   number of bytes per rfpll write transactions, e.g., LMK04208={sdo byte, 4
 *   data bytes} (5) and LMK04828B/04832/LMX2594 are {sdo byte, 3 data bytes} (4)
 */
int prog_pll(I2CDev dev, uint8_t spi_sdosel, uint32_t* buf, uint16_t len, uint8_t pkt_len) {
  int res = RFCLK_SUCCESS;

  uint8_t* rfclk_pkt_buffer;
  rfclk_pkt_buffer = malloc(sizeof(uint8_t)*pkt_len);

  for (int i=0; i<len; i++) {
    format_rfclk_pkt(spi_sdosel, buf[i], rfclk_pkt_buffer, pkt_len);
    res = i2c_write(dev, rfclk_pkt_buffer, pkt_len);
    if (res == RFCLK_FAILURE) {
      printf("i2c failed to program pll\n"); // TODO: move printf()s to stderr;
      return res;
    }
  }

  free(rfclk_pkt_buffer);

  return res;
}

/*
 * Readback lmk config info
 *
 * dev:
 *   i2c device struct used to communicate with the rfpll (e.g., spi bridge)
 * regbuf:
 *   buffer of current register configuration (typically just read and stored
 *   from a tcs file)
 *
 */
int get_lmk04828_config(I2CDev dev, uint32_t* regbuf) {

  // at this point we assume sdo mux has already been set to correctly read back

  printf("Reading LMK04828 register config\n");
  uint8_t R351[4];
  // hardcoded readback value computed from R531 to config spi readback
  format_rfclk_pkt(LMK_SDO_SS, 0x00015f3b, R351, 4); // magic 4, lmk04208 i2c packets are 4 bytes
  if(RFCLK_FAILURE==i2c_write(dev, R351, 4)) {
    printf("error setting R351 for readback on LMK04828\n");
    return RFCLK_FAILURE;
  }

  uint32_t lmk_config_data[256]; // arbitrary size of 256
  uint32_t* lmk_cd = lmk_config_data;

  uint8_t lmk_reg_read[3] = {0x0, 0x0, 0x0};
  uint8_t lmk_tx_read[4] = {0x0, 0x0, 0x0, 0x0};
  for (int i=0; i<LMK_REG_CNT; i++, lmk_cd++) {
    // LMK address to read do not simply increment as with the LMX so we pull
    // out of valid addresses from the LMK and use those, end up double counting
    // registers that are part of the reset sequence

    lmk_tx_read[0] = SELECT_SPI_SDO(LMK_SDO_SS);
    lmk_tx_read[1] = (0xff & (regbuf[i] >> 16)) | REG_RW_BIT;
    lmk_tx_read[2] =  0xff & (regbuf[i] >> 8);

    lmk_reg_read[0] = 0x0;
    lmk_reg_read[1] = 0x0;
    lmk_reg_read[2] = 0x0;

    if (RFCLK_FAILURE==i2c_write(dev, lmk_tx_read, 4)) {
      printf("error writing reg to read\n");
      return RFCLK_FAILURE;
    }
    if (RFCLK_FAILURE==i2c_read(dev, lmk_reg_read, 3)) {
      printf("error reading target reg\n");
      return RFCLK_FAILURE;
    }
    *lmk_cd = (regbuf[i] & 0xffff00) + lmk_reg_read[2];
  }

  // revert PLL1_LD_MUX/PLL1_LD_TYPE reg back (register 0x15F, R351)
  format_rfclk_pkt(LMK_SDO_SS, 0x00015f3e, R351, 4); // magic 4, lmk04208 i2c packets are 4 bytes
  if(RFCLK_FAILURE==i2c_write(dev, R351, 4)) {
    printf("error setting R351 for readback on LMK04828\n");
    return RFCLK_FAILURE;
  }

  // display lmk config info
  printf("LMK04828 readback config data are:\n");
  for (int i=0; i<LMK_REG_CNT; i++) {
    if (i%9==8) {
      printf("0x%06x,\n", lmk_config_data[i]);
    } else {
      printf("0x%06x, ", lmk_config_data[i]);
    }
  }
  printf("\n");

  return RFCLK_SUCCESS;
}

/* Readback lmx config info
 *
 * dev:
 *   i2c device struct used to communicate with the rfpll (e.g., spi bridge)
 * regbuf:
 *   buffer of current register configuration (typically just read and stored
 *   from a tcs file)
 *
 * NOTE: when called this is hardcoded to only read LMX_224_225
 */
int get_lmx2594_config(I2CDev dev, uint32_t* regbuf) {

  // at this point we assume sdo mux has already been set to correctly read back

  printf("\nReading LMX2594 register config\n");
  // set MUX_OUT_LD_SEL of lmx register R0 for readback
  uint8_t R0[4];
  // hardcoded R0 from LMX config array determined as (R0 & ~LMX_MUXOUT_LD_SEL)
  format_rfclk_pkt(LMX_SDO_SS224_225, 0x00002418, R0, 4);// magic 4, lmx2594 i2c packets are 4 bytes
  if(RFCLK_FAILURE==i2c_write(dev, R0, 4)) {
    printf("error setting R0 register for readback\n");
    return RFCLK_FAILURE;
  }

  uint32_t lmx_config_data[256];
  uint32_t* lmx_cd = lmx_config_data;

  uint8_t reg_read[3] = {0x0, 0x0, 0x0};
  uint8_t tx_read[4] = {0x0, 0x0, 0x0, 0x0};
  for (int i=0; i<113; i++, lmx_cd++) { //113 registers in LMX
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
    if (RFCLK_FAILURE==i2c_write(dev, tx_read, 4)) {
      printf("error writing reg to read\n");
      return RFCLK_FAILURE;
    }
    if (RFCLK_FAILURE==i2c_read(dev, reg_read, 3)) {
      printf("error reading target reg\n");
      return RFCLK_FAILURE;
    }
    *lmx_cd = ((uint8_t)i << 16) + (reg_read[1] << 8) + reg_read[2];
  }

  // revert the MUX_OUT_LD_SEL bit
  format_rfclk_pkt(LMX_SDO_SS224_225, 0x0000241C, R0, 4); // magic 4, lmx2594 i2c packets are 4 bytes
  i2c_write(dev, R0, 4);

  // display lmx config info
  printf("LMX2594 config data are:\n");
  for (int i=112, j=0; i>=0; i--, j++) {
    if (j%9==8) {
      printf("0x%06x,\n", lmx_config_data[i]);
    } else {
      printf("0x%06x, ", lmx_config_data[i]);
    }
  }
  printf("\n");

  return RFCLK_SUCCESS;
}

/*
 * General readback method used to contain platform dependednt readback setups
 *
 * pll_type:
 *   readback from lmk=0, readback from lmx=anything else
 * regbuf:
 *   buffer of current register configuration (typically just read and stored
 *   from a tcs file)
 *
 */
int get_pll_config(uint8_t pll_type, uint32_t* regbuf) {
  int res = RFCLK_SUCCESS;

  if (pll_type == 0) {
    /* readback lmk config info*/

    // set mux for sdo readback
    #if (PLATFORM == ZCU216) | (PLATFORM == ZCU208)
    // use fabric gpio to select chip
    res = set_sdo_mux(LMK_MUX_SEL);
    usleep(0.5e6);
    if (res == RFCLK_FAILURE) {
      printf("gpio sdo mux not set correctly\n");
      return res;
    }

    res = get_lmk04828_config(I2C_DEV_CLK104, regbuf);

    #else
    // use iox, read current iox gpio reg value, mask this with desired mux sel, write
    uint8_t iox_gpio[2] = {IOX_GPIO_REG, 0x0};
    res = i2c_write(I2C_DEV_IOX, &(iox_gpio[0]), 1);
    if (res == RFCLK_FAILURE) {
      return res;
    };

    res = i2c_read(I2C_DEV_IOX, &(iox_gpio[1]), 1);
    if (res == RFCLK_FAILURE) {
      return res;
    }
    printf("current gpio reg configuration 0x%02x\n", iox_gpio[1]);

    iox_gpio[1] = (iox_gpio[1] & MUX_SEL_BASE) | LMK_MUX_SEL;

    res = i2c_write(I2C_DEV_IOX, iox_gpio, 2);
    if (res == RFCLK_FAILURE) {
      return res;
    }

    printf("WARN: readback for lmk not implemented\n");

    #endif

  } else {
    /* lmx readback */

    // set mux for sdo readback
    #if (PLATFORM == ZCU216) | (PLATFORM == ZCU208)
    // use fabric gpio to select chip
    res = set_sdo_mux(LMX_MUX_SEL_224_225);
    usleep(0.5e6);
    if (res == RFCLK_FAILURE) {
      printf("gpio sdo mux not set correctly\n");
      return res;
    }

    #else
    // use iox, read current iox gpio reg value, mask this with desired mux sel, write
    uint8_t iox_gpio[2] = {IOX_GPIO_REG, 0x0};
    res = i2c_write(I2C_DEV_IOX, &(iox_gpio[0]), 1);
    if (res == RFCLK_FAILURE) {
      return res;
    };

    res = i2c_read(I2C_DEV_IOX, &(iox_gpio[1]), 1);
    if (res == RFCLK_FAILURE) {
      return res;
    }
    printf("current gpio reg configuration 0x%02x\n", iox_gpio[1]);

    iox_gpio[1] = (iox_gpio[1] & MUX_SEL_BASE) | LMX_MUX_SEL_224_225;

    res = i2c_write(I2C_DEV_IOX, iox_gpio, 2);
    if (res == RFCLK_FAILURE) {
      return res;
    }
    #endif

    // NOTE: when reading back LMX assumed that all LMX have been written as
    // this only will readback configuration for LMX on tile 224/225
    #if (PLATFORM == ZCU216) | (PLATFROM == ZCU208)
    res = get_lmx2594_config(I2C_DEV_CLK104, regbuf);
    #elif PLATFORM == ZCU111
    res = get_lmx2594_config(I2C_DEV_PLL_SPI_BRIDGE, regbuf);
    #elif PLATFORM == ZRF16
    res = get_lmx2594_config(I2C_DEV_LMX_SPI_BRIDGE, regbuf);
    #else
    printf("platform does not support lmx readback\n");
    #endif
  }

  return res;
}

/*
 * Use the fabric GPIO in zcu216/208 to switch SDO select for readback
 *
 */
#if (PLATFORM == ZCU216) | (PLATFORM == ZCU208)
int set_sdo_mux(int mux_sel) {
  // TODO: move printf()s to stderr
  int fd_value;
  char gpio_path_value[64];

  sprintf(gpio_path_value, "/sys/class/gpio/gpio%s/value", CLK104_GPIO_MUX_SEL0);
  fd_value = open(gpio_path_value, O_RDWR);
  if (fd_value < 0) {
    printf("ERROR: could not open MUX_SEL0 (bit 0)\n");
    return RFCLK_FAILURE;
  }

  if (mux_sel & 0x1) {
    write(fd_value, "1", 2); // toggle hi,  "echo 1 > /sys/class/gpio/gpio510/value"
  } else {
    write(fd_value, "0", 2); // toggle low, "echo 0 > /sys/class/gpio/gpio510/value"
  }
  close(fd_value);

  sprintf(gpio_path_value, "/sys/class/gpio/gpio%s/value", CLK104_GPIO_MUX_SEL1);
  fd_value = open(gpio_path_value, O_RDWR);
  if (fd_value < 0) {
    printf("ERROR: could not open value for MUX_SEL1 (bit 1)\n");
    return RFCLK_FAILURE;
  }

  if (mux_sel & (0x1 << 1)) {
    write(fd_value, "1", 2); //toggle hi
  } else {
    write(fd_value, "0", 2); //toggle low
  }
  close(fd_value);

  return RFCLK_SUCCESS;
}

/*
 * Initialize fabric GPIO used by zcu216/208 to toggle sdo select lines on clk104 board
 *
 */
int init_clk104_gpio(int gpio_id) {
  // Init fabric gpio
  sprintf(CLK104_GPIO_MUX_SEL0, "%d", gpio_id); // reported gpio id by the kernel from boot messages
  sprintf(CLK104_GPIO_MUX_SEL1, "%d", gpio_id+1);

  int fd_export;
  int fd_direction;
  char gpio_path_direction[64];

  fd_export = open("/sys/class/gpio/export", O_WRONLY);
  if (fd_export < 0) {
    printf("ERROR: could not open gpio device to prepare for export\n");
    return RFCLK_FAILURE;
  }

  write(fd_export, CLK104_GPIO_MUX_SEL0, 4); //"echo 310 > /sys/class/export/gpio"
  write(fd_export, CLK104_GPIO_MUX_SEL1, 4);

  // set the direction of the GPIOs to outputs
  sprintf(gpio_path_direction, "/sys/class/gpio/gpio%s/direction", CLK104_GPIO_MUX_SEL0);
  fd_direction = open(gpio_path_direction, O_RDWR);
  if (fd_direction < 0) {
    close(fd_export);
    printf("ERROR: could not open first exported gpio to set output direction\n");
    return RFCLK_FAILURE;
  }

  write(fd_direction, "out", 4); // "echo out > /sys/class/gpio/gpio510/direction"
  close(fd_direction);

  // repeat for second gpio
  sprintf(gpio_path_direction, "/sys/class/gpio/gpio%s/direction", CLK104_GPIO_MUX_SEL1);
  fd_direction = open(gpio_path_direction, O_RDWR);
  if (fd_direction < 0) {
    close(fd_export);
    printf("ERROR: could not open second exported gpio to set output direction\n");
    return RFCLK_FAILURE;
  }

  write(fd_direction, "out", 4);
  close(fd_direction);

  close(fd_export);
  return RFCLK_SUCCESS;
}
#endif
