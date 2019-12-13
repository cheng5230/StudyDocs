#ifndef __HW_TABLE_H__
#define __HW_TABLE_H__

#include "tuya_common_light.h"


HW_TABLE_S* get_hw_table (VOID);
VOID hw_config_def_printf (VOID);
OPERATE_RET tuya_light_config_param_get (VOID);
KEY_FUNCTION_E tuya_light_long_key_fun_get (VOID);
KEY_FUNCTION_E tuya_light_normal_key_fun_get (VOID);

#endif
