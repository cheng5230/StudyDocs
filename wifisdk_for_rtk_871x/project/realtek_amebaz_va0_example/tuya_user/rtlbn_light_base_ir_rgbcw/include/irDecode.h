#ifndef __IRDECODE_H__
#define __IRDECODE_H__

#include "tuya_device.h"
#include "adapter_platform.h"
#include "tuya_cloud_error_code.h"
#include "tuya_iot_wifi_api.h"
#include "tuya_cloud_types.h"
#include "tuya_led.h"
#include "tuya_uart.h"
#include "tuya_gpio.h"
#include "tuya_key.h"
#include "uni_time_queue.h"
#include "gw_intf.h"
#include "uni_log.h"
#include "uni_thread.h"
#include "sys_api.h"
#include "gpio_test.h"
#include "timer_api.h"
#include "gpio_irq_api.h"
#include ".compile_usr_cfg.h"

//hw_timer_define
STATIC gtimer_t ir_timer;
//STATIC gtimer_t reset_timer;
STATIC SEM_HANDLE get_switch_sem;
STATIC gpio_irq_t ir_irq;
STATIC gpio_t test_gpio_x;
STATIC gtimer_t test_timer;

#define irCntAllowance   10
#define PRINTIRCNT       0
#define IR_GPIO_NUM      PA_19
//#define TEST_GPIO_NUM   PA_19
#define TIMER_CNT_MAX   ((145 + irCntAllowance))


typedef enum
{
  KEY_1 = 0x83,//开�?  
  KEY_2 = 0x82,//冷暖
  KEY_3 = 0X81,//
  KEY_4 = 0x80,//

  KEY_5 = 0x87,//变暖
  KEY_6 = 0x86,//
  KEY_7 = 0x85,//
  KEY_8 = 0x84,//

  KEY_9  = 0x8b,//
  KEY_10 = 0x8a,//
  KEY_11 = 0x89,//
  KEY_12 = 0x88,//

  KEY_13 = 0X8f,//
  KEY_14 = 0x8e,//
  KEY_15 = 0x8d,//
  KEY_16 = 0x8c,//

  KEY_17 = 0x93,//
  KEY_18 = 0x92,//
  KEY_19 = 0x91,//
  KEY_20 = 0x90,//

  KEY_21 = 0x97,//跳变
  KEY_22 = 0x96,//渐变
  KEY_23 = 0x95,//上层
  KEY_24 = 0x94//下层
} IRCMD;

typedef enum
{
  KEY_a = 0xe0,//设定
  KEY_b = 0x20,//夜灯
  KEY_c = 0Xe0,//30
  KEY_d = 0xd0,//60
  KEY_e = 0xc0,//开�?
  KEY_f = 0x00,//�?  
  KEY_g = 0x80,//�?
} IRCMD_new;

typedef enum
{
  // 5路红外遥控器 
  #if USER_IR5LU_OR_IR2LU
  KEY_BRIGHTNESS_UP = 0x96,
  KEY_BRIGHTNESS_DOWN = 0xA6,
  KEY_POWER_ON_OFF = 0x46,
  KEY_RGB_TURN_ROUND = 0x16,
  KEY_WHITE_W = 0x56,
  KEY_WHITE_C = 0xC6,
  KEY_CW_TURN_ROUND = 0x58,
  KEY_WHITE_xiaoye = 0x86,

  KEY_R_LEVEL_5 = 0x20,
  KEY_G_LEVEL_5 = 0xA0,
  KEY_B_LEVEL_5 = 0x60,
  KEY_W_LEVEL_5 = 0xE0,

  KEY_R_LEVEL_4 = 0x10,
  KEY_G_LEVEL_4 = 0x90,
  KEY_B_LEVEL_4 = 0x50,
  KEY_MODE_FLASH = 0x76,

  KEY_R_LEVEL_3 = 0x30,
  KEY_G_LEVEL_3 = 0xB0,
  KEY_B_LEVEL_3 = 0x70,
  KEY_MODE_STROBE = 0xF0,

  KEY_R_LEVEL_2 = 0x66,
  KEY_G_LEVEL_2 = 0x88,
  KEY_B_LEVEL_2 = 0x48,
  KEY_MODE_FADE = 0xC8,

  KEY_R_LEVEL_1 = 0x28,
  KEY_G_LEVEL_1 = 0xA8,
  KEY_B_LEVEL_1 = 0x68,
  KEY_MODE_SMOOTH = 0x36,
  #else
  // 2路红外遥控器
  KEY_BRIGHTNESS_UP = 0x60,
  KEY_BRIGHTNESS_DOWN = 0xc0,
  KEY_POWER_ON = 0x08,
  KEY_POWER_OFF = 0x40,
  KEY_WHITE_W = 0x4a,
  KEY_WHITE_C = 0xce,
  KEY_CW_TURN_ROUND = 0x6c,
  KEY_WHITE_xiaoye = 0x12,
  
  KEY_RGB_TURN_ROUND = 0x16,
  KEY_R_LEVEL_5 = 0x20,
  KEY_G_LEVEL_5 = 0xA0,
  KEY_B_LEVEL_5 = 0x50,
  KEY_W_LEVEL_5 = 0xE0,

  KEY_R_LEVEL_4 = 0x10,
  KEY_G_LEVEL_4 = 0x90,
  KEY_B_LEVEL_4 = 0x56,
  KEY_MODE_FLASH = 0x76,

  KEY_R_LEVEL_3 = 0x30,
  KEY_G_LEVEL_3 = 0xB0,
  KEY_B_LEVEL_3 = 0x70,
  KEY_MODE_STROBE = 0xF0,

  KEY_R_LEVEL_2 = 0x66,
  KEY_G_LEVEL_2 = 0x88,
  KEY_B_LEVEL_2 = 0x48,
  KEY_MODE_FADE = 0xC8,

  KEY_R_LEVEL_1 = 0x28,
  KEY_G_LEVEL_1 = 0xA8,
  KEY_B_LEVEL_1 = 0x68,
  KEY_MODE_SMOOTH = 0x36,
  #endif
} IRCMD_rfs;


typedef enum
{
  IRCODEERROR = -1,
  IRCODE0,
  IRCODE1,
  IRCODESTART,
  IRCODEREPEAT,
  IRCODESTARTHEAD
} IRCODE;

typedef struct
{
  volatile BOOL timer_switch;
  volatile UINT timer_val;
  volatile UINT cur_timer_val;
} SFT_TIMER;

typedef struct
{
  SFT_TIMER IrDecodeTimer;
  SEM_HANDLE ir_cmddeal_sem;
  THRD_HANDLE ir_thread;
  MUTEX_HANDLE ir_mutex;
} IRDEAL;

//VOID gra_change_timer_cb(void);
VOID UserInit (void);
typedef VOID (*IR_CALLBACK) (IRCMD cmd, IRCODE irType);

//VOID UserIrCmdDeal_test(IRCMD cmd,IRCODE irType);
void light_strip_infrared_init (IR_CALLBACK callback);

#endif

