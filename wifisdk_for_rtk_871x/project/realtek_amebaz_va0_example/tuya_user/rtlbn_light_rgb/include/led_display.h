#ifndef __LED_DISPLAY_H_
#define __LED_DISPLAY_H_

#include "tuya_adapter_platform.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct 
{
    unsigned char f_r;
    unsigned char f_g;
    unsigned char f_b;
    unsigned char f_c;

    unsigned char f_w;
    unsigned char step_num;
    unsigned short r_step;    

    unsigned short g_step;
    unsigned short b_step;

    unsigned short c_step;
    unsigned short w_step;

    //b0: r dir, 0:+, 1:-
    //b1: r step: 1 step=1, r_step ref to step intevel, 0: ref r_step.  
    //b2: g dir, 0:+, 1:-
    //b3: g step: 1 step=1, g_step ref to step intevel, 0: ref g_step.  
    //b4: b dir, 0:+, 1:-
    //b5: b step: 1 step=1, b_step ref to step intevel, 0: ref b_step.  
    //b6: c dir, 0:+, 1:-
    //b7: c step: 1 step=1, c_step ref to step intevel, 0: ref c_step.  
    //b8: w dir, 0:+, 1:-
    //b9: w step: 1 step=1, w_step ref to step intevel, 0: ref w_step.  
    unsigned short step_fun;
    unsigned char  timer_id;
    unsigned char  shade_index;
}LIGHT_SHADE_CTRL_T;



#define RGB_4BITS 1

/******* RGB 8位表示方法      ******/
//#define H_BIT_1 0xfc  
//#define H_BIT_0 0xc0  
//#define BYTES_1_PIXEL_BK_BUF 3
//#define BYTES_1_PIXEL_FT_BUF (BYTES_1_PIXEL_BK_BUF * 8)
//#define DATA_BUF_PIEXL        24
//#define DEFAULT_LED_NUM       60
//#define DISPLAY_BUF_MAX_PIXEL 60
//#define DISPLAY_DIMMER        60
//#define DATA_BUF_SIZE         60*DATA_BUF_PIEXL

/******* RGB 4位表示方法      ******/
#define BYTES_1_PIXEL_BK_BUF  3
#define BYTES_1_PIXEL_FT_BUF  (BYTES_1_PIXEL_BK_BUF * 4)
#define DATA_BUF_PIEXL        12
#define COLORDIMMER           200
#define GROUP_PIXEL_NUM       6

#if LIGHT_SK2812_config        // 5V灯带
#define DISPLAY_BUF_MAX_PIXEL 52*3
#define DATA_BUF_SIZE         DISPLAY_BUF_MAX_PIXEL*DATA_BUF_PIEXL
#define MARQUEE_MAXNUM        DISPLAY_BUF_MAX_PIXEL/3
#define DISPLAY_FIRINGNUMS    60
#define H_BIT_1 0xc0  
#define L_BIT_1 0x0c  
#define H_BIT_0 0x80  
#define L_BIT_0 0x08  
#else
  #if LIGHT_DEFALT_RGB_or_GRB    // 12V 灯带宏定义
  #define  DISPLAY_BUF_MAX_PIXEL 52
  #define  DATA_BUF_SIZE         DISPLAY_BUF_MAX_PIXEL*DATA_BUF_PIEXL
  #define  MARQUEE_MAXNUM        DISPLAY_BUF_MAX_PIXEL
  #define DISPLAY_FIRINGNUMS     20
  #else
    #if LIGHT_WS2812_RGBC_BALL   // 12V 球泡灯带
    #define  DISPLAY_BUF_MAX_PIXEL 32
    #define  DATA_BUF_SIZE         DISPLAY_BUF_MAX_PIXEL*(DATA_BUF_PIEXL+DATA_BUF_PIEXL)
    #define  MARQUEE_MAXNUM        DISPLAY_BUF_MAX_PIXEL
    #define  DISPLAY_FIRINGNUMS    60
    #else                         // 5V灯带
    #define DISPLAY_BUF_MAX_PIXEL 36 //52*3
    #define  DATA_BUF_SIZE        DISPLAY_BUF_MAX_PIXEL*DATA_BUF_PIEXL
    #define  MARQUEE_MAXNUM       DISPLAY_BUF_MAX_PIXEL/3
    #define DISPLAY_FIRINGNUMS    60
    #endif
  #endif
#define H_BIT_1 0xe0  
#define L_BIT_1 0x0e  
#define H_BIT_0 0x80  
#define L_BIT_0 0x08   
#endif


#define R_MASK        0x0003
#define R_DIR_NEG     0x0001
#define R_STEP_ITEVL  0x0002
#define G_MASK        0x000C
#define G_DIR_NEG     0x0004
#define G_STEP_ITEVL  0x0008
#define B_MASK        0x0030
#define B_DIR_NEG     0x0010
#define B_STEP_ITEVL  0x0020
#define C_MASK        0x00C0
#define C_DIR_NEG     0x0040
#define C_STEP_ITEVL  0x0080
#define W_MASK        0x0300
#define W_DIR_NEG     0x0100
#define W_STEP_ITEVL  0x0200

#define MAX_STEP_NUM              64
#define SHADE_THREAD_TIMES_US     300
#define MIN_SHADE_STEP_US         1000

#define FIX_RADIX     7
#define MAX_SHADE_NUM 6
static LIGHT_SHADE_CTRL_T* __light_shade_ctrl[MAX_SHADE_NUM];


extern volatile unsigned char __is_led_driver_busy; 
extern unsigned char tx_buf[240];
extern unsigned char  __back_buf[DATA_BUF_SIZE];
extern unsigned char __front_buf[DATA_BUF_SIZE];
extern unsigned int __front_buf_len;
extern volatile unsigned char ShadeStartFlag;
extern volatile unsigned char ShadeInvertFlag;
extern volatile unsigned char ShadeCntsFlag; 
extern unsigned int __back_buf_pixel_num;
extern unsigned char __c;
extern unsigned char __w; 

int display_init(unsigned int pixel_num);
unsigned int get_display_buf_pixel_num();
int update_display_one_pixel(unsigned char r, unsigned char g, unsigned char b, unsigned char c, unsigned char w,  unsigned int offset);
int update_display_data(unsigned char r, unsigned char g, unsigned char b, unsigned char c, unsigned char w, unsigned int offset, unsigned int pixel_num);
int update_display_data_all(unsigned char r, unsigned char g, unsigned char b, unsigned char c, unsigned char w);
void display_service(void);
void SetBufData(void);

void* user_malloc(char* func, unsigned int size);
void user_free(char* func, void* addr);
  
#define MALLOC(x) user_malloc((char*)__FUNCTION__, x)
#define FREE(x) user_free((char*)__FUNCTION__, x)

#ifdef __cplusplus
}
#endif

#endif

  
  
