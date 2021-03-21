
#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // usleep

#include <errno.h>
#include <assert.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "alpaca_i2c_utils.h"

#include "idt8a34001_regs.h"

#define DELAY_100us 100
#define NUM_I2C_RETRIES 5

static I2CSlave i2c_devs[] = {
  { "/dev/i2c-6" , 0x74, (1 << 0), 0x54, -1 }, // eeprom
  { "/dev/i2c-10", 0x74, (1 << 4), 0x5b, -1 }, // 8a34001
  { "/dev/i2c-20", 0x75, (1 << 7), 0x50, -1 }, // SFP0 Socket, A0h SFF-8472 memory space
  { "/dev/i2c-20", 0x75, (1 << 7), 0x51, -1 }  // SFP0 Module, A2h SFF-8472 memory space
};

#define I2C0_DEV_PATH "/dev/i2c-0"
#define I2C1_DEV_PATH "/dev/i2c-1"
int fd_i2c1;

#define SUCCESS 1
#define FAILURE 0

int i2c_write_bus(int fd, uint8_t addr, uint8_t *buf, uint16_t len) {
  int ret = SUCCESS;
  struct i2c_rdwr_ioctl_data packets;
  struct i2c_msg messages;
  messages.addr = addr;
  messages.flags = 0;
  messages.len = len;
  messages.buf = buf;
  packets.msgs = &messages;
  packets.nmsgs = 1;
  if (ioctl(fd, I2C_RDWR, &packets) < 0) {
    ret = FAILURE;
  }
  return ret;
}

int i2c_read_bus(int fd, uint8_t addr, uint8_t *buf, uint16_t len) {
  int ret = SUCCESS;
  struct i2c_rdwr_ioctl_data packets;
  struct i2c_msg messages;
  messages.addr = addr;
  messages.flags = I2C_M_RD;
  messages.len = len;
  messages.buf = buf;
  packets.msgs = &messages;
  packets.nmsgs = 1;
  if (ioctl(fd, I2C_RDWR, &packets) < 0) {
    ret = FAILURE;
  }
  return ret;
}

// implementing repeated start to accomplish a register read a write is followed
// by a read
int i2c_read_regs_bus(int fd, uint8_t addr, uint8_t *offset, uint16_t olen, uint8_t *buf, uint16_t len) {
  int ret = SUCCESS;
  struct i2c_rdwr_ioctl_data packets;
  struct i2c_msg messages[2];
  messages[0].addr = addr;
  messages[0].flags = 0; // write
  messages[0].len = olen;
  messages[0].buf = offset;
  messages[1].addr = addr;
  messages[1].flags = I2C_M_RD;
  messages[1].len = len;
  messages[1].buf = buf;
  packets.msgs = messages;
  packets.nmsgs = 2;
  if (ioctl(fd, I2C_RDWR, &packets) < 0) {
    ret = FAILURE;
  }
  return ret;
}

int i2c_set_mux(I2CSlave *dev_ptr) {
   /*
   * This method is hard coded to set the mux on i2c bus 1 if these utils were
   * to extend to be bus agnostic we would need to be able to resolve the slave
   * device with the parent bus (e.g., /dev/i2c-6 lives on bus 1, /dev/i2c-1
   */
  int ret = SUCCESS; 
   /*
   * I2C bus 1 on the zcu216 has two muxes, one at 0x74 and one at 0x75. Would
   * it be necesary to disable the mux that is not of interest as to not send data
   * on that mux? I think the answer would be yes if there were addr collisions.
   * In this case the only device with a collision is si570 both having 0x5d but
   * with that clock not used it isn't an issue right now.
   */
  ret = i2c_write_bus(fd_i2c1, dev_ptr->mux_addr, &(dev_ptr->mux_sel), 1);
  if (ret == FAILURE) {
    return ret;
  }
  return ret;
}

int i2c_get_mux(I2CSlave *dev_ptr, uint8_t *buf) {
  int ret = SUCCESS;
  ret = i2c_read_bus(fd_i2c1, dev_ptr->mux_addr, buf, 1);
  if (ret == FAILURE) {
    return ret;
  }
  return ret;
}

int i2c_write(I2CDev dev, uint8_t *buf, uint16_t len) {
  uint8_t curmux = 0;
  int i;
  I2CSlave *dev_ptr = &i2c_devs[dev];

  for (i=0; i < NUM_I2C_RETRIES; i++) {
    // set mux
    if (FAILURE == i2c_set_mux(dev_ptr)) { continue; }
    // write
    if (FAILURE == i2c_write_bus(dev_ptr->fd, dev_ptr->slave_addr, buf, len)) { continue; }
    // read switch status
    if (FAILURE == i2c_get_mux(dev_ptr, &curmux)) { continue; }
    // make sure it was as expected
    if (curmux == dev_ptr->mux_sel) {
      // read successful
      break;
    } else {
      // delay and attempt again
      printf("warn: i2c1 mux status change\n");
      usleep(DELAY_100us*(i+1));
    }
  }
  if (i < NUM_I2C_RETRIES) {
    return SUCCESS;
  } else {
    printf("error could not write, reached number of retries...\n");
    return FAILURE;
  }
}

int i2c_read(I2CDev dev, uint8_t *buf, uint16_t len) {
  uint8_t curmux = 0;
  int i;
  I2CSlave *dev_ptr = &i2c_devs[dev];

  for (i=0; i < NUM_I2C_RETRIES; i++) {
    // set mux
    if (FAILURE == i2c_set_mux(dev_ptr)) { continue; }
    // write
    if (FAILURE == i2c_read_bus(dev_ptr->fd, dev_ptr->slave_addr, buf, len)) { continue; }
    // read switch status
    if (FAILURE == i2c_get_mux(dev_ptr, &curmux)) { continue; }
    // make sure it was as expected
    if (curmux == dev_ptr->mux_sel) {
      // read successful
      break;
    } else {
      // delay and attempt again
      printf("warn: i2c1 mux status change\n");
      usleep(DELAY_100us*(i+1));
    }
  }
  if (i < NUM_I2C_RETRIES) {
    return SUCCESS;
  } else {
    printf("error could not write, reached number of retries...\n");
    return FAILURE;
  }
}

int i2c_read_regs(I2CDev dev, uint8_t *offset, uint16_t olen, uint8_t *buf, uint16_t len) {
  uint8_t curmux = 0;
  int i;
  I2CSlave *dev_ptr = &i2c_devs[dev];

  for (i=0; i < NUM_I2C_RETRIES; i++) {
    // set mux
    if (FAILURE == i2c_set_mux(dev_ptr)) {
      printf("could not set mux\n");
      continue;
    }
    // write
    if (FAILURE==i2c_read_regs_bus(dev_ptr->fd, dev_ptr->slave_addr, offset, olen, buf, len)) {
      printf("could not run low level i2c_read_regs()\n");
      continue;
    }
    // read switch status
    if (FAILURE == i2c_get_mux(dev_ptr, &curmux)) {
      printf("could not read mux status\n");
      continue;
    }
    // make sure it was as expected
    if (curmux == dev_ptr->mux_sel) {
      // read successful
      printf("read successful\n");
      break;
    } else {
      printf("curmux = %x, mux_sel=%x\n", curmux, dev_ptr->mux_sel);
      // delay and attempt again
      printf("warn: i2c1 mux status change\n");
      usleep(DELAY_100us*(i+1));
    }
  }
  if (i < NUM_I2C_RETRIES) {
    return SUCCESS;
  } else {
    printf("error could not write, reached number of retries...\n");
    return FAILURE;
  }
}

int main() {
  printf("I am an alpaca i2c teapot\n");

  // initialize i2c buses
  fd_i2c1 = open(I2C1_DEV_PATH, O_RDWR);
  if (fd_i2c1 < 0) {
    printf("Error opening I2C bus 1\n");
  }

  I2CSlave* i2c_devptr = &i2c_devs[I2C_DEV_EEPROM];
  printf("linux path for eeprom:%s\n", i2c_devptr->dev_path);

  i2c_devptr->fd = open(i2c_devptr->dev_path, O_RDWR);
  if (i2c_devptr->fd < 0) {
    printf("Error opening i2c dev EEPROM\n");
  }

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
  i2c_devptr = &i2c_devs[I2C_DEV_8A34001];

  i2c_devptr->fd = open(i2c_devptr->dev_path, O_RDWR);
  if (i2c_devptr->fd < 0) {
    printf("Error opening i2c dev 8a34001\n");
  }

  //reading hw revision module base addr 0x8180 with offset 0x007a. The target
  //address is 0x8180 + 0x0071 = 0x81FA.
  //To make ther read we do two steps. Write the page register then request the
  //register using the repeated stop `i2c_read_regs`.
  uint8_t page_reg[4] = {0xfc, 0x00, 0x10, 0x20};
  uint8_t hw_rev_offset = 0xfa;
  uint8_t hw_rev = 0x11;
  // write page reg
  i2c_write(I2C_DEV_8A34001, page_reg, 4);
  // read back
  i2c_read_regs(I2C_DEV_8A34001, &hw_rev_offset, 1, &hw_rev, 1);

  printf("hw_rev=%02X\n", hw_rev);

  //reading/writing user scratchpad module base addr 0xCF50
  uint8_t scratch_page_reg[4] = {0xfc, 0x00, 0x10, 0x20};
  uint8_t scratch_offset = 0x00;
  uint8_t scratch = 0xee;
  printf("before read scratch=0x%02X\n", scratch);
  // write page reg
  i2c_write(I2C_DEV_8A34001, scratch_page_reg, 4);
  // read back
  i2c_read_regs(I2C_DEV_8A34001, &scratch_offset, 1, &scratch, 1);

  printf("after first read scratch=0x%02X\n", scratch);
  uint8_t scratch_to_write[2] = {scratch_offset, 0x74};
  // don't need to set page reg as it is already set
  i2c_write(I2C_DEV_8A34001, scratch_to_write, 2);

  // read to see if we get what was written
  // don't think we need to set page reg since again it is already set
  i2c_read_regs(I2C_DEV_8A34001, &scratch_offset, 1, &scratch, 1);

  printf("read after write scratch=0x%02X\n", scratch);

  printf("writing config to 8a34001...\n");
  // now lets program the 8a34001...
  for (int i = 0; i < IDT8A34001_NUM_VALUES; i++) {
    i2c_write(I2C_DEV_8A34001, idt_values[i], idt_lengths[i]);
  }
  printf("should be programmed...\n");

  return 0;
}
