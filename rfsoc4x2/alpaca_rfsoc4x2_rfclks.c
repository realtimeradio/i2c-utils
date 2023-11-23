#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <errno.h>
#include <assert.h>

#include <sys/stat.h>

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

  /* program rfsoc4x2 plls */
  int ret;
  // init spi devices
  spi_dev_t spidev;
  // generic config to reuse
  spidev.mode = SPI_MODE_0 | SPI_CS_HIGH;
  spidev.bits = 8;
  spidev.speed = 500000;
  spidev.delay = 0;

  /* program */
  if (pll_type == 0) {
    // configure lmk
    strcpy(spidev.device, LMK_SPIDEV);
    init_spi_dev(&spidev);
    ret = prog_pll(&spidev, rp, prg_cnt, pkt_len);

    /* readback */
    get_pll_config(&spidev, pll_type, rp);

  } else {
    // rfsoc4x2 has one adc rfpll and one dac rfpll
    strcpy(spidev.device, ADC_RFPLL_SPIDEV);
    init_spi_dev(&spidev);
    ret = prog_pll(&spidev, rp, prg_cnt, pkt_len);
    close_spi_dev(&spidev);

    /* readback */
    printf("ADC LMX register readback not yet implemented for rfsoc4x2, only LED status is shown\n");
    printf("rfsoc4x2 does not support register readback, only led status\n");

    // now dac rfpll
    strcpy(spidev.device, DAC_RFPLL_SPIDEV);
    init_spi_dev(&spidev);

    /* readback */
    printf("DAC LMX register readback not yet implemented for rfsoc4x2, only LED status is shown\n");
    ret = prog_pll(&spidev, rp, prg_cnt, pkt_len);
  }

  // release memory from tcs pll config
  free(rp);

  // close spi device
  close_spi_dev(&spidev);

  return 0;
}



