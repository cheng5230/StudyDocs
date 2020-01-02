#ifndef MBED_SPI_EXT_API_H
#define MBED_SPI_EXT_API_H

#define PWM_1            _PA_23 
#define WS2812_GPIO      _PA_23 

#define USE_GPIO        0
#define USE_SPI         1

// SPI1 (S2)
#define SPI1_MOSI  PA_23
#define SPI1_MISO  PA_22
#define SPI1_SCLK  PA_18
#define SPI1_CS    PA_19
#define SHOW_R  0x1
#define SHOW_G  0x2
#define SHOW_B  0X3
#define SHOW_W  0x4

#define SPI1_COLCK 10000000
#define PIXEL_MAX  100      //最大 150 

void WS2812b_Init(void);
void WS2812B_Test(void);

#endif

