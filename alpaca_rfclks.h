#ifndef ALPACA_RFCLKS_H_
#define ALPACA_RFCLKS_H_

#include "alpaca_platform.h"

/*
 * ZCU111/ZCU216 mux switch has basically the same configuration (pg. 56 zcu111
 * user guide and pg.3 of clk104 schematic) I0A is one LMX for the ADC, I1A is
 * the LMX for the DAC, I2A is the LMK, the only difference is that zcu216 does
 * not have a second RF ADC LMX PLL, the ZCU111 does, this is I3A for teh ZCU111
 * but the ZCU216 is not connected
 *
 * The HTG board is (check, this was off the top of my head) I0A tile ADC
 * 224/225 LMX, I1A is ADC 226/227, I2A is DAC first two tiles, I3A is second
 * two tiles.
 *
 * It may be possible to define a 'rf pll clocking' struct of sorts similar to
 * i2c that solves this but with setting via i2c and gpio different between
 * zcu216 makes implementation tricky
 *
 * The mux is only used for readback though, so not super critical unless
 * debugging became important
 *
 * htg and zcu111 use hardware gpio expanders (IOX) as to toggle the mux,
 * zcu216/208 use a fabric controlled gpio
 */

#if PLATFORM == ZCU111
  /* zcu111: lmk04208, lmx2594 */ //TODO: confirm these again
  #define LMK4208_SDO_SS        1 /* LMK04208, SS1 on bridge, I2A on mux */
  #define LMX2594_SDO_SS224_225 3 /* ADC LMX2594 for tiles 224/225, SS3 on bridge, I0A on mux */
  #define LMX2594_SDO_SS226_227 2 /* ADC LMX2594 for tiles 226/227, SS2 on bridge, I1A on mux */
  #define LMX2594_SDO_SS228_229 0 /* DAC LMX2594 for DAC tiles, SS0 on bridge I3A on mux */

  // the zcu111 uses an IOX but it is not on a slave mux
  #define IOX_SLV_ADDR 0x20
  #define IOX_CONF_REG 0x07     // Configuration 1 (second set of ports)
  #define IOX_GPIO_REG 0x03     // Output port 1   (second set of ports)
  #define LMK_MUX_SEL             (2 << 1) // all shifted by 1 since connections on iox chip are
  #define LMX_ADC_MUX_SEL_224_225 (0 << 1) // are moved up on the chip by 1 port (second and
  #define LMX_ADC_MUX_SEL_226_227 (1 << 1) // thid bits)
  #define LMX_DAC_MUX_SEL_228_229 (3 << 1)

  #define LMK_REG_CNT 26

  //TODO what is the lmk04208 muxout addr/val (won't matter without readback though)
  #define LMK_MUXOUT_REG_ADDR -1 /* LMK MUXOUT reg. address */
  #define LMK_MUXOUT_REG_VAL  -1 /* LMK MUXOUT reg. value */

#elif PLATFORM == ZCU216
  /* zcu216: lmk04828b, lmx2594 */
  #define LMK_SDO_SS     1 /* LMK PLL,   SS1 on bridge, I2A on mux */
  #define LMX_ADC_SDO_SS 3 /* ADC RFPLL, SS3 on bridge, I0A on mux */ // TODO add tile number to be consistent across platform
  #define LMX_DAC_SDO_SS 2 /* DAC RFPLL, SS2 on bridge, I1A on mux */
  // clk104 does not have a connection on SS0 and I3A on mux

  // For the SDIO mux the it is a two bit mux selecting the SDO line to go back
  // through the SPI bridge so setting S0,S1 to 0b11 (decimal 3) results in
  // selecting the LMK
  #define LMK_MUX_SEL 2     /* LMK04828 */
  #define LMX_ADC_MUX_SEL 0 /* ADC LMX2594 PLL */ //TODO add tile number to be consistent across platform
  #define LMX_DAC_MUX_SEL 1 /* DAC LMX2594 PLL */
  char CLK104_GPIO_MUX_SEL0[4];
  char CLK104_GPIO_MUX_SEL1[4];

  #define LMK_REG_CNT 136 // zcu216/208 (128 (0-127) works, but seems to be a
                          // discrepencey as all tics outputs have 135 values (0-134))? Or have I just been
                          // getting lucky, because it looks like loading the tcs for this from the XILINX
                          // eval gives 135 outputs and programming from
                          // those also seem to lock

  #define LMK_MUXOUT_REG_ADDR 0X15F /* LMK MUXOUT reg. address */
  #define LMK_MUXOUT_REG_VAL  0X3B  /* LMK MUXOUT reg. value */

#elif PLATFORM == ZCU208
  /* zcu208: lmk04828b, lmx2594 */ //TODO: fill out, should be much the same as zcu216
  //
  // clk104 does not have a connection on SS0 and I3A on mux
  //
  #define LMK_REG_CNT 136 // zcu216/208 (128 (0-127) works, but seems to be a
                          // discrepencey as all tics outputs have 135 values (0-135))? Or have I just been
                          // getting lucky, because it looks like loading the tcs for this from the xilinx 
                          // eval gives 136 outputs and programming from
                          // those also seem to lock

  #define LMK_MUXOUT_REG_ADDR 0X15F /* LMK MUXOUT reg. address */
  #define LMK_MUXOUT_REG_VAL  0X3B  /* LMK MUXOUT reg. value */

#elif PLATFORM == ZRF16
  /* zrf16 lmk04832, lmx2594 */
  #define LMK_SDO_SS        0 /* LMK04832 PLL on its own bridge connected to SS0, nothing else on that bridge is connected */
  #define LMX_SDO_SS224_225 0 /* ADC LMX2594 RFPLL for tiles 224/225, SS0 on bridge, I0A on mux */
  #define LMX_SDO_SS226_227 1 /* ADC LMX2594 RFPLL for tiles 226/227, SS1 on bridge, I1A on mux */
  //#define LMX_SDO_SS228_229 /* DAC LMX2594 RFPLL for tiles 228/229, TODO */

  #define IOX_CONF_REG 0x03
  #define IOX_GPIO_REG 0x01
  #define LMK_MUX_SEL             -1 //TODO: readback not supported? check schematic for a wire back
  #define LMX_ADC_MUX_SEL_224_225 0
  #define LMX_ADC_MUX_SEL_226_227 1
  //TODO#define LMX_DAC_MUX_SEL_228_229

  #define LMK_REG_CNT 125

  //TODO what is the lmk04832 muxout addr/val (won't matter without readback though)
  #define LMK_MUXOUT_REG_ADDR -1 /* LMK MUXOUT reg. address */
  #define LMK_MUXOUT_REG_VAL  -1  /* LMK MUXOUT reg. value */

#elif PLATFORM == PYNQ2x2
  /* PYNQ2x2: lmk TODO, lmx2594 */
  // TODO: fill out, On this board there are two LMX2594 one that drives ADC tiles
  // 224/226 and the other that drives DAC tiles 228/229
  #define LMK_SDO_SS        3  /* LMK04832 PLL connected to SS3 on bridge, I3A on mux */
  #define LMX_SDO_SS224_226 0  /* ADC LMX2594 RFPLL for tile 224, SS0 on bridge, I0A on mux */
  #define LMX_SDO_SS228_229 0  /* TODO */

  // TODO: fill out IOX network

#else
  // why does this error not throw?
  #error "PLATFORM NOT CONFIUGURED"
#endif

/* common platform definitions */
#define REG_RW_BIT 0x80      /* the 8th bit of the address section of the LMK/LMX indicates Read/Write to the register */

#define LMX2594_REG_CNT 116      /* {apply rst, remove rst, prgm 113 registers, program R0 a second time} */
#define LMX2594_RST_VAL 0x000002 /* write to R0, assert rst bit */
#define LMX_MUXOUT_REG_ADDR 0x0  /* LMX MUXOUT reg. address (R0) */
#define LMX_MUXOUT_REG_VAL  0x0  /* LMX MUXOUT reg. value */
#define LMX_MUXOUT_LD_SEL   0x4  /* idea here was that instead this would be the bit we toggle on and off to achive readback */

#define SELECT_SPI_SDO(X) (1 << X)

#define RFCLK_SUCCESS 0
#define RFCLK_FAILURE 1

void format_rfclk_pkt(uint8_t sdoselect, uint32_t d, uint8_t* buffer);
uint32_t* readtcs(FILE* tcsfile, uint16_t len, uint8_t pll_type);
int prog_pll(I2CDev dev, uint8_t spi_sdosel, uint32_t* buf, uint16_t len);
//TODO: implement an lmk/lmx readback

#if (PLATFORM == ZCU216) | (PLATFORM == ZCU208)
/* zcu216 or zcu208 for CLK104 */
int set_sdo_mux(int mux_sel);
int init_clk104_gpio(int gpio_id);
#endif

#endif /* ALPACA_RFCLKS_H_ */
