#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t


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

  /* program zrf16 plls */
  int ret;
  // init i2c
  init_i2c_bus();
  // init spi bridges
  init_i2c_dev(I2C_DEV_PLL_SPI_BRIDGE);
  // init io expander
  init_i2c_dev(I2C_DEV_IOX);

  // init io expander configuration
  // iox configuration packet, power-on defaults are all `1` setting the iox
  // configured as inputs. Need to set bits to `0` to configure as output
  uint8_t iox_config[2] = {IOX_CONF_REG, (0xff & ~MUX_SEL_BASE)};
  i2c_write(I2C_DEV_IOX, iox_config, 2);

  // write to the GPIO reg to lower the outputs since power-on default is high
  // (use the same data from iox_config[1] packet to do this since already zeros)
  // this will default select the rfpll at mux_sel 0 (e.g., tile 224/225 LMX)
  uint8_t iox_gpio[2] = {IOX_GPIO_REG, iox_config[1]};
  i2c_write(I2C_DEV_IOX, iox_gpio, 2);

  // init spi device configuration
  uint8_t spi_config[2] = {0xf0, 0x03};
  i2c_write(I2C_DEV_PLL_SPI_BRIDGE, spi_config, 2);

  /* program */
  if (pll_type == 0) {
    // configure lmk
    ret = prog_pll(I2C_DEV_PLL_SPI_BRIDGE, LMK_SDO_SS, rp, prg_cnt, pkt_len);
  } else {
    // rfsoc2x2 only supports two inputs with one lmx2594 driving adc tiles 224/226
    // despite the SDO slave select macro, this configures the one lmx2594's for both tiles
    ret = prog_pll(I2C_DEV_PLL_SPI_BRIDGE, LMX_SDO_SS224_225, rp, prg_cnt, pkt_len);
  }

  /* readback */
  get_pll_config(pll_type, rp);

  // release memory from tcs pll config
  free(rp);

  // close i2c devices
  close_i2c_dev(I2C_DEV_PLL_SPI_BRIDGE);
  close_i2c_dev(I2C_DEV_IOX);
  close_i2c_bus();

  return 0;
}



