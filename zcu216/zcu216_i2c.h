#define I2C_DEVICES_MAP
#ifdef ZCU216 \
  X(I2C_DEV_EEPROM,  { "/dev/i2c-6" , 0x74, (1 << 0), 0x54, -1 }) \
  X(I2C_DEV_8A34001, { "/dev/i2c-10", 0x74, (1 << 4), 0x5b, -1 })
#endif

/********************************************************************/

/* With device struct
 *
 * The following works, but do I like the DEVICE_STRUCT definition or is it too
 * verbose
 */

// alpaca_i2c_utils.c
#define X(name, dev) dev,
static I2CSlave i2c_devs[] = { I2C_DEVICES_MAP };
#undef X

// alpaca_i2c_utils.h
#define DEVICE_STRUCT(dp, ma, ms, sa, fd) {dp, ma, ms, sa, fd}

#define I2C_DEVICES_MAP \
  X(I2C_DEV_EEPROM,  DEVICE_STRUCT("/dev/i2c-6" , 0x74, (1 << 0), 0x54, -1 )) \
  X(I2C_DEV_8A34001, DEVICE_STRUCT("/dev/i2c-10", 0x74, (1 << 4), 0x5b, -1 ))

#define X(name, dev) name,
typedef enum dev { I2C_DEVICES_MAP } I2CDev;
#undef X

// no device struct

// alpaca_i2c_utils.c
//#define X(n, p, b, s, a, f) {p, b, s, a, f},
static I2CSlave i2c_devs[] = { I2C_DEVICES_MAP };
#undef X

//alpaca_i2c_utils.h
#define I2C_DEVICES_MAP \
  X(I2C_DEV_EEPROM,  "/dev/i2c-6" , 0x74, (1 << 0), 0x54, -1 ) \
  X(I2C_DEV_8A34001, "/dev/i2c-10", 0x74, (1 << 4), 0x5b, -1 )

#define X(n, p, b, s, a, f) n,
typedef enum dev { I2C_DEVICES_MAP } I2CDev;
#undef X
