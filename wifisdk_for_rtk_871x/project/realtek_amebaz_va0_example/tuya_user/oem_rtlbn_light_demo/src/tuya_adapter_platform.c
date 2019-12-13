/***********************************************************
*  File: tuya_adapter_platform.c
*  Author: mjl
*  Date: 20181106
***********************************************************/
#include "tuya_adapter_platform.h"
#include "tuya_iot_com_api.h"
#include "tuya_light_hard.h"
#include "tuya_device.h"
#include "tuya_common_light.h"
#include "tuya_light_lib.h"
#include "wf_basic_intf.h"

#define CHAN_NUM 5
#define PWM_MAX 1000

#define DEVICE_MOD "device_mod"

#define DEV_LIGHT_PROD_TEST "dev_prod_test"

extern UCHAR_T pwm_num;

STATIC UINT_T io_info[5];
STATIC gtimer_t hw_timer;
STATIC TUYA_GW_WIFI_NW_STAT_E wifi_stat = 0xff;

STATIC UCHAR_T user_pwm_channel_num;
STATIC UINT_T user_pwm_period;
typedef struct
{
    pwmout_t pwm_info[5];
    UINT_T io_info[5];
    BYTE_T pwm_free_io[5];
    UINT_T user_pwm_duty[5];
} PWM_PARAM_S;
STATIC PWM_PARAM_S pwm_param = {
    .pwm_free_io = {1, 1, 1, 1, 1}};

STATIC BOOL_T pwm_low_power_flag = FALSE;
extern BOOL_T light_light_dp_proc(TY_OBJ_DP_S *root);
extern VOID light_dp_data_autosave(VOID);
extern VOID light_dp_upload(VOID);
extern VOID light_dp_data_fast_default_set(VOID);
extern VOID light_dp_data_default_set(VOID);

/******************************************************************************
 * FunctionName : gpio_test
 * Description  : gpio检测
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
BOOL_T gpio_test(VOID)
{
    return gpio_test_cb(RTL_BOARD_WR3);
}

/******************************************************************************
 * FunctionName : tuya_light_pre_device_init
 * Description  : 灯的快速启动硬件配置
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_pre_device_init(VOID)
{
    sys_log_uart_off(); /* 关闭打印，否则可能会影响授权 */

    tuya_fast_boot_init(); /* 灯的快速启动设定！ */

    tuya_light_rst_count_add(); /* 读取重启的次数并+1 */

    wf_lowpower_disable();  /* 首先先关闭低功耗 */
    tuya_set_lp_mode(TRUE); /* 使能低功耗模式 */
}

/******************************************************************************
 * FunctionName : tuya_light_app_init
 * Description  : 灯的固件启动
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_app_init(VOID)
{
    OPERATE_RET op_ret;

    sys_log_uart_on();            /* 打开打印 */
    tuya_light_rst_count_judge(); /* 启动次数的判断 */

    GW_WF_NWC_FAST_STAT_T wifi_type;
    op_ret = tuya_iot_wf_fast_get_nc_type(&wifi_type);
    if (OPRT_OK != op_ret)
    {
        PR_NOTICE("Not Authorization !");
        return;
    }
    PR_NOTICE("wifi statue %d", wifi_type);

    if (wifi_type == GWNS_FAST_LOWPOWER)
    {
        tuya_light_config_stat(CONF_STAT_LOWPOWER);
    }
    else if (wifi_type == GWNS_FAST_UNCFG_SMC)
    {
        tuya_light_config_stat(CONF_STAT_SMARTCONFIG);
    }
    else if (wifi_type == GWNS_FAST_UNCFG_AP)
    {
        tuya_light_config_stat(CONF_STAT_APCONFIG);
    }
    else
    {
        //nothing to do
    }

    PR_DEBUG("%s", tuya_iot_get_sdk_info());      /* 输出SDK的版本信息 */
    PR_DEBUG("%s:%s", APP_BIN_NAME, USER_SW_VER); /* 输出固件的名字及版本 */
    SetLogManageAttr(LOG_LEVEL_NOTICE);           /* 正式版本，请将level设置为NOTICE，否则会影响授权！！！！ */

#ifdef _IS_OEM
    tuya_light_oem_set(TRUE);
#endif

    light_prod_init(PROD_TEST_TYPE_V2, prod_test);
}

/******************************************************************************
 * FunctionName : tuya_light_device_init
 * Description  : 灯的硬件的启动
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_device_init(VOID)
{

    tuya_light_smart_frame_init(USER_SW_VER);

    tuya_light_start(); /* 必须在云服务启动后调用 */

    return;
}

/******************************************************************************
 * FunctionName : tuya_light_oem_set
 * Description  : 设定oem固件的接口
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_oem_set(BOOL_T flag)
{
    tuya_iot_oem_set(flag);
}

/******************************************************************************
 * FunctionName : tuya_light_pwm_init
 * Description  : pwm的初期化 按照pin的列表初期化pwm
 * Parameters   :   period      --> 周期
                    duty        --> 占空比
                    pwm_channel_num
                    pin_info_list
 * Returns      : none
*******************************************************************************/
VOID tuya_light_pwm_init(IN UINT_T period, IN UINT_T *duty, IN UINT_T pwm_channel_num, IN UINT_T *pin_info_list)
{
    UCHAR_T i;

    user_pwm_channel_num = pwm_channel_num;
    user_pwm_period = period;

    for (i = 0; i < pwm_channel_num; i++)
    {
        pwmout_init(&(pwm_param.pwm_info[i]), pin_info_list[i]);
        pwmout_period_us(&(pwm_param.pwm_info[i]), period);
        pwmout_write(&(pwm_param.pwm_info[i]), 0);

        pwm_param.io_info[i] = pin_info_list[i];
        pwm_param.pwm_free_io[i] = 0;

        pwmout_stop(&(pwm_param.pwm_info[i])); /* 先关闭pwm，由于pwm初始化为0会存在问题！ */
    }
}

/******************************************************************************
 * FunctionName : pwm_set_duty
 * Description  : 
 * Parameters   :   period      --> 周期
                    duty        --> 占空比
                    pwm_channel_num
                    pin_info_list
 * Returns      : none
*******************************************************************************/
VOID pwm_set_duty(UINT_T duty, UCHAR_T channel)
{

    STATIC FLOAT_T _duty;

    pwm_param.user_pwm_duty[channel] = duty;
    if (duty != 0)
    {
        pwmout_start(&(pwm_param.pwm_info[channel]));
        if (pwm_param.pwm_free_io[channel] == 1) //如果该通道被free，需要重新初始化
        {
            pwmout_init(&(pwm_param.pwm_info[channel]), pwm_param.io_info[channel]);
            pwmout_period_us(&(pwm_param.pwm_info[channel]), user_pwm_period);

            pwm_param.pwm_free_io[channel] = 0;
        }
        _duty = (FLOAT_T)duty / 1000.0;
        pwmout_write(&(pwm_param.pwm_info[channel]), _duty);
    }
    else
    {
        pwmout_stop(&(pwm_param.pwm_info[channel]));
        //RTL8710 PWM不支持完全0%，需要手工切换到GPIO模式关闭
        pwmout_free(&(pwm_param.pwm_info[channel])); //暂时释放0%对应通道

        //对应free管切换到GPIO模式
        tuya_light_gpio_init(pwm_param.io_info[channel], TUYA_GPIO_OUT);
        tuya_gpio_mode_set(pwm_param.io_info[channel], TY_GPIO_PULLUP_PULLDOWN);

        //直接置0替换PWM = 0%
        tuya_light_gpio_ctl(pwm_param.io_info[channel], 0);

        pwm_param.pwm_free_io[channel] = 1;
    }
}

/******************************************************************************
 * FunctionName : tuya_light_send_data
 * Description  : 灯设定输出
 * Parameters   :   period      --> 周期
                    duty        --> 占空比
                    pwm_channel_num
                    pin_info_list
 * Returns      : none
*******************************************************************************/
VOID tuya_light_send_data(IN USHORT_T R_value, IN USHORT_T G_value, IN USHORT_T B_value, IN USHORT_T CW_value, IN USHORT_T WW_value)
{
    tuya_light_send_pwm_data(R_value, G_value, B_value, CW_value, WW_value);
}

OPERATE_RET tuya_light_common_flash_write(IN UCHAR_T *name, IN UCHAR_T *data, IN UINT_T len)
{
    OPERATE_RET op_ret;
    op_ret = wd_common_write(name, data, len);

    return op_ret;
}

OPERATE_RET tuya_light_common_flash_read(IN UCHAR_T *name, OUT UCHAR_T **data)
{
    OPERATE_RET op_ret;
    UINT_T len = 0;

    op_ret = wd_common_read(name, data, &len);
    PR_DEBUG("op_ret %d len %d %s", op_ret, len, *data);
    return op_ret;
}

OPERATE_RET tuya_light_prod_test_flash_write(IN UCHAR_T *data, IN UINT_T len)
{
    return tuya_light_common_flash_write(DEV_LIGHT_PROD_TEST, data, len);
}

OPERATE_RET tuya_light_prod_test_flash_read(OUT UCHAR_T **data)
{
    return tuya_light_common_flash_read(DEV_LIGHT_PROD_TEST, data);
}

STATIC VOID hw_test_timer_cb(UINT_T id)
{
    tuya_light_hw_timer_cb();
}

/******************************************************************************
 * FunctionName : tuya_light_hw_timer_init
 * Description  : 硬件定时器init
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_hw_timer_init(VOID)
{
    gtimer_init(&hw_timer, TIMER2);
    gtimer_start_periodical(&hw_timer, TUYA_LIGHT_HW_TIMER_CYCLE, (void *)hw_test_timer_cb, (uint32_t)&hw_timer);
    gtimer_start(&hw_timer);
}

VOID tuya_light_enter_lowpower_mode(VOID)
{
    UCHAR_T i;

    gtimer_stop(&hw_timer);

    if (pwm_low_power_flag == FALSE)
    {
        for (i = 0; i < user_pwm_channel_num; i++)
        {
            pwmout_stop(&(pwm_param.pwm_info[i]));
        }
        tuya_light_pwm_stop_sta_set();

        pwm_low_power_flag = TRUE;
    }

    wf_lowpower_enable();

    PR_NOTICE("light enter sleep mode");
}

VOID tuya_light_exit_lowpower_mode(VOID)
{
    UCHAR_T i;

    wf_lowpower_disable();

    if (pwm_low_power_flag == TRUE)
    {
        for (i = 0; i < user_pwm_channel_num; i++)
        {
            pwmout_init(&(pwm_param.pwm_info[i]), pwm_param.io_info[i]);
            pwmout_period_us(&(pwm_param.pwm_info[i]), user_pwm_period);
            pwm_set_duty(pwm_param.user_pwm_duty[i], i);
        }

        for (i = 0; i < user_pwm_channel_num; i++)
        {
            pwmout_start(&(pwm_param.pwm_info[i]));
        }

        pwm_low_power_flag == FALSE;
    }

    gtimer_start(&hw_timer);

    PR_NOTICE("light exit sleep mode");
}

BOOL_T tuya_light_dev_poweron(VOID)
{
    CHAR_T *rst_inf = system_get_rst_info();
    CHAR_T rst_num = atoi(rst_inf + 23);
    PR_DEBUG("rst_inf->reaso is %d", rst_num);
    if (rst_num == REASON_DEFAULT_RST)
    {
        return TRUE;
    }
    return FALSE;
}

OPERATE_RET tuya_light_CreateAndStart(OUT TUYA_THREAD *pThrdHandle,
                                      IN P_THRD_FUNC pThrdFunc,
                                      IN PVOID pThrdFuncArg,
                                      IN USHORT_T stack_size,
                                      IN TRD_PRI pri,
                                      IN UCHAR_T *thrd_name)
{
    THRD_PARAM_S thrd_param;
    thrd_param.stackDepth = stack_size;
    thrd_param.priority = pri;
    thrd_param.thrdname = Malloc(32);
    memcpy(thrd_param.thrdname, thrd_name, strlen(thrd_name));
    return CreateAndStart(pThrdHandle, NULL, NULL, pThrdFunc, pThrdFuncArg, &thrd_param);
}

/******************************************************************************
 * FunctionName : tuya_light_print_port_init
 * Description  : realtek 平台上打印的初期化
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_print_port_init(VOID)
{
}

UCHAR_T *tuya_light_ty_get_enum_str(DP_CNTL_S *dp_cntl, UCHAR_T enum_id)
{
    if (dp_cntl == NULL)
    {
        return NULL;
    }

    if (enum_id >= dp_cntl->prop.prop_enum.cnt)
    {
        return NULL;
    }

    return dp_cntl->prop.prop_enum.pp_enum[enum_id];
}

/******************************************************************************
 * FunctionName : device_cb
 * Description  : 设备DP点处理的回调
 * Parameters   :   period      --> 周期
 * Returns      : none
*******************************************************************************/
STATIC VOID device_cb(IN CONST TY_RECV_OBJ_DP_S *dp)
{
    BOOL_T op_led = FALSE;
    BOOL_T op_temp = FALSE;
    UCHAR_T i = 0;

    if (NULL == dp)
    {
        PR_ERR("dp error");
        return;
    }

    UCHAR_T nxt = dp->dps_cnt;
    PR_DEBUG("dp_cnt:%d", nxt);

    for (i = 0; i < nxt; i++)
    {
        op_temp = light_light_dp_proc(&(dp->dps[i]));
        if (TRUE == op_temp)
        {
            op_led = TRUE;
        }
    }

    PR_DEBUG("op_led=%d", op_led);

    if (op_led == TRUE)
    { /* 如果下发有效数据，就启动数据保存的timer并且上报 */
        light_dp_data_autosave();
        light_dp_upload();
    }
}

STATIC VOID reset_cb(GW_RESET_TYPE_E type)
{
    if ((type == GW_REMOTE_UNACTIVE) || (type == GW_REMOTE_RESET_FACTORY))
    {
        light_dp_data_default_set(); /* reset & save into flash */
    }
    else
    {
        light_dp_data_fast_default_set();
    }
}

/******************************************************************************
 * FunctionName : tuya_light_smart_frame_init
 * Description  : sdk的注册初期化
 * Parameters   : CHAR_T *sw_ver --> 固件的版本
 * Returns      : none
*******************************************************************************/
OPERATE_RET tuya_light_smart_frame_init(CHAR_T *sw_ver)
{
    OPERATE_RET op_ret;

    TY_IOT_CBS_S wf_cbs = {
        NULL,
        NULL,
        reset_cb,
        device_cb,
        NULL,
        NULL,
        NULL,
    };

    /* wifi设备初期化 */
    /* 设定 WF_START_SMART_FIRST 上电默认为EZ配网方式            */

    PR_NOTICE("wifi cfg", tuya_light_get_wifi_cfg());
    op_ret = tuya_iot_wf_soc_dev_init_param(tuya_light_get_wifi_cfg(), WF_START_SMART_FIRST, &wf_cbs, PRODECT_KEY, PRODECT_KEY, sw_ver);
    if (OPRT_OK != op_ret)
    {
        PR_ERR("tuya_iot_wf_soc_dev_init err:%02x", op_ret);
        return OPRT_COM_ERROR;
    }

    op_ret = tuya_light_reg_get_wf_nw_stat_cb(); /* 注册获取wifi状态的接口 */

    if (OPRT_OK != op_ret)
    {
        PR_ERR("tuya_light_reg_get_wf_nw_stat_cb err:%02x", op_ret);
        return op_ret;
    }
}

DEV_CNTL_N_S *tuya_light_get_dev_cntl(VOID)
{
    DEV_CNTL_N_S *dev_cntl;
    dev_cntl = get_gw_cntl()->dev;
    return dev_cntl;
}

OPERATE_RET tuya_light_dp_report(CHAR_T *out)
{
    return sf_obj_dp_report_async(get_gw_cntl()->gw_if.id, out, FALSE);
}

STATIC VOID key_process(TY_GPIO_PORT_E port, PUSH_KEY_TYPE_E type, INT_T cnt)
{
    tuya_light_key_process(port, type, cnt);
}

OPERATE_RET tuya_light_button_reg(IN UINT_T gpio_no, IN BOOL_T is_high, IN UINT_T long_key_time, KEY_CALLBACK callback)
{
    OPERATE_RET ret;

    KEY_USER_DEF_S button;

    button.port = gpio_no;
    button.low_level_detect = !is_high;
    button.call_back = callback;
    button.long_key_time = long_key_time;
    button.seq_key_detect_time = 0;
    button.lp_tp = LP_ONCE_TRIG;

    ret = reg_proc_key(&button);

    return ret;
}
INT_T tuya_light_get_free_heap_size(VOID)
{
    return SysGetHeapSize();
}

/******************************************************************************
 * FunctionName : set_wf_gw_status
 * Description  : tuya SDK wifi状态变更回调，并设定局部变量wifi_stat
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID set_wf_gw_status(IN CONST GW_WIFI_NW_STAT_E stat)
{
    wifi_stat = stat;
    PR_DEBUG("wifi_stat = %d", wifi_stat);

    tuya_light_wf_stat_cb(wifi_stat); /* 根据wifi状态来处理--显示式样等 */
}

/******************************************************************************
 * FunctionName : tuya_light_reg_get_wf_nw_stat_cb
 * Description  : 灯获取wifi状态的回调注册
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
OPERATE_RET tuya_light_reg_get_wf_nw_stat_cb(VOID)
{
    OPERATE_RET op_ret;

    op_ret = tuya_iot_reg_get_wf_nw_stat_cb(set_wf_gw_status);
    if (OPRT_OK != op_ret)
    {
        PR_ERR("tuya_iot_reg_get_wf_nw_stat_cb err:%02x", op_ret);
        return op_ret;
    }
}

/******************************************************************************
 * FunctionName : tuya_light_get_wf_gw_status
 * Description  : 灯获取wifi的状态
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
TUYA_GW_WIFI_NW_STAT_E tuya_light_get_wf_gw_status(VOID)
{
    return wifi_stat;
}

BYTE_T tuya_light_get_gw_status(VOID)
{
    PR_DEBUG("wifi_stat = %d", wifi_stat);
    if ((STAT_UNPROVISION == wifi_stat) || (STAT_AP_STA_UNCFG == wifi_stat))
    {
        return TUYA_LIGHT_UN_ACTIVE;
    }
    return TUYA_LIGHT_ACTIVE;
}

/******************************************************************************
 * FunctionName : tuya_light_get_gw_mq_conn_stat
 * Description  : 根据wifi_stat 状态判断是否mqtt连接成功
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
BOOL_T tuya_light_get_gw_mq_conn_stat(VOID)
{
    if (STAT_CLOUD_CONN == wifi_stat || STAT_AP_CLOUD_CONN == wifi_stat)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/******************************************************************************
 * FunctionName : tuya_light_dev_reset
 * Description  : tuya light设备移除
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_dev_reset(VOID)
{
    //gw_wifi_reset(WRT_AUTO);
    tuya_iot_wf_gw_fast_unactive(tuya_light_get_wifi_cfg(), WF_START_SMART_FIRST);
}

/******************************************************************************
 * FunctionName : tuya_light_gpio_init
 * Description  : GPIO 输入输出的设定
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
OPERATE_RET tuya_light_gpio_init(UINT_T gpio, BYTE_T type)
{
    if (type == TUYA_GPIO_IN)
    {
        return tuya_gpio_inout_set(gpio, TRUE);
    }
    else if (type == TUYA_GPIO_OUT)
    {
        return tuya_gpio_inout_set(gpio, FALSE);
    }
}

/******************************************************************************
 * FunctionName : tuya_light_gpio_read
 * Description  : gpio端口的状态读取
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
INT_T tuya_light_gpio_read(UINT_T gpio)
{
    return tuya_gpio_read(gpio);
}

/******************************************************************************
 * FunctionName : tuya_light_gpio_ctl
 * Description  : gpio端口的设定
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
OPERATE_RET tuya_light_gpio_ctl(UINT_T gpio, BYTE_T high)
{
    if (high == TUYA_GPIO_LOW)
    {
        return tuya_gpio_write(gpio, FALSE);
    }
    else if (high == TUYA_GPIO_HIGH)
    {
        return tuya_gpio_write(gpio, TRUE);
    }
}

/******************************************************************************
 * FunctionName : tuya_ws_db_user_param_read
 * Description  : 灯的json配置文件的读取接口
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
OPERATE_RET tuya_ws_db_user_param_read(OUT BYTE_T **buf, OUT UINT_T *len)
{
    return ws_db_user_param_read(buf, len);
}
