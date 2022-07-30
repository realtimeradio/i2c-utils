#ifndef ALPACA_SPI_H
#define ALPACA_SPI_H
#include <stdint.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

#define LMK_SPIDEV       "/dev/spidev0.0"
#define ADC_RFPLL_SPIDEV "/dev/spidev0.2"
#define DAC_RFPLL_SPIDEV "/dev/spidev0.1"

typedef struct SPIDevice {
  char device[32];  // Large enouch for something like:  "/dev/spidev32767.0"
  uint32_t fd;      // linux file descriptor
  uint8_t mode;
  uint8_t bits;
  uint32_t speed;
  uint16_t delay;
  // Some sane defaults for the int types would be {-1, 0, 8, 500000, 0}
} spi_dev_t;

int init_spi_dev(spi_dev_t *spidev);
int close_spi_dev(spi_dev_t *spidev);

int read_spi_pkt(spi_dev_t *spidev, uint8_t *buf, uint8_t len);
int write_spi_pkt(spi_dev_t *spidev, uint8_t *buf, uint8_t len);
#endif // ALPACA_SPI_H
