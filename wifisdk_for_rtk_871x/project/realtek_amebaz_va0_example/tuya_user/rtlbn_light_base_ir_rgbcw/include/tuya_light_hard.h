#ifndef __LIGHT_HARD_H__
#define __LIGHT_HARD_H__

#include "tuya_adapter_platform.h"
#include "tuya_light_lib.h"

//DP类型定义
#define DPID_SWITCH          20   // switch_led
#define DPID_MODE            21   // work_mode
#define DPID_BRIGHT          22   // bright_value
#define DPID_TEMPR           23   // temp_value
#define DPID_COLOR           24   // colour_data
#define DPID_SCENE           25   // scene_data
#define DPID_COUNTDOWN     26   // countdown
#define DPID_MUSIC           27   // music_data
#define DPID_CONTROL     28
#define DPID_STA_KEEP

#define KEY_CHANGE_SCENE_FUN  //开启此功能大约多消耗2-3k内存，根据实际硬件能力慎重使用！


VOID tuya_light_output_gpio_init (VOID);

VOID tuya_light_device_hw_init (VOID);

OPERATE_RET tuya_light_key_init (KEY_CALLBACK callback);

VOID tuya_light_send_pwm_data (USHORT R_value, USHORT G_value, USHORT B_value, USHORT CW_value, USHORT WW_value);

VOID tuya_light_dev_reset (VOID);

BOOL tuya_light_dev_poweron (VOID);

VOID tuya_light_gamma_adjust (USHORT red, USHORT green, USHORT blue, USHORT* adjust_red, USHORT* adjust_green,
                              USHORT* adjust_blue);

LIGHT_CW_TYPE_E tuya_light_cw_type_get (VOID);

LIGHT_RESET_NETWORK_STA_E tuya_light_reset_dev_mode_get (VOID);

UINT light_key_id_get (VOID);

VOID light_lowpower_enable (VOID);

VOID light_lowpower_disable (VOID);

VOID tuya_set_wf_config_default_on (VOID);

VOID tuya_set_wf_config_default_off (VOID);

VOID tuya_light_pwm_stop_sta_set (VOID);

//LIGHT_CW_TYPE_E tuya_light_cw_type_config_get(VOID);

//VOID tuya_light_gpio_pin_config(VOID);

#endif
