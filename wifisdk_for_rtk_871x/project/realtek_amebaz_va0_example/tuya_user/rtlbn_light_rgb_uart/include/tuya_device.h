/***********************************************************
*  File: tuya_device.h
*  Author: mjl
*  Date: 20181106
***********************************************************/
#ifndef _TUYA_DEVICE_H
#define _TUYA_DEVICE_H

#include "tuya_adapter_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef  __DEVICE_GLOBALS
#define __DEVICE_EXT
#else
#define __DEVICE_EXT extern
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
// device information define
#define SW_VER USER_SW_VER

//#define _IS_OEM

#if _IS_OEM
//#define PRODECT_KEY "key4ua8kaadg8ywr"  //线上版本

#else
//#define PRODECT_KEY "h3aka6lbaynnv6v5"//新版灯 fake 4ch rgbc_fake

//#define PRODECT_KEY "ljhhufehvbppv4ok"//旧版灯


//5ch e3s
//#define CFG_JS "{\"ProdFunc\":{\"cmod\":{\"type\":\"fixed\",\"value\":\"rgbwc\"},\"wfcfg\":{\"valueType\":\"bool\",\"value\":false,\"index\":2},\"dmod\":{\"valueType\":\"bool\",\"value\":false,\"index\":3},\"cwtype\":{\"valueType\":\"bool\",\"value\":false},\"brightmax\":{\"valueType\":\"range\",\"range\":[0,100],\"step\":1,\"value\":100,\"index\":4},\"brightmin\":{\"valueType\":\"range\",\"range\":[0,100],\"step\":1,\"value\":10},\"pwmhz\":{\"valueType\":\"range\",\"range\":[500,10000],\"step\":500,\"value\":1000}},\"other\":{\"rstbr\":{\"valueType\":\"range\",\"range\":[10,100],\"step\":1,\"value\":50,\"index\":1},\"rstcor\":{\"type\":\"fixed\",\"value\":\"c\"},\"defbright\":{\"valueType\":\"range\",\"range\":[0,100],\"step\":1,\"value\":50,\"index\":2},\"rstnum\":{\"valueType\":\"range\",\"range\":[3,5],\"step\":1,\"value\":3,\"index\":3},\"defcolor\":{\"type\":\"fixed\",\"value\":\"c\"}},\"color\":{\"r\":{\"pin\":{\"list\":[4,5,12,13,14],\"value\":4},\"index\":1,\"lv\":{\"valueType\":\"bool\",\"value\":true}},\"g\":{\"pin\":{\"list\":[4,5,12,13,14],\"value\":12},\"index\":2,\"lv\":{\"valueType\":\"bool\",\"value\":true}},\"b\":{\"pin\":{\"list\":[4,5,12,13,14],\"value\":14},\"index\":3,\"lv\":{\"valueType\":\"bool\",\"value\":true}},\"c\":{\"pin\":{\"list\":[4,5,12,13,14],\"value\":5},\"index\":4,\"lv\":{\"valueType\":\"bool\",\"value\":true}},\"w\":{\"pin\":{\"list\":[4,5,12,13,14],\"value\":13},\"index\":5,\"lv\":{\"valueType\":\"bool\",\"value\":true}}},\"gamma\":{\"coefficient\":{\"index\":1,\"gr\":{\"valueType\":\"range\",\"range\":[0,1],\"step\":0.01,\"value\":1},\"gg\":{\"valueType\":\"range\",\"range\":[0,1],\"step\":0.01,\"value\":1},\"gb\":{\"valueType\":\"range\",\"range\":[0,1],\"step\":0.01,\"value\":1}},\"white\":{\"index\":2,\"wr\":{\"valueType\":\"range\",\"range\":[0,100],\"step\":1,\"value\":100},\"wg\":{\"valueType\":\"range\",\"range\":[0,100],\"step\":1,\"value\":100},\"wb\":{\"valueType\":\"range\",\"range\":[0,100],\"step\":1,\"value\":100}}}}"
//#define CFG_JS "{\"light\":{\"wfcfg\":{\"type\":\"fixed\",\"value\":\"spcl\"},\"rstcor\":{\"type\":\"fixed\",\"value\":\"C\"},\"cmod\":{\"type\":\"fixed\",\"value\":\"rgbcw\"},\"dmod\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"cwtype\":{\"type\":\"fixed\",\"value\":\"cct\"},\"defbright\":{\"type\":\"fixed\",\"value\":50},\"deftemper\":{\"type\":\"fixed\",\"value\":100},\"brightmax\":{\"type\":\"fixed\",\"value\":100},\"brightmin\":{\"type\":\"fixed\",\"value\":10},\"pwmhz\":{\"type\":\"fixed\",\"value\":1000},\"pwmchannel\":{\"type\":\"fixed\",\"value\":5},\"color\":{\"r\":{\"pin\":{\"list\":[4],\"value\":4},\"lv\":{\"list\":[true],\"value\":true}},\"g\":{\"pin\":{\"list\":[12],\"value\":12},\"lv\":{\"list\":[true],\"value\":true}},\"b\":{\"pin\":{\"list\":[14],\"value\":14},\"lv\":{\"list\":[true],\"value\":true}},\"c\":{\"pin\":{\"list\":[13],\"value\":13},\"lv\":{\"list\":[false],\"value\":false}},\"w\":{\"pin\":{\"list\":[5],\"value\":5},\"lv\":{\"list\":[false],\"value\":false}}},\"white\":{\"wg\":{\"type\":\"fixed\",\"value\":255},\"wr\":{\"type\":\"fixed\",\"value\":255},\"wb\":{\"type\":\"fixed\",\"value\":255}},\"rstcnt\":{\"type\":\"fixed\",\"value\":3},\"gamma\":{\"gg\":{\"type\":\"fixed\",\"value\":1},\"gr\":{\"type\":\"fixed\",\"value\":1},\"gb\":{\"type\":\"fixed\",\"value\":1}},\"rstbr\":{\"type\":\"fixed\",\"value\":50}}}"

//4ch e3s
//#define CFG_JS "{\"light\":{\"wfcfg\":{\"type\":\"fixed\",\"value\":\"spcl\"},\"rstcor\":{\"type\":\"fixed\",\"value\":\"C\"},\"cmod\":{\"type\":\"fixed\",\"value\":\"rgbc\"},\"dmod\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"cwtype\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"defbright\":{\"type\":\"fixed\",\"value\":50},\"deftemper\":{\"type\":\"fixed\",\"value\":100},\"brightmax\":{\"type\":\"fixed\",\"value\":100},\"brightmin\":{\"type\":\"fixed\",\"value\":10},\"pwmhz\":{\"type\":\"fixed\",\"value\":4000},\"pwmchannel\":{\"type\":\"fixed\",\"value\":4},\"color\":{\"r\":{\"pin\":{\"list\":[4],\"value\":4},\"lv\":{\"list\":[true],\"value\":true}},\"g\":{\"pin\":{\"list\":[12],\"value\":12},\"lv\":{\"list\":[true],\"value\":true}},\"b\":{\"pin\":{\"list\":[14],\"value\":14},\"lv\":{\"list\":[true],\"value\":true}},\"c\":{\"pin\":{\"list\":[5],\"value\":5},\"lv\":{\"list\":[false],\"value\":false}}},\"white\":{\"wg\":{\"type\":\"fixed\",\"value\":255},\"wr\":{\"type\":\"fixed\",\"value\":255},\"wb\":{\"type\":\"fixed\",\"value\":255}},\"rstcnt\":{\"type\":\"fixed\",\"value\":3},\"gamma\":{\"gg\":{\"type\":\"fixed\",\"value\":1},\"gr\":{\"type\":\"fixed\",\"value\":1},\"gb\":{\"type\":\"fixed\",\"value\":1}},\"rstbr\":{\"type\":\"fixed\",\"value\":50}}}"
//#define CFG_JS "{\"light\":{\"rstcor\":{\"type\":\"fixed\",\"value\":\"C\"},\"cmod\":{\"type\":\"fixed\",\"value\":\"rgbc\"},\"dmod\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"color\":{\"r\":{\"pin\":{\"list\":[4],\"value\":4},\"lv\":{\"list\":[true],\"value\":true}},\"b\":{\"pin\":{\"list\":[14],\"value\":14},\"lv\":{\"list\":[true],\"value\":true}},\"c\":{\"pin\":{\"list\":[5],\"value\":5},\"lv\":{\"list\":[true],\"value\":true}},\"g\":{\"pin\":{\"list\":[12],\"value\":12},\"lv\":{\"list\":[true],\"value\":true}}},\"white\":{\"wg\":{\"valueType\":\"range\",\"range\":[255,255],\"step\":1,\"value\":255},\"wr\":{\"valueType\":\"range\",\"range\":[255,255],\"step\":1,\"value\":255},\"wb\":{\"valueType\":\"range\",\"range\":[255,255],\"step\":1,\"value\":255}},\"rstcnt\":{\"type\":\"fixed\",\"value\":3},\"gamma\":{\"gg\":{\"valueType\":\"range\",\"range\":[1,1],\"step\":1,\"value\":1},\"gr\":{\"valueType\":\"range\",\"range\":[1,1],\"step\":1,\"value\":1},\"gb\":{\"valueType\":\"range\",\"range\":[1,1],\"step\":1,\"value\":1}},\"rstbr\":{\"valueType\":\"range\",\"range\":[0,255],\"step\":1,\"value\":50}}}"

//3ch e3s
//#define CFG_JS "{\"light\":{\"wfcfg\":{\"type\":\"fixed\",\"value\":\"spcl\"},\"rstcor\":{\"type\":\"fixed\",\"value\":\"C\"},\"cmod\":{\"type\":\"fixed\",\"value\":\"rgb\"},\"dmod\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"cwtype\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"defbright\":{\"type\":\"fixed\",\"value\":50},\"deftemper\":{\"type\":\"fixed\",\"value\":100},\"brightmax\":{\"type\":\"fixed\",\"value\":100},\"brightmin\":{\"type\":\"fixed\",\"value\":10},\"pwmhz\":{\"type\":\"fixed\",\"value\":1000},\"pwmchannel\":{\"type\":\"fixed\",\"value\":3},\"color\":{\"r\":{\"pin\":{\"list\":[4],\"value\":4},\"lv\":{\"list\":[true],\"value\":true}},\"g\":{\"pin\":{\"list\":[12],\"value\":12},\"lv\":{\"list\":[true],\"value\":true}},\"b\":{\"pin\":{\"list\":[14],\"value\":14},\"lv\":{\"list\":[true],\"value\":true}}},\"white\":{\"wg\":{\"type\":\"fixed\",\"value\":255},\"wr\":{\"type\":\"fixed\",\"value\":255},\"wb\":{\"type\":\"fixed\",\"value\":255}},\"rstcnt\":{\"type\":\"fixed\",\"value\":3},\"gamma\":{\"gg\":{\"type\":\"fixed\",\"value\":1},\"gr\":{\"type\":\"fixed\",\"value\":1},\"gb\":{\"type\":\"fixed\",\"value\":1}},\"rstbr\":{\"type\":\"fixed\",\"value\":50}}}"

//2ch e3s
//#define CFG_JS "{\"light\":{\"wfcfg\":{\"type\":\"fixed\",\"value\":\"spcl\"},\"rstcor\":{\"type\":\"fixed\",\"value\":\"C\"},\"cmod\":{\"type\":\"fixed\",\"value\":\"cw\"},\"dmod\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"cwtype\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"defbright\":{\"type\":\"fixed\",\"value\":50},\"deftemper\":{\"type\":\"fixed\",\"value\":100},\"brightmax\":{\"type\":\"fixed\",\"value\":100},\"brightmin\":{\"type\":\"fixed\",\"value\":10},\"pwmhz\":{\"type\":\"fixed\",\"value\":1000},\"pwmchannel\":{\"type\":\"fixed\",\"value\":2},\"color\":{\"c\":{\"pin\":{\"list\":[5],\"value\":5},\"lv\":{\"list\":[true],\"value\":true}},\"w\":{\"pin\":{\"list\":[13],\"value\":13},\"lv\":{\"list\":[true],\"value\":true}}},\"white\":{\"wg\":{\"type\":\"fixed\",\"value\":255},\"wr\":{\"type\":\"fixed\",\"value\":255},\"wb\":{\"type\":\"fixed\",\"value\":255}},\"rstcnt\":{\"type\":\"fixed\",\"value\":3},\"gamma\":{\"gg\":{\"type\":\"fixed\",\"value\":1},\"gr\":{\"type\":\"fixed\",\"value\":1},\"gb\":{\"type\":\"fixed\",\"value\":1}},\"rstbr\":{\"type\":\"fixed\",\"value\":50}}}"

//1ch e3s
//#define CFG_JS "{\"light\":{\"wfcfg\":{\"type\":\"fixed\",\"value\":\"spcl\"},\"rstcor\":{\"type\":\"fixed\",\"value\":\"C\"},\"cmod\":{\"type\":\"fixed\",\"value\":\"c\"},\"dmod\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"cwtype\":{\"type\":\"fixed\",\"value\":\"pwm\"},\"defbright\":{\"type\":\"fixed\",\"value\":50},\"deftemper\":{\"type\":\"fixed\",\"value\":100},\"brightmax\":{\"type\":\"fixed\",\"value\":100},\"brightmin\":{\"type\":\"fixed\",\"value\":10},\"pwmhz\":{\"type\":\"fixed\",\"value\":1000},\"pwmchannel\":{\"type\":\"fixed\",\"value\":1},\"color\":{\"c\":{\"pin\":{\"list\":[5],\"value\":5},\"lv\":{\"list\":[true],\"value\":true}}},\"white\":{\"wg\":{\"type\":\"fixed\",\"value\":255},\"wr\":{\"type\":\"fixed\",\"value\":255},\"wb\":{\"type\":\"fixed\",\"value\":255}},\"rstcnt\":{\"type\":\"fixed\",\"value\":3},\"gamma\":{\"gg\":{\"type\":\"fixed\",\"value\":1},\"gr\":{\"type\":\"fixed\",\"value\":1},\"gb\":{\"type\":\"fixed\",\"value\":1}},\"rstbr\":{\"type\":\"fixed\",\"value\":50}}}"


//#define GMR_JS "r"
//#define GMG_JS "g"
//#define GMB_JS "b"

#endif
#define DEF_DEV_ABI DEV_SINGLE
/***********************************************************
*************************variable define********************
***********************************************************/

#define USE_NEW_PRODTEST
/***********************************************************
*************************function define********************
***********************************************************/
/***********************************************************
*  Function: device_init
*  Input:
*  Output:
*  Return:
***********************************************************/
__DEVICE_EXT \
OPERATE_RET device_init (VOID);


#ifdef __cplusplus
}
#endif
#endif

