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
  I2CDev i2cdev;

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

  int prg_cnt = (pll_type == 0) ? LMK_REG_CNT : LMX2594_REG_CNT;
  int pkt_len = (pll_type == 0) ? LMK_PKT_SIZE: LMX_PKT_SIZE;

  i2cdev = I2C_DEV_PLL_SPI_BRIDGE;

  // init i2c
  init_i2c_bus();
  // init spi bridge
  init_i2c_dev(i2cdev);

  // configure spi device
  uint8_t spi_config[2] = {0xf0, 0x03}; // spi bridge configuration packet
  ret = i2c_write(i2cdev, spi_config, 2);
  if (ret == RFCLK_FAILURE) {
    printf("failed to configure spi bridge\n");
    return ret;
  }

  // reset pll
  if (pll_type == 0) {
    rst_pkt = LMK04208_RST_VAL;

    spi_sdosel = LMK_SDO_SS;
    format_rfclk_pkt(spi_sdosel, rst_pkt, rfclk_pkt_buffer, pkt_len);
    ret = i2c_write(i2cdev, rfclk_pkt_buffer, pkt_len);
    if (ret == RFCLK_FAILURE) {
      printf("i2c could not reset lmk pll\n");
      return ret;
    }
  } else {
    rst_pkt = LMX2594_RST_VAL;

    //lmx for tiles 224/225
    spi_sdosel = LMX_SDO_SS224_225;
    format_rfclk_pkt(spi_sdosel, rst_pkt, rfclk_pkt_buffer, pkt_len);
    ret = i2c_write(i2cdev, rfclk_pkt_buffer, pkt_len);
    if (ret == RFCLK_FAILURE) {
      printf("i2c could not reset tile 224/225 lmx pll\n");
      return ret;
    }

    //lmx for tiles 226/227
    spi_sdosel = LMX_SDO_SS226_227;
    format_rfclk_pkt(spi_sdosel, rst_pkt, rfclk_pkt_buffer, pkt_len);
    ret = i2c_write(i2cdev, rfclk_pkt_buffer, pkt_len);
    if (ret == RFCLK_FAILURE) {
      printf("i2c could not reset tile 226/227 lmx pll\n");
      return ret;
    }

    //lmx for tiles 228/229
    spi_sdosel = LMX_SDO_SS228_229;
    format_rfclk_pkt(spi_sdosel, rst_pkt, rfclk_pkt_buffer, pkt_len);
    ret = i2c_write(i2cdev, rfclk_pkt_buffer, pkt_len);
    if (ret == RFCLK_FAILURE) {
      printf("i2c could not reset tile 226/227 lmx pll\n");
      return ret;
    }
  }

  close_i2c_dev(i2cdev);
  close_i2c_bus();

  return 0;
}
