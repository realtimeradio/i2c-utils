#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // write

#include <sys/fcntl.h>

#define SUCCESS 0
#define FAILURE 1

/*
 * The LMK reset pin on the RFSoC 4x2 is attached to a PS MIO. The GPIO needs to
 * be registerd using the xilinx linux gpio sysfs inteface and then configured as an
 * output and set to 0 to bring the LMK out of reset.
 *
 * The zynqMP MIO GPIOs base is 338 and having 174 pins (0th pin starts at 338
 * and ends at 511). The reset is on MIO 7 giving the offset 345.
 *
 * For more info on xilinx Linux GPIO driver:
 * https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842398/Linux+GPIO+Driver
 *
 */

// use sysfs for adding a zynqmp gpio and configure as an 'out' port and set to 0 
int init_zynqmp_gpio_out(int gpio_id) {
  int fd_export;
  int fd_direction;
  int fd_value;
  char gpio_export[4];
  char gpio_path_direction[64];
  char gpio_path_value[64];

  sprintf(gpio_export, "%d", gpio_id); // gpio base is 388 and 

  fd_export = open("/sys/class/gpio/export", O_WRONLY);
  if (fd_export < 0) {
    printf("ERROR: could not open gpio device to prepare for export\n");
    return FAILURE;
  }

  write(fd_export, gpio_export, 4); //"echo 345 > /sys/class/export/gpio"
  close(fd_export);

  // set the direction of the GPIOs to outputs
  sprintf(gpio_path_direction, "/sys/class/gpio/gpio%s/direction", gpio_export);
  fd_direction = open(gpio_path_direction, O_RDWR);
  if (fd_direction < 0) {
    printf("ERROR: could not open first exported gpio to set output direction\n");
    return FAILURE;
  }

  write(fd_direction, "out", 4); // "echo out > /sys/class/gpio/gpio345/direction"
  close(fd_direction);

  // set the value of the GPIO to low ('0')
  sprintf(gpio_path_value, "/sys/class/gpio/gpio%s/value", gpio_export);
  fd_value = open(gpio_path_value, O_RDWR);
  if (fd_value < 0) {
    printf("ERROR: could not set gpio low\n");
    return FAILURE;
  }

  // write gpio low (bring lmk out of reset)
  write(fd_value, "0", 2);  // "echo 0 > /sys/class/gpio/gpio345/value"
  close(fd_value);

  return SUCCESS;
}

int main(int argc, char* argv[]) {

  init_zynqmp_gpio_out(345);
  return 0;
}
