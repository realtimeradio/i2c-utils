#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // usleep

#include "alpaca_i2c_utils.h"
#include "idt8a34001_regs.h"

int main() {
  printf("I am an alpaca i2c teapot\n");

  // initialize i2c buses
  init_i2c_bus();

  // initialize single device, in this case the eeprom
  init_i2c_dev(I2C_DEV_EEPROM);

  // example usage reading the mac address from the eeprom
  uint8_t mac_addr[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  // from m24128-br data sheet the eeprom is expecting two bytes to address a
  // register. We set then 0x0020 as the offset and then read 6 bytes
  uint8_t mac_offset[2] = {0x0, 0x20};

  i2c_read_regs(I2C_DEV_EEPROM, mac_offset, 2,  mac_addr, 6);

  for (int i=0; i<6; i++) {
    if(i) {
      printf(":%02X", mac_addr[i]);
    } else {
      printf("%02X", mac_addr[i]);
    }
  }
  printf("\n");

  // exmple writing to eeprom and reading back
  uint8_t msg[8] = {0x0, 0xf0, 0x74, 0x65, 0x61, 0x70, 0x6f, 0x74};
  i2c_write(I2C_DEV_EEPROM, msg, 8);

  // example usage reading from the 8a34001
  init_i2c_dev(I2C_DEV_8A34001);

  //reading hw revision module base addr 0x8180 with offset 0x007a. The target
  //address is 0x8180 + 0x007a = 0x81FA.
  //To make ther read we do two steps. Write the page register then request the
  //register using the repeated stop `i2c_read_regs`.
  //uint8_t page_reg[4] = {0xfc, 0x00, 0x10, 0x20};
  uint8_t page_reg[5] = {0xfc, 0x00, 0x81, 0x10, 0x20};
  uint8_t hw_rev_offset = 0xfa;
  uint8_t hw_rev = 0x00;
  // write page reg
  i2c_write(I2C_DEV_8A34001, page_reg, 5);
  // read back
  i2c_read_regs(I2C_DEV_8A34001, &hw_rev_offset, 1, &hw_rev, 1);

  printf("hw_rev=%02X\n", hw_rev);

  // NOTE: Programming guide said that a reg should be read or written in a
  // transaction and so since the scratch module is 4 bytes (32-bit reg) we
  // right and read all at the same time.
  //reading/writing user scratchpad module base addr 0xCF50
  //uint8_t scratch_page_reg[4] = {0xfc, 0x00, 0x10, 0x20};
  uint8_t scratch_page_reg[5] = {0xfc, 0x00, 0xcf, 0x10, 0x20};
  uint8_t scratch_offset = 0x50;
  //uint8_t scratch = 0xee;
  uint8_t scratch[4] = {0xaa, 0xbb, 0xcc,0xdd};
  //printf("before read scratch=0x%02X\n", scratch);
  // write page reg
  i2c_write(I2C_DEV_8A34001, scratch_page_reg, 5);
  // read back
  //i2c_read_regs(I2C_DEV_8A34001, &scratch_offset, 1, &scratch, 1);
  i2c_read_regs(I2C_DEV_8A34001, &scratch_offset, 1, scratch, 4);

  //printf("after first read scratch=0x%02X\n", scratch);
  printf("read first read scratch=0x%02X\n", scratch[0]);
  printf("%02X", scratch[1]);
  printf("%02X", scratch[2]);
  printf("%02X", scratch[3]);

  //uint8_t scratch_to_write[2] = {scratch_offset, 0x74};
  uint8_t scratch_to_write[5] = {scratch_offset, 0x12, 0x23, 0x45, 0x67};
  // don't need to set page reg as it is already set
  //i2c_write(I2C_DEV_8A34001, scratch_to_write, 2);
  i2c_write(I2C_DEV_8A34001, scratch_to_write, 5);

  // read to see if we get what was written
  // don't think we need to set page reg since again it is already set
  //i2c_read_regs(I2C_DEV_8A34001, &scratch_offset, 1, &scratch, 1);
  i2c_read_regs(I2C_DEV_8A34001, &scratch_offset, 1, scratch, 4);

  //printf("read after write scratch=0x%02X\n", scratch);
  printf("read after write scratch=0x%02X", scratch[0]);
  printf("%02X", scratch[1]);
  printf("%02X", scratch[2]);
  printf("%02X\n", scratch[3]);

  printf("writing config to 8a34001...\n");
  // now lets program the 8a34001...
  for (int i = 0; i < IDT8A34001_NUM_VALUES; i++) {
    i2c_write(I2C_DEV_8A34001, idt_values[i], idt_lengths[i]);
  }
  printf("should be programmed...\n");

  close_i2c_dev(I2C_DEV_EEPROM);
  close_i2c_dev(I2C_DEV_8A34001);
  close_i2c_bus();

  return 0;
}
