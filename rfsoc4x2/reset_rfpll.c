#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define LMK_SPIDEV       "/dev/spidev0.0"
#define ADC_RFPLL_SPIDEV "/dev/spidev0.2"
#define DAC_RFPLL_SPIDEV "/dev/spidev0.1"

#include "alpaca_spi.h"
#include "alpaca_rfclks.h"

void usage(char* name) {
  printf("%s -lmk|-lmx\n", name);
}

int main(int argc, char**argv) {
  int ret;
  int pll_type;
  uint8_t rfclk_pkt_buffer[3]; // (both LMK/LMX pkt len is 3
  uint32_t rst_pkt;

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

  /* program rfsoc4x2 plls */
  // init spi devices with a generic config for reuse
  spi_dev_t spidev;
  spidev.mode = SPI_MODE_0 | SPI_CS_HIGH;
  spidev.bits = 8;
  spidev.speed = 500000;
  spidev.delay = 0;

  rst_pkt = (pll_type == 0) ? LMK04832_RST_VAL : LMX2594_RST_VAL;
  format_rfclk_pkt(rst_pkt, rfclk_pkt_buffer, pkt_len);

  // reset pll
  if (pll_type == 0) {
    strcpy(spidev.device, LMK_SPIDEV);
    init_spi_dev(&spidev);
    ret = write_spi_pkt(&spidev, rfclk_pkt_buffer, pkt_len);
  } else {
    // open adc rfpll and reset
    strcpy(spidev.device, ADC_RFPLL_SPIDEV);
    init_spi_dev(&spidev);
    ret = write_spi_pkt(&spidev, rfclk_pkt_buffer, pkt_len);
    if (ret == RFCLK_FAILURE) {
      printf("could not reset pll\n");
      close_spi_dev(&spidev);
      return ret;
    }
    close_spi_dev(&spidev);
    // now open dac rfpll and reset
    strcpy(spidev.device, DAC_RFPLL_SPIDEV);
    init_spi_dev(&spidev);
    ret = write_spi_pkt(&spidev, rfclk_pkt_buffer, pkt_len);
  }

  if (ret == RFCLK_FAILURE) {
    printf("could not reset pll\n");
    close_spi_dev(&spidev);
    return ret;
  }

  close_spi_dev(&spidev);

  return 0;
}
