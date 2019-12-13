/***********************************************************
File: hw_table.h 
Author: 徐靖航 JingHang Xu
Date: 2017-05-26
Description:
    硬件结构抽象表，表达通用硬件配置。
    描述内容：
        1.wifi指示灯[数量(0~1)]：
            驱动方式：高、低驱动
        2.开关、插座控制通道(ctrl_channel)[1~n]
            继电器(relay): 高、低驱动
            按钮：高、低驱动或不存在
            LED指示灯：高、低驱动或不存在
            
***********************************************************/
// ================================= 类型声明定义 =================================
// IO驱动类型: 高、低、IO不存在
#ifndef HW_TABLE_H
#define HW_TABLE_H

#include "tuya_gpio.h"
#include "tuya_key.h"
#include "tuya_led.h"
#include "tuya_iot_wifi_api.h"

#define DPID_NOT_EXIST (-1)

// ================================= IO类型抽象 =================================
typedef enum 
{
    IO_DRIVE_LEVEL_HIGH,		// 高电平有效
    IO_DRIVE_LEVEL_LOW,			// 低电平有效
    IO_DRIVE_LEVEL_NOT_EXIST	// 该IO不存在
}IO_DRIVE_TYPE;

typedef enum
{
    INIT_FALSE,
    INIT_TRUE,
    INIT_MEM
}INIT_ST;

typedef enum
{
	LEVEL_TYPE, //电平触发
	EDGE_TYPE   //边沿触发
}TRIGGER_E;     //触发类型

// IO类型抽象
typedef struct
{
    IO_DRIVE_TYPE type;	// 有效电平类型: 高电平有效; 低电平有效; io不存在
    TY_GPIO_PORT_E pin;	// 引脚号
}IO_CONFIG;

// debug输出管脚信息
void io_config_debug(IO_DRIVE_TYPE io_type,TY_GPIO_PORT_E io_pin, char *name);


// IO类型继承抽象 输出类型管脚抽象（然而C没有继承的特性）
typedef struct
{
    IO_CONFIG io_cfg;       // IO配置
    LED_HANDLE io_handle;   // LED句柄 用控制LED方法来控制所有的输出类型管脚
}OUTPUT_PIN;


typedef struct
{
    IO_DRIVE_TYPE   type;       // 有效电平类型: 高电平有效; 低电平有效; io不存在
    TRIGGER_E       trig_t;     // 触发类型：电平触发，边沿触发
    KEY_USER_DEF_S  key_cfg;    // 按键配置结构体 
}BUTTON_PIN;

// 设置输出管脚状态 
// is_active:   TRUE    - 控制引脚为有效电平类型
//              FALSE   - 控制引脚为无效电平类型
void out_pin_set_stat(IO_CONFIG io_cfg, BOOL is_active);


// 设置LED的亮灭
//output       led引脚及有效电平
//is_active:    亮灭状态
void led_pin_set_stat(OUTPUT_PIN *output, BOOL is_active);


// 设置led管脚闪烁状态
// flash_type: 闪烁相位
//      OL_FLASH_HIGH
//      OL_FLASH_LOW
// half_period: 半个闪烁周期的时间(单位: ms)
void led_pin_set_flash(OUTPUT_PIN *output, LED_LT_E flash_type, unsigned short half_period);

// ================================= 通道类型抽象 =================================
// 开关、插座控制通道
// 目前包含:
//		继电器
//		控制按键
//		状态指示灯
typedef enum{
    WFL_OFF,    //wifi灯常灭
    WFL_ON,     //wifi灯常开
    WFL_DIR_CHANNEL1,  //wifi灯指示通道channel 1
    WFL_DIR_CHANNEL2,
    WFL_DIR_CHANNEL3,
    WFL_DIR_CHANNEL4,
    WFL_DIR_CHANNEL5,
    WFL_DIR_CHANNEL6,
    WFL_DIR_CHANNEL7,
    WFL_DIR_CHANNEL8,
    WFL_DIR_CHANNEL_ALL
}WFL_STAT;
	
typedef struct
{
    OUTPUT_PIN	    relay;			// 继电器
    BUTTON_PIN	    button;			// 控制按键
    OUTPUT_PIN	    led;			// 状态指示灯
    INIT_ST         default_stat;   // 启动时的状态
    int			    dpid;			// 该通道绑定的dpid
    int			    cd_dpid;		// 该通道绑定的倒计时dpid 小于0表示不存在
    int 		    cd_msec;			// 通道倒计时 -1 停止
    BOOL		    stat;			// 通道状态 TRUE - 有效; FALSE - 无效
    UINT_T 			timer_cnt;		//倒计时上报CNT
    UINT_T 			last_sys_ms;
}CTRL_CHANNEL;

typedef struct
{
    OUTPUT_PIN              wfl;     //wifi灯GPIO口
    WFL_STAT                wfl_connected;     //wifi灯连接后的指示状态
    WFL_STAT                wfl_not_connect;    //wifi灯未连接时指示状态
}WIFI_LED;

typedef enum
{
	LOW_POWER, //低功耗模式
	FLASH_ON  //上电快闪模式
}MODE_WIFI;

typedef enum
{
    NOT_CONNECT,    //未连接
    CONNECTING,     //连接中
    CONNECTED       //已连接
}WIFI_CONN_STAT;

// ================================= 硬件表类型抽象 =================================
// HW_TABLE结构体类型
// 抽象描述硬件配置
typedef struct
{
	MODE_WIFI       wf_mode;
    WIFI_LED        wifi_stat_led;		// wifi状态指示灯
    WIFI_LED        wifi_stat_led1;		// wifi状态双指示灯
    WIFI_CONN_STAT  wifi_conn_stat;     //wifi连接状态
    OUTPUT_PIN      power_stat_led;     //电源状态指示灯
    BUTTON_PIN      power_button;	    // 控制按键
    unsigned int    press_hold_time;    // 长按触发时间 单位: 秒
    int 		    channel_num;        // 通道数量
    int 			power_up_sta_init;   //
    CTRL_CHANNEL    *channels;			// 通道列表 *!* 不要重新指向其他位置
}HW_TABLE;

// ================================ 类型声明定义结束 ================================

//参数检查
#define IS_IO_TYPE_ALL_PERIPH(PERIPH) (((PERIPH) == IO_DRIVE_LEVEL_HIGH) ||\
                                       ((PERIPH) == IO_DRIVE_LEVEL_LOW)   ||\
                                       ((PERIPH) == IO_DRIVE_LEVEL_NOT_EXIST))





#define IO_ACTIVE_TYPE(type)        ( ((type) == IO_DRIVE_LEVEL_HIGH) ? (TRUE) : (FALSE) )

#define DRV_ACTIVE_TYPE(type)       ( ((type) == IO_DRIVE_LEVEL_HIGH) ? (OL_HIGH) : (OL_LOW) )
#define DRV_PASSTIVE_TYPE(type)     ( ((type) == IO_DRIVE_LEVEL_HIGH) ? (OL_LOW)  : (OL_HIGH) )

#define DRV_STATE_TYPE(stat, type)  ( (stat) ? (DRV_ACTIVE_TYPE(type)) : (DRV_PASSTIVE_TYPE(type)) )

#define REV_COUNT_TIME(val)         ( ((val) == (-1)) ? (0) : (val) )
#define GET_COUNT_TIME(val)         ( ((val) == (0)) ? (-1) : (val) )
// ================================== 方法声明定义 ==================================
// 使用JSON配置hw_table信息
// 返回 0成功
// 返回-1失败
// *!* 失败时切勿继续运行应用
//int hw_set_by_json(HW_TABLE *hw_table, cJSON *root);

// 初始化hw_table
// 并注册所需硬件
// 返回注册成功与否
OPERATE_RET hw_init(HW_TABLE *hw_table,
    void (*hw_key_process)(TY_GPIO_PORT_E, PUSH_KEY_TYPE_E, INT_T)
    );

// 控制通道
// hw_table:	硬件对象
// channel_no:	被控制的通道号
// is_active:	通道是否有效 - 有效对应TURE - 对应该插座、开关通电
void hw_set_channel(HW_TABLE *hw_table, int channel_no, BOOL is_active);
// 用dpid控制通道
// hw_table:	硬件对象
// dpid:	dpid
// is_active:	通道是否有效 - 有效对应TURE - 对应该插座、开关通电
// return: (int) 有动作 通道下标(大于等于0); 无动作 -1; 未找到 -2
int hw_set_channel_by_dpid(HW_TABLE *hw_table, int dpid, BOOL is_active);
// 切换通道状态 有效<->无效
// hw_table:	硬件对象
// channel_no:	被控制的通道号
void hw_trig_channel(HW_TABLE *hw_table, int channel_no);
// 切换wifi指示灯状态
// hw_table:	硬件对象
// wifi_stat:	wifi状态
void hw_set_wifi_led_stat(HW_TABLE *hw_table, GW_WIFI_NW_STAT_E wifi_stat);
// 用倒计时dpid控制通道
// hw_table:	硬件对象
// dpid:		倒计时dpid
// return: (int) 有动作 通道下标(大于等于0); 未找到 -1
int hw_find_channel_by_cd_dpid(HW_TABLE *hw_table, int dpid);
// ================================ 方法声明定义结束 ================================

// ================================ 全局变量声明定义 ================================
extern HW_TABLE g_hw_table;

// ============================== 全局变量声明定义结束 ==============================
#endif