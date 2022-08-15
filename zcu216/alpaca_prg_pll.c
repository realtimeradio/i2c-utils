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

  /* program zcu216 plls */
  int ret;
  // init i2c
  init_i2c_bus();
  // init spi bridge
  init_i2c_dev(I2C_DEV_CLK104);
  // init fabric gpio for SDIO readback (no IO Expander on zcu216/208)
  init_clk104_gpio(510);

  // default init sdo mux to lmk
  ret = set_sdo_mux(LMK_MUX_SEL);
  usleep(0.5e6);
  if (ret == RFCLK_FAILURE) {
    printf("gpio sdo mux not set correctly, errorno: %d\n", ret);
    return 0;
  }

  // init spi device configuration
  uint8_t spi_config[2] = {0xf0, 0x03};
  i2c_write(I2C_DEV_CLK104, spi_config, 2);

  /* program */
  if (pll_type == 0) {
    // configure clk104 lmk (is an optional ref. clk input to tile 226)
    ret = prog_pll(I2C_DEV_CLK104, LMK_SDO_SS, rp, prg_cnt, pkt_len);
  } else {
    // configure clk104 adc lmx2594 to tile 225
    ret = prog_pll(I2C_DEV_CLK104, LMX_SDO_SS224_225, rp, prg_cnt, pkt_len);
  }

  /* readback */
  get_pll_config(pll_type, rp);

  // release memory pll tcs config
  free(rp);

  // close i2c devices
  close_i2c_dev(I2C_DEV_CLK104);
  close_i2c_bus();

  return 0;

}



