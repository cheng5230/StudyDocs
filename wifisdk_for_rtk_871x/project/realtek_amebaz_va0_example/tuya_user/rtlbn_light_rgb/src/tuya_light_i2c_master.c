/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: i2c_master.c
 *
 * Description: i2c master API
 *
 * Modification history:
 *     2014/3/12, v1.0 create this file.
*******************************************************************************/
#include "tuya_light_i2c_master.h"


xSemaphoreHandle i2c_busy;

STATIC uint8_t i2c_scl_pin = I2C_SCL_1;
STATIC uint8_t i2c_sda_pin = I2C_SDA_1;

#define IIC_DELAY 100 //单位时间 us
#define DELAYTIME 2   //

void i2c_gpio_init (uint8_t scl_pin_id, uint8_t sda_pin_id)
{
  i2c_scl_pin = scl_pin_id;
  i2c_sda_pin = sda_pin_id;
  tuya_light_gpio_init (i2c_scl_pin, TUYA_GPIO_OUT);
  tuya_light_gpio_init (i2c_sda_pin, TUYA_GPIO_OUT);
  vSemaphoreCreateBinary (i2c_busy); //创造信号量
}

STATIC void I2C_Delayms (uint32_t time)
{
  volatile uint16_t i;

  while (time --)
  {
    i = IIC_DELAY;

    while (i --);
  }
}
STATIC void Simulate_I2C_Delay (void)
{
  volatile uint16_t j = 10;

  while (j)
  {
    j --;
  }
}
STATIC uint8_t Simulate_I2C_Start (uint8_t SDA_IO, uint8_t SCL_IO)
{
  I2C_SDA_SET (SDA_IO);
  I2C_SCL_SET (SCL_IO);
  Simulate_I2C_Delay();

  if (!I2C_SDA_GPIO_Read (SDA_IO) )
  {
    return ERROR;
  }

  I2C_SDA_RESET (SDA_IO);
  Simulate_I2C_Delay();

  if (I2C_SDA_GPIO_Read (SDA_IO) )
  {
    return ERROR;
  }

  I2C_SDA_RESET (SDA_IO);
  Simulate_I2C_Delay();
  return SUCCESS;
}
STATIC void Simulate_I2C_Stop (uint8_t SDA_IO, uint8_t SCL_IO)
{
  I2C_SCL_RESET (SCL_IO);
  Simulate_I2C_Delay();
  I2C_SDA_RESET (SDA_IO);
  Simulate_I2C_Delay();
  I2C_SCL_SET (SCL_IO);
  Simulate_I2C_Delay();
  I2C_SDA_SET (SDA_IO);
  Simulate_I2C_Delay();
}
STATIC void Simulate_I2C_Ack (uint8_t SDA_IO, uint8_t SCL_IO)
{
  I2C_SCL_RESET (SCL_IO);
  Simulate_I2C_Delay();
  I2C_SDA_RESET (SDA_IO);
  Simulate_I2C_Delay();
  I2C_SCL_SET (SCL_IO);
  Simulate_I2C_Delay();
  I2C_SCL_RESET (SCL_IO);
  Simulate_I2C_Delay();
}

STATIC void Simulate_I2C_No_Ack (uint8_t SDA_IO, uint8_t SCL_IO)
{
  I2C_SCL_RESET (SCL_IO);
  Simulate_I2C_Delay();
  I2C_SDA_SET (SDA_IO);
  Simulate_I2C_Delay();
  I2C_SCL_SET (SCL_IO);
  Simulate_I2C_Delay();
  I2C_SCL_RESET (SCL_IO);
  Simulate_I2C_Delay();
}
STATIC uint8_t Simulate_I2C_Wait_Ack (uint8_t SDA_IO, uint8_t SCL_IO)
{
  I2C_SCL_RESET (SCL_IO);
  Simulate_I2C_Delay();
  I2C_SDA_SET (SDA_IO);
  Simulate_I2C_Delay();
  I2C_SCL_SET (SCL_IO);
  Simulate_I2C_Delay();

  if (I2C_SDA_GPIO_Read (SDA_IO) )
  {
    I2C_SCL_RESET (SCL_IO);
    return ERROR;
  }

  I2C_SCL_RESET (SCL_IO);
  return SUCCESS;
}
STATIC void Simulate_I2C_Send_Byte (uint8_t SDA_IO, uint8_t SCL_IO, uint8_t SendByte)
{
  uint8_t i = 8;

  while (i --)
  {
    I2C_SCL_RESET (SCL_IO);
    Simulate_I2C_Delay();

    if (SendByte & 0x80)
    {
      I2C_SDA_SET (SDA_IO);
    }
    else
    {
      I2C_SDA_RESET (SDA_IO);
    }

    SendByte <<= 1;
    Simulate_I2C_Delay();
    I2C_SCL_SET (SCL_IO);
    Simulate_I2C_Delay();
  }

  I2C_SCL_RESET (SCL_IO);
}

STATIC uint8_t Simulate_I2C_Receive_Byte (uint8_t SDA_IO, uint8_t SCL_IO)
{
  uint8_t i = 8;
  uint8_t ReceiveByte=0;
  I2C_SDA_SET (SDA_IO);
  tuya_light_gpio_init (i2c_sda_pin, TUYA_GPIO_IN);

  while (i --)
  {
    ReceiveByte <<= 1;
    I2C_SCL_RESET (SCL_IO);
    Simulate_I2C_Delay();
    I2C_SCL_SET (SCL_IO);
    Simulate_I2C_Delay();

    if (I2C_SDA_GPIO_Read (SDA_IO) )
    {
      ReceiveByte |= 0x01;
    }
  }

  tuya_light_gpio_init (i2c_sda_pin, TUYA_GPIO_OUT);
  I2C_SCL_RESET (SCL_IO);
  return ReceiveByte;
}

uint8_t SM2315_WritePage (uint8_t* pBuffer, uint8_t WriteAddr, uint16_t NumByteToWrite)
{
  //uint8_t Addr_H = WriteAddr / 0x100;
  //uint8_t Addr_L = WriteAddr % 0x100;
  //PR_DEBUG("I2C WRITE...");
  xSemaphoreTake (i2c_busy, portMAX_DELAY);

  //PR_DEBUG("I2C WRITE START...");
  if (!Simulate_I2C_Start (i2c_sda_pin, i2c_scl_pin) )
  {
    xSemaphoreGive (i2c_busy);
    return ERROR;
  }

  /*  Simulate_I2C_Send_Byte(Device_Addr & 0xFE);

    if(!Simulate_I2C_Wait_Ack())
    {
      Simulate_I2C_Stop();
      xSemaphoreGive(i2c_busy);
      return ERROR;
    }*/
  //Simulate_I2C_Send_Byte(Addr_H);
  //Simulate_I2C_Wait_Ack();
  Simulate_I2C_Send_Byte (i2c_sda_pin, i2c_scl_pin, WriteAddr);

  if (!Simulate_I2C_Wait_Ack (i2c_sda_pin, i2c_scl_pin) )
  {
    Simulate_I2C_Stop (i2c_sda_pin, i2c_scl_pin);
    xSemaphoreGive (i2c_busy);
    return ERROR;
  }

  while (NumByteToWrite --)
  {
    Simulate_I2C_Send_Byte (i2c_sda_pin, i2c_scl_pin, *pBuffer ++);
    Simulate_I2C_Wait_Ack (i2c_sda_pin, i2c_scl_pin);
  }

  Simulate_I2C_Stop (i2c_sda_pin, i2c_scl_pin);
  I2C_Delayms (DELAYTIME);
  //PR_DEBUG("I2C WRITE END...");
  xSemaphoreGive (i2c_busy);
  return SUCCESS;
}

uint8_t SM16726B_WritePage (uint8_t* pBuffer, uint16_t NumByteToWrite)
{
  //uint8_t Addr_H = WriteAddr / 0x100;
  //uint8_t Addr_L = WriteAddr % 0x100;
  //PR_DEBUG("I2C WRITE...");
  xSemaphoreTake (i2c_busy, portMAX_DELAY);
  uint8_t start_bit_cnt = 6;
  //PR_DEBUG("I2C WRITE START...");
  /*  if(!Simulate_I2C_Start(SDA_IO, SCL_IO))
    {
      xSemaphoreGive(i2c_busy);
      return ERROR;
    }
    Simulate_I2C_Send_Byte(Device_Addr & 0xFE);

    if(!Simulate_I2C_Wait_Ack())
    {
      Simulate_I2C_Stop();
      xSemaphoreGive(i2c_busy);
      return ERROR;
    }

    //Simulate_I2C_Send_Byte(Addr_H);
    //Simulate_I2C_Wait_Ack();
    Simulate_I2C_Send_Byte(SDA_IO, SCL_IO, WriteAddr);
    if(!Simulate_I2C_Wait_Ack(SDA_IO, SCL_IO))
    {
      Simulate_I2C_Stop(SDA_IO, SCL_IO);
      xSemaphoreGive(i2c_busy);
      return ERROR;
    }*/

  while (start_bit_cnt--)
  {
    Simulate_I2C_Send_Byte (i2c_sda_pin, i2c_scl_pin, 0x00);
  }

  Simulate_I2C_Send_Byte (i2c_sda_pin, i2c_scl_pin, 0x01);

  while (NumByteToWrite --)
  {
    Simulate_I2C_Send_Byte (i2c_sda_pin, i2c_scl_pin, *pBuffer ++);
  }

  I2C_SDA_SET (i2c_sda_pin);
  Simulate_I2C_Delay();
  I2C_SCL_RESET (i2c_scl_pin);
  Simulate_I2C_Delay();
  I2C_SCL_SET (i2c_scl_pin);
  Simulate_I2C_Delay();
  I2C_SCL_RESET (i2c_scl_pin);
  I2C_Delayms (DELAYTIME);
  //PR_DEBUG("I2C WRITE END...");
  xSemaphoreGive (i2c_busy);
  return SUCCESS;
}


