#ifndef __TUYA_LIGHT_I2C_MASTER_H__
#define __TUYA_LIGHT_I2C_MASTER_H__

#include "tuya_adapter_platform.h"


#define ERROR 0
#define SUCCESS 1

enum gpio_level
{
  GPIO_LOW = 0,
  GPIO_HIGH,
};

#define         I2C_SCL_SET(GPIO_NUM)     tuya_light_gpio_ctl(GPIO_NUM,GPIO_HIGH)
#define         I2C_SCL_RESET(GPIO_NUM)     tuya_light_gpio_ctl(GPIO_NUM,GPIO_LOW)

#define         I2C_SDA_SET(GPIO_NUM)           tuya_light_gpio_ctl(GPIO_NUM,GPIO_HIGH)
#define         I2C_SDA_RESET(GPIO_NUM)           tuya_light_gpio_ctl(GPIO_NUM,GPIO_LOW)

#define         I2C_SCL_GPIO_Read(GPIO_NUM)       tuya_light_gpio_read(GPIO_NUM)
#define         I2C_SDA_GPIO_Read(GPIO_NUM)       tuya_light_gpio_read(GPIO_NUM)

#define SM2135_Device_Addr  0xC0

void i2c_gpio_init (uint8_t scl_pin_id, uint8_t sda_pin_id);
uint8_t SM2315_WritePage (uint8_t* pBuffer, uint8_t WriteAddr, uint16_t NumByteToWrite);
uint8_t SM16726B_WritePage (uint8_t* pBuffer, uint16_t NumByteToWrite);


#endif
