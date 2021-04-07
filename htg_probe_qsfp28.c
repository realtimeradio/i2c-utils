#include <stdio.h>
#include <string.h>
#include <stdint.h> // uint8_t, uint16_t
#include <unistd.h> // usleep

#include "alpaca_i2c_utils.h"

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

int main() {
  printf("HTG test probe QSFP28 cages\n");

  init_i2c_bus();

  init_i2c_dev(I2C_DEV_QSFP28_A);
  init_i2c_dev(I2C_DEV_QSFP28_A_MOD);
  init_i2c_dev(I2C_DEV_QSFP28_B);
  init_i2c_dev(I2C_DEV_QSFP28_B_MOD);

  uint8_t addr = 0;
  int8_t qsfp28a_found = 0;
  int8_t qsfp28b_found = 0;
  if (i2c_write(I2C_DEV_QSFP28_A, &addr, 1)) {
    printf("QSFP28_A found...\n");
    qsfp28a_found = 1;
  } else {
    printf("QSFP28_A not found...\n");
  }

  if (i2c_write(I2C_DEV_QSFP28_B, &addr, 1)) {
    printf("QSFP28_B found...\n");
    qsfp28b_found = 1;
  } else {
    printf("QSFP28_B not found...\n");
  }

  /* */
  uint8_t buf[256]; 
  uint8_t vendor[17];
  uint8_t partno[17];
  const size_t vendor_len = 16;

  // read from qsfp28 a
  if (qsfp28a_found) {
    i2c_read(I2C_DEV_QSFP28_A, buf, sizeof(buf));
    memcpy(vendor, &buf[SFF_VENDOR], vendor_len);
    printf("\n\n****QSFP28 A*****\nType: %x\nVendor: %s\n", buf[0], vendor);
  }

  // read from qsfp28 b
  if (qsfp28b_found) {
    i2c_read(I2C_DEV_QSFP28_B, buf, sizeof(buf));
    memcpy(vendor, &buf[SFF_VENDOR], vendor_len); 
    printf("\n\n****QSFP28 B*****\nType: %x\nVendor: %s\n", buf[0], vendor);
  }

  /* */
  close_i2c_dev(I2C_DEV_QSFP28_A);
  close_i2c_dev(I2C_DEV_QSFP28_A_MOD);
  close_i2c_dev(I2C_DEV_QSFP28_B);
  close_i2c_dev(I2C_DEV_QSFP28_B_MOD);

  close_i2c_bus();

  return 0;
}
  
