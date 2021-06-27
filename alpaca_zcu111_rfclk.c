#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h>

#include <errno.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "alpaca_i2c_utils.h"
#include "alpaca_rfclks.h"

void usage(char* name) {
  printf("%s -lmk|-lmx <path/to/clk/file.txt>\n", name);
}

int main(int argc, char**argv) {

  // file data
  FILE* fileptr;
  char* tcsfile;
  struct stat st;
  // pll config data
  uint32_t* rp;
  uint8_t pll_type;

  // parse pll type
  if (argc > 1) {
    if (strcmp(argv[1], "-lmk") == 0) {
      pll_type = 0;
    } else if (strcmp(argv[1], "-lmx") == 0) {
      pll_type = 1;
    } else {
      printf("must specify -lmk|-lmx\n");
      usage(argv[0]);
      return 0;
    }
  } else {
    printf("must specify -lmk|-lmx\n");
    usage(argv[0]);
    return 0;
  }

  // parase and check if file exists
  if (argc > 2) {
    tcsfile = argv[2];
    if (stat(tcsfile, &st) != 0) {
      printf("file %s does not exist\n", tcsfile);
      return 0;
    }
  } else {
    printf("must pass in full file path\n");
    usage(argv[0]);
    return 0;
  }

  /* begin to process clock file */
  fileptr = fopen(tcsfile, "r");
  if (fileptr == NULL) {
    printf("problem opening %s\n", tcsfile);
    return 0;
  }

  int prg_cnt = (pll_type == 0) ? LMK_REG_CNT : LMX2594_REG_CNT;
  int pkt_len = (pll_type == 0) ? LMK_PKT_SIZE: LMX_PKT_SIZE;
  
  rp = readtcs(fileptr, prg_cnt, pll_type);
  if (rp == NULL) {
    printf("problem allocating memory for config buffer, or parsing clock file\n");
    return 0;
  }

  printf("loaded the following config:\n");
  for (int i=0; i<prg_cnt; i++) {
    if (i%9==8) {
      printf("0x%06x,\n", rp[i]);
    } else {
      printf("0x%06x, ", rp[i]);
    }
  }
  printf("\n\n");

  /* program zcu111 plls */
  int ret;
  // init i2c
  init_i2c_bus();
  // init spi bridge
  init_i2c_dev(I2C_DEV_PLL_SPI_BRIDGE);
  // init io expander
  init_i2c_dev(I2C_DEV_IOX);

  // configure io expander
  // iox configuration packet, power-on defaults are all `1` setting the iox
  // configured as inputs. Need to set bits to `0` to configure as output
  uint8_t iox_config[2] = {IOX_CONF_REG, (0xff & ~MUX_SEL_BASE)}; // iox configuration packet
  i2c_write(I2C_DEV_IOX, iox_config, 2);

  // write to the GPIO reg to lower the outputs since power-on default is high
  // (use the same data from iox_config[1] packet to do this since already zeros)
  // this will default select the rfpll at mux_sel 0 (e.g., tile 224/225 LMX)
  uint8_t iox_gpio[2] = {IOX_GPIO_REG, iox_config[1]};
  i2c_write(I2C_DEV_IOX, iox_gpio, 2);

  // configure spi device
  printf("configuring spi\n");
  uint8_t spi_config[2] = {0xf0, 0x03}; // spi bridge configuration packet
  i2c_write(I2C_DEV_PLL_SPI_BRIDGE, spi_config, 2);

  if (pll_type == 0) {
    // configure lmk
    printf("programming lmk\n");
    ret = prog_pll(I2C_DEV_PLL_SPI_BRIDGE, LMK_SDO_SS, rp, prg_cnt, pkt_len);
  } else {
    // TODO this is different than zcu216/208 that only has one adc lmx
    // configure adc lmx2594's to all 4 adc tiles 225
    ret = prog_pll(I2C_DEV_PLL_SPI_BRIDGE, LMX_SDO_SS224_225, rp, prg_cnt, pkt_len);
    ret = prog_pll(I2C_DEV_PLL_SPI_BRIDGE, LMX_SDO_SS226_227, rp, prg_cnt, pkt_len);
  }

  if (pll_type == 0) {
    // TODO: determine if readback on zcu111 lmk04208 is supported
    printf("WARN: readback of lmk not implemented (supported?)\n");
    // if supported take implementation from prog_pll for zcu216
  } else {
    /* readback lmx config info */
    printf("\nReading LMX2594 register config\n");

    /*
     * sdo mux alread set to lmk for adc tile 224/224 from above in iox
     * configuration sequence, otherwise need to set
     */

    // set MUX_OUT_LD_SEL of lmx register R0 for readback
    uint8_t R0[4];
    // hardcoded R0 from LMX config array determined as (R0 & ~LMX_MUXOUT_LD_SEL)
    format_rfclk_pkt(LMX_SDO_SS224_225, 0x00002418, R0, 4); // TODO: explain magic 4
    if(RFCLK_FAILURE==i2c_write(I2C_DEV_PLL_SPI_BRIDGE, R0, 4)) {
      printf("error setting R0 register for readback\n");
      return 0;
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
      if (RFCLK_FAILURE==i2c_write(I2C_DEV_PLL_SPI_BRIDGE, tx_read, 4)) {
        printf("error writing reg to read\n");
        return 0;
      }
      if (RFCLK_FAILURE==i2c_read(I2C_DEV_PLL_SPI_BRIDGE, reg_read, 3)) {
        printf("error reading target reg\n");
        return 0;
      }
      *lmx_cd = ((uint8_t)i << 16) + (reg_read[1] << 8) + reg_read[2];
    }

    // revert the MUX_OUT_LD_SEL bit
    format_rfclk_pkt(LMX_SDO_SS224_225, 0x0000241C, R0, 4); // TODO: explain magic 4
    i2c_write(I2C_DEV_PLL_SPI_BRIDGE, R0, 4);

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

  }

  // release memory pll tcs config
  free(rp);

  // close i2c devices
  close_i2c_dev(I2C_DEV_PLL_SPI_BRIDGE);
  close_i2c_dev(I2C_DEV_IOX);
  close_i2c_bus();

  return 0;
}
