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

int main(int argc, char**argv) {

  FILE* fileptr;
  char* tcsfile;
  struct stat st;

  if (argc > 1) {
    tcsfile = argv[1];
    if (stat(tcsfile, &st) != 0) {
      printf("file %s does not exist\n", tcsfile);
      return 0;
    }
  } else {
    printf("must pass in full file path\n");
    return 0;
  }

  fileptr = fopen(tcsfile, "r");
  if (fileptr == NULL) {
    printf("problem opening %s\n", tcsfile);
    return 0;
  }

  uint32_t* rp;
  rp = malloc(sizeof(uint32_t)*LMK_REG_CNT);
  if (rp == NULL) {
    printf("could not create array\n");
    return 0;
  }
  printf("%p\n", rp);

  char R[128];
  char ln[128];
  int n;

  int i=0;
  printf("starting read loop\n");
  while (fgets(ln, sizeof(ln), fileptr) != NULL) {
    n = sscanf(ln, " %s %*s %x", R, &rp[i]);
    if (n != 3) {
      n = sscanf(ln, " %s %x", R, &rp[i]);
    }
    i++;
  }

  for (i=0; i<LMK_REG_CNT; i++) {
    printf("0x%06x\n", rp[i]);
  }

  /* PROGRAM PLL */
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
  if (ret == 0) {
    printf("gpio sdo mux not set correctly\n");
    return 0;
  }

  // configure spi device
  uint8_t spi_config[2] = {0xf0, 0x03}; // spi bridge configuration packet
  i2c_write(I2C_DEV_CLK104, spi_config, 2);

  // configure lmk (valid optional clock input to tile 226)
  uint8_t rfclk_pkt_buffer[4];
  for (int i=0; i<LMK_REG_CNT; i++) {
    //printf("writing %x to the LMK...\n", LMK_ARRAY[i]);
    format_rfclk_pkt(LMK_SDO_SS, rp[i], rfclk_pkt_buffer);
    i2c_write(I2C_DEV_CLK104, rfclk_pkt_buffer, 4);
  }

  /* READBACK PLL */
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
    lmk_tx_read[1] = (0xff & (rp[i] >> 16)) | REG_RW_BIT;
    lmk_tx_read[2] =  0xff & (rp[i] >> 8);

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
    *lmk_cd = (rp[i] & 0xffff00) + lmk_reg_read[2];
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

  // release memory
  free(rp);

  // close i2c devices
  close_i2c_dev(I2C_DEV_CLK104);
  close_i2c_bus();

  return 0;

}



