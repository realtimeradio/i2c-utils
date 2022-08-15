
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h> // CHAR_BIT

#include "alpaca_spi.h"

#define OLED_SPIDEV "/dev/spidev1.0"
#define SUCCESS 0
#define FAILURE 1

// With SPI operation of the OLED display, single byte data is transmitted
// starting with an single byte handshake sequence of five 'high' bits,
// read/write bit, and a data/command bit with a single low bit in the LSB/
//    {1, 1, 1, 1, 1, R/W, D/C, 0}
// write R/W = 0, read R/W = 1
// command D/C = 0, data D/C = 1
#define OLED_WR_CMD_BYTE 0xF8 // 0b1111_1000
#define OLED_WR_DAT_BYTE 0xFA // 0b1111_1010

#define SYNC (0xF8)
#define READ (1 << 2)
#define WRITE (0)
#define DATA (1 << 1)
#define CMD  (0)

#define SLEEP_TIME_uS 10000

// the first byte indicates either a CMD or DATA byte
#define INIT_LENGTH 28
uint16_t init_sequence[INIT_LENGTH] = {
  // this init configuration sequence is lifted straight from the example from
  // the datasheet
  0x002a, 0x0071, 0x0200, 0x0028, 0x0008, 0x002a, 0x0079, 0x00d5, 0x0070,
  0x0078, 0x0008, 0x0006, 0x0072, 0x0200, 0x002a, 0x0079, 0x00da, 0x0000,
  0x00dc, 0x0000, 0x0081, 0x007f, 0x00d9, 0x00f1, 0x00db, 0x0040, 0x0078,
  0x0028
};

void fmt_spi_write_pkt(uint8_t dc, uint8_t payload, uint8_t *buf) {

  buf[0] = SYNC | WRITE | dc;

  // copy payload into a uint16_t
  uint16_t v = (payload << 4);
  uint16_t r = v;
  uint16_t s = sizeof(v) * CHAR_BIT - 1; // extra shift needed at end

  // flip bits
  for (v >>= 1; v; v >>= 1) {
    r <<= 1;
    r |= v & 1;
    s--;
  }
  r <<= s; // shift when v's highest bits are zero

  // left shift top byte
  uint8_t *z = (uint8_t *) &r;
  z[1] = z[1] << 4;

  // set buffer with the formatted data
  buf[1] = z[1];
  buf[2] = z[0];

#ifdef VERBOSE
  printf("{ %0x, %0x, %0x }\n", buf[0], buf[1], buf[2]);
#endif
  return;
}

void display_string(spi_dev_t *spidev, char *str) {

  int ln = 0;
  uint32_t length = strlen(str);
  uint8_t buffer[3];

  //TODO do not count newline whitespaces
  if (length > 32) {
    printf("string to long to display\n");
    return;
  }

  uint8_t sequence[3] = {0x0001, 0x0080, 0x000c};

  // configure display to receive string
#ifdef VERBOSE
  printf("configuring oled to receive string data\n");
#endif
  for (int i=0; i<3; i++) {
    uint8_t dc_mode = ((sequence[i] >> 8) & 0xff);
    uint8_t payload = (sequence[i] & 0xff);
    fmt_spi_write_pkt(dc_mode, payload, buffer);
    write_spi_pkt(spidev, buffer, 3);
  }
  usleep(SLEEP_TIME_uS);

  // TODO handle new line
#ifdef VERBOSE
  printf("sending!\n");
#endif
  for (uint8_t i=0; i<length; i++) {
    fmt_spi_write_pkt(DATA, str[i], buffer);
    write_spi_pkt(spidev, buffer, 3);
  }

  return;
}


/*
int spi_write_bus(spi_dev_t *spidev, uint8_t *tx_buf, uint8_t *rx_buf) {
  int ret;
  struct spi_ioc_transfer spi_transfer;

  spi_transfer.tx_buf = tx_buf;
  spi_transfer.rx_buf = rx_buf;
  spi_transfer.speed_hz = spidev->speed;
  spi_transfer.len = 1;
  spi_transfer.delay_usecs = spidev->delay;

  ret = ioctl(spidev->fd, SPI_IOC_MESSAGE(1), spi_transfer);
  if (ret < 1) {
    printf("ioctl failed and returned errno %s\n", strerror(errno));
  }
  return ret;
}

int format_oled_pkt(uint8_t type, uint8_t data, uint8_t buffer uint8_t len) {
  int ret = SUCCESS;

  buffer[0] = (type==OLED_CMD) ? OLED_CMD_BYTE : OLED_DAT_BYTE;


  return ret; 
}

int write_oled(uint8_t type, uint) {
  int ret = SUCCESS;
  return ret;
}
*/

int main(int argc, char* argv[]) {

  if (argc != 2) {
    printf("error, missing string argument to display\n");
    return 0;
  }

  // init spi device
  spi_dev_t oled_spi;
  strcpy(oled_spi.device, OLED_SPIDEV);
  oled_spi.mode = SPI_MODE_3 | SPI_CS_HIGH;
  oled_spi.bits = 8;
  oled_spi.speed = 500000;
  oled_spi.delay = 0;

  init_spi_dev(&oled_spi);

  // init oled device
#ifdef VERBOSE
  printf("configuring oled display\n");
#endif
  uint8_t buffer[3];
  for (int i=0; i<INIT_LENGTH; i++) {
    uint8_t dc_mode = ((init_sequence[i] >> 8) & 0xff);
    uint8_t payload = (init_sequence[i] & 0xff);
    
    fmt_spi_write_pkt(dc_mode, payload, buffer);
    write_spi_pkt(&oled_spi, buffer, 3);
  }

  // send the data
#ifdef VERBOSE
  printf("setting up to send string \"%s\" to the oled display\n", argv[1]);
#endif
  display_string(&oled_spi, argv[1]);
  return 0;

}
