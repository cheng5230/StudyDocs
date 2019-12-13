/***********************************************************
File: hw_table.h 
Author: 徐靖航 JingHang Xu
Date: 2017-06-01
Description:
    开关插座等控制继电器开关为主的设备模板
    无倒计时 版本            
***********************************************************/
#define _TUYA_DEVICE_GLOBAL
#include "tuya_device.h"
#include "adapter_platform.h"
#include "tuya_iot_wifi_api.h"
#include "tuya_led.h"
#include "tuya_uart.h"
#include "tuya_gpio.h"
#include "tuya_key.h"
#include "hw_table.h"
#include "uni_time_queue.h"
#include "gw_intf.h"
#include "uni_log.h"
#include "uni_thread.h"
#include "gpio_test.h"
#include "uf_file.h"
#include "cJSON.h"
#include "gpio_api.h"   // mbed
#include "uni_thread.h"
#include "pwmout_api.h" 
#include "gpio_irq_api.h" 
#include "uni_mutex.h"

#include ".compile_usr_cfg.h"



/***********************************************************
*************************micro define***********************
***********************************************************/
//存储状态
//断电记忆
#define POWER_MEMORY           "power"
#define POWER_UP_STA_MEMORY    "power_up_sta"

#define LIGHT_VALUE_MEMORY    "light_value"



//上电状态存储
#define  STORE_CHANGE          "init_light_value_save"
#define  POWER_UP_STORE_CHANGE "init_power_up_stat_save"


#define WM_SUCCESS 0
#define WM_FAIL 1
#define GPIO_IRQ_PC6_PIN        PA_12
#define GPIO_IRQ_TOUCH_PIN      PA_23



#define HC164_DATA_H			tuya_gpio_inout_set(TY_GPIOA_0,1)
#define HC164_DATA_L			tuya_gpio_inout_set(TY_GPIOA_0,0)

#define HC164_CLK_H 			tuya_gpio_inout_set(TY_GPIOA_14,1)
#define HC164_CLK_L 			tuya_gpio_inout_set(TY_GPIOA_14,0)

#define SGRT010_SCL_H           tuya_gpio_inout_set(TY_GPIOA_18,1)
#define SGRT010_SCL_L           tuya_gpio_inout_set(TY_GPIOA_18,0)



#define CHANNEL_NUM  (g_hw_table.channel_num)

#define GWCM_MODE(wf_mode)  (((wf_mode) == LOW_POWER) ? (GWCM_LOW_POWER) : (GWCM_OLD_PROD) )
#define DEVICE_LOW_POWER_ENTER_IMEW	1000				//APP关闭后低功耗进入延时（等待灯光处理完毕）单位 ms
/***********************************************************
*************************typedef define***********************
***********************************************************/
//产测
typedef struct{
UCHAR_T      num;
UCHAR_T      relay_act_cnt;
TIMER_ID     relay_tm;
}PT_RELAY_ACT_INFOR;

/***********************************************************
*************************variable define********************
***********************************************************/
TIMER_ID wfl_timer;

BOOL is_save_stat = TRUE; // 断电记忆开

TIMER_ID save_stat_timer;
TIMER_ID led_flash_show_timer;
TIMER_ID upload_sta_timer;
TIMER_ID touch_irq_timer;




STATIC TIMER_ID __low_power_timer;// 倒计时定时器
int cd_upload_period = 30;// 倒计时状态上报周期 单位:秒
STATIC GW_WIFI_NW_STAT_E __wifi_stat;

#define PWM_1      			 _PA_15 
PinName  pwm_PC4_pin[1] =  {PWM_1};
pwmout_t pwm_PC4;
uint32_t PC4_period[8] = {1000,2000,3000,4000,5000,6000,7000,8000};
static uint32_t curr_period = 1000;
static BOOL_T   curr_power_switch = TRUE;
MUTEX_HANDLE mutex;
static BOOL_T is_write_read_flash = FALSE;
gpio_irq_t gpio_touch;



// 继电器动作定时器回调
TIMER_ID all_relay_tm;         // 所有继电器动作定时器句柄
PT_RELAY_ACT_INFOR  *pt_channel_tbl = NULL;

/***********************************************************
*************************function define********************
***********************************************************/
STATIC VOID key_process(TY_GPIO_PORT_E port,PUSH_KEY_TYPE_E type,INT_T cnt);
STATIC VOID wfl_timer_cb(UINT timerID,PVOID pTimerArg);
STATIC OPERATE_RET dp_upload_proc(CHAR_T *jstr);
// 状态同步回调   
void update_all_stat(void);

// 状态储存回调 用于断电记忆
STATIC VOID save_stat_timer_cb(UINT timerID,PVOID pTimerArg);
// 倒计时回调
STATIC VOID cd_timer_cb(UINT timerID,PVOID pTimerArg);
STATIC VOID prod_test(BOOL flag, CHAR_T rssi);
OPERATE_RET read_light_value(void);
STATIC VOID save_stat_timer_cb(UINT timerID,PVOID pTimerArg) ;

STATIC UINT_T __get_system_time_delt(UINT_T* last_sys_ms);
void light_pwm_pulsewidth_set(unsigned int period);
void _set_curr_period(uint32_t value);
void _led_display(uint8_t data,uint8_t *index);
OPERATE_RET light_gpio_init();

OPERATE_RET tuya_light_CreateAndStart(OUT THRD_HANDLE *pThrdHandle,\
                           IN  P_THRD_FUNC pThrdFunc,\
                           IN  PVOID pThrdFuncArg,\
                           IN  USHORT_T stack_size,\
                           IN  TRD_PRI pri,\
                           IN  UCHAR_T *thrd_name);

void test_thread();


// 产测相关代码 ======================================================================
// 继电器动作定时器回调


/***********************************************************
*  Function: app_init
*  Input: none
*  Output: none
*  Return: none
*  Note: called by user_main
***********************************************************/
VOID app_init(VOID) 
{
#if _IS_OEM
	PR_NOTICE("\n ENABLE OEM KEY: %s \n", PRODECT_KEY);
	tuya_iot_oem_set(TRUE);
#endif	
	app_cfg_set(GWCM_MODE(g_hw_table.wf_mode), prod_test);

}

/***********************************************************
*  Function: gpio_test
*  Input: none
*  Output: none
*  Return: none
*  Note: For production testing
***********************************************************/
BOOL_T gpio_test(VOID)
{
	return gpio_test_all();
}

VOID mf_user_callback(VOID)
{
    
}


/***********************************************************
*  Function: pre_device_init
*  Input: none
*  Output: none
*  Return: none
*  Note: to initialize device before device_init
***********************************************************/
VOID pre_device_init(VOID)
{

    if(WIFI_LED1_USR_TYPE != IO_DRIVE_LEVEL_NOT_EXIST)
    {
        PR_NOTICE("key:%d, relay:%d, led0:%d, led1:%d",POWER_BUTTON_USR_GPIO, RELAY_CONTROL_USR_GPIO, WIFI_LED_USR_GPIO, WIFI_LED1_USR_GPIO);
    }
    else
    {
        PR_NOTICE("key:%d, relay:%d, led:%d",POWER_BUTTON_USR_GPIO, RELAY_CONTROL_USR_GPIO, WIFI_LED_USR_GPIO);
    }
    PR_NOTICE("%s:%s:%s:%s",APP_BIN_NAME,USER_SW_VER,__DATE__,__TIME__);
    PR_NOTICE("%s",tuya_iot_get_sdk_info());	
#if USER_DEBUG_APP
		SetLogManageAttr(LOG_LEVEL_DEBUG);                                         //yao
		PR_DEBUG("\n USER SET LOG_LEVEL_DEBUG \n");
#else
		SetLogManageAttr(LOG_LEVEL_NOTICE);
		PR_DEBUG("\n USER SET LOG_LEVEL_NOTICE \n");
#endif	



}


//================产测相关代码 ============================//
#define PROD_TEST_RELAY_TRIG_NUM 6

/***********************************************************
*  Function: 单个继电器动作定时器回调
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
STATIC VOID single_relay_tm_cb(UINT timerID,PVOID pTimerArg)
{
    UCHAR_T num;

    PR_DEBUG("relay timer callback");
    // 置反目标通道

    num = *((UCHAR_T*)pTimerArg);
    
    PR_DEBUG("%d ch action", num);
    if(num >= CHANNEL_NUM)
    {
        PR_ERR("TimerArg Err !");
        return;
    }
    if(NULL == pt_channel_tbl)
    {
        PR_ERR("pt_channel_tbl pointer null!");
        return;
    }

    hw_trig_channel(&g_hw_table, num);
    pt_channel_tbl[num].relay_act_cnt++;

    // 如果执行少于三次则再次启动定时器
    if(pt_channel_tbl[num].relay_act_cnt < PROD_TEST_RELAY_TRIG_NUM){
        sys_start_timer(pt_channel_tbl[num].relay_tm, 500, TIMER_ONCE);
    }else{
        pt_channel_tbl[num].relay_act_cnt = 0;
    }
}

BOOL_T is_soft_reset_or_cnt_3(void)
{
    return TRUE;
}

VOID print_version_info(void)
{
    PR_DEBUG("%s",tuya_iot_get_sdk_info());
    PR_DEBUG("%s:%s",APP_BIN_NAME,DEV_SW_VERSION);
}

/***********************************************************
*  Function: 所有电器依次动作三次
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
STATIC VOID all_relay_tm_cb(UINT timerID,PVOID pTimerArg)
{
    static UCHAR_T act_cnt = 0;
    static UCHAR_T target_ch = 0;

    PR_DEBUG("all relay timer callback");
    
	if( target_ch < CHANNEL_NUM)
	{
		hw_trig_channel(&g_hw_table, target_ch);
	    act_cnt++;
  	}
    // 如果执行少于三次则再次启动定时器
    //所有通道都执行完毕
    if( target_ch >= CHANNEL_NUM ){
        act_cnt = 0;
        target_ch = 0;
    }
    //单通道三次未执行完毕
    else if(act_cnt < PROD_TEST_RELAY_TRIG_NUM){
        sys_start_timer(all_relay_tm, 500, TIMER_ONCE);
    }
    //单通道三次执行完毕，切换通道
    else{
        act_cnt = 0;
        target_ch++;
        sys_start_timer(all_relay_tm, 500, TIMER_ONCE);	
    }
}
/***********************************************************
*  Function: 产测时的按键回调
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
STATIC VOID pt_key_process(TY_GPIO_PORT_E port,PUSH_KEY_TYPE_E type,INT_T cnt)
{
    PR_DEBUG("gpio_no: %d",port);
    PR_DEBUG("type: %d",type);
    PR_DEBUG("cnt: %d",cnt);

    INT_T i;//通道下标 通道号为通道下标

    /*******总控******/
     if((g_hw_table.power_button.type !=  IO_DRIVE_LEVEL_NOT_EXIST)&&\
        (g_hw_table.power_button.key_cfg.port == port))
     {
        if(NORMAL_KEY == type){
			if(IsThisSysTimerRun(all_relay_tm)==false){
				sys_start_timer(all_relay_tm, 500, TIMER_ONCE);
		    }
        }
     }

    /*******分控*******/
    for(i=0; i<CHANNEL_NUM; i++)
    {
        if((g_hw_table.channels[i].button.type  !=  IO_DRIVE_LEVEL_NOT_EXIST)&&\
           (g_hw_table.channels[i].button.key_cfg.port == port))
        {
            if(NORMAL_KEY == type){
                if(IsThisSysTimerRun(pt_channel_tbl[i].relay_tm)==false){
                    sys_start_timer(pt_channel_tbl[i].relay_tm, 500, TIMER_ONCE);
                }
            }
        }
    }
     
}

/***********************************************************
*  Function: 产测回调
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
STATIC VOID prod_test(BOOL flag, CHAR_T rssi)
{
    INT_T i;
    BOOL is_single_ctrl = FALSE;


    // TODO: 信号值测试
    
    PR_NOTICE("product test mode");
    OPERATE_RET op_ret;// 注册操作的返回值
    // 注册产测需要使用的硬件 ----------------------------

    // 硬件配置初始化 配置完后所有通道默认无效
    op_ret = hw_init(&g_hw_table, pt_key_process);
    if(OPRT_OK != op_ret) {
        PR_ERR("op_ret:%d",op_ret);
        return;
    }
	
    PR_NOTICE("module have scaned the ssid, and enter product test...");
    PR_NOTICE("rssi value:%d ", rssi);
    if(flag == FALSE)
    {
		hw_set_wifi_led_stat(&g_hw_table, STAT_AP_STA_UNCFG);
        PR_ERR("no auth");
        return;
    }

    //设置通道初始状态
    for(i=0; i<CHANNEL_NUM; i++){
        hw_set_channel(&g_hw_table, i, TRUE);
    }
    
    // 主控继电器动作定时器
    if(g_hw_table.power_button.type !=  IO_DRIVE_LEVEL_NOT_EXIST)
    {
        op_ret = sys_add_timer(all_relay_tm_cb, NULL, &all_relay_tm);
        if(OPRT_OK != op_ret) {
            PR_ERR("add relay_tm err");
            return ;
        }
    }
    //判断是否存在分控
    for(i=0; i<CHANNEL_NUM; i++){
        if(g_hw_table.channels[i].button.type !=  IO_DRIVE_LEVEL_NOT_EXIST){
            is_single_ctrl = TRUE;
        }
    }
    //分控继电器动作定时器
    if(TRUE == is_single_ctrl)
    {
        pt_channel_tbl = (PT_RELAY_ACT_INFOR*)Malloc(CHANNEL_NUM * sizeof(PT_RELAY_ACT_INFOR));
        if(NULL == pt_channel_tbl){
            PR_ERR("Malloc Failed!");
            return;
        }
        memset((UCHAR_T*)pt_channel_tbl, 0x00, CHANNEL_NUM * sizeof(PT_RELAY_ACT_INFOR));
        for(i=0; i<CHANNEL_NUM; i++)
        {
            pt_channel_tbl[i].num = i; 
            if(g_hw_table.channels[i].button.type !=  IO_DRIVE_LEVEL_NOT_EXIST){
                op_ret = sys_add_timer(single_relay_tm_cb, &pt_channel_tbl[i].num, &pt_channel_tbl[i].relay_tm);
                if(OPRT_OK != op_ret) {
                    PR_ERR("add relay_tm err");
                    return ;
                }
            }
        }
    }
    
    // 注册结束 -----------------------------------------

    // 产测流程 - 步骤1 wifi指示灯控制为配网状态 快闪wifi状态指示灯
    hw_set_wifi_led_stat(&g_hw_table, STAT_UNPROVISION);
}

#if USER_DEFINE_LOWER_POWER
STATIC VOID lowpower_ctrl(BOOL_T enable)
{
    static UCHAR_T lowpower_enable = 0;
    PR_NOTICE("===============lowpower_ctrl %d=================",lowpower_enable);
    if(enable==1 && lowpower_enable == 0)
    {
        PR_NOTICE("device enter sleep mode");
        wf_lowpower_enable();
        lowpower_enable = 1;
        PR_NOTICE("===============lowpower_ctrl %d=================",lowpower_enable);
        return;
    }

    if(enable==0 && lowpower_enable == 1)
    {
        PR_NOTICE("device exit sleep mode");
        wf_lowpower_disable();
        lowpower_enable = 0;
        PR_NOTICE("===============lowpower_ctrl %d=================",lowpower_enable);
    }
    
}

STATIC VOID lowpower_timer_cb(UINT_T timerID, PVOID_T pTimerArg)
{
    if(__wifi_stat == STAT_UNPROVISION || __wifi_stat == STAT_AP_STA_UNCFG)
    {
        return;
    }
    PR_NOTICE("===============lowpower_timer_cb=================");
    lowpower_ctrl(TRUE);
}

STATIC VOID lowpower_disable()
{
    PR_NOTICE("===============lowpower_disable=================");
    lowpower_ctrl(FALSE);
    sys_start_timer(__low_power_timer, DEVICE_LOW_POWER_ENTER_IMEW, TIMER_ONCE);
}
#endif

/***********************************************************
*  Function: wifi状态改变回调
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
VOID status_changed_cb(IN CONST GW_STATUS_E status)
{
    PR_DEBUG("gw status changed to %d", status);
}

OPERATE_RET get_file_data_cb(IN CONST FW_UG_S *fw, IN CONST UINT_T total_len, IN CONST UINT_T offset,
                                     IN CONST BYTE_T *data, IN CONST UINT_T len, OUT UINT_T *remain_len, IN PVOID pri_data)
{
    PR_DEBUG("Rev File Data");
    PR_DEBUG("Total_len:%d ", total_len);
    PR_DEBUG("Offset:%d Len:%d", offset, len);

    return OPRT_OK;
}

VOID upgrade_notify_cb(IN CONST FW_UG_S *fw, IN CONST INT_T download_result, IN PVOID pri_data)
{
    PR_DEBUG("download  Finish");
    PR_DEBUG("download_result:%d", download_result);
}

VOID gw_ug_inform_cb(IN CONST FW_UG_S *fw)
{
    PR_DEBUG("Rev GW Upgrade Info");
    PR_DEBUG("fw->fw_url:%s", fw->fw_url);
//    PR_DEBUG("fw->fw_md5:%s", fw->fw_md5);                                   //yao
    PR_DEBUG("fw->sw_ver:%s", fw->sw_ver);
    PR_DEBUG("fw->file_size:%d", fw->file_size);

    tuya_iot_upgrade_gw(fw, get_file_data_cb, upgrade_notify_cb, NULL);
}

OPERATE_RET upload_power_switch_stat(BOOL_T value)
{
    OPERATE_RET op_ret;
    INT_T dp_cnt = 1;

    TY_OBJ_DP_S *dp_arr = (TY_OBJ_DP_S *)Malloc(dp_cnt*SIZEOF(TY_OBJ_DP_S));
    if(NULL == dp_arr) {
        PR_ERR("malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(dp_arr, 0, dp_cnt*SIZEOF(TY_OBJ_DP_S));

    dp_arr[0].dpid = DP_SWITCH;
    dp_arr[0].type = PROP_BOOL;
    dp_arr[0].time_stamp = 0;
    dp_arr[0].value.dp_bool = value;

    op_ret = dev_report_dp_json_async(get_gw_cntl()->gw_if.id,dp_arr,dp_cnt);
    Free(dp_arr);
    dp_arr = NULL;
    if(OPRT_OK != op_ret) {
        PR_ERR("upload_all_dp_stat op_ret:%d",op_ret);
    }

    return op_ret;
}

OPERATE_RET upload_light_contr_value(uint32_t value)
{
    OPERATE_RET op_ret;
    INT_T dp_cnt = 1;

    TY_OBJ_DP_S *dp_arr = (TY_OBJ_DP_S *)Malloc(dp_cnt*SIZEOF(TY_OBJ_DP_S));
    if(NULL == dp_arr) {
        PR_ERR("malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(dp_arr, 0, dp_cnt*SIZEOF(TY_OBJ_DP_S));

    dp_arr[0].dpid = DP_LIGHT_CONTR;
    dp_arr[0].type = PROP_VALUE;
    dp_arr[0].time_stamp = 0;
    dp_arr[0].value.dp_value = value/8;

    op_ret = dev_report_dp_json_async(get_gw_cntl()->gw_if.id,dp_arr,dp_cnt);
    Free(dp_arr);
    dp_arr = NULL;
    if(OPRT_OK != op_ret) {
        PR_ERR("upload_all_dp_stat op_ret:%d",op_ret);
    }

    return op_ret;
}


OPERATE_RET upload_all_dp_stat(VOID)                      //yao
{
    OPERATE_RET op_ret;
    INT_T count_sec = 0;
    INT_T ch_idx = 0, dp_idx = 0;
    INT_T dp_cnt = 2;

    TY_OBJ_DP_S *dp_arr = (TY_OBJ_DP_S *)Malloc(dp_cnt*SIZEOF(TY_OBJ_DP_S));
    if(NULL == dp_arr) {
        PR_ERR("malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(dp_arr, 0, dp_cnt*SIZEOF(TY_OBJ_DP_S));
	
    dp_arr[dp_idx].dpid = DP_SWITCH;
    dp_arr[dp_idx].type = PROP_BOOL;
    dp_arr[dp_idx].time_stamp = 0;
    dp_arr[dp_idx].value.dp_bool = TRUE;            //all 强制开

	dp_idx++;
	dp_arr[dp_idx].dpid = DP_LIGHT_CONTR;
	dp_arr[dp_idx].type = PROP_VALUE;
	dp_arr[dp_idx].time_stamp = 0;
	dp_arr[dp_idx].value.dp_value = curr_period/8;

	PR_DEBUG("dp_cnt =%d,dp_idx = %d\n",dp_cnt,dp_idx);
    op_ret = dev_report_dp_json_async(get_gw_cntl()->gw_if.id,dp_arr,dp_cnt);
    Free(dp_arr);
    dp_arr = NULL;
    if(OPRT_OK != op_ret) {
        PR_ERR("upload_all_dp_stat op_ret:%d",op_ret);
    }

    return op_ret;
}


/***********************************************************
*  Function: object类型dp数据解析，发送回复帧
*  Input:   dp 待解析的数据指针
*  Output: 
*  Return: 
***********************************************************/
VOID dev_obj_dp_cb(IN CONST TY_RECV_OBJ_DP_S *dp)
{
    INT_T i = 0,ch_index;
	OPERATE_RET op_ret;
#if USER_DEFINE_LOWER_POWER
    lowpower_disable();
#endif

    for(i = 0;i < dp->dps_cnt;i++) {
        PR_DEBUG("dpid:%d type:%d time_stamp:%d",dp->dps[i].dpid,dp->dps[i].type,dp->dps[i].time_stamp);

		if(dp->dps[i].dpid == DP_SWITCH){
			TY_OBJ_DP_S count_dp;
			count_dp.dpid = dp->dps[i].dpid;
			count_dp.type = PROP_BOOL;
			count_dp.time_stamp = 0;

			if(dp->dps[i].value.dp_bool == TRUE){
				light_switch_onoff(TRUE);
				led_display_by_dpvalue(curr_period);
			}
			else{
				light_switch_onoff(FALSE);
				led_display_by_dpvalue(0);
			}

			count_dp.value.dp_bool = dp->dps[i].value.dp_bool;
            op_ret = dev_report_dp_json_async(dp->cid,&count_dp,1);
            if(OPRT_OK != op_ret) {
                PR_ERR("dev_report_dp_json_async op_ret:%d",op_ret);
                break;
            }
		}
		else if(dp->dps[i].dpid == DP_LIGHT_CONTR){
			TY_OBJ_DP_S count_dp;
			uint32_t curr_value;
			count_dp.dpid = dp->dps[i].dpid;
			count_dp.type = PROP_VALUE;
			count_dp.time_stamp = 0;	

			if(dp->dps[i].value.dp_value < 0 || dp->dps[i].value.dp_value > 1000)
			{
				PR_ERR("err data %d",dp->dps[i].value.dp_value);
			}
			else{
				curr_value = dp->dps[i].value.dp_value * 8;
				_set_curr_period(curr_value);
				led_display_by_dpvalue(curr_value);
			}

			count_dp.value.dp_value = dp->dps[i].value.dp_value;
			op_ret = dev_report_dp_json_async(dp->cid,&count_dp,1);
			if(OPRT_OK != op_ret) {
				PR_ERR("dev_report_dp_json_async op_ret:%d",op_ret);
				break;
			}		
		}
    }

	sys_start_timer(save_stat_timer, 5000, TIMER_ONCE);
    PR_DEBUG("dp->cid:%s dp->dps_cnt:%d",dp->cid,dp->dps_cnt);
}
/***********************************************************
*  Function: raw类型dp数据解析
*  Input:   dp 待解析的数据指针
*  Output: 
*  Return: 
***********************************************************/
VOID dev_raw_dp_cb(IN CONST TY_RECV_RAW_DP_S *dp)
{
    PR_DEBUG("raw data dpid:%d",dp->dpid);

    PR_DEBUG("recv len:%d",dp->len);
    #if 0
    INT_T i = 0;
    
    for(i = 0;i < dp->len;i++) {
        PR_DEBUG_RAW("%02X ",dp->data[i]);
    }
    #endif
    PR_DEBUG_RAW("\n");
    PR_DEBUG("end");
}

/***********************************************************
*  Function: 设备查询命令回调
*  Input:   dp_qry 接收到的查询命令
*  Output: 
*  Return: 
***********************************************************/
VOID dev_dp_query_cb(IN CONST TY_DP_QUERY_S *dp_qry)
{
    PR_DEBUG("Recv DP Query Cmd");
}
/***********************************************************
*  Function: 获取wifi状态
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
STATIC VOID __get_wf_status(IN CONST GW_WIFI_NW_STAT_E stat)
{
#if USER_DEFINE_LOWER_POWER
    __wifi_stat = stat;
    lowpower_disable();
#endif    
    hw_set_wifi_led_stat(&g_hw_table, stat);
    return;
}

/***********************************************************
*  Function: 设备状态初始化
*  Input: 
*  Output:
*  Return:
***********************************************************/
OPERATE_RET dev_default_stat_init(VOID)
{
    INT_T i;
    OPERATE_RET ret;

	curr_power_switch = TRUE;
	_set_curr_period(1000);    //默认的值
    if( is_save_stat )
    {   // 状态储存定时器 用于断电记忆
        ret = sys_add_timer(save_stat_timer_cb, NULL, &save_stat_timer);
        if(OPRT_OK != ret) {
            return ret;
        }
    
       //读出所有通道状态
        ret = read_light_value();
        if(ret != OPRT_OK){
            PR_ERR("read stat failed!");
        }
    }

    return OPRT_OK;
}


OPERATE_RET tuya_light_CreateAndStart(OUT THRD_HANDLE *pThrdHandle,\
                           IN  P_THRD_FUNC pThrdFunc,\
                           IN  PVOID pThrdFuncArg,\
                           IN  USHORT_T stack_size,\
                           IN  TRD_PRI pri,\
                           IN  UCHAR_T *thrd_name)
{
	THRD_PARAM_S thrd_param;
    thrd_param.stackDepth = stack_size;
    thrd_param.priority = pri;
	memcpy(thrd_param.thrdname,thrd_name,strlen(thrd_name));
    return CreateAndStart(pThrdHandle, NULL, NULL, pThrdFunc, pThrdFuncArg, &thrd_param);
}


//74HC164 驱动

 /***把代码发送到移位寄存器***/
void send164( uint8_t udata ){
  uint8_t i;
  for( i = 0; i < 8; i ++ ){
    if( udata & 0x80 ){
      HC164_DATA_H;
      HC164_CLK_L;
      HC164_CLK_H;
    }else{
      HC164_DATA_L;
      HC164_CLK_L;
      HC164_CLK_H;
    }
    udata = udata<<1;
  }
}

uint8_t _get_164data(uint8_t indata,uint8_t *index){
	uint8_t outdata;
	int i,j;

	outdata = 0;
	for(i = 0;i < 8;i++){
		if(indata & (0x01<<i)){
			break;
		}
	}

	if(i != 8){
		for(j = 0;j < (7 - i);j++){
			outdata |= (0x01<<j);
		}
		*index = i;
	}
	else{
		for(j = 0;j < 8;j++){
			outdata |= (0x01<<j);
		}
		*index = 0;
	}

	return outdata;
}

void _led_display(uint8_t data,uint8_t *index){
	uint8_t data_164;

	data_164 = _get_164data(data,index);
	PR_NOTICE("send 164 data %d\n",data_164);
	send164(data_164);							//亮LED
}

void led_display_by_dpvalue(uint32_t period){
	uint8_t index;
	
	if(period){
		if(period >= 1000)
			_led_display(1<<(period/1000 - 1),&index);
		else
			_led_display(1,&index);

	}
	else{
		_led_display(0,&index);

	}
}

void led_show_timer_cb(UINT_T timerID, PVOID_T pTimerArg){
	static BOOL_T count = TRUE;
	uint8_t index;

	if(count){
		count = FALSE;
		_led_display(128,&index);     //全亮
		
	}else{
		count = TRUE;
		_led_display(0,&index);	  //全灭
	}

}

void wifi_led_display(GW_WIFI_NW_STAT_E wifi_sta){
	uint8_t index;

	if(wifi_sta == STAT_UNPROVISION){
		sys_start_timer(led_flash_show_timer, 250, TIMER_CYCLE);
		light_switch_onoff(FALSE);
	}
	else if(wifi_sta == STAT_AP_STA_UNCFG){
		sys_start_timer(led_flash_show_timer, 1500, TIMER_CYCLE);
		light_switch_onoff(FALSE);
	}
	else{
		if(IsThisSysTimerRun(led_flash_show_timer)){
			sys_stop_timer(led_flash_show_timer);
			_led_display(0,&index);
			light_switch_onoff(TRUE);
		}

		#if 0

		if(wifi_sta == STAT_STA_DISC){
			DelayMs(50);
			is_write_read_flash = TRUE;
		}
		else if(wifi_sta == STAT_CLOUD_CONN){
			is_write_read_flash = FALSE;
		}	
		#endif
	}

}

void light_pwm_pulsewidth_set(unsigned int period){
	uint8_t index;


#if 1
	if(period){
		if(period < 8000)
		   DelayUs(8000 - period);
		pwmout_start(&pwm_PC4);
	    pwmout_period_ms(&pwm_PC4, 10);            
	    pwmout_pulsewidth_us(&pwm_PC4, period);	

	}
	else{   					 // period 为0，关灯
		pwmout_stop(&pwm_PC4);
	}
#else
	
	pwmout_period_ms(&pwm_PC4, 10); 		   
	pwmout_pulsewidth_us(&pwm_PC4, period); 
#endif
}

void _set_curr_period(uint32_t value){

	MutexLock(mutex);
	curr_period = value;
	MutexUnLock(mutex);
}



void light_switch_onoff(BOOL_T onoff){

	if(onoff){
		_set_curr_period(curr_period);
		curr_power_switch = TRUE;
	}
	else{
		//_set_curr_period(0);
		curr_power_switch = FALSE;
	}
}


void upload_timer_cb(UINT_T timerID, PVOID_T pTimerArg){

	upload_all_dp_stat();
}


void test_thread(){

	while(1){
		DelayMs(1000);
		SystemSleep(100);
	}

}

/****************触摸线程********************/
void _light_thread_touch(){
	int i;
	uint8_t data,index;
	uint8_t bit_data;
	
	while(1){
		if(!tuya_gpio_read(TY_GPIOA_23)){
			DelayUs(40);
			data = 0;
			if(!tuya_gpio_read(TY_GPIOA_23)){ //防抖
				for(i = 0;i < 11;i++){		  // 发送11个周期的时钟信号
					SGRT010_SCL_L;
					DelayUs(60);
					SGRT010_SCL_H;
					if(i < 8){
						bit_data = !tuya_gpio_read(TY_GPIOA_23);
						data |=  (uint8_t)(bit_data << i);
					}
					DelayUs(50);
				}
				if(i == 11 && data){
					PR_NOTICE("get data %d\n",data);
					_led_display(data,&index);	
					//_get_164data(data,&index);
					_set_curr_period(PC4_period[index]);		 //设置curr_period，中断会根据值设置PWM，点亮灯
					//upload_light_contr_value(PC4_period[index]);
					//upload_all_dp_stat();
					sys_start_timer(upload_sta_timer, 10, TIMER_ONCE);
					sys_start_timer(save_stat_timer, 5000, TIMER_ONCE);
				}
			}
		}
		SystemSleep(1);
	}	
}


void gpio_PC6_irq_handler(){

	//sys_start_timer(pwm_timer, (10 - curr_period/1000), TIMER_ONCE);
	//DelayUs(10*1000 - curr_period);
	if(!is_write_read_flash){
		if(curr_power_switch){
			MutexLock(mutex);
			light_pwm_pulsewidth_set(curr_period);
			MutexUnLock(mutex);
		}
		else{
			light_pwm_pulsewidth_set(0);
		}
	}
}


void gpio_touch_irq_handle(){

	gpio_irq_disable(&gpio_touch);	
	sys_start_timer(touch_irq_timer, 1, TIMER_ONCE);
}


void touch_timer_cb(UINT_T timerID, PVOID_T pTimerArg){
	int i;
	uint8_t data,index;
	uint8_t bit_data;
    OPERATE_RET op_ret = OPRT_OK;	

	PR_NOTICE("touch message\n");
	
	data = 0;
	for(i = 0;i < 11;i++){		  // 发送11个周期的时钟信号
		SGRT010_SCL_L;
		DelayUs(60);
		SGRT010_SCL_H;
		if(i < 8){
			bit_data = !tuya_gpio_read(TY_GPIOA_23);
			data |=  (uint8_t)(bit_data << i);
		}
		DelayUs(50);
	}
	PR_NOTICE("\n get data %d\n",data);
	if(i == 11 && data){
		PR_NOTICE("get data %d\n",data);
		_led_display(data,&index);	
		_set_curr_period(PC4_period[index]);		 //设置curr_period，中断会根据值设置PWM，点亮灯
		//sys_start_timer(upload_sta_timer, 10, TIMER_ONCE);
		//sys_start_timer(save_stat_timer, 5000, TIMER_ONCE);
	}
    gpio_irq_enable(&gpio_touch);	
	
}

OPERATE_RET light_gpio_init(){
    OPERATE_RET op_ret = OPRT_OK;
    gpio_irq_t gpio_PC6;

	//A18  :触摸SCL 
	op_ret = tuya_gpio_inout_set(TY_GPIOA_18,FALSE);
	if(OPRT_OK != op_ret) {
		PR_ERR("tuya_gpio_18 init fail");		
		return op_ret;
	}
	op_ret = tuya_gpio_write(TY_GPIOA_18,FALSE);
	if(OPRT_OK != op_ret) {
		PR_ERR("tuya_gpio_18 write err:%d",op_ret);
		return op_ret;
	}

#if 1
	//A23  : 触摸SDA
	op_ret = tuya_gpio_inout_set(TY_GPIOA_23,TRUE);
	if(OPRT_OK != op_ret) {
		PR_ERR("tuya_gpio_23 init fail");		
		return op_ret;
	}
#endif	

	//A0 : HC164 LED
	op_ret = tuya_gpio_inout_set(TY_GPIOA_0,FALSE);
	if(OPRT_OK != op_ret) {
		PR_ERR("tuya_gpio_0 init fail");		
		return op_ret;
	}
	op_ret = tuya_gpio_write(TY_GPIOA_0,FALSE);
	if(OPRT_OK != op_ret) {
		PR_ERR("tuya_gpio_14 write err:%d",op_ret);
		return op_ret;
	}	

	//A14  :HC164 CLK 
	op_ret = tuya_gpio_inout_set(TY_GPIOA_14,FALSE);
	if(OPRT_OK != op_ret) {
		PR_ERR("tuya_gpio_14 init fail");		
		return op_ret;
	}
	op_ret = tuya_gpio_write(TY_GPIOA_14,FALSE);
	if(OPRT_OK != op_ret) {
		PR_ERR("tuya_gpio_14 write err:%d",op_ret);
		return op_ret;
	}	

    op_ret = CreateMutexAndInit(&mutex);
    if(OPRT_OK != op_ret) {
       PR_ERR("CreateMutexAndInit err ");
    }		

	pwmout_init(&pwm_PC4, pwm_PC4_pin[0]);
	pwmout_stop(&pwm_PC4);

	//A15 :PC4 下降沿触发调光
    gpio_irq_init(&gpio_PC6, GPIO_IRQ_PC6_PIN, gpio_PC6_irq_handler, NULL);
    gpio_irq_set(&gpio_PC6, IRQ_RISE, 1);   // Falling Edge Trigger
    gpio_irq_enable(&gpio_PC6);	

#if 0

	//A23
    gpio_irq_init(&gpio_touch, GPIO_IRQ_TOUCH_PIN, gpio_touch_irq_handle, NULL);
    gpio_irq_set(&gpio_touch, IRQ_FALL, 1);   // Falling Edge Trigger
    gpio_irq_enable(&gpio_touch);	
#else
	THRD_HANDLE thread_touch;
	op_ret = tuya_light_CreateAndStart(&thread_touch, _light_thread_touch, NULL,1024,TRD_PRIO_2,"sta_touch_task");
	if(op_ret != OPRT_OK) {
		PR_ERR("light_gpio_init err:%d",op_ret);
		return op_ret;
	}
	PR_NOTICE("\n tuya_light_CreateAndStart %d \n", op_ret);
#endif
	
	PR_NOTICE("gpio init success \n");

}

/***********************************************************
*  Function: device_init
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
OPERATE_RET device_init(VOID)
{
    OPERATE_RET op_ret = OPRT_OK;

    TY_IOT_CBS_S wf_cbs = {
        status_changed_cb,\
        gw_ug_inform_cb,\
        NULL,
        dev_obj_dp_cb,\
        dev_raw_dp_cb,\
        dev_dp_query_cb,\
        NULL,
    };

#if USER_DEFINE_LOWER_POWER
    PR_NOTICE("\n ENABLE LOW POWER \n");
	tuya_set_lp_mode(TRUE);
#endif	

	PR_DEBUG("g_hw_table.wf_mode:%d",g_hw_table.wf_mode);//GWCM_MODE(g_hw_table.wf_mode)
    op_ret = tuya_iot_wf_soc_dev_init_param(GWCM_MODE(g_hw_table.wf_mode),WF_START_SMART_FIRST,&wf_cbs,PRODECT_KEY,PRODECT_KEY,DEV_SW_VERSION);      //yao
    if(OPRT_OK != op_ret) {
        PR_ERR("tuya_iot_wf_soc_dev_init_param err:%d",op_ret);
        return op_ret;
    }

    op_ret = tuya_iot_reg_get_wf_nw_stat_cb(__get_wf_status);
    if(OPRT_OK != op_ret) {
        PR_ERR("tuya_iot_reg_get_wf_nw_stat_cb err:%d",op_ret);
        return op_ret;
    }

    // 硬件配置初始化 配置完后所有通道默认无效
    op_ret = hw_init(&g_hw_table, key_process);
    if(OPRT_OK != op_ret) {
        PR_ERR("hw_init:%d",op_ret);
        return op_ret;
    }	

	op_ret = light_gpio_init();
	if(OPRT_OK != op_ret) {
		PR_ERR("light_gpio_init err:%d",op_ret);
		return;
	}

    op_ret = sys_add_timer(led_show_timer_cb, NULL, &led_flash_show_timer);
    if(OPRT_OK != op_ret) {
        return op_ret;
    }	

    op_ret = sys_add_timer(upload_timer_cb, NULL, &upload_sta_timer);
    if(OPRT_OK != op_ret) {
        return op_ret;
    }
	
    op_ret = sys_add_timer(touch_timer_cb, NULL, &touch_irq_timer);
    if(OPRT_OK != op_ret) {
		PR_ERR("touch_irq_timer err:%d",op_ret);
        return op_ret;
    }	
	
    //设备状态初始化
    //判断是否需要开启断电记忆功能
    dev_default_stat_init();

	if(is_save_stat){          //重启之后保存状态，
		sys_start_timer(save_stat_timer, 5000, TIMER_ONCE);
	}	

#if USER_DEFINE_LOWER_POWER
	op_ret = sys_add_timer(lowpower_timer_cb, NULL, &__low_power_timer);
    if(OPRT_OK != op_ret) {
        return op_ret;
    }
#endif	

    INT_T size = SysGetHeapSize();//lql
	PR_NOTICE("device_init ok  free_mem_size:%d",size);
	
    return op_ret;
}


/***********************************************************
*  Function: 按键处理
*  Input: 
*  Output: 
*  Return: 
***********************************************************/
STATIC VOID key_process(TY_GPIO_PORT_E port,PUSH_KEY_TYPE_E type,INT_T cnt)
{
    INT_T i;//通道下标 
    char  *pbuff = NULL;
    cJSON *root = NULL;
    char name[10] = {0};

#if USER_DEFINE_LOWER_POWER
    lowpower_disable();
#endif

    PR_NOTICE("port: %d,type: %d,cnt: %d",port,type,cnt);

     {
         if(LONG_KEY == type) {
//            gw_wifi_reset(WRT_AUTO);	
			tuya_iot_wf_gw_unactive();
         }else if(NORMAL_KEY == type){
         
			if(curr_power_switch){
				light_switch_onoff(FALSE);
				led_display_by_dpvalue(0);
			}else{
				light_switch_onoff(TRUE);
				led_display_by_dpvalue(curr_period);
			}
	
			upload_power_switch_stat(curr_power_switch);
         }
         
         return;
     }
}

STATIC VOID save_stat_timer_cb(UINT timerID,PVOID pTimerArg)     //yao
{
	OPERATE_RET op_ret;
	INT_T i;
	cJSON *root = NULL, *js_power = NULL;
	UCHAR_T  *out = NULL;
	uint32_t write_data = 1000;
#if 0
    uFILE* file = ufopen(STORE_CHANGE, "w+");
    if(NULL == file) 
    {
        PR_ERR("ufopen failed");
        return OPRT_OPEN_FILE_FAILED;
    }


    root = cJSON_CreateObject();
    if(NULL == root)
    {
        PR_DEBUG("cJSON CreateObject Fail!");
        return;
    }
    js_power = cJSON_CreateArray();
    if(NULL == js_power)
    {
        PR_DEBUG("cJSON CreateArry Fail!");
        cJSON_Delete(root);
        return;
    }

	PR_DEBUG("save stat");
	
	MutexLock(mutex);
	write_data = curr_period;
	MutexUnLock(mutex);
	
    cJSON_AddNumberToObject(root, LIGHT_VALUE_MEMORY, write_data);
    out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if( NULL == out)
    {
        PR_ERR("cJSON_PrintUnformatted failed!");
        return ;
    }
	PR_DEBUG("%s", out);
	INT_T len = strlen(out);
	INT_T write_len = ufwrite(file, out, len);
	Free(out);
	out = NULL;
	if(len != write_len) 
	{
		PR_ERR("ufwrite fail write_len=%d",write_len); 
		return OPRT_WR_FLASH_ERROR;
	}
#else

	root = cJSON_CreateObject();
	if(NULL == root)
	{
		PR_DEBUG("cJSON CreateObject Fail!");
		return;
	}
	js_power = cJSON_CreateArray();
	if(NULL == js_power)
	{
		PR_DEBUG("cJSON CreateArry Fail!");
		cJSON_Delete(root);
		return;
	}

	PR_DEBUG("save stat");

	MutexLock(mutex);
	write_data = curr_period;
	MutexUnLock(mutex);	
	cJSON_AddNumberToObject(root, LIGHT_VALUE_MEMORY, write_data);

	out = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	if( NULL == out)
	{
		PR_ERR("cJSON_PrintUnformatted failed!");
		return ;
	}
	PR_DEBUG("%s", out);
	
	is_write_read_flash = TRUE;
	op_ret = wd_common_write(STORE_CHANGE,out,strlen(out));
	is_write_read_flash = FALSE;
	Free(out);
	if(OPRT_OK != op_ret) { 		
		PR_ERR("wd_common_write err:%d",op_ret);
		return op_ret;
	}

#endif
}

OPERATE_RET read_light_value(void)
{
    OPERATE_RET op_ret;
    cJSON *root = NULL, *js_power = NULL, *js_ch_stat = NULL;
    UCHAR_T *pbuff =NULL;
    int i,buff_len = 0;

#if 0
    UCHAR *buf = NULL;
    INT_T file_size = ufgetsize(STORE_CHANGE);
    if(0 == file_size) {
        PR_ERR("ufgetsize == 0");
        return OPRT_COM_ERROR;
    }
    
    //op_ret = tuya_light_common_flash_read(LIGHT_DATA_KEY, &buf);
    uFILE* file = ufopen(STORE_CHANGE, "r");
    if(NULL == file) {
        PR_ERR("ufopen failed");
        return OPRT_COM_ERROR;
    }
    
    buf = Malloc(file_size);
    if(NULL == buf)
    {
        PR_ERR("Malloc=%d failed", file_size);
        ufclose(file);
        return OPRT_COM_ERROR;
    }
    INT_T read_len =  ufread(file, buf, file_size);
    ufclose(file);
    file = NULL;
    if(0 == read_len) {
        PR_ERR("ufread failed read_len==0");
        Free(buf);
        return OPRT_COM_ERROR;
    }
    PR_ERR("ufread read_len %d, file_size %d",read_len, file_size);

    PR_DEBUG("read flash:[%s]",buf);

    // 分析json
    PR_DEBUG("read stat: %s", buf);
    root = cJSON_Parse(buf);
    Free(buf);
    // 如果json分析出错 退出函数
    if(root == NULL){
        PR_ERR("cjson parse err");
        return OPRT_COM_ERROR;
    }

    js_power = cJSON_GetObjectItem(root, LIGHT_VALUE_MEMORY);
    // 如果找不到power对象 退出函数
    if(NULL == js_power) {
        PR_ERR("cjson get power error");
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }

	if(js_power->valueint < 0 || js_power->valueint > 1000){
        PR_ERR("cjson get power error");
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
	}

	_set_curr_period((uint32_t)js_power->valueint);

	PR_DEBUG("read value: %d", js_power->valueint);

    cJSON_Delete(root);
    return OPRT_OK;
#else
	
	is_write_read_flash = TRUE;           			//读写flash，占用太长时间，暂停校准PWM
	op_ret = wd_common_read(STORE_CHANGE,&pbuff,&buff_len);	
	is_write_read_flash = FALSE;
	if(OPRT_OK != op_ret) {
		PR_ERR("wd_common_read err:%d",op_ret);
		if(pbuff != NULL){
			Free(pbuff);
			pbuff = NULL;
		}
		return op_ret;
	}

	// 分析json
	PR_DEBUG("read stat: %s", pbuff);
	root = cJSON_Parse(pbuff);
	Free(pbuff);
	pbuff = NULL;	
	// 如果json分析出错 退出函数
	if(root == NULL){
		PR_ERR("cjson parse err");
		return OPRT_COM_ERROR;
	}

	js_power = cJSON_GetObjectItem(root, LIGHT_VALUE_MEMORY);
	// 如果找不到power对象 退出函数
	if(NULL == js_power) {
		PR_ERR("cjson get curr_period error");
		cJSON_Delete(root);
		return OPRT_COM_ERROR;
	}

	if(js_power->valueint < 0 || js_power->valueint > 10000){
		PR_ERR("cjson get curr_period error");
		cJSON_Delete(root);
		return OPRT_COM_ERROR;
	}

	_set_curr_period((uint32_t)js_power->valueint);
	led_display_by_dpvalue((uint32_t)js_power->valueint);
	PR_DEBUG("read curr_period: %d", js_power->valueint);

	cJSON_Delete(root);
	return OPRT_OK;

#endif
}


STATIC UINT_T __get_system_time_delt(UINT_T* last_sys_ms)
{
    TIME_S secTime;
    TIME_MS msTime;
    uni_get_system_time(&secTime, &msTime);
    UINT_T delt_sec = secTime*1000 + msTime - *last_sys_ms;
    if(*last_sys_ms == 0)
    {
        delt_sec = 0;
    }
    *last_sys_ms = secTime*1000 + msTime;
    return delt_sec;
}


/***********************************************************
*  Function: 倒计时处理
*  Input:   
*  Output:
*  Return: 
***********************************************************/
STATIC VOID cd_timer_cb(UINT timerID,PVOID pTimerArg)
{
    UCHAR_T i = 0;// 通道号
    OPERATE_RET op_ret;

    // 遍历通道
    for(i = 0; i<g_hw_table.channel_num; i++) {
        if(g_hw_table.channels[i].cd_msec < 0) {
			g_hw_table.channels[i].timer_cnt= 0;
            continue;// 通道计时关闭
        }else {
#if USER_DEFINE_LOWER_POWER
            lowpower_disable();
#endif
            // 通道计时中
            g_hw_table.channels[i].cd_msec -= __get_system_time_delt(&g_hw_table.channels[i].last_sys_ms);
			PR_NOTICE("count down %d\n", g_hw_table.channels[i].cd_msec);
            if(g_hw_table.channels[i].cd_msec <= 0) {// 计时到达 
                g_hw_table.channels[i].cd_msec = -1; // 关闭通道定时
                // 置反通道状态
                g_hw_table.channels[i].timer_cnt = 0;
                hw_trig_channel(&g_hw_table, i);
                upload_channel_stat(i);
		        if(is_save_stat){
		            sys_start_timer(save_stat_timer, 5000, TIMER_ONCE);
		        }				
            }else {
                // 计时未到达
                // 每10 count 的整数倍上报一次
                ++g_hw_table.channels[i].timer_cnt;
                if(g_hw_table.channels[i].timer_cnt % 10 == 0) {
                    upload_channel_stat(i);
                }
            }
        }
    }

}

