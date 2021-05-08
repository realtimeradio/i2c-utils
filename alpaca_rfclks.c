#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // write

#include <errno.h>
#include <assert.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "alpaca_i2c_utils.h"
#include "alpaca_rfclks.h"

// TODO help out by explaining why this works for both lmk/lmx chips
void format_rfclk_pkt(uint8_t sdoselect, uint32_t d, uint8_t* buffer) {
  buffer[0] = SELECT_SPI_SDO(sdoselect);
  buffer[1] = (d >> 16) & 0xff;
  buffer[2] = (d >> 8)  & 0xff;
  buffer[3] =        d  & 0xff;
}

uint32_t* readtcs(FILE* tcsfile, uint16_t len, uint8_t pll_type) {
  uint32_t* rp;
  rp = malloc(sizeof(uint32_t)*len);
  if (rp == NULL) {
    return rp;
  }

  char R[128];
  char ln[128];
  int n;
  int i;

  // lmk=0, lmx2594 anything else
  // TICS raw register hex files for the different lmk parts contain the
  // complete programming sequence for rfsocs. For the lmx we must agument by
  // implementing the programming sequence as described in the data sheet to
  // apply rst, remove rst, program 113 registers, apply R0 again. LMX raw hex
  // text files start with the 113 program registers. Setting to 2 here leaves
  // room to fill in reset assert/de-assert
  i = (pll_type == 0) ? 0 : 2;

  // TODO need to make sure this sscanf sequence works for all chips, only tested on lmk04828b
  while (fgets(ln, sizeof(ln), tcsfile) != NULL) {
    n = sscanf(ln, " %s %*s %x", R, &rp[i]);
    if (n != 3) {
      n = sscanf(ln, " %s %x", R, &rp[i]);
    }
    i++;
  }

  // prepare programming sequence for lmx2594 {apply rst, remove rst, 113 prgm registers, apply R0 again}
  if (pll_type == 0) {
    rp[0] = LMX2594_RST_VAL; // apply reset
    rp[1] = 0x000000;        // remove reset
    rp[len-1] = rp[len-2];   // apply R0 a second time
  }

  return rp;
}

int prog_pll(I2CDev dev, uint8_t spi_sdosel, uint32_t* buf, uint16_t len) {
  int res = RFCLK_SUCCESS;
  uint8_t rfclk_pkt_buffer[4];

  for (int i=0; i<len; i++) {
    format_rfclk_pkt(spi_sdosel, buf[i], rfclk_pkt_buffer);
    res = i2c_write(dev, rfclk_pkt_buffer, 4); // TODO define magic 4? (packet size for lmk/lmx)
    if (res == RFCLK_FAILURE) {
      printf("i2c failed to program pll\n");
      return res;
    }
  }

  return res;
}

#if (PLATFORM == ZCU216) | (PLATFORM == ZCU208)
int set_sdo_mux(int mux_sel) {
  // TODO: axi gpio driver support reading if wanted to read before doing anything
  // TODO: move printf to stderr
  int fd_value;
  char gpio_path_value[64];

  sprintf(gpio_path_value, "/sys/class/gpio/gpio%s/value", CLK104_GPIO_MUX_SEL0);
  fd_value = open(gpio_path_value, O_RDWR);
  if (fd_value < 0) {
    printf("ERROR: could not open MUX_SEL0 (bit 0)\n");
    return RFCLK_FAILURE;
  }

  if (mux_sel & 0x1) {
    write(fd_value, "1", 2); // toggle hi,  "echo 1 > /sys/class/gpio/gpio510/value"
  } else {
    write(fd_value, "0", 2); // toggle low, "echo 0 > /sys/class/gpio/gpio510/value"
  }
  close(fd_value);

  sprintf(gpio_path_value, "/sys/class/gpio/gpio%s/value", CLK104_GPIO_MUX_SEL1);
  fd_value = open(gpio_path_value, O_RDWR);
  if (fd_value < 0) {
    printf("ERROR: could not open value for MUX_SEL1 (bit 1)\n");
    return RFCLK_FAILURE;
  }

  if (mux_sel & (0x1 << 1)) {
    write(fd_value, "1", 2); //toggle hi
  } else {
    write(fd_value, "0", 2); //toggle low
  }
  close(fd_value);

  return RFCLK_SUCCESS;
}

int init_clk104_gpio(int gpio_id) {
  // Init fabric gpio
  sprintf(CLK104_GPIO_MUX_SEL0, "%d", gpio_id); // reported gpio id by the kernel from boot messages
  sprintf(CLK104_GPIO_MUX_SEL1, "%d", gpio_id+1);

  int fd_export;
  int fd_direction;
  char gpio_path_direction[64];

  fd_export = open("/sys/class/gpio/export", O_WRONLY);
  if (fd_export < 0) {
    printf("ERROR: could not open gpio device to prepare for export\n");
    return RFCLK_FAILURE;
  }

  write(fd_export, CLK104_GPIO_MUX_SEL0, 4); //"echo 310 > /sys/class/export/gpio"
  write(fd_export, CLK104_GPIO_MUX_SEL1, 4);

  // set the direction of the GPIOs to outputs
  sprintf(gpio_path_direction, "/sys/class/gpio/gpio%s/direction", CLK104_GPIO_MUX_SEL0);
  fd_direction = open(gpio_path_direction, O_RDWR);
  if (fd_direction < 0) {
    close(fd_export);
    printf("ERROR: could not open first exported gpio to set output direction\n");
    return RFCLK_FAILURE;
  }

  write(fd_direction, "out", 4); // "echo out > /sys/class/gpio/gpio510/direction"
  close(fd_direction);

  // repeat for second gpio
  sprintf(gpio_path_direction, "/sys/class/gpio/gpio%s/direction", CLK104_GPIO_MUX_SEL1);
  fd_direction = open(gpio_path_direction, O_RDWR);
  if (fd_direction < 0) {
    close(fd_export);
    printf("ERROR: could not open second exported gpio to set output direction\n");
    return RFCLK_FAILURE;
  }

  write(fd_direction, "out", 4);
  close(fd_direction);

  close(fd_export);
  return RFCLK_SUCCESS;
}
#endif
