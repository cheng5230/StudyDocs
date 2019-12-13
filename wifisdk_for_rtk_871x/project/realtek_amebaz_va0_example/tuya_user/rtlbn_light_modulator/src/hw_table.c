/***********************************************************
File: hw_table.c 
Author: 徐靖航 JingHang Xu
Date: 2017-06-01
Description:
    硬件结构抽象表，表达通用硬件配置。
    描述内容：
        1.wifi指示灯[数量(0~1)]：
            驱动方式：高、低驱动
        2.开关、插座控制通道(ctrl_channel)[1~n]
            继电器(relay): 高、低驱动
            按钮：高、低驱动或不存在(目前只能配置是否存在)
            LED指示灯：高、低驱动或不存在
            
***********************************************************/
#include <mem_pool.h> // 编译器非标准，在使用下面方法初始化结构体时依赖此头文件
#include "cJSON.h"
#include "hw_table.h"
#include "uni_log.h"
#include ".compile_usr_cfg.h"
#include "gpio_api.h"   // mbed
#define KEY_TIMER_MS      	 20    //key timer inteval
#define KEY_SEC_RST_TIME   50000

static unsigned int wifi_dir_val[9] = {0x00000001,0x00000010,0x00000100,0x00001000,0x00010000,0x00100000,0x01000000,0x10000000,0x11111111};

static CTRL_CHANNEL channels[] =    
{
    // channel 0
    {
        // 继电器
        .relay.io_cfg= {.type = RELAY_CONTROL_USR_TYPE, .pin = RELAY_CONTROL_USR_GPIO},
        // 按钮
        .button = {.type = RELAY_BUTTON_USR_TYPE, .trig_t = RELAY_BUTTON_TRIG_TYPE, .key_cfg.port = RELAY_BUTTON_USR_GPIO},
        // 通道状态指示灯
        .led.io_cfg = {.type = RELAY_LED_USR_TYPE, .pin =RELAY_LED_USR_GPIO},
         //初始状态
        .default_stat = RELAY_DEAULT_STATE,
        //倒计时计数
        .cd_msec = -1,
        // 绑定dpid
        .dpid   = RELAY_BOOL_DPID,
        // 绑定倒计时dpid DPID_NOT_EXIST表示不存在倒计时
        .cd_dpid = RELAY_COUNT_DOWN_DPID,
    },
	// channel 1
	{
		// 继电器
		.relay.io_cfg= {.type = RELAY1_CONTROL_USR_TYPE, .pin = RELAY1_CONTROL_USR_GPIO},
		// 按钮
		.button = {.type = RELAY1_BUTTON_USR_TYPE, .trig_t = RELAY1_BUTTON_TRIG_TYPE, .key_cfg.port = RELAY1_BUTTON_USR_GPIO},
		// 通道状态指示灯
		.led.io_cfg = {.type = RELAY1_LED_USR_TYPE, .pin =RELAY1_LED_USR_GPIO},
		 //初始状态
		.default_stat = RELAY1_DEAULT_STATE,
		//倒计时计数
		.cd_msec = -1,
		// 绑定dpid
		.dpid	= RELAY1_BOOL_DPID,
		// 绑定倒计时dpid DPID_NOT_EXIST表示不存在倒计时
		.cd_dpid = RELAY1_COUNT_DOWN_DPID,
	}	
};


// 硬件配置表全局变量
HW_TABLE g_hw_table = 
{
     //wifi模式
    .wf_mode = WIFI_MODE,
    // wifi状态指示灯 有效对应wifi连接 无效对应wifi未连接
    .wifi_stat_led = {.wfl.io_cfg = {.type = WIFI_LED_USR_TYPE, .pin = WIFI_LED_USR_GPIO},\
                      .wfl_connected = WIFI_CONNECTED_LED_STATE, .wfl_not_connect = WIFI_NOT_CONNECTED_LED_STATE},
    .wifi_stat_led1 = {.wfl.io_cfg = {.type = WIFI_LED1_USR_TYPE, .pin = WIFI_LED1_USR_GPIO},\
    				  .wfl_connected = WIFI_CONNECTED_LED_STATE, .wfl_not_connect = WIFI_NOT_CONNECTED_LED_STATE},
    //电源指示灯
    .power_stat_led.io_cfg = {.type = POWER_LED_USR_TYPE, .pin = POWER_LED_USR_GPIO},
    //总控按键
    .power_button = {.type = POWER_BUTTON_USR_TYPE, .trig_t = POWER_BUTTON_TRIG_TYPE, .key_cfg.port = POWER_BUTTON_USR_GPIO},
    //长按时间(s)
    .press_hold_time = POWER_BUTTON_PRESS_HOLD_TIME,
    //通道列表 
    .channels = channels
};

extern OPERATE_RET upload_all_dp_stat(VOID);

/***********************************************************
*  Function:  IO配置信息打印
*  Input:
*  Output:
*  Return: 
***********************************************************/
void io_config_debug(IO_DRIVE_TYPE io_type,TY_GPIO_PORT_E io_pin, char *name)
{
    if((io_type == IO_DRIVE_LEVEL_NOT_EXIST) || (name == NULL))
    {
        // 错误返回
        return;
    }
    PR_NOTICE("IO - %s:\tpin-%d\ttype %s", name, io_pin,
             (io_type == IO_DRIVE_LEVEL_HIGH)?("high"):("low"));
}

/***********************************************************
*  Function: LED管脚注册
*  Input:    output    输出管脚及响应电平
*  Output:
*  Return: 
***********************************************************/
static OPERATE_RET led_pin_reg(OUTPUT_PIN *output)
{
    OPERATE_RET op_ret;

    if(output == NULL)
    {
        PR_ERR("NULL pointer");
        return OPRT_INVALID_PARM;
    }

    if(!IS_IO_TYPE_ALL_PERIPH(output->io_cfg.type)){
        PR_ERR("IO type not define");
        return OPRT_INVALID_PARM;
    }
    
    // 高电平有效 或者低电平有效时注册该输出管脚
    if(output->io_cfg.type != IO_DRIVE_LEVEL_NOT_EXIST){
       op_ret = tuya_create_led_handle(output->io_cfg.pin,\
                                      IO_ACTIVE_TYPE(output->io_cfg.type),\
                                      &(output->io_handle));
       if(OPRT_OK != op_ret) {
           return op_ret;
       }

    }

    return OPRT_OK;
}

/***********************************************************
*  Function: 输出管脚初始化
*  Input:    output    输出管脚及响应电平
*  Output:
*  Return: 
***********************************************************/
static OPERATE_RET out_pin_init(OUTPUT_PIN *output)
{
    OPERATE_RET op_ret;

    if(output == NULL)
    {
        PR_ERR("NULL pointer");
        return OPRT_INVALID_PARM;
    }

    if(!IS_IO_TYPE_ALL_PERIPH(output->io_cfg.type)){
        PR_ERR("IO type not define");
        return OPRT_INVALID_PARM;
    }

    //普通输出io
    if(output->io_cfg.type != IO_DRIVE_LEVEL_NOT_EXIST){
        op_ret = tuya_gpio_inout_set(output->io_cfg.pin,FALSE);
        if(OPRT_OK != op_ret) {
            return op_ret;
        }
    }

    return OPRT_OK;
}

/***********************************************************
*  Function:  按键引脚初始化
*  Input:     pbutton 按键引脚信息
              hw_key_process  按键处理回调函数
*  Output:
*  Return: 
***********************************************************/
static OPERATE_RET button_pin_init(BUTTON_PIN *pbutton,
                                       void (*hw_key_process)(TY_GPIO_PORT_E , PUSH_KEY_TYPE_E, INT_T))
{
    OPERATE_RET ret;
    
    if((NULL == pbutton) || (NULL == hw_key_process))
    {
        PR_ERR("NULL pointer");
        return OPRT_INVALID_PARM;
    }

    if(IO_DRIVE_LEVEL_NOT_EXIST == pbutton->type)
    {
        return OPRT_OK;
    }
    
    pbutton->key_cfg.low_level_detect    = !IO_ACTIVE_TYPE(pbutton->type);
    PR_DEBUG("key_cfg.low_level_detect:%s", (pbutton->key_cfg.low_level_detect)?"true":"false");
    pbutton->key_cfg.call_back           = hw_key_process;
    pbutton->key_cfg.long_key_time       = g_hw_table.press_hold_time * 1000u;
    pbutton->key_cfg.seq_key_detect_time = 0;
//	pbutton->key_cfg.max_long_key_time = KEY_SEC_RST_TIME;                                //yao
    
    pbutton->key_cfg.lp_tp  =  (pbutton->trig_t == EDGE_TYPE)?FALLING_EDGE_TRIG: LP_ONCE_TRIG;

    PR_DEBUG("pbutton->key_cfg.lp_tp:%s", (pbutton->key_cfg.lp_tp == LP_ONCE_TRIG)?"LP_ONCE_TRIG":"FALLING_EDGE_TRIG");

    ret = reg_proc_key(&(pbutton->key_cfg));

    return ret;
}


/***********************************************************
*  Function: 设置输出IO的状态     
*  Input:    output     输出管脚及响应电平
             is_active: 状态
*  Output:
*  Return: 
***********************************************************/
void out_pin_set_stat(IO_CONFIG io_cfg, BOOL is_active)
{
    if(!IS_IO_TYPE_ALL_PERIPH(io_cfg.type)){
        PR_ERR("IO type not define");
        return;
    }

    // IO不存在则跳过
    if(io_cfg.type != IO_DRIVE_LEVEL_NOT_EXIST){
        tuya_gpio_write(io_cfg.pin, DRV_STATE_TYPE(is_active, io_cfg.type));
    }
}


/***********************************************************
*  Function: 设置LED的亮灭
*  Input:    output       led引脚及有效电平
             is_active:    亮灭状态
*  Output:
*  Return: 
***********************************************************/
void led_pin_set_stat(OUTPUT_PIN *output, BOOL is_active)
{
    // 检查LED_HANDLE防止崩溃
    if(output == NULL && output->io_handle != NULL)
    {
        PR_ERR("NULL pointer");
        return;
    }

    if(!IS_IO_TYPE_ALL_PERIPH(output->io_cfg.type)){
        PR_ERR("IO type not define");
        return OPRT_INVALID_PARM;
    }

    // IO不存在则跳过
    if(output->io_cfg.type != IO_DRIVE_LEVEL_NOT_EXIST){
        tuya_set_led_light_type(output->io_handle,\
                                DRV_STATE_TYPE(is_active, output->io_cfg.type),\
                                0, 0);
    }
}

/***********************************************************
*  Function: 设置io闪烁
*  Input:    output      被控管教及有效电平
             flash_type: 闪烁相位
             half_period 半个闪烁周期的时间(单位: ms)
*  Output:
*  Return: 
***********************************************************/
void led_pin_set_flash(OUTPUT_PIN *output, LED_LT_E flash_type, unsigned short half_period)
{   
    if(output == NULL)
    {
        PR_ERR("NULL pointer");
        return;
    }
    // IO不存在时跳过
    if(output->io_cfg.type != IO_DRIVE_LEVEL_NOT_EXIST){
        tuya_set_led_light_type(output->io_handle, flash_type, half_period,LED_TIMER_UNINIT);
    }
}
void Led_a11_init(){

    gpio_write(TY_GPIOA_11, 0);
    
    PR_NOTICE("Led_a11_init success\n");
}

/***********************************************************
*  Function: 硬件初始化,并进行相应的注册
*  Input:    hw_table 硬件抽象表
             hw_key_process  按键回调
*  Output:
*  Return:   操作结果是否成功
***********************************************************/
OPERATE_RET hw_init(HW_TABLE *hw_table,
    void (*hw_key_process)(TY_GPIO_PORT_E , PUSH_KEY_TYPE_E, INT_T)
    )
{
    int i;
    OPERATE_RET op_ret = OPRT_OK;// 返回状态

    if(hw_table == NULL || hw_key_process == NULL)
    {
        PR_ERR("NULL pointer");
        return OPRT_INVALID_PARM;
    }

    PR_DEBUG("initialize hardware...");

	op_ret = key_init(NULL,0,KEY_TIMER_MS);
    if(op_ret != OPRT_OK){
        PR_ERR("key_init err:%d",op_ret);
        return op_ret;
    }

    hw_table->channel_num = sizeof(channels) / sizeof(CTRL_CHANNEL);
	for(i = 0;i < hw_table->channel_num;i++){
		if(hw_table->channels[i].relay.io_cfg.type == IO_DRIVE_LEVEL_NOT_EXIST)
			hw_table->channel_num--;
	}
    PR_DEBUG("channel num: %d", hw_table->channel_num);

    // io_cfg debug用字符串
    char name_buff[20];
    // 初始化控制通道
    for(i=0; i<hw_table->channel_num; ++i)
    {
        hw_table->channels[i].stat = FALSE; // 初始状态
        
        // 1.初始化通道中的继电器
        op_ret = out_pin_init(&(hw_table->channels[i].relay));
        if(op_ret != OPRT_OK){
            PR_ERR("ch%d relay init failed!", i);
            return op_ret;
        }
        sprintf(name_buff, "relay[%d]", i);
        io_config_debug(hw_table->channels[i].relay.io_cfg.type,\
                        hw_table->channels[i].relay.io_cfg.pin, name_buff);

        // 2.初始化通道中的按键
        sprintf(name_buff, "button[%d]", i);
        io_config_debug(hw_table->channels[i].button.type,\
                        hw_table->channels[i].button.key_cfg.port, name_buff);
        PR_NOTICE("trig_type:%s",(hw_table->channels[i].button.trig_t == EDGE_TYPE)?"edge":"level");
        op_ret = button_pin_init(&(hw_table->channels[i].button), hw_key_process);
        if(op_ret != OPRT_OK){
            PR_ERR("ch%d button init failed!", i);
            return op_ret;
        }

        // 3.初始化通道中的状态指示灯
        op_ret = led_pin_reg(&(hw_table->channels[i].led));
        if(op_ret != OPRT_OK){
            PR_ERR("ch%d relay init failed!", i);
            return op_ret;
        }
        sprintf(name_buff, "led[%d]", i);
        io_config_debug(hw_table->channels[i].led.io_cfg.type,\
                        hw_table->channels[i].led.io_cfg.pin,name_buff);

        // 4.初始倒计时为停止
        hw_table->channels[i].cd_msec = -1;

        PR_NOTICE("CH[%d],    DPID[%d]", i, hw_table->channels[i].dpid);
        PR_NOTICE("CH[%d],    CDDPID[%d]", i, hw_table->channels[i].cd_dpid);
    }

    
    // 初始化wifi状态指示灯
    op_ret = led_pin_reg(&(hw_table->wifi_stat_led.wfl));
    if(op_ret != OPRT_OK){
        PR_ERR("wifi led init failed!");
        return op_ret;
    }
    io_config_debug(hw_table->wifi_stat_led.wfl.io_cfg.type ,\
                    hw_table->wifi_stat_led.wfl.io_cfg.pin, "wifi_stat");

    // 初始化wifi状态指示灯1
    op_ret = led_pin_reg(&(hw_table->wifi_stat_led1.wfl));
    if(op_ret != OPRT_OK){
        PR_ERR("wifi1 led init failed!");
        return op_ret;
    }
    io_config_debug(hw_table->wifi_stat_led1.wfl.io_cfg.type ,\
                    hw_table->wifi_stat_led1.wfl.io_cfg.pin, "wifi1_stat");	

    // 初始化电源状态指示灯
    op_ret = led_pin_reg(&(hw_table->power_stat_led));
    if(op_ret != OPRT_OK){
        PR_ERR("power led init failed!");
        return op_ret;
    }
    io_config_debug(hw_table->power_stat_led.io_cfg.type,\
                    hw_table->power_stat_led.io_cfg.pin,"power_stat");

    // 3.初始化总电源按键
    io_config_debug(hw_table->power_button.type,\
                    hw_table->power_button.key_cfg.port, "power_button");
    PR_NOTICE("trig_type:%s",(hw_table->channels[i].button.trig_t == EDGE_TYPE)?"edge":"level");
    op_ret = button_pin_init(&(hw_table->power_button), hw_key_process);
    if(op_ret != OPRT_OK){
        PR_ERR("power button init failed!", i);
        return op_ret;
    } 
//    Led_a11_init();
    
    return op_ret;
}

/***********************************************************
*  Function: 根据wifi指示灯状态,设置指示灯状态
*  Input:    wfl_stat  wifi指示灯状态
*  Output:
*  Return:
***********************************************************/
void hw_set_wfl_state(WFL_STAT      wfl_state)
{
    BOOL is_any_active = FALSE;
    INT_T i;

    if(WFL_OFF == wfl_state)
    {
        led_pin_set_stat(&(g_hw_table.wifi_stat_led.wfl ), FALSE);
    }
    else if(WFL_ON == wfl_state)
    {
        led_pin_set_stat(&(g_hw_table.wifi_stat_led.wfl), TRUE);
    }
    else if(wfl_state <= WFL_DIR_CHANNEL_ALL)
    {
    	wfl_state = wfl_state -2;
        for(i=0; i<g_hw_table.channel_num; ++i)
        {
        	if(wifi_dir_val[wfl_state] & wifi_dir_val[i]){
            	is_any_active = is_any_active || g_hw_table.channels[i].stat;
        	}
        }
        led_pin_set_stat(&(g_hw_table.wifi_stat_led.wfl), is_any_active);
		led_pin_set_stat(&(g_hw_table.wifi_stat_led1.wfl), is_any_active);
    }
}


/***********************************************************
*  Function: 根据wifi指示灯状态,设置指示灯状态
*  Input:    wfl_stat  wifi指示灯状态
*  Output:
*  Return:
***********************************************************/
void hw_set_power_led_state(VOID)
{
    BOOL is_any_active = FALSE;
    INT_T i;
	WFL_STAT wfl_state;

	if(g_hw_table.wifi_conn_stat == CONNECTED){
		wfl_state = g_hw_table.wifi_stat_led.wfl_connected;
	}
	else{
		wfl_state = g_hw_table.wifi_stat_led.wfl_not_connect;
	}

	
    if(WFL_OFF == wfl_state)
    {
        led_pin_set_stat(&(g_hw_table.wifi_stat_led.wfl ), FALSE);
		return;
    }
    else if(WFL_ON == wfl_state)
    {
        led_pin_set_stat(&(g_hw_table.wifi_stat_led.wfl), TRUE);
		return;
    }
    else if(wfl_state <= WFL_DIR_CHANNEL_ALL)
    {
    	wfl_state = wfl_state -2;
        for(i=0; i<g_hw_table.channel_num; ++i)
        {
        	if(wifi_dir_val[wfl_state] & wifi_dir_val[i]){
            	is_any_active = is_any_active || g_hw_table.channels[i].stat;
        	}
        }
    }

    //无电源指示灯
    if(IO_DRIVE_LEVEL_NOT_EXIST == g_hw_table.power_stat_led.io_cfg.type)
    {
        if((NOT_CONNECT == g_hw_table.wifi_conn_stat) &&\
           (g_hw_table.wifi_stat_led.wfl_not_connect >= WFL_DIR_CHANNEL1))
        {
            led_pin_set_stat(&(g_hw_table.wifi_stat_led.wfl), is_any_active);
			led_pin_set_stat(&(g_hw_table.wifi_stat_led1.wfl), is_any_active);
        }
        else if( (CONNECTED == g_hw_table.wifi_conn_stat) &&\
                 (g_hw_table.wifi_stat_led.wfl_connected>= WFL_DIR_CHANNEL1))
        {
            led_pin_set_stat(&(g_hw_table.wifi_stat_led.wfl), is_any_active);
			led_pin_set_stat(&(g_hw_table.wifi_stat_led1.wfl), is_any_active);
        }
        else
        {
            ;
        }
    }
    else
    {
        led_pin_set_stat(&(g_hw_table.power_stat_led), is_any_active);
    }
}

/***********************************************************
*  Function: 控制通道
*  Input:    hw_table:    硬件对象
             channel_no:  被控制的通道号 范围[0, 通道数-1]
             is_active:   通道是否有效 
*  Output:
*  Return:
***********************************************************/
void hw_set_channel(HW_TABLE *hw_table, int channel_no, BOOL is_active)
{

    if(hw_table == NULL)
    {
        PR_ERR("NULL pointer");
    }
    // 测试下标是否越界 下标范围[0, 通道数-1]
    if(channel_no < 0 || channel_no >= hw_table->channel_num)
    {
        PR_ERR("channel_no error: %d", channel_no);
        return;
    }
    // 控制继电器和状态指示灯
    out_pin_set_stat(hw_table->channels[channel_no].relay.io_cfg, is_active);
    led_pin_set_stat(&(hw_table->channels[channel_no].led), is_active);
    hw_table->channels[channel_no].stat = is_active;

    //停止倒计时
    hw_table->channels[channel_no].cd_msec = -1;
	
	// 控制电源指示灯
    hw_set_power_led_state();

}
/***********************************************************
*  Function: 翻转通道状态
*  Input:    hw_table:    硬件对象
             channel_no:  被控制的通道号 范围[0, 通道数-1]
*  Output:
*  Return:
***********************************************************/
void hw_trig_channel(HW_TABLE *hw_table, int channel_no)
{
    if(hw_table == NULL)
    {
        PR_ERR("NULL pointer");
    }
    // 按目前通道状态反置通道状态
    if(hw_table->channels[channel_no].stat == TRUE)
    {
        hw_set_channel(hw_table, channel_no, FALSE);
    }
    else
    {
        hw_set_channel(hw_table, channel_no, TRUE);
    }
}


extern void wifi_led_display(GW_WIFI_NW_STAT_E wifi_sta);

/***********************************************************
*  Function: 设置wifi灯状态
*  Input:    hw_table:    硬件对象
             wifi_stat:  wifi目前联网状态
*  Output:
*  Return:
***********************************************************/
void hw_set_wifi_led_stat(HW_TABLE *hw_table, GW_WIFI_NW_STAT_E wifi_stat)
{
    if(hw_table == NULL)
    {
        PR_ERR("NULL pointer");
    }
    switch(wifi_stat)
    {
    case STAT_UNPROVISION: 
        // 智能配网 快闪
        //g_hw_table.wifi_conn_stat = CONNECTING;
		wifi_led_display(STAT_UNPROVISION);
        break;
        
    case STAT_AP_STA_UNCFG:
        // AP配网 慢闪
        //g_hw_table.wifi_conn_stat = CONNECTING;
		wifi_led_display(STAT_AP_STA_UNCFG);
        break;
        
    case STAT_LOW_POWER:
    case STAT_AP_STA_DISC:
    case STAT_STA_DISC:
        // 低功耗及智能配网未连接
        //g_hw_table.wifi_conn_stat = NOT_CONNECT;
		wifi_led_display(STAT_LOW_POWER);
        break;
        
    case STAT_AP_STA_CONN:
    case STAT_STA_CONN:
        //g_hw_table.wifi_conn_stat = CONNECTED;
		wifi_led_display(STAT_AP_STA_CONN);
        break;
        
    case STAT_CLOUD_CONN: 
    case STAT_AP_CLOUD_CONN: {
        //g_hw_table.wifi_conn_stat = CONNECTED;
		wifi_led_display(STAT_CLOUD_CONN);
        upload_all_dp_stat();
        break;
    }
    default:{
       break;
    }
    }
}

/***********************************************************
*  Function: 通过pid设置相应通道状态
*  Input:    hw_table:    硬件对象
             dpid:        通道pid
             is_active    控制状态
*  Output:
*  Return:
***********************************************************/
int hw_set_channel_by_dpid(HW_TABLE *hw_table, int dpid, BOOL is_active)
{
    if(hw_table == NULL)
    {
        PR_ERR("NULL pointer");
        return -1;
    }
    int i;
    // 遍历通道列表
    for(i=0; i<hw_table->channel_num; ++i)
    {
        // 判断dpid是否一致
        if(dpid == hw_table->channels[i].dpid)
        {
            hw_set_channel(hw_table, i, is_active);
            return i;
        }
    }
    // 至此未返回说明未找到通道
    return -1;
}

/***********************************************************
*  Function: 通过dpid寻找对应的通道号
*  Input:    hw_table:    硬件对象
             dpid:        通道pid
*  Output:
*  Return:    对应的通道号        范围[0, 通道数-1]
             -1 为未找到通道
***********************************************************/
int hw_find_channel_by_cd_dpid(HW_TABLE *hw_table, int dpid)
{
    if(hw_table == NULL)
    {
        PR_ERR("NULL pointer");
        return -1;
    }
    int i;
    // 遍历通道列表
    for(i=0; i<hw_table->channel_num; ++i)
    {
        // 判断dpid是否一致
        if(dpid == hw_table->channels[i].cd_dpid)
        {
            return i;
        }
    }
    return -1;
}




