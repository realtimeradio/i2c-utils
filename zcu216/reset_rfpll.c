#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h>

#include "alpaca_i2c_utils.h"
#include "alpaca_rfclks.h"

void usage(char* name) {
  printf("%s -lmk|-lmx\n", name);
}

int main(int argc, char**argv) {
  int ret;
  int pll_type;
  uint8_t spi_sdosel;
  uint8_t rfclk_pkt_buffer[4];
  uint32_t rst_pkt;
  I2CDev dev;

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

  // init i2c
  init_i2c_bus();
  // init spi bridge
  init_i2c_dev(I2C_DEV_CLK104);
  // init fabric gpio for SDIO readback (no IO Expander on zcu216/208)
  init_clk104_gpio(320);

  // configure spi device
  uint8_t spi_config[2] = {0xf0, 0x03}; // spi bridge configuration packet
  ret = i2c_write(I2C_DEV_CLK104, spi_config, 2);
  if (ret == RFCLK_FAILURE) {
    printf("failed to configure spi bridge\n");
    return ret;
  }

  // reset pll
  dev = I2C_DEV_CLK104;

  if (pll_type == 0) {
    spi_sdosel = LMK_SDO_SS;
    rst_pkt = LMK04828_RST_VAL;
  } else {
    spi_sdosel = LMX_ADC_SDO_SS;
    rst_pkt = LMX2594_RST_VAL;
  }

  format_rfclk_pkt(spi_sdosel, rst_pkt, rfclk_pkt_buffer);
  ret = i2c_write(dev, rfclk_pkt_buffer, 4); // TODO define magic 4? (packet size for lmk/lmx)
  if (ret == RFCLK_FAILURE) {
    printf("i2c failed to program pll\n");
    return ret;
  }

  return 0;
}
