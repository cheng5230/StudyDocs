#ifndef _LIGHT_LIB_H_
#define _LIGHT_LIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_adapter_platform.h"

/*
version: 1.0.2 2018-11-29
1：  支持配置配网方式：上电低功耗/上电默认smartconfig
2：  支持配置上电复位次数
3：  支持1-5路灯
4：  支持配置默认亮度/色温百分比值
5：  支持配置最大/最小亮度
6：  支持V2版双路由产测
7：  支持自定义产测
8：  支持双路白光CW/CCT驱动方式
9：  支持新老面板
10：支持默认上电状态为纯彩色（RGB），亮度可选
11：支持断电记忆可选
12：支持配网方式选择（上电次数/按键长按）
*/

#define MAX_TEST_SSID_LEN          16
#define PRODUCT_KEY_DATA_KEY       "XYX_PRODUCT_KEY_DATA_KEY"
#define DEBUG_PRINT                PR_INFO

//情景参数
#define SCENE_HEAD_LEN        2    //情景头长度
#define SCENE_UNIT_LEN        26          //新版灯情景单元长度
#define SCENE_UNIT_NUM_MAX    8    //最大情景单元数 
#define COLOR_DATA_LEN        12     //颜色数据长度
#define SCENE_DATA_LEN_MIN  (SCENE_HEAD_LEN + SCENE_UNIT_LEN)       //情景数据最小长度
#define SCENE_DATA_LEN_MAX  (SCENE_HEAD_LEN + SCENE_UNIT_LEN * SCENE_UNIT_NUM_MAX)  //情景数据最大长度


typedef enum
{
  WHITE_MODE = 0,
  COLOR_MODE,
  SCENE_MODE,
  MUSIC_MODE,
} SCENE_MODE_E;

typedef struct
{
  BOOL_T power;
  SCENE_MODE_E mode;
  USHORT_T bright;
  USHORT_T col_temp;
  UCHAR_T color[COLOR_DATA_LEN + 1];
  UCHAR_T scene[SCENE_DATA_LEN_MAX + 1];
  UINT_T countdown;
#if USER_DEFINE_LIGHT_STRIP_LED_CONFIG
  UCHAR_T rgbcw_order[RGBCW_ORDER_DATA_LEN];
#endif
} DP_DATA_S;

typedef enum
{
  SCENE_TYPE_STATIC = 0,
  SCENE_TYPE_JUMP,
  SCENE_TYPE_SHADOW,
} SCENE_TYPE_E;

#define  SCENE_TYPE_STATIC       0 // 流光
#define SCENE_TYPE_BREATH        1//  呼吸
#define SCENE_TYPE_RAIN_DROP     2// 火焰
#define SCENE_TYPE_COLORFULL     3// 炫彩
#define SCENE_TYPE_MARQUEE       4//  跑马灯
#define SCENE_TYPE_BLINKING      5//  眨眼
#define SCENE_TYPE_SNOWFLAKE     6// 满天星
#define SCENE_TYPE_STREAMER_COLOR 7// 追光
#define SCENE_TYPE_ALL            8

#define RED        (uint32_t)0xFF0000
#define GREEN      (uint32_t)0x00FF00
#define BLUE       (uint32_t)0x0000FF
#define WHITE      (uint32_t)0xFFFFFF
#define BLACK      (uint32_t)0x000000
#define YELLOW     (uint32_t)0xFFFF00
#define CYAN       (uint32_t)0x00FFFF
#define MAGENTA    (uint32_t)0xFF00FF
#define PURPLE     (uint32_t)0x400080
#define ORANGE     (uint32_t)0xFF3000
#define PINK       (uint32_t)0xFF1493
#define ULTRAWHITE (uint32_t)0xFFFFFFFF

//20~100 Less cooling = taller flames.  More cooling = shorter flames.
#define COOLING  25
//50~200 Higher chance = more roaring fire.  Lower chance = more flickery fire.
#define SPARKING 180

#define TRIPLEDS_DIV 5


typedef enum
{
  CTRL_TYPE_JUMP = 0,
  CTRL_TYPE_SHADE,
} CRTL_TYPE_E;

//新版本最小的范围也超过了255，全部使用USHORT兼容
typedef struct
{
  USHORT_T red;
  USHORT_T green;
  USHORT_T blue;
  USHORT_T white;
  USHORT_T warm;
} BRIGHT_DATA_S;

typedef struct 
{
  unsigned char r;
  unsigned char g;
	unsigned char b;
	unsigned char c;
	unsigned char w;		
}LIGHT_DATA_T;

typedef struct
{
  BRIGHT_DATA_S fin;                  //灯光目标值
  BRIGHT_DATA_S curr;                 //灯光当前值

  SCENE_TYPE_E scene_type;
  UCHAR scene_cnt;

  BOOL_T scene_enable;                  //SCENE使能标志
  BOOL_T scene_new;                     //计算新值标志

  BOOL_T shade_new;//新渐变标志
  BOOL_T shade_flag;
  BOOL_T scene_cbflag; 
  USHORT scene_times;
  //BOOL_T memory;//断电记忆标志
} LIGHT_HANDLE_S;

typedef struct
{
  uint32_t total;
  uint32_t hw_cnt;
  uint32_t cbtotal;
  BOOL_T enable;
} HW_TIMER_S;


typedef struct
{
  TIMER_ID timer_init_dpdata;
  TIMER_ID timer_wf_stat;
  TIMER_ID timer_data_autosave;
  TIMER_ID timer_countdown;
  TIMER_ID timer_ram_checker;
  TIMER_ID timer_key;
#if USER_DEFINE_LOWER_POWER
  TIMER_ID lowpower;
#endif
  MUTEX_HANDLE mutex;
  MUTEX_HANDLE light_send_mutex;

  SEM_HANDLE sem_shade;
  SEM_HANDLE sem_cdscene;
  SEM_HANDLE sem_uart;
} SYS_HANDLE_S;

STATIC SYS_HANDLE_S sys_handle;

typedef struct
{
  SCENE_TYPE_E type;
  UCHAR_T speed;//需要精确数值保证计算
  UCHAR_T times;//0-100
} SCENE_HEAD_S;


//wifi status
typedef enum
{
  CONF_STAT_SMARTCONFIG = 0,
  CONF_STAT_APCONFIG,
  CONF_STAT_UNCONNECT,
  CONF_STAT_CONNECT,
  CONF_STAT_LOWPOWER,
} CONF_STAT_E;

//light type
typedef enum
{
  LIGHT_COLOR_C = 1,
  LIGHT_COLOR_CW = 2,
  LIGHT_COLOR_RGB = 3,
  LIGHT_COLOR_RGBC = 4,
  LIGHT_COLOR_RGBCW = 5,
} LIGHT_COLOR_TYPE_E;

typedef enum
{
  LIGHT_DRV_MODE_PWM_IO = 0,    /* PWM driver */
  LIGHT_DRV_MODE_IIC_SMx726,    /* IIC SM16726/SM726 driver */
  LIGHT_DRV_MODE_IIC_SM2135,    /* IIC SM2135 driver */
  LIGHT_DRV_MODE_PWM_9231,    /* 9231 */
} LIGHT_DRIVER_MODE_E;

//
typedef enum
{
  INIT_COLOR_R = 0,
  INIT_COLOR_G,
  INIT_COLOR_B,
  INIT_COLOR_C,
  INIT_COLOR_W,
} INIT_LIGHT_COLOR_E;

//driver mode of warm and cold lights
typedef enum
{
  LIGHT_CW_PWM = 0,
  LIGHT_CW_CCT,
} LIGHT_CW_TYPE_E;

//Color control logic
typedef enum
{
  RGB_POWER_OFF = 0,
  RGB_POWER_ON,
} COLOR_POWER_CTL_E;

//Power on by default
typedef enum
{
  WIFI_CFG_OLD_PROD = 0,              // GWCM_OLD mode with product
  WIFI_CFG_LOW_POWER = 1,              // with low power mode
  WIFI_CFG_SPCL_MODE = 2,              // special with low power mode
  WIFI_CFG_OLD_CPT = 3,                // old mode scan
} WIFI_CFG_DEFAULT_E;

typedef enum
{
  PROD_TEST_TYPE_CUSTOM = 0,     //Custom, a callback must be passed in
  PROD_TEST_TYPE_V1,             //Version1 of single route production test
  PROD_TEST_TYPE_V2,             //Version2 of double route production test
} PROD_TEST_TYPE_E;

typedef enum
{
  LEVEL_LOW = FALSE,
  LEVEL_HIGH = TRUE,
} LEVEL_STA_E;

//Reset the wifi configuration mode
typedef enum
{
  LIGHT_RESET_BY_POWER_ON_ONLY = 0,
  LIGHT_RESET_BY_LONG_KEY_ONLY = 1,
  LIGHT_RESET_BY_ANYWAY = 2,
} LIGHT_RESET_NETWORK_STA_E;

//APP switch control mode
typedef enum
{
  POWER_ONOFF_BY_SHADE = 0,
  POWER_ONOFF_BY_DIRECT,
} LIGHT_POWER_ONOFF_TYPE_E;

//The final effect type of the data currently being sent
//(Some drivers do not support white light and color light output at the same time)
typedef enum
{
  WHITE_DATA_SEND = 0,
  COLOR_DATA_SEND,
  MIX_DATA_SEND,
  NULL_DATA_SEND,
} LIGHT_CURRENT_DATA_SEND_MODE_E;

typedef enum
{
  WHITE_NET_LIGHT = 0,
  COLOR_NET_LIGHT,
} NET_LIGHT_TYPE_E;

typedef struct
{
  WIFI_CFG_DEFAULT_E wf_cfg;
  UCHAR_T wf_rst_cnt;
  LIGHT_COLOR_TYPE_E color_type;
  LIGHT_DRIVER_MODE_E light_drv_mode;
  LIGHT_POWER_ONOFF_TYPE_E power_onoff_type;
  //
  NET_LIGHT_TYPE_E net_light_type;
  INIT_LIGHT_COLOR_E def_light_color;
  BYTE_T def_bright_precent;
  BYTE_T def_temp_precent;

  BYTE_T bright_max_precent;
  BYTE_T bright_min_precent;

  BYTE_T color_bright_max_precent;
  BYTE_T color_bright_min_precent;

  BOOL_T whihe_color_mutex;
}
LIGHT_DEFAULT_CONFIG_S;

//产测结构体定义
typedef struct
{
  UCHAR_T cc_time;
  UCHAR_T cw_time;
  UCHAR_T rgb_time;
  UCHAR_T suc_light_num;
} AGING_TIME_DEF;

typedef struct
{
  UCHAR_T fuc1_time;
} FUC_TIME_DEF;

//产测默认参数结构体定义
typedef struct
{
  UCHAR_T test1_ssid[MAX_TEST_SSID_LEN + 1];
  UCHAR_T test2_ssid[MAX_TEST_SSID_LEN + 1];

  AGING_TIME_DEF aging_time;

  BOOL_T retest;
} LIGHT_PROD_PAR;

extern BOOL_T data_upload_flag;
extern uint32_t voiceColor[6];
extern uint32_t PaomaColor[3];
extern unsigned char Connect_flag;
extern uint8_t VoiceSpeed[10];
extern uint32_t PaoColor[9];



OPERATE_RET tuya_light_init (VOID);

OPERATE_RET tuya_light_start (VOID);

VOID light_init_stat_set (VOID);

OPERATE_RET tuya_light_cw_type_set (LIGHT_CW_TYPE_E type);

OPERATE_RET tuya_light_reset_mode_set (LIGHT_RESET_NETWORK_STA_E mode);

OPERATE_RET tuya_light_cw_max_power_set (UCHAR_T max_power);

VOID tuya_light_memory_flag_set (BOOL_T status);

VOID light_key_fun_free_heap_check_start (VOID);

VOID light_key_fun_free_heap_check_stop (VOID);

VOID light_key_fun_power_onoff_ctrl (VOID);

VOID light_key_fun_wifi_reset (VOID);

TUYA_WF_CFG_MTHD_SEL light_get_wifi_cfg (VOID);
VOID hsv2rgb (FLOAT_T h, FLOAT_T s, FLOAT_T v, USHORT_T* color_r, USHORT_T* color_g, USHORT_T* color_b);
VOID light_dp_upload (VOID);
VOID prod_test (BOOL_T flag, SCHAR_T rssi);
void light_shade_stop (void);
VOID tuya_light_ctrl_precision_set (USHORT_T val);
VOID light_bright_start (VOID);
USHORT_T tuya_light_ctrl_precision_get (VOID);
uint32_t __GetWs2812FxColors(uint8_t cnt,uint8_t nums);
void light_VoiceData(uint8_t delta);

/*************************************************************************************
函数功能: 获取灯光最终输出模式
输入参数: 无
输出参数: 无
返 回 值: 灯光渐变最后的模式
备    注:   无
*************************************************************************************/
LIGHT_CURRENT_DATA_SEND_MODE_E light_send_final_data_mode_get (VOID);
extern unsigned char __scene_cnt;
extern BOOL_T data_testFlag;
extern BOOL_T Wifi_ConFlag;
extern BOOL_T _light_threadFlag; 


#ifdef __cplusplus
}
#endif

#endif

