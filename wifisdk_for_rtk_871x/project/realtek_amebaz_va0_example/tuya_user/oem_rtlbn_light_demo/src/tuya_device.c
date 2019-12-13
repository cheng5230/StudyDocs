/***********************************************************
*  File: device.c 
*  Author: nzy
*  Date: 20150605
***********************************************************/
#define __DEVICE_GLOBALS
#include "tuya_device.h"
#include "tuya_adapter_platform.h"

/******************************************************************************
 * FunctionName : pre_device_init
 * Description  : 灯快速启动（硬件初期化）
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID pre_device_init(VOID)
{
    tuya_light_pre_device_init();
}

/******************************************************************************
 * FunctionName : app_init
 * Description  : 执行sdk之后的，应用层初期化
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID app_init(VOID)
{
    tuya_light_app_init();
}

/******************************************************************************
 * FunctionName : device_init
 * Description  : 在执行app之后的设备初期化
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
OPERATE_RET device_init(VOID)
{
    tuya_light_device_init();
}
