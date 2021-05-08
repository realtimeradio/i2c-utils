#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // write

#include <errno.h>
#include <assert.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "alpaca_rfclks.h"

// TODO explain why this works for both lmk/lmx chips
void format_rfclk_pkt(uint8_t sdoselect, uint32_t d, uint8_t* buffer) {
  buffer[0] = SELECT_SPI_SDO(sdoselect);
  buffer[1] = (d >> 16) & 0xff;
  buffer[2] = (d >> 8)  & 0xff;
  buffer[3] =        d  & 0xff;
}

#if (PLATFORM == ZCU216) | (PLATFORM == ZCU208)
int set_sdo_mux(int mux_sel) {
  // TODO: axi gpio driver support reading if wanted to read before doing anything
  int fd_value;
  char gpio_path_value[64];

  sprintf(gpio_path_value, "/sys/class/gpio/gpio%s/value", CLK104_GPIO_MUX_SEL0);
  fd_value = open(gpio_path_value, O_RDWR);
  if (fd_value < 0) {
    printf("ERROR: could not open MUX_SEL0 (bit 0)\n");
    return RFCLK_FAILURE;
  }

  if (mux_sel & 0x1) {
    printf("bit 0=1\n");
    write(fd_value, "1", 2); // toggle hi,  "echo 1 > /sys/class/gpio/gpio510/value"
  } else {
    printf("bit 0=0\n");
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
    printf("bit 1=1\n");
    write(fd_value, "1", 2); //toggle hi
  } else {
    printf("bit 1=0\n");
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
  char gpio_path_value[64];

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
