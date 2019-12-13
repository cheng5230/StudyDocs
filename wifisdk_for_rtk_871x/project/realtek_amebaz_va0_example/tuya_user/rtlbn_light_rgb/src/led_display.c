#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include ".compile_usr_cfg.h"
#include "tuya_light_lib.h"
#include "tuya_light_hard.h"
#include "uf_file.h"
#include "led_display.h"

#include "spi_api.h"
#include "spi_ex_api.h"
#include "gpio_api.h"
#include "device.h"
#include "uni_mutex.h"


uint8_t tx_buf[240];

#ifndef NULL
#define NULL ((void*)(0))
#endif

unsigned int __back_buf_pixel_num = 0;
static char __refresh_bk_buf = 0;
unsigned char __c = 0;
unsigned char __w = 0;

unsigned char __front_buf[DATA_BUF_SIZE];
unsigned char  __back_buf[DATA_BUF_SIZE];
unsigned int __front_buf_len = 0;
unsigned char __buf_send_times = 1;
volatile unsigned char __is_led_driver_busy = 0;
volatile unsigned char ShadeStartFlag = 0;
volatile unsigned char ShadeInvertFlag = 0;
volatile unsigned char ShadeCntsFlag = 0;


void set_cw_pwm(unsigned char c, unsigned char w)
{
	//DEBUG_PRINT("c:%d,w:%d",c, w);
}

int is_driver_busy()
{
    return __is_led_driver_busy;
}

void set_led_driver_buf_send_times(unsigned int times)
{
    __buf_send_times = times;
}

void send_led_driver_dma(unsigned char* buf, unsigned int len)
{
  //__send_data_dma0(buf, len);
  data_upload_flag = TRUE;
}

int display_init(unsigned int pixel_num)
{
  unsigned int send_buf_times = 1;
  if(pixel_num > DISPLAY_BUF_MAX_PIXEL)
  {
    //pixel_num must be multiple of DISPLAY_BUF_MAX_PIXEL.
    pixel_num = (pixel_num + (DISPLAY_BUF_MAX_PIXEL - 1)) / DISPLAY_BUF_MAX_PIXEL * DISPLAY_BUF_MAX_PIXEL;
    __back_buf_pixel_num = DISPLAY_BUF_MAX_PIXEL;
    send_buf_times = pixel_num / DISPLAY_BUF_MAX_PIXEL;
    
  }
  else
  {
      //multiple of 4 pixels.  
      pixel_num = ((pixel_num+3) >> 2) << 2;
      __back_buf_pixel_num = pixel_num;
  }
  
    //frontground buffer.
    #if LIGHT_WS2812_RGBC_BALL
	  __front_buf_len = __back_buf_pixel_num * (BYTES_1_PIXEL_FT_BUF+BYTES_1_PIXEL_FT_BUF);
	  #else
    __front_buf_len = __back_buf_pixel_num * BYTES_1_PIXEL_FT_BUF;
	  #endif

    set_led_driver_buf_send_times(send_buf_times);

    return 0;
}

unsigned int get_display_buf_pixel_num()
{
    return __back_buf_pixel_num;
}


BRIGHT_DATA_S get_current_display_data(unsigned char index)
{
    BRIGHT_DATA_S data;
    #if LIGHT_WS2812_RGBC_BALL
     data.green = 255-(( __back_buf[0]* COLORDIMMER) >> 8);
  	 data.red =255-((__back_buf[1]* COLORDIMMER) >> 8);
  	 data.blue = 255-((__back_buf[2]* COLORDIMMER) >> 8);
     data.white= 0;
     data.warm = 0;
    #else
    data.green = __back_buf[0];
    data.red = __back_buf[1];
    data.blue = __back_buf[2];
    data.white = __c;
    data.warm = __w;
    #endif
    //DEBUG_PRINT("get------- red:%d green:%d blue :%d \n",__back_buf[1],__back_buf[0],__back_buf[2]);
    return data;
}

static int inline __update_display_4pixel_algin(unsigned char r, unsigned char g, unsigned char b, unsigned char* buf, unsigned int pixel_num)
{
	  unsigned int i = 0;
    if(pixel_num == 0)
    {
        return 0;
    }
    if(pixel_num % 4 != 0)
    {
        DEBUG_PRINT("error ret -4\n");
        return -1;
    }
    
		pixel_num = pixel_num*3;
    for(;i<pixel_num; i+=3)
    {
      buf[i+0] = g;
      buf[i+1] = r;
      buf[i+2] = b;
    }
    return 0;
}

static void inline __update_display_one_pixel(unsigned char r, unsigned char g, unsigned char b, unsigned char** buf)
{
    *(*buf)++ = g;
    *(*buf)++ = r;
    *(*buf)++ = b;
}

int update_display_one_pixel(unsigned char r, unsigned char g, unsigned char b, unsigned char c, unsigned char w, unsigned int offset)
{
    unsigned char* buf = NULL;
    //DEBUG_PRINT("offset:%d,r:%d,g:%d,b:%d,c:%d,w:%d\n", offset, r, g, b, c, w);
    if(offset >= DISPLAY_BUF_MAX_PIXEL)
    {
        //DEBUG_PRINT("error ret -1\n");
        return -1;
    }
    
    #if LIGHT_WS2812_RGBC_BALL
     r = 255-((r * COLORDIMMER) >> 8);
  	 g =255-((g * COLORDIMMER) >> 8);
  	 b = 255-((b * COLORDIMMER) >> 8);
     __c= 255-((c* COLORDIMMER) >> 8);
     __w = w;
     #else
     __c = c;
     __w = w;
     #endif
    buf = __back_buf+offset*BYTES_1_PIXEL_BK_BUF;
    __update_display_one_pixel(r, g, b, &buf);
    __refresh_bk_buf = 1;
    return 0;
}

int update_display_data(unsigned char r, unsigned char g, unsigned char b, unsigned char c, unsigned char w, unsigned int offset, unsigned int pixel_num)
{
    unsigned char* buf = NULL;
    //DEBUG_PRINT("tick:%d, offset:%d, pixel_num=%d,r:%d,g:%d,b:%d,c:%d,w:%d\n", __system_ticks, offset, pixel_num, r, g, b, c, w);
    if(offset+pixel_num > DISPLAY_BUF_MAX_PIXEL)
    {
        DEBUG_PRINT("error ret -0\n");
        return -1;
    }
    buf = __back_buf+offset*BYTES_1_PIXEL_BK_BUF;
    unsigned int pixel4_num = (pixel_num >> 2)<<2; 
    if(__update_display_4pixel_algin(r, g, b, buf, pixel4_num)!=0)
    {
        //DEBUG_PRINT("error ret -2\n");
        return -1;
    }
    buf+=pixel4_num*BYTES_1_PIXEL_BK_BUF;
    unsigned int rest_pixel_num = pixel_num - pixel4_num;
    unsigned int i = 0;
    for(;i<rest_pixel_num; ++i )
    {
        __update_display_one_pixel(r, g, b, &buf);
    }
    __c = c;
    __w = w;
    __refresh_bk_buf = 1;    
    return 0;
}

int update_display_data_all(unsigned char r, unsigned char g, unsigned char b, unsigned char c, unsigned char w)
{
	  //DEBUG_PRINT("r:%d,g:%d,b:%d,c:%d,w:%d\n", r, g, b, c, w);
	   #if LIGHT_WS2812_RGBC_BALL
     r = 255-((r * COLORDIMMER) >> 8);
  	 g =255-((g * COLORDIMMER) >> 8);
  	 b = 255-((b * COLORDIMMER) >> 8);
     __c= 255-((c* COLORDIMMER) >> 8);
     __w = w;
     #else
      __c = c;
      __w = w;
     #endif
      
    if(__update_display_4pixel_algin(r, g, b, __back_buf, __back_buf_pixel_num)!=0)
    {
        //DEBUG_PRINT("error ret -3\n");
        return -1;
    }
    __refresh_bk_buf = 1;
    return 0;
}


void default_set_light_shade_data( unsigned char index, unsigned char r, unsigned char g, unsigned char b, unsigned char c, unsigned char w)
{
    //DEBUG_PRINT("index:%d,r:%d,g:%d,b:%d,c:%d,w:%d num:%d\n", index, r, g, b, c, w,__scene_cnt);
    update_display_data_all(r, g, b, c, w);
}

void SetBufData(void)
{
  unsigned int i,j,k = 0;
  for(i=0, j=0;i<DISPLAY_BUF_MAX_PIXEL;++i, ++j)  
  {
    #if RGB_4BITS
      #if LIGHT_DEFALT_RGB_or_GRB
        #define FT_INDEX  (j*BYTES_1_PIXEL_FT_BUF)
        #define BK_INDEX  (i*BYTES_1_PIXEL_BK_BUF)
        //R
        __front_buf[FT_INDEX]   =  ((__back_buf[BK_INDEX+2]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX]   |= ((__back_buf[BK_INDEX+2]&0x40)==0x40)? L_BIT_1 : L_BIT_0;
        __front_buf[FT_INDEX+1] =  ((__back_buf[BK_INDEX+2]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+1] |= ((__back_buf[BK_INDEX+2]&0x10)==0x10)? L_BIT_1 : L_BIT_0;
        __front_buf[FT_INDEX+2] =  ((__back_buf[BK_INDEX+2]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+2] |= ((__back_buf[BK_INDEX+2]&0x04)==0x04)? L_BIT_1 : L_BIT_0;
        __front_buf[FT_INDEX+3] =  ((__back_buf[BK_INDEX+2]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+3] |= ((__back_buf[BK_INDEX+2]&0x01)==0x01)? L_BIT_1 : L_BIT_0;
        //B
        __front_buf[FT_INDEX+4] =  ((__back_buf[BK_INDEX+1]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+4] |= ((__back_buf[BK_INDEX+1]&0x40)==0x40)? L_BIT_1 : L_BIT_0;
        __front_buf[FT_INDEX+5] =  ((__back_buf[BK_INDEX+1]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+5] |= ((__back_buf[BK_INDEX+1]&0x10)==0x10)? L_BIT_1 : L_BIT_0;
        __front_buf[FT_INDEX+6] =  ((__back_buf[BK_INDEX+1]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+6] |= ((__back_buf[BK_INDEX+1]&0x04)==0x04)? L_BIT_1 : L_BIT_0;
        __front_buf[FT_INDEX+7] =  ((__back_buf[BK_INDEX+1]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+7] |= ((__back_buf[BK_INDEX+1]&0x01)==0x01)? L_BIT_1 : L_BIT_0;
        //g
        __front_buf[FT_INDEX+8] =  ((__back_buf[BK_INDEX+0]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+8] |= ((__back_buf[BK_INDEX+0]&0x40)==0x40)? L_BIT_1 : L_BIT_0;
        __front_buf[FT_INDEX+9] =  ((__back_buf[BK_INDEX+0]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+9] |= ((__back_buf[BK_INDEX+0]&0x10)==0x10)? L_BIT_1 : L_BIT_0;
        __front_buf[FT_INDEX+10] =  ((__back_buf[BK_INDEX+0]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+10] |= ((__back_buf[BK_INDEX+0]&0x04)==0x04)? L_BIT_1 : L_BIT_0;
        __front_buf[FT_INDEX+11] =  ((__back_buf[BK_INDEX+0]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
        __front_buf[FT_INDEX+11] |= ((__back_buf[BK_INDEX+0]&0x01)==0x01)? L_BIT_1 : L_BIT_0; 
      #else
        #if LIGHT_WS2812_RGBC_BALL
        #define FT_INDEX  (j*(BYTES_1_PIXEL_FT_BUF+BYTES_1_PIXEL_FT_BUF))
        #define BK_INDEX  (i*BYTES_1_PIXEL_BK_BUF)
          //G
          __front_buf[FT_INDEX]   =  ((__back_buf[BK_INDEX]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX]   |= ((__back_buf[BK_INDEX]&0x40)==0x40)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+1] =  ((__back_buf[BK_INDEX]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+1] |= ((__back_buf[BK_INDEX]&0x10)==0x10)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+2] =  ((__back_buf[BK_INDEX]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+2] |= ((__back_buf[BK_INDEX]&0x04)==0x04)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+3] =  ((__back_buf[BK_INDEX]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+3] |= ((__back_buf[BK_INDEX]&0x01)==0x01)? L_BIT_1 : L_BIT_0;
          //R
          __front_buf[FT_INDEX+4] =  ((__back_buf[BK_INDEX+1]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+4] |= ((__back_buf[BK_INDEX+1]&0x40)==0x40)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+5] =  ((__back_buf[BK_INDEX+1]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+5] |= ((__back_buf[BK_INDEX+1]&0x10)==0x10)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+6] =  ((__back_buf[BK_INDEX+1]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+6] |= ((__back_buf[BK_INDEX+1]&0x04)==0x04)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+7] =  ((__back_buf[BK_INDEX+1]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+7] |= ((__back_buf[BK_INDEX+1]&0x01)==0x01)? L_BIT_1 : L_BIT_0;
          //B
          __front_buf[FT_INDEX+8] =  ((__back_buf[BK_INDEX+2]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+8] |= ((__back_buf[BK_INDEX+2]&0x40)==0x40)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+9] =  ((__back_buf[BK_INDEX+2]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+9] |= ((__back_buf[BK_INDEX+2]&0x10)==0x10)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+10] =  ((__back_buf[BK_INDEX+2]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+10] |= ((__back_buf[BK_INDEX+2]&0x04)==0x04)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+11] =  ((__back_buf[BK_INDEX+2]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+11] |= ((__back_buf[BK_INDEX+2]&0x01)==0x01)? L_BIT_1 : L_BIT_0;  
      	  //w
          __front_buf[FT_INDEX+12]   =  ((__c&0x80)==0x80)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+12]   |= ((__c&0x40)==0x40)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+13] =  ((__c&0x20)==0x20)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+13] |= ((__c&0x10)==0x10)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+14] =  ((__c&0x08)==0x08)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+14] |= ((__c&0x04)==0x04)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+15] =  ((__c&0x02)==0x02)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+15] |= ((__c&0x01)==0x01)? L_BIT_1 : L_BIT_0;

          //0 -- 0xff
          __front_buf[FT_INDEX+16] =  H_BIT_1 ;
          __front_buf[FT_INDEX+16] |= L_BIT_1 ;
          __front_buf[FT_INDEX+17] =  H_BIT_1 ;
          __front_buf[FT_INDEX+17] |= L_BIT_1 ;
          __front_buf[FT_INDEX+18] =  H_BIT_1 ;
          __front_buf[FT_INDEX+18] |= L_BIT_1 ;
          __front_buf[FT_INDEX+19] =  H_BIT_1 ;
          __front_buf[FT_INDEX+19] |= L_BIT_1 ;
          //0 - 0xff
          __front_buf[FT_INDEX+20] =  H_BIT_1 ;
          __front_buf[FT_INDEX+20] |= L_BIT_1 ;
          __front_buf[FT_INDEX+21] =  H_BIT_1 ;
          __front_buf[FT_INDEX+21] |= L_BIT_1 ;
          __front_buf[FT_INDEX+22] =  H_BIT_1 ;
          __front_buf[FT_INDEX+22] |= L_BIT_1 ;
          __front_buf[FT_INDEX+23] =  H_BIT_1 ;
          __front_buf[FT_INDEX+23] |= L_BIT_1 ; 
          
          #if 0
          for(k=0;k<3;k++)
          {
            __front_buf[12+24*k]   =  ((__c&0x80)==0x80)? H_BIT_1 : H_BIT_0;
            __front_buf[12+24*k]   |= ((__c&0x40)==0x40)? L_BIT_1 : L_BIT_0;
            __front_buf[13+24*k] =  ((__c&0x20)==0x20)? H_BIT_1 : H_BIT_0;
            __front_buf[13+24*k] |= ((__c&0x10)==0x10)? L_BIT_1 : L_BIT_0;
            __front_buf[14+24*k] =  ((__c&0x08)==0x08)? H_BIT_1 : H_BIT_0;
            __front_buf[14+24*k] |= ((__c&0x04)==0x04)? L_BIT_1 : L_BIT_0;
            __front_buf[15+24*k] =  ((__c&0x02)==0x02)? H_BIT_1 : H_BIT_0;
            __front_buf[15+24*k] |= ((__c&0x01)==0x01)? L_BIT_1 : L_BIT_0;
          }
          #endif
        #else
        #define FT_INDEX  (j*BYTES_1_PIXEL_FT_BUF)
        #define BK_INDEX  (i*BYTES_1_PIXEL_BK_BUF)
          //G
          __front_buf[FT_INDEX]   =  ((__back_buf[BK_INDEX+0]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX]   |= ((__back_buf[BK_INDEX+0]&0x40)==0x40)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+1] =  ((__back_buf[BK_INDEX+0]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+1] |= ((__back_buf[BK_INDEX+0]&0x10)==0x10)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+2] =  ((__back_buf[BK_INDEX+0]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+2] |= ((__back_buf[BK_INDEX+0]&0x04)==0x04)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+3] =  ((__back_buf[BK_INDEX+0]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+3] |= ((__back_buf[BK_INDEX+0]&0x01)==0x01)? L_BIT_1 : L_BIT_0;
          //B
          __front_buf[FT_INDEX+4] =  ((__back_buf[BK_INDEX+1]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+4] |= ((__back_buf[BK_INDEX+1]&0x40)==0x40)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+5] =  ((__back_buf[BK_INDEX+1]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+5] |= ((__back_buf[BK_INDEX+1]&0x10)==0x10)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+6] =  ((__back_buf[BK_INDEX+1]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+6] |= ((__back_buf[BK_INDEX+1]&0x04)==0x04)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+7] =  ((__back_buf[BK_INDEX+1]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+7] |= ((__back_buf[BK_INDEX+1]&0x01)==0x01)? L_BIT_1 : L_BIT_0;
          //R
          __front_buf[FT_INDEX+8] =  ((__back_buf[BK_INDEX+2]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+8] |= ((__back_buf[BK_INDEX+2]&0x40)==0x40)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+9] =  ((__back_buf[BK_INDEX+2]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+9] |= ((__back_buf[BK_INDEX+2]&0x10)==0x10)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+10] =  ((__back_buf[BK_INDEX+2]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+10] |= ((__back_buf[BK_INDEX+2]&0x04)==0x04)? L_BIT_1 : L_BIT_0;
          __front_buf[FT_INDEX+11] =  ((__back_buf[BK_INDEX+2]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
          __front_buf[FT_INDEX+11] |= ((__back_buf[BK_INDEX+2]&0x01)==0x01)? L_BIT_1 : L_BIT_0;    
        #endif
     #endif
    #else
    #define FT_INDEX  (j*BYTES_1_PIXEL_FT_BUF)
    #define BK_INDEX  (i*BYTES_1_PIXEL_BK_BUF)
     //G
    __front_buf[FT_INDEX]   =  ((__back_buf[BK_INDEX]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+1]   = ((__back_buf[BK_INDEX]&0x40)==0x40)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+2] =  ((__back_buf[BK_INDEX]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+3] = ((__back_buf[BK_INDEX]&0x10)==0x10)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+4] =  ((__back_buf[BK_INDEX]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+5] = ((__back_buf[BK_INDEX]&0x04)==0x04)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+6] =  ((__back_buf[BK_INDEX]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+7] = ((__back_buf[BK_INDEX]&0x01)==0x01)? H_BIT_1 : H_BIT_0;
    //R
    __front_buf[FT_INDEX+8] =  ((__back_buf[BK_INDEX+1]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+9] = ((__back_buf[BK_INDEX+1]&0x40)==0x40)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+10] =  ((__back_buf[BK_INDEX+1]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+11] = ((__back_buf[BK_INDEX+1]&0x10)==0x10)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+12] =  ((__back_buf[BK_INDEX+1]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+13] = ((__back_buf[BK_INDEX+1]&0x04)==0x04)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+14] =  ((__back_buf[BK_INDEX+1]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+15] = ((__back_buf[BK_INDEX+1]&0x01)==0x01)? H_BIT_1 : H_BIT_0;
    //B
    __front_buf[FT_INDEX+16] =  ((__back_buf[BK_INDEX+2]&0x80)==0x80)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+17] = ((__back_buf[BK_INDEX+2]&0x40)==0x40)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+18] =  ((__back_buf[BK_INDEX+2]&0x20)==0x20)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+19] = ((__back_buf[BK_INDEX+2]&0x10)==0x10)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+20]=  ((__back_buf[BK_INDEX+2]&0x08)==0x08)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+21]= ((__back_buf[BK_INDEX+2]&0x04)==0x04)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+22]=  ((__back_buf[BK_INDEX+2]&0x02)==0x02)? H_BIT_1 : H_BIT_0;
    __front_buf[FT_INDEX+23]= ((__back_buf[BK_INDEX+2]&0x01)==0x01)? H_BIT_1 : H_BIT_0; 
    #endif
  }
}

static int __back_buf_refresh()
{
  //unsigned int i,j = 0;
  if(is_driver_busy() == 1)
  {
    //DEBUG_PRINT("error ret -5\n");
    return -1;
  }
  SetBufData();
  send_led_driver_dma(__front_buf, __front_buf_len);
  return 0;
}

void display_service(void)
{
  if(__refresh_bk_buf == 0)
  {
      return;
  }
  //DEBUG_PRINT(" 02 \n");
  if(__back_buf_refresh( ) == 0)
  {
      set_cw_pwm(__c, __w);
      __refresh_bk_buf = 0;
  }
}

static int __total_malloc_size = 0;
void* user_malloc(char* func, unsigned int size)
{
    void* temp_addr = (void*)malloc(size+4);
    if(temp_addr != NULL)
    {
        __total_malloc_size+=size;
        DEBUG_PRINT("%s malloc %d addr=%x __total_malloc_size=%d\r\n",func, size, (unsigned int)((unsigned char*)temp_addr+4),  __total_malloc_size);
        *((unsigned int*)temp_addr) = size;
        return (void*)((unsigned char*)temp_addr+4);
    }
  	DEBUG_PRINT("%s malloc %d fail\r\n",func, size);
    return NULL;
}

void user_free(char* func, void* addr)
{
    if(addr == NULL)
    {
  		DEBUG_PRINT("free NULL addr fail\n");
      return;
    }
    void* temp_addr = (void*)((unsigned char*)addr - 4);
    unsigned int size = *((unsigned int*)temp_addr);
    
    __total_malloc_size-= size;
	  DEBUG_PRINT("%s free %d addr=%x __total_malloc_size=%d\n",func, size, (unsigned int)addr, __total_malloc_size);
    free(temp_addr);
}

static inline short __abs1(short value)
{
	return value > 0 ? value : -value;
}


static inline unsigned char __calc_next_color_value(LIGHT_SHADE_CTRL_T* ctrl, unsigned short step, unsigned short step_fun, unsigned short step_dir, unsigned short step_neg, unsigned char f_value)
{   
    short delt=0;
	  delt = (ctrl->step_num * step) >> FIX_RADIX;
    //DEBUG_PRINT("%s, step=%d delt:%d, ctrl->step_num=%d, step_dir=%x, step_neg=%x,step_funs=%x\n", __FUNCTION__, step, delt, ctrl->step_num, step_dir, step_neg,step_fun);
    if((step_fun & step_neg) == step_neg)
    {
        //step is intevel.
        delt = (ctrl->step_num << FIX_RADIX) / step;
        //DEBUG_PRINT("%s, intevel delt:%d\n", __FUNCTION__, delt);
    }
    if((step_fun & step_dir) == step_dir)
    {
        delt = 0- delt;
        //DEBUG_PRINT("%s, neg delt:%d\n", __FUNCTION__, delt);
    }
    //DEBUG_PRINT("%s, f_value:%d - delt:%d = %d\n", __FUNCTION__, f_value, delt, f_value -delt);
    return (f_value -delt);
}

static inline void __calc_one_color_step(LIGHT_SHADE_CTRL_T* ctrl, short delt, unsigned short* step, unsigned short dir_neg, unsigned short step_intvel)
{
    if(delt == 0)
    {
        *step = 0;
        return;
    }
    *step = (__abs1(delt) << FIX_RADIX) / ctrl->step_num;    
    if(delt<0)
    {
        ctrl->step_fun |=  dir_neg;
    }
    if(__abs1(delt) < ctrl->step_num)
    {
        ctrl->step_fun |=  step_intvel;
        *step = (ctrl->step_num << FIX_RADIX) / __abs1(delt);
    }
}

static inline void __calc_step(LIGHT_SHADE_CTRL_T* ctrl, BRIGHT_DATA_S* curr)
{
    unsigned short max_delt = 0;
    short r_delt = 0;
    short g_delt = 0;
    short b_delt = 0;
    short c_delt = 0;
    short w_delt = 0;

    //DEBUG_PRINT("fin-------r_delt=%d g_delt:%d, b_delt:%d, c_delt:%d, w_delt:%d\n", ctrl->f_r, ctrl->f_g, ctrl->f_b, ctrl->f_c, ctrl->f_w);
    //DEBUG_PRINT("cur-------r_delt=%d g_delt:%d, b_delt:%d, c_delt:%d, w_delt:%d\n", curr->red, curr->green, curr->blue, curr->white, curr->warm);
    r_delt = ctrl->f_r - curr->red;
    max_delt = __abs1(r_delt);
    g_delt = ctrl->f_g - curr->green;
    max_delt = (max_delt < __abs1(g_delt))? __abs1(g_delt) : max_delt;
    b_delt = ctrl->f_b - curr->blue;
    max_delt = (max_delt < __abs1(b_delt))? __abs1(b_delt) : max_delt;
    c_delt = ctrl->f_c - curr->white;
    max_delt = (max_delt < __abs1(c_delt))? __abs1(c_delt) : max_delt;  
    w_delt = ctrl->f_w - curr->warm;
    max_delt = (max_delt < __abs1(w_delt))? __abs1(w_delt) : max_delt;
    
    if(ctrl->step_num == 0)
    {
        ctrl->step_num = (unsigned char)max_delt;
        if(max_delt>MAX_STEP_NUM)
        {
            ctrl->step_num = MAX_STEP_NUM;
        }
        //DEBUG_PRINT("ajusted step_num:%d\n", ctrl->step_num);
    }
    ctrl->step_fun = 0x0000;
    
    //DEBUG_PRINT("r_delt=%d g_delt:%d, b_delt:%d, c_delt:%d, w_delt:%d, max_delt:%d\n", r_delt, g_delt, b_delt, c_delt, w_delt, max_delt);
    __calc_one_color_step(ctrl, r_delt, &(ctrl->r_step), R_DIR_NEG, R_STEP_ITEVL);
    __calc_one_color_step(ctrl, g_delt, &(ctrl->g_step), G_DIR_NEG, G_STEP_ITEVL);
    __calc_one_color_step(ctrl, b_delt, &(ctrl->b_step), B_DIR_NEG, B_STEP_ITEVL);
    __calc_one_color_step(ctrl, c_delt, &(ctrl->c_step), C_DIR_NEG, C_STEP_ITEVL);
    __calc_one_color_step(ctrl, w_delt, &(ctrl->w_step), W_DIR_NEG, W_STEP_ITEVL);
    //DEBUG_PRINT("r_step=%d g_step:%d, b_step:%d, c_step:%d, w_step:%d\n", ctrl->r_step, ctrl->g_step, ctrl->b_step, ctrl->c_step, ctrl->w_step);
}


int __light_shade_start(unsigned char index, unsigned int step_time_us, unsigned int step_num, BRIGHT_DATA_S curr, BRIGHT_DATA_S* fin) 
{  
  LIGHT_SHADE_CTRL_T* ctrl = __light_shade_ctrl[index];
  unsigned char r, g, b, c, w;
  unsigned char timer_id = 0;
  if(ShadeStartFlag==0)
  {
      /***************** 初始化获取      *****************************************/
      ShadeStartFlag = 1;
      if(index >= MAX_SHADE_NUM)
      {
        return -1;
      }
      if(ctrl == NULL)
      {
        ctrl = MALLOC(sizeof(LIGHT_SHADE_CTRL_T));
        if(ctrl == NULL)
        {
          return -1;
        }
        __light_shade_ctrl[index] = ctrl;
        ctrl->shade_index = index;
        timer_id = 0xFF;
      }
      ctrl->f_r = fin->red;
      ctrl->f_g = fin->green;
      ctrl->f_b = fin->blue;
      ctrl->f_c = fin->white;
      ctrl->f_w = fin->warm; 
      ctrl->step_num = step_num;
      
      if(step_num > MAX_STEP_NUM)
      {
        ctrl->step_num = MAX_STEP_NUM;
        //ajust step_time_us.
        step_time_us = (step_num * step_time_us + MAX_STEP_NUM - 1) / MAX_STEP_NUM;
      }
      //DEBUG_PRINT("ajusted step_time_us:%d, step_num:%d\n", step_time_us, ctrl->step_num);
      if(step_time_us < MIN_SHADE_STEP_US)
      step_time_us = MIN_SHADE_STEP_US;
      //
      step_time_us -= SHADE_THREAD_TIMES_US;
      //DEBUG_PRINT("ajusted step_time_us:%d\n", step_time_us);

      __calc_step(ctrl, &curr);
  }
  else
  {
      /***************** 呼吸渐变       *****************************************/
      --(ctrl->step_num);
      if(ctrl->step_num == 0)
      {
          ShadeStartFlag = 0;
          if(ShadeInvertFlag)
          {
            ShadeCntsFlag = 1;
            ShadeInvertFlag = 0;
            __scene_cnt++;  
          }
          else
          {
             ShadeInvertFlag = 1;
          }
      }
      r = __calc_next_color_value(ctrl, ctrl->r_step, ctrl->step_fun&R_MASK, R_DIR_NEG, R_STEP_ITEVL, ctrl->f_r);
      g = __calc_next_color_value(ctrl, ctrl->g_step, ctrl->step_fun&G_MASK, G_DIR_NEG, G_STEP_ITEVL, ctrl->f_g);
      b = __calc_next_color_value(ctrl, ctrl->b_step, ctrl->step_fun&B_MASK, B_DIR_NEG, B_STEP_ITEVL, ctrl->f_b);
      c = __calc_next_color_value(ctrl, ctrl->c_step, ctrl->step_fun&C_MASK, C_DIR_NEG, C_STEP_ITEVL, ctrl->f_c);
      w = __calc_next_color_value(ctrl, ctrl->w_step, ctrl->step_fun&W_MASK, W_DIR_NEG, W_STEP_ITEVL, ctrl->f_w);
      //DEBUG_PRINT("c:%d-%d-%d-%d-%d f:%d-%d-%d-%d-%d, us:%d-%d-%d-%d-%d\n", curr.red, curr.green, curr.blue, curr.white, curr.warm, r, g, b, c,w, ctrl->f_r, ctrl->f_g,ctrl->f_b,ctrl->f_c,ctrl->f_w);
      default_set_light_shade_data(ctrl->shade_index, r, g, b, c, w);
  }
  return 0;    
}

int light_shade_start_total(unsigned char index, unsigned int time_us, BRIGHT_DATA_S* fin) 
{
    if(fin == NULL)
    {
        return -1;
    }
    
    return __light_shade_start(index, MIN_SHADE_STEP_US, ((time_us + MIN_SHADE_STEP_US - 1)/MIN_SHADE_STEP_US), get_current_display_data(index), fin);
}

