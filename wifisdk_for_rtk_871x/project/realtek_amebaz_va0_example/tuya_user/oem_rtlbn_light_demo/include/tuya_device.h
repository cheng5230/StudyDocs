/***********************************************************
*  File: tuya_device.h 
*  Author: mjl
*  Date: 20181106
***********************************************************/
#ifndef _TUYA_DEVICE_H
#define _TUYA_DEVICE_H

#include "tuya_adapter_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __DEVICE_GLOBALS
#define __DEVICE_EXT
#else
#define __DEVICE_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
// device information define
#define SW_VER USER_SW_VER

#define _IS_OEM

#ifdef _IS_OEM
#define PRODECT_KEY "keyqstpxesc3t8h5" //线上版本

#else
#define PRODECT_KEY "b0xq4dl47a0sotok" //新版灯

#endif

#ifdef __cplusplus
}
#endif
#endif
