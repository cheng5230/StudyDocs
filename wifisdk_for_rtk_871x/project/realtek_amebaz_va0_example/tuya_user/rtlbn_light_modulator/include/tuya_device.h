/***********************************************************
*  File: tuya_device.h
*  Author: nzy
*  Date: 20170909
***********************************************************/
#ifndef _TUYA_DEVICE_H
#define _TUYA_DEVICE_H

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _TUYA_DEVICE_GLOBAL
    #define _TUYA_DEVICE_EXT 
#else
    #define _TUYA_DEVICE_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
// device information define
#define DEV_SW_VERSION USER_SW_VER
//#define PRODECT_KEY DEV_PRODECT_KEY
#define DEF_DEV_ABI DEV_SINGLE


typedef enum{
	DP_SWITCH = 1,
	DP_LIGHT_CONTR = 2
	//DP_COUNT_DOWN = 9,
	//DP_SAVE_STA = 39
}DPID_E;

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: pre_device_init
*  Input: none
*  Output: none
*  Return: none
*  Note: to initialize device before device_init
***********************************************************/
_TUYA_DEVICE_EXT \
VOID pre_device_init(VOID);

/***********************************************************
*  Function: device_init 
*  Input: none
*  Output: none
*  Return: none
***********************************************************/
_TUYA_DEVICE_EXT \
OPERATE_RET device_init(VOID);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


