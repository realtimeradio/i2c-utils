#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // usleep

#include "alpaca_i2c_utils.h"

// i2c result values
#define SUCCESS 0
#define FAILURE 1

// Offsets within SFF status block
enum {
    SFF_ID = 0,
    SFF_VENDOR = 20,
    SFF_PARTNO = 40,
    SFF_STATUS = 110
} SFF_8472_OFFSET;

// SFP Status bits from Address A2h, register 110
typedef struct {
    const uint8_t st_bit;
    const char* st_msg;
} SfpStatus;

static const SfpStatus sfp_st[] = {
  { (1 << 7), "Tx Disable" },
  { (1 << 6), "Soft Tx Disable" },
  { (1 << 5), "RS1" },
  { (1 << 4), "RS0" },
  { (1 << 3), "Soft RS0" },
  { (1 << 2), "Tx Fault" },
  { (1 << 1), "Rx LOS" },
  { (1 << 0), "Data Ready" }
};

// read A2 status
uint16_t get_sfp_status(I2CDev dev) {
  // Read status byte
  // Set pointer to SFF status register (see SFF-8742 spec for bit definitions)
  uint8_t ret = FAILURE;
  uint8_t sff_status_addr = SFF_STATUS;
  uint8_t sfp_status;

  ret = i2c_write(I2C_DEV_SFP0_MOD, &sff_status_addr, sizeof(sff_status_addr));
  if (ret==FAILURE) {
    printf("failed to write sfp status\n");
    return 256; // greater than 255, uint8_t, to indicate error
  }

  // Read status byte
  ret = i2c_read(I2C_DEV_SFP0_MOD, &sfp_status, sizeof(sfp_status));
  if (ret==FAILURE) {
    printf("failed to read sfp status\n");
    return 256;
  }

  // Decode SFF Status
  if (sfp_status) {
    // Decode status bits
    for (int i=0; i<8; i++) {
      if (sfp_st[i].st_bit & sfp_status) {
        printf("%s\n", sfp_st[i].st_msg);
      }
    }
  } else {
    // No bits set, all clear (sfp_status == 0)
    printf("Normal\n");
  }
  return sfp_status;
}

int main() {
  uint8_t i2c_ret;
  I2CDev sfp_tcvr[4] = {I2C_DEV_SFP0, I2C_DEV_SFP1, I2C_DEV_SFP2, I2C_DEV_SFP3};
  I2CDev sfp_mods[4] = {I2C_DEV_SFP0_MOD, I2C_DEV_SFP1_MOD, I2C_DEV_SFP2_MOD, I2C_DEV_SFP3_MOD};

  printf("****** ZCU216 probe zsfp+ cages ******\n");
  printf("opening i2c bus...\n");
  init_i2c_bus();
  for (uint8_t i=0; i<4; i++) {
    init_i2c_dev(sfp_tcvr[i]);
    init_i2c_dev(sfp_mods[i]);
  }

  uint8_t addr = 0;
  uint8_t sfp_found = 0;
  printf("checking for transceivers...\n");
  for (uint8_t i=0; i<4; i++) {
    if (SUCCESS==i2c_write(sfp_tcvr[i], &addr, 1)) {
      printf("SFP%u found...\n", i);
      sfp_found = sfp_found | (1 << i);
    } else {
      printf("SFP%u not found...\n", i);
    }
  }
  printf("sfp_found = %u\n", sfp_found);

  /* */
  uint8_t buf[256]; 
  uint8_t vendor[17];
  uint8_t partno[17];
  const size_t vendor_len = 16;

  // read sfp SFF-8742 A0 id block for cages 0-3
  printf("reading module vendor info...\n");
  for (uint8_t i=0; i<4; i++) {
    if (sfp_found && (1<<i)) {
      i2c_read(sfp_tcvr[i], buf, sizeof(buf));
      memcpy(vendor, &buf[SFF_VENDOR], vendor_len);
      printf("****SFP%u*****\nType: %x\nVendor: %s\n", i, buf[0], vendor);
    }
  }

  // read sfp SFF-8742 A2 status for cages 0-3
  printf("reading module status...\n");
  uint8_t sfp_status;
  for (uint8_t i=0; i<4; i++) {
    if (sfp_found && (1<<i)) {
      sfp_status = get_sfp_status(sfp_mods[i]);
    } else {
      printf("SFP%u was not found, cannot read status\n", i);
    }
  }

  /* */
  printf("closing i2c bus...\n");
  for (uint8_t i=0; i<4; i++) {
    close_i2c_dev(sfp_tcvr[i]);
    close_i2c_dev(sfp_mods[i]);
  }
  close_i2c_bus();

  return 0;
}
