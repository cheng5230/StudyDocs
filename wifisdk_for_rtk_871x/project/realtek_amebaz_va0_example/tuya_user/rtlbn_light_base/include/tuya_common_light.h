#ifndef __COMMON_LIGHT_H__
#define __COMMON_LIGHT_H__

#include "tuya_adapter_platform.h"


#define LIGHT_TEST_KEY      "light_test_key"
#define AGING_TESTED_TIME     "aging_tested_time"

#define ENABLE 1
#define DISABLE 0
#define NONE -1

#define USER_LIGTH_MODE_C             0
#define USER_LIGTH_MODE_CW            1
#define USER_LIGTH_MODE_RGB           2
#define USER_LIGTH_MODE_RGBC          3
#define USER_LIGTH_MODE_RGBCW         4
#define USER_LIGTH_MODE_RGBC_FAKE     5
#define USER_LIGTH_MODE_RGBW_FAKE     6
#define USER_LIGTH_MODE_RGBCW_FAKEC   7
#define USER_LIGTH_MODE_RGBCW_FAKEW   8
#define USER_LIGTH_MODE_RGBCW_FAKEFC  9
#define USER_LIGTH_MODE_RGBCW_FAKEFW  10
#define USER_LIGTH_MODE_RGBCW_FAKE_CUS 11
#define USER_LIGTH_MODE_TYPE_MAX      12


typedef struct gpio_info
{
  uint32_t gpio_mux;
  uint32_t gpio_func;
  uint32_t gpio_num;
} gpio_info_s;


/**********************************************************/
typedef enum
{
  WF_CFG_OLD_PROD = 0, // GWCM_OLD mode with product
  WF_CFG_LOW_POWER = 1, // with low power mode
  WF_CFG_SPCL = 2, // special with low power mode
  WF_CFG_OLD = 3, // do not have low power mode
}
WF_INIT_CFG_E;

typedef enum
{
  LIGHT_MODE_RGBCW = 0,
  LIGHT_MODE_RGBC,
  LIGHT_MODE_RGB,
  LIGHT_MODE_CW,
  LIGHT_MODE_C,
} LIGHT_MODE_E;

typedef enum
{
  DRV_MODE_PWM_IO = 0,    /* 纯PWM驱动 */
  DRV_MODE_IIC_SMx726,    /* IIC驱动   SM16726/SM726芯片 默认单彩光IIC */
  DRV_MODE_IIC_SM2135,    /* IIC驱动   SM2135芯片 默认全IIC */
  DRV_MODE_PWM_9231,      /* 圆模块 */
} DRIVER_MODE_E;

typedef enum
{
  MODULE_TYPE_01 = 0,
  MODULE_TYPE_02,
  MODULE_TYPE_03,
  MODULE_TYPE_04,
  MODULE_TYPE_05,
} MODULE_TYPE_E;

typedef enum
{
  CW_TYPE = 0,
  CCT_TYPE,
} CONFIG_CW_TYPE_E;

typedef enum
{
  CONFIG_COLOR_R = 0,
  CONFIG_COLOR_G,
  CONFIG_COLOR_B,
  CONFIG_COLOR_C,
  CONFIG_COLOR_W,
} CONIFG_COLOR_E;

typedef enum
{
  HIGH_LEVEL = TRUE,
  LOW_LEVEL = FALSE,
} HW_LEVEL_STA_E;

typedef enum
{
  DRV_RESET_BY_POWER_ON_TIME_COUNT = 0,
  DRV_RESET_BY_LONG_KEY = 1,
  DRV_RESET_BY_ANYWAY = 2,
} RESET_DEV_MODE_E;

typedef enum
{
  KEY_FUN_RESET_WIFI = 0x0001,      /* 重配网 */
  KEY_FUN_POWER_CTRL = 0x0002,      /* 开关 */
  KEY_FUN_RAINBOW_CHANGE = 0x0004,    /* 七彩变化 */
  KEY_FUN_SCENE_CHANGE = 0X0008,      /* 情景切换 */
  KEY_FUN_POWER_RAINBOW_SCENE = 0X0010,   /* POWER+RAINBOW+SCENE */
  KEY_FUN_COLD_WHITE,           /* 冷白光 */
  KEY_FUN_WARM_WHITE,           /* 暖白光 */
} KEY_FUNCTION_E;

typedef struct
{
  HW_LEVEL_STA_E level_sta;
  signed char pin_num;
} LIGHT_PIN_INFO_S;

typedef struct
{
  signed char scl_pin;
  signed char sda_pin;
  signed char out_pin_num[5];
  signed char sm2135_rgb_current; /* 2135芯片彩光电流 */
  signed char sm2135_cw_current;  /* 2135芯片彩光电流 */
} IIC_PIN_INFO_S;

typedef struct
{
  KEY_FUNCTION_E long_key_fun;
  KEY_FUNCTION_E normal_key_fun;
  HW_LEVEL_STA_E level;
  SHORT id;
  USHORT times;
} KEY_CONFIG_INFO_S;

typedef enum
{
  POWER_ONOFF_MODE_SHADE = FALSE,
  POWER_ONOFF_MODE_DIRECT,
} LIGHT_POWER_ONOFF_MODE_E;

typedef struct
{
  /* 功能相关配置*/
  MODULE_TYPE_E module_type;          /* wf模块类型 */
  WF_INIT_CFG_E wf_init_cfg;          /* wf初始状态 */
  LIGHT_MODE_E light_mode;                /* 类型定义 */
  LIGHT_POWER_ONOFF_MODE_E power_onoff_mode;  /* 开关灯输出模式 */

  DRIVER_MODE_E driver_mode;              /* 驱动方式定义 */
  CONFIG_CW_TYPE_E cw_type;         /* 白灯输出方式 */
  //
  UINT16_T pwm_frequency;           /* PWM频率 */
  UINT8_T  cw_max_power;            /* cw最大混光功率 */
  //
  BYTE bright_max_precent;          /* 白光最大亮度 */
  BYTE bright_min_precent;          /* 白光最低亮度 */
  BYTE color_max_precent;           /* 彩光最大亮度 */
  BYTE color_min_precent;           /* 彩光最低亮度 */
  //
  BOOL dev_memory;              /* 上电记忆功能 */
  BOOL cw_rgb_mutex;              /* 白光彩光分控 */

  /* 配网相关配置 */
  RESET_DEV_MODE_E reset_dev_mode;      /* 重配网方式 */
  UINT8_T reset_count;                    /* 重置次数定义 */
  //
  CONIFG_COLOR_E netconfig_color;       /* 配网灯光颜色       */
  UINT8_T netconfig_bright;         /* 配网灯光亮度       */
  //
  CONIFG_COLOR_E def_light_color;       /* 默认初始颜色 */
  BYTE def_bright_precent;          /* 默认初始亮度 */
  BYTE def_temp_precent;            /* 默认初始色温 */

  /* 通道相关配置 */
  UINT8_T  pwm_channel_num;           /* PWM通道数量 */
  LIGHT_PIN_INFO_S light_pin[5];        /* 引脚信息 */
  UINT8_T lowpower_pin;           /* 低功耗引脚 */
  IIC_PIN_INFO_S i2c_cfg;           /* IIC引脚信息 */

  /* 按键相关配置 */
  KEY_CONFIG_INFO_S key;            /* 按键配置信息 */
} HW_TABLE_S;


#endif
