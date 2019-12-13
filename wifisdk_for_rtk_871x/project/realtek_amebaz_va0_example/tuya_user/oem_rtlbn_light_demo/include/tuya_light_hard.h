#ifndef __LIGHT_HARD_H__
#define __LIGHT_HARD_H__

#include "tuya_adapter_platform.h"
#include "tuya_light_lib.h"

#define DPID_SWITCH 20    // switch_led
#define DPID_MODE 21      // work_mode
#define DPID_BRIGHT 22    // bright_value
#define DPID_TEMPR 23     // temp_value
#define DPID_COLOR 24     // colour_data
#define DPID_SCENE 25     // scene_data
#define DPID_COUNTDOWN 26 // countdown
#define DPID_MUSIC 27     // music_data
#define DPID_CONTROL 28
#define DPID_STA_KEEP

/******************************************************************************
 * FunctionName : tuya_light_device_hw_init
 * Description  : 灯硬件配置初始化
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_device_hw_init(VOID);

VOID tuya_light_config_stat(CONF_STAT_E stat);

VOID tuya_light_config_param_set(LIGHT_DEFAULT_CONFIG_S *config);

VOID tuya_light_send_pwm_data(USHORT R_value, USHORT G_value, USHORT B_value, USHORT CW_value, USHORT WW_value);

VOID tuya_light_dev_reset(VOID);

VOID tuya_light_gamma_adjust(USHORT red, USHORT green, USHORT blue, USHORT *adjust_red, USHORT *adjust_green, USHORT *adjust_blue);

LIGHT_CW_TYPE_E tuya_light_cw_type_get(VOID);

LIGHT_RESET_NETWORK_STA_E tuya_light_reset_dev_mode_get(VOID);

VOID light_lowpower_enable(VOID);

VOID light_lowpower_disable(VOID);

VOID tuya_light_pwm_stop_sta_set(VOID);

#endif
