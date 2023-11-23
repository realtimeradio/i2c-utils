#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define LMK_SPIDEV "/dev/spidev0.0"

#include "alpaca_spi.h"
#include "alpaca_rfclks.h"

int main(int argc, char**argv) {
  int ret;
  uint8_t rfclk_pkt_buffer[3]; // (both LMK/LMX pkt len is 3
  int prg_cnt = LMK_REG_CNT;
  int pkt_len = LMX_PKT_SIZE;

  /* program rfsoc4x2 plls */
  // init spi devices with a generic config for reuse
  spi_dev_t spidev;
  spidev.mode = SPI_MODE_0 | SPI_CS_HIGH;
  spidev.bits = 8;
  spidev.speed = 500000;
  spidev.delay = 0;

  strcpy(spidev.device, LMK_SPIDEV);
  init_spi_dev(&spidev);

  // clear RB_PLL1_LD_LOST by toggle high and then low the CLR_PLL1_LD_LOST bit
  // in register 0x182 (R386)
  uint8_t R386[LMK_PKT_SIZE];
  format_rfclk_pkt(0x00018201, R386, LMK_PKT_SIZE); // lmk04828 spi packets are 3 bytes
  if(RFCLK_FAILURE==write_spi_pkt(&spidev, R386, LMK_PKT_SIZE)) {
    printf("error setting RB_PLL1_LD_LOST in R386 for LMK04828\n");
    close_spi_dev(&spidev);
    return RFCLK_FAILURE;
  }

  format_rfclk_pkt(0x00018200, R386, LMK_PKT_SIZE); // lmk04828 spi packets are 3 bytes
  if(RFCLK_FAILURE==write_spi_pkt(&spidev, R386, LMK_PKT_SIZE)) {
    printf("error clearning RB_PLL1_LD_LOST in R386 for LMK04828\n");
    close_spi_dev(&spidev);
    return RFCLK_FAILURE;
  }

  close_spi_dev(&spidev);
  return 0;
}
