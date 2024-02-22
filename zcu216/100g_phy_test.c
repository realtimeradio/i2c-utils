#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // usleep

#include "alpaca_i2c_utils.h"
#include "phytest_idt8a34001_regs.h"

int main() {
  // initialize i2c buses
  init_i2c_bus();

  // example usage reading from the 8a34001
  init_i2c_dev(I2C_DEV_8A34001);

  printf("writing config to 8a34001...\n");
  for (int i = 0; i < IDT8A34001_NUM_VALUES; i++) {
    i2c_write(I2C_DEV_8A34001, idt_values[i], idt_lengths[i]);
  }
  printf("should be programmed...\n");

  close_i2c_dev(I2C_DEV_8A34001);
  close_i2c_bus();

  return 0;
}
