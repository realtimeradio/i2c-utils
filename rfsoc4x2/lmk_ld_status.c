#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "alpaca_spi.h"
#include "alpaca_rfclks.h"

#define LMK_SPIDEV "/dev/spidev0.0"

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

  // RFSoC4x2 STATUS_LD2 connected to SDO, configure PLL2_LD_MUX for spi
  // readback (register 0x16E, R366)
  uint8_t R366[LMK_PKT_SIZE];
  format_rfclk_pkt(0x00016e3b, R366, LMK_PKT_SIZE); // lmk04828 spi packets are 3 bytes
  if(RFCLK_FAILURE==write_spi_pkt(&spidev, R366, LMK_PKT_SIZE)) {
    printf("error setting R366 for readback on LMK04828\n");
    close_spi_dev(&spidev);
    return RFCLK_FAILURE;
  }

  uint32_t R386_buf = 0x018200;
  uint8_t lmk_tx_read[LMK_PKT_SIZE] = {0x0};
  uint8_t lmk_reg_read[LMK_PKT_SIZE] = {0x0};

  lmk_tx_read[0] = (0xff & (R386_buf >> 16)) | REG_RW_BIT;
  lmk_tx_read[1] =  0xff & (R386_buf >> 8);

  lmk_reg_read[0] = 0x0;
  lmk_reg_read[1] = 0x0;
  lmk_reg_read[2] = 0x0;

  spi_transfer(&spidev, lmk_tx_read, lmk_reg_read, LMK_PKT_SIZE);

  R386_buf = (R386_buf & 0xffff00) + lmk_reg_read[2];

  // revert PLL2_LD_MUX/PLL2_LD_TYPE reg back (register 0x16E, R366)
  format_rfclk_pkt(0x00016e13, R366, LMK_PKT_SIZE);
  if(RFCLK_FAILURE==write_spi_pkt(&spidev, R366, LMK_PKT_SIZE)) {
    printf("error setting R366 for readback on LMK04828\n");
    close_spi_dev(&spidev);
    return RFCLK_FAILURE;
  }

  // display lmk config info
  printf("LMK04828 readback status raw register: 0x%06x\n", R386_buf);
  printf("PLL1 LD LOST: %s\n", (R386_buf & 0x4) ? "LOST" : "STABLE");
  printf("PLL1 LD STAT: %s\n", (R386_buf & 0x2) ? "LOCK" : "UNLOCK");

  close_spi_dev(&spidev);
  return 0;
}
