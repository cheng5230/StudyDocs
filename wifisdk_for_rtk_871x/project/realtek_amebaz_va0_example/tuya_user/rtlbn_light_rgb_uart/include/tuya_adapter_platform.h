/***********************************************************
*  File: tuya_adapter_platform.c
*  Author: mjl
*  Date: 20181106
***********************************************************/

#ifndef TUYA_ADAPTER_PLATFORM_H
#define TUYA_ADAPTER_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif


#include "tuya_light_types.h"
#include "tuya_cloud_types.h"
#include "tuya_cloud_error_code.h"

#include "adapter_platform.h"
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
#include "pwmout_api.h"
#include "timer_api.h"
#include "tuya_light_i2c_master.h"
#include "cJSON.h"


//IO
#define PWM_0_OUT_IO_NUM  PA_5  //0channel--R
#define PWM_1_OUT_IO_NUM  PA_15 //1channel--G
#define PWM_2_OUT_IO_NUM  PA_16 //5channel--B
#define PWM_3_OUT_IO_NUM  PA_12 //4channel--WC
#define PWM_4_OUT_IO_NUM  PA_1  //3channel--WW
#define PWM_5_OUT_IO_NUM  PA_24

//I2C
#define I2C_SCL_0     PA_19
#define I2C_SDA_0     PA_5
#define I2C_SCL_1     PA_5
#define I2C_SDA_1     PA_14
#define I2C_SCL_2     PA_15
#define I2C_SDA_2     PA_14
#define I2C_SCL_3     I2C_SCL_0
#define I2C_SDA_3     I2C_SDA_0
#define I2C_SCL_4     PA_17
#define I2C_SDA_4     PA_14
//low_POWER
#define LOW_POWER_0    PA_12
#define LOW_POWER_1    PA_17
#define LOW_POWER_2    PA_2
#define LOW_POWER_3    LOW_POWER_0
#define LOW_POWER_4    PA_15

#define KEY_BUTTON     PA_0

#define MODULE_TYPE_1  "WR2L"
#define MODULE_TYPE_2  "WR3-WR3L"
#define MODULE_TYPE_3  "RLC2V-RLC7V"
#define MODULE_TYPE_4  ""
#define MODULE_TYPE_5  "RLC4"


// wifi config method select

#define TUYA_GWCM_OLD     GWCM_OLD  // do not have low power mode
#define TUYA_GWCM_LOW_POWER GWCM_LOW_POWER  // with low power mode
#define TUYA_GWCM_SPCL_MODE GWCM_SPCL_MODE  // special with low power mode
#define TUYA_GWCM_OLD_PROD  GWCM_OLD_PROD   // GWCM_OLD mode with product


#define TUYA_STAT_LOW_POWER STAT_LOW_POWER // idle status,use to external config network
#define TUYA_STAT_UNPROVISION STAT_UNPROVISION // smart config status
#define TUYA_STAT_AP_STA_UNCFG STAT_AP_STA_UNCFG // ap WIFI config status
#define TUYA_STAT_AP_STA_DISC STAT_AP_STA_DISC // ap WIFI already config,station disconnect
#define TUYA_STAT_AP_STA_CONN STAT_AP_STA_CONN // ap station mode,station connect
#define TUYA_STAT_STA_DISC STAT_STA_DISC // only station mode,disconnect
#define TUYA_STAT_STA_CONN STAT_STA_CONN // station mode connect
#define TUYA_STAT_CLOUD_CONN STAT_CLOUD_CONN // cloud connect
#define TUYA_STAT_AP_CLOUD_CONN STAT_AP_CLOUD_CONN // cloud connect and ap start


#define TUYA_LIGHT_UN_ACTIVE GW_RESET
#define TUYA_LIGHT_ACTIVE GW_ACTIVED

#define TUYA_LIGHT_PIN(n)   (PA_0+(n))
#define TUYA_GPIO_IN  1
#define TUYA_GPIO_OUT  0
#define TUYA_GPIO_LOW 0
#define TUYA_GPIO_HIGH 1

#define TUYA_LIGHT_HW_TIMER_CYCLE     100                  //us
#define TUYA_LIGHT_NORMAL_DELAY_TIME    300               //hw_arm = 0.4ms
#define TUYA_LIGHT_SCENE_DELAY_TIME     100         //情景渐变计算假定延时时间100ms
#define TUYA_LIGHT_STANDARD_DELAY_TIMES   300         //无极调光时间（100 * ms）  
#define LOWPOWER_MODE_DELAY_TIME      2000        //ms


typedef  THRD_HANDLE TUYA_THREAD;
typedef  TY_UART_PORT_E TUYA_UART_PORT;
typedef  GW_WIFI_NW_STAT_E  TUYA_GW_WIFI_NW_STAT_E;

typedef VOID (*APP_PROD_CB) (BOOL_T flag, CHAR_T rssi); //lql

typedef enum
{
  REASON_DEFAULT_RST = 0,         /**< normal startup by power on */
  //    REASON_WDT_RST,             /**< hardware watch dog reset */
  //    REASON_EXCEPTION_RST,       /**< exception reset, GPIO status won't change */
  //    REASON_SOFT_WDT_RST,        /**< software watch dog reset, GPIO status won't change */
  REASON_SOFT_RESTART = 9,        /**< software restart ,system_restart , GPIO status won't change */
  //    REASON_DEEP_SLEEP_AWAKE,    /**< wake up from deep-sleep */
  //    REASON_EXT_SYS_RST          /**< external system reset */
} rst_reason;

void pwm_set_duty (UINT_T duty, UCHAR_T channel);



typedef BYTE_T TUYA_WF_CFG_MTHD_SEL; // wifi config method select
typedef BYTE_T TUYA_LIGHT_CMD_T;



VOID tuya_light_oem_set (BOOL_T flag);
VOID tuya_light_pwm_init (IN UINT_T period, IN  UINT_T* duty, IN  UINT_T pwm_channel_num, IN  UINT_T* pin_info_list);
VOID tuya_light_pwm_channel_reverse_set (UCHAR_T channel);
VOID tuya_light_pwm_send_init (VOID);
USHORT_T tuya_light_pwm_set_hubu (USHORT_T WW_value);
VOID tuya_light_send_data (IN USHORT_T R_value, IN USHORT_T G_value, IN  USHORT_T B_value, IN USHORT_T CW_value,
                           IN USHORT_T WW_value);
OPERATE_RET tuya_light_common_flash_init (VOID);
OPERATE_RET tuya_light_common_flash_write (IN          UCHAR_T* name, IN  UCHAR_T* data, IN  UINT_T len);
OPERATE_RET tuya_light_common_flash_read (IN         UCHAR_T* name, OUT UCHAR_T** data);
VOID tuya_light_print_port_init (VOID);
OPERATE_RET tuya_light_prod_test_flash_write (IN UCHAR_T* data, IN  UINT_T len);
OPERATE_RET tuya_light_prod_test_flash_read (OUT UCHAR_T** data);
VOID tuya_light_hw_timer_init (VOID);
VOID tuya_light_enter_lowpower_mode (VOID);
VOID tuya_light_exit_lowpower_mode (VOID);
BOOL tuya_light_dev_poweron (VOID);
OPERATE_RET tuya_light_CreateAndStart (OUT TUYA_THREAD* pThrdHandle, IN  P_THRD_FUNC pThrdFunc, IN  PVOID pThrdFuncArg,
                                       IN  USHORT_T stack_size, IN  TRD_PRI pri, IN  UCHAR_T* thrd_name);
#if _IS_OEM
OPERATE_RET tuya_light_smart_frame_init (CHAR_T* product_key, CHAR_T* sw_ver);
#else
OPERATE_RET tuya_light_smart_frame_init (CHAR_T* sw_ver);
#endif
DEV_CNTL_N_S* tuya_light_get_dev_cntl (VOID);
OPERATE_RET tuya_light_dp_report (CHAR_T* out);
VOID tuya_light_key_process (INT_T gpio_no, PUSH_KEY_TYPE_E type, INT_T cnt);
OPERATE_RET tuya_light_button_reg (IN UINT_T gpio_no, IN BOOL_T is_high, IN  UINT_T long_key_time,
                                   IN KEY_CALLBACK callback);
INT_T tuya_light_get_free_heap_size (VOID);
TUYA_GW_WIFI_NW_STAT_E tuya_light_get_wf_gw_status (VOID);
VOID tuya_light_dev_reset (VOID);
BYTE_T tuya_light_get_gw_status (VOID);
UCHAR_T* tuya_light_ty_get_enum_str (DP_CNTL_S* dp_cntl, UCHAR_T enum_id);
BOOL_T tuya_light_get_gw_mq_conn_stat (VOID);
OPERATE_RET tuya_light_gpio_init (UINT_T gpio, BYTE_T type);
OPERATE_RET tuya_light_gpio_ctl (UINT_T gpio, BYTE_T high);
INT_T tuya_light_gpio_read (UINT_T gpio);



#ifdef __cplusplus
}
#endif

#endif


