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

#define DELAY_100us 100
#define NUM_I2C_RETRIES 5

#define I2C0_DEV_PATH "/dev/i2c-0"
#define I2C1_DEV_PATH "/dev/i2c-1"
int fd_i2c0;
int fd_i2c1;

#define SUCCESS 0
#define FAILURE 1

#define X(name, dev) dev,
static I2CSlave i2c_devs[] = { I2C_DEVICES_MAP };
#undef X

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
  int ret = SUCCESS;

  // device is not addressed via mux (TODO test for zcu111)
  if (dev_ptr->mux_addr == 0xff) {
    return SUCCESS;
  }

  /*
  * i2c bus 1 on the zcu216 has two muxes, one at 0x74 and one at 0x75. Would
  * it be necesary to disable the mux that is not of interest (first writing a
  * packet to that mux configuring it as to not send data on that mux? I think
  * the answer would be yes if there were addr collisions.  In this case the
  * only device with a collision is si570 both having 0x5d but with that clock
  * not used it isn't an issue right now.
  */

  ret = i2c_write_bus(*(dev_ptr->parent_fd), dev_ptr->mux_addr, &(dev_ptr->mux_sel), 1);
  if (ret == FAILURE) {
    return ret;
  }
  return ret;
}

int i2c_get_mux(I2CSlave *dev_ptr, uint8_t *buf) {
  int ret = SUCCESS;

  // device is not addressed via mux (TODO test for zcu111)
  if (dev_ptr->mux_addr == 0xff) {
    return SUCCESS;
  }

  ret = i2c_read_bus(*(dev_ptr->parent_fd), dev_ptr->mux_addr, buf, 1);
  if (ret == FAILURE) {
    return ret;
  }
  return ret;
}

int init_i2c_bus() {
  // initialize i2c buses

  // TODO: Most MPSOC designs enable both I2C buses, but this may not be the
  // case, probably a smarter way to to initialize a bus of interest
  fd_i2c1 = open(I2C1_DEV_PATH, O_RDWR);
  if (fd_i2c1 < 0) {
    printf("ERROR: could not open I2C bus 1\n");
    return FAILURE;
  }

  fd_i2c0 = open(I2C0_DEV_PATH, O_RDWR);
  if (fd_i2c0 < 0) {
    printf("ERROR: could not open I2C bus 0\n");
    return FAILURE;
  }
  return SUCCESS;
}

int close_i2c_bus() {
  if (fd_i2c1 > 0) {
    close(fd_i2c1);
  }

  if (fd_i2c0 > 0) {
    close(fd_i2c0);
  }

  return SUCCESS;
}

int init_i2c_dev(I2CDev dev) {
  I2CSlave* i2c_devptr = &i2c_devs[dev];

  i2c_devptr->fd = open(i2c_devptr->dev_path, O_RDWR);
  if (i2c_devptr->fd < 0) {
    printf("ERROR: could not open i2c dev\n");
    return FAILURE;
  }
  return SUCCESS;
}

int close_i2c_dev(I2CDev dev) {
  I2CSlave* i2c_devptr = &i2c_devs[dev];

  if (i2c_devptr->fd > 0) {
    close(i2c_devptr->fd);
  }

  return SUCCESS;
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
      printf("WARNING: mux status changed during transaction\n");
      usleep(DELAY_100us*(i+1));
    }
  }
  if (i < NUM_I2C_RETRIES) {
    return SUCCESS;
  } else {
    printf("ERROR: could not write, reached number of retries...\n");
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
      printf("WARNING: mux status changed during transaction\n");
      usleep(DELAY_100us*(i+1));
    }
  }
  if (i < NUM_I2C_RETRIES) {
    return SUCCESS;
  } else {
    printf("ERROR: could not read, reached number of retries...\n");
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
      printf("could not run low level i2c_read_regs() %d\n", i);
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
      // delay and attempt again
      printf("WARNING: mux status changed during transaction\n");
      usleep(DELAY_100us*(i+1));
    }
  }
  if (i < NUM_I2C_RETRIES) {
    return SUCCESS;
  } else {
    printf("ERROR: could not read, reached number of retries...\n");
    return FAILURE;
  }
}

