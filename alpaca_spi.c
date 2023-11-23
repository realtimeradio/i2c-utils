#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

#include "alpaca_spi.h"

#define SLEEP_TIME_uS 10000

#define SUCCESS 0
#define FAILURE 1

int init_spi_dev(spi_dev_t *spidev) {
  int ret = SUCCESS;
  int status = 0;

  spidev->fd = open(spidev->device, O_RDWR | O_SYNC);
  if (spidev->fd < 0) {
    printf("failed to open spi device %s\n", spidev->device);
    return FAILURE;
  }

  status = ioctl(spidev->fd, SPI_IOC_WR_MODE, &(spidev->mode));
  if (status < 0) {
    printf("failed to set SPI_IOC_WR_MODE\n");
    return FAILURE;
  }

  status = ioctl(spidev->fd, SPI_IOC_WR_BITS_PER_WORD, &(spidev->bits));
  if (status < 0) {
    printf("failed to set SPI_IOC_WR_BITS_PER_WORD\n");
    return FAILURE;
  }

  status = ioctl(spidev->fd, SPI_IOC_WR_MAX_SPEED_HZ, &(spidev->speed));
  if (status < 0) {
    printf("failed to set SPI_IOC_WR_MAX_SPEED_HZ\n");
    return FAILURE;
  }

  status = ioctl(spidev->fd, SPI_IOC_RD_MAX_SPEED_HZ, &(spidev->speed));
  if (status < 0) {
    printf("failed to set SPI_IOC_WR_MAX_SPEED_HZ\n");
    return FAILURE;
  }

#ifdef VERBOSE
  printf("spi info:\n");
  printf("\t spi mode: 0x%x\n", spidev->mode);
  printf("\t bits per word: %u\n", spidev->bits);
  printf("\t max speed: %d Hz\n", spidev->speed);
#endif

  return ret;
}

int close_spi_dev(spi_dev_t *spidev) {
  close(spidev->fd);
  spidev->fd = -1;
  return SUCCESS;
}

int read_spi_pkt(spi_dev_t *spidev, uint8_t *buf, uint8_t len) {

  int num_rd;
  num_rd = read(spidev->fd, buf, len);
  usleep(SLEEP_TIME_uS);
  return num_rd;
}

int write_spi_pkt(spi_dev_t *spidev, uint8_t *buf, uint8_t len) {

  int ret = SUCCESS;
  int num_wr;

  num_wr = write(spidev->fd, buf, len);
  usleep(SLEEP_TIME_uS);
  return ret;
}

int spi_transfer(spi_dev_t *spidev, uint8_t const *tx, uint8_t const *rx, uint8_t len) {
  int ret = SUCCESS;

  struct spi_ioc_transfer xfer;
  memset(&xfer, 0, sizeof(xfer)); // Initialize data structures to 0

  xfer.tx_buf = (unsigned long)tx;
  xfer.rx_buf = (unsigned long)rx;
  xfer.speed_hz = spidev->speed;
  xfer.bits_per_word = spidev->bits;
  xfer.len = len; // each transfer is only 1 byte long
  xfer.delay_usecs = spidev->delay;
  ret = ioctl(spidev->fd, SPI_IOC_MESSAGE(1), xfer);

  if (ret < 1) {
    printf("ioctl failed and returned errno %s\n", strerror(errno));
    ret = FAILURE;
  }
  return ret;

}
