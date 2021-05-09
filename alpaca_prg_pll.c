#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // usleep 

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

  /* program zcu216 plls */
  int ret;
  // init i2c
  init_i2c_bus();
  // init spi bridge
  init_i2c_dev(I2C_DEV_CLK104);
  // init fabric gpio for SDIO readback (no IO Expander on zcu216/208)
  init_clk104_gpio(320);

  // set sdo mux to lmk
  ret = set_sdo_mux(LMK_MUX_SEL);
  usleep(0.5e6);
  if (ret == RFCLK_FAILURE) {
    printf("gpio sdo mux not set correctly, errorno: %d\n", ret);
    return 0;
  }

  // configure spi device
  uint8_t spi_config[2] = {0xf0, 0x03}; // spi bridge configuration packet
  i2c_write(I2C_DEV_CLK104, spi_config, 2);

  if (pll_type == 0) {
    // configure clk104 lmk (is an optional ref. clk input to tile 226)
    ret = prog_pll(I2C_DEV_CLK104, LMK_SDO_SS, rp, LMK_REG_CNT);
  } else {
    // configure clk104 adc lmx2594 to tile 225
    ret = prog_pll(I2C_DEV_CLK104, LMX_ADC_SDO_SS, rp, LMX2594_REG_CNT);
  }

  if (pll_type == 0) {
    /* readback lmk config info*/
    // TODO move readback to common method
    printf("Reading LMK04828 register config\n");

    /* sdo mux alread set to lmk from above in init sequence, otherwise would set */

    uint8_t R351[4];
    // hardcoded readback value computed from R531 to config spi readback
    format_rfclk_pkt(LMK_SDO_SS, 0x00015f3b, R351);
    if(RFCLK_FAILURE==i2c_write(I2C_DEV_CLK104, R351, 4)) {
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
      lmk_tx_read[1] = (0xff & (rp[i] >> 16)) | REG_RW_BIT;
      lmk_tx_read[2] =  0xff & (rp[i] >> 8);

      lmk_reg_read[0] = 0x0;
      lmk_reg_read[1] = 0x0;
      lmk_reg_read[2] = 0x0;

      if (RFCLK_FAILURE==i2c_write(I2C_DEV_CLK104, lmk_tx_read, 4)) {
        printf("error writing reg to read\n");
        return 0;
      }
      if (RFCLK_FAILURE==i2c_read(I2C_DEV_CLK104, lmk_reg_read, 3)) {
        printf("error reading target reg\n");
        return 0;
      }
      *lmk_cd = (rp[i] & 0xffff00) + lmk_reg_read[2];
    }

    // revert PLL1_LD_MUX/PLL1_LD_TYPE reg back (register 0x15F, R351)
    format_rfclk_pkt(LMK_SDO_SS, 0x00015f3e, R351);
    if(RFCLK_FAILURE==i2c_write(I2C_DEV_CLK104, R351, 4)) {
      printf("error setting R351 for readback on LMK04828\n");
      return 0;
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
  } else {
    printf("WARN: readback of lmx not implemented\n");
  }

  // release memory pll tcs config
  free(rp);

  // close i2c devices
  close_i2c_dev(I2C_DEV_CLK104);
  close_i2c_bus();

  return 0;

}



