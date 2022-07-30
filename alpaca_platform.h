#ifndef ALPACA_PLATFORM_H_
#define ALPACA_PLATFORM_H_

#define ZCU216   0
#define ZRF16    1  // HTG49DR and HTG29DR boards
#define ZCU208   2
#define ZCU111   3
#define RFSoC2x2 4
#define RFSoC4x2 5

// declare platform type and include "alpaca_platform.h" 
#ifndef PLATFORM
#define PLATFORM 0
#endif

#if PLATFORM != RFSoC4x2
  #define I2C_COM_BUS
#else
  #define SPI_COM_BUS
#endif

#endif /* ALPACA_PLATFORM_H_ */
