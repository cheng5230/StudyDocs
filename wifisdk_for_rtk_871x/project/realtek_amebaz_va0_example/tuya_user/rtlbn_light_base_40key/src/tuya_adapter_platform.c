/***********************************************************
*  File: tuya_adapter_platform.c
*  Author: mjl
*  Date: 20181106
***********************************************************/
#include ".compile_usr_cfg.h"
#include "tuya_adapter_platform.h"
#include "tuya_common_light.h"
#include "tuya_device.h"
#include "tuya_light_lib.h"
#include "tuya_hw_table.h"
#include "wf_basic_intf.h"

#define CHAN_NUM 5
#define PWM_MAX 1023

extern VOID tuya_light_app_init (VOID);
extern VOID tuya_light_device_init (VOID);
extern BOOL_T light_light_dp_proc (TY_OBJ_DP_S* root);
extern VOID light_dp_data_autosave (VOID);
extern VOID light_dp_upload (VOID);

/***************************************************************************************/
/***************************************************************************************/

/*                  RLTK_AMEBAZ_87XX                  /

/***************************************************************************************/
/***************************************************************************************/

#define DEVICE_MOD        "device_mod"
#define DEV_LIGHT_PROD_TEST   "dev_prod_test"

extern UCHAR_T pwm_num;

STATIC UINT_T io_info[5];
STATIC gtimer_t hw_timer;
STATIC TUYA_GW_WIFI_NW_STAT_E wifi_stat = 0xff;
STATIC pwmout_t pwm_info[5];
STATIC UCHAR_T user_pwm_channel_num;
STATIC UINT_T user_pwm_period;
STATIC UINT_T user_pwm_duty[5];
STATIC BOOL_T pwm_low_power_flag = FALSE;


extern VOID tuya_light_device_init (VOID);

BOOL_T gpio_test (VOID)
{
  return gpio_test_cb (RTL_BOARD_WR3);
}

VOID pre_device_init (VOID)
{
  tuya_light_pre_device_init();
}

VOID app_init (VOID)
{
  tuya_light_app_init();
}

/******************************************************************************
 * FunctionName : device_init
 * Description  : 应用设备初期化
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
OPERATE_RET device_init (VOID)
{
  tuya_light_device_init();
  return OPRT_OK;
}

VOID tuya_light_oem_set (BOOL_T flag)
{
  tuya_iot_oem_set (flag);
}

VOID tuya_light_set_pwm_mute (IN UINT_T pwm_channel_num, IN  UINT_T* pin_info_list)
{
  UCHAR_T i;

  for (i=0; i<pwm_channel_num; i++)
  {
    gpio_t gpio_g;
    gpio_init (&gpio_g, pin_info_list[i]);
    gpio_set (pin_info_list[i]);
    gpio_mode (&gpio_g, PullDown);
    gpio_dir (&gpio_g, PIN_OUTPUT);
    gpio_write (&gpio_g, 0);
  }
}


VOID tuya_light_pwm_init (IN UINT_T period, IN  UINT_T* duty, IN UINT_T pwm_channel_num, IN  UINT_T* pin_info_list)
{
  UCHAR_T i;
  user_pwm_channel_num = pwm_channel_num;
  user_pwm_period = period;

  for (i=0; i<pwm_channel_num; i++)
  {
    pwmout_init (&pwm_info[i], pin_info_list[i]);
    pwmout_period_us (&pwm_info[i], period);
    pwmout_pulsewidth_us (&pwm_info[i], *duty);
    io_info[i] = pin_info_list[i];
  }
}

VOID pwm_set_duty (UINT_T duty, UCHAR_T channel)
{
  STATIC FLOAT_T  _duty;
  _duty = (FLOAT_T) duty / 1023.0;
  pwmout_write (&pwm_info[channel], _duty);
  user_pwm_duty[channel] = duty;
}

VOID tuya_light_pwm_channel_reverse_set (UCHAR_T channel)
{
  ;
}

USHORT_T tuya_light_pwm_set_hubu (USHORT_T WW_value)
{
  HW_TABLE_S* hw_table = get_hw_table();
  USHORT_T value;
  value = WW_value;
  return value;
}

VOID tuya_light_pwm_send_init (VOID)
{
  ;
}


VOID tuya_light_send_data (IN USHORT_T R_value, IN  USHORT_T G_value, IN  USHORT_T B_value, IN USHORT_T CW_value,
                           IN USHORT_T WW_value)
{
  tuya_light_send_pwm_data (R_value, G_value, B_value, CW_value, WW_value);
}

VOID tuya_prod_light_send_data (IN USHORT_T R_value, IN  USHORT_T G_value, IN  USHORT_T B_value, IN USHORT_T CW_value,
                                IN USHORT_T WW_value)
{
  tuya_prod_light_send_pwm_data (R_value, G_value, B_value, CW_value, WW_value);
}

OPERATE_RET tuya_light_common_flash_init (VOID)
{
  return ws_db_init (DEVICE_MOD, NULL, NULL);
  PR_DEBUG ("tuya_light_common_flash_init ret 0");
  return 0;
}

OPERATE_RET tuya_light_common_flash_write (IN UCHAR_T* name, IN  UCHAR_T* data, IN  UINT_T len)
{
  OPERATE_RET op_ret;
  op_ret = wd_common_write (name, data, len);
  return op_ret;
}

OPERATE_RET tuya_light_common_flash_read (IN         UCHAR_T* name, OUT UCHAR_T** data)
{
  OPERATE_RET op_ret;
  UINT_T len;
  op_ret = wd_common_read (name, data, &len);
  PR_DEBUG ("op_ret %d len %d %s", op_ret, len, *data);
  return op_ret;
}
OPERATE_RET tuya_light_prod_test_flash_write (IN          UCHAR_T* data, IN  UINT_T len)
{
  return tuya_light_common_flash_write (DEV_LIGHT_PROD_TEST, data, len);
}
OPERATE_RET tuya_light_prod_test_flash_read (OUT UCHAR_T** data)
{
  return tuya_light_common_flash_read (DEV_LIGHT_PROD_TEST, data);
}

STATIC VOID hw_test_timer_cb (uint32_t id)
{
  tuya_light_hw_timer_cb();
}

VOID tuya_light_hw_timer_init (VOID)
{
  gtimer_init (&hw_timer, TIMER2);
  gtimer_start_periodical (&hw_timer, TUYA_LIGHT_HW_TIMER_CYCLE, (void*) hw_test_timer_cb, (uint32_t) &hw_timer);
  gtimer_start (&hw_timer);
}

VOID tuya_light_enter_lowpower_mode (VOID)
{
  u8_t i;
  gtimer_stop (&hw_timer);

  if (pwm_low_power_flag == FALSE)
  {
    for (i=0; i<user_pwm_channel_num; i++)
    {
      pwmout_stop (&pwm_info[i]);
    }

    tuya_light_pwm_stop_sta_set();
    pwm_low_power_flag = TRUE;
  }

  wf_lowpower_enable();
  PR_NOTICE ("light enter sleep mode");
}

VOID tuya_light_exit_lowpower_mode (VOID)
{
  u8_t i;
  wf_lowpower_disable();

  if (pwm_low_power_flag == TRUE)
  {
    for (i = 0; i < user_pwm_channel_num; i ++)
    {
      pwmout_init (&pwm_info[i], io_info[i]);
      pwmout_period_us (&pwm_info[i], user_pwm_period);
      pwm_set_duty (user_pwm_duty[i], i);
    }

    for (i=0; i<user_pwm_channel_num; i++)
    {
      pwmout_start (&pwm_info[i]);
    }

    pwm_low_power_flag == FALSE;
  }

  gtimer_start (&hw_timer);
  PR_NOTICE ("light exit sleep mode");
}

BOOL tuya_light_dev_poweron (VOID)
{
  CHAR_T* rst_inf = system_get_rst_info();
  CHAR_T rst_num = atoi (rst_inf+23);
  PR_DEBUG ("rst_inf->reaso is %d", rst_num);

  if (rst_num == REASON_DEFAULT_RST)
  {
    return TRUE;
  }

  return FALSE;
}

OPERATE_RET tuya_light_CreateAndStart (OUT TUYA_THREAD* pThrdHandle, \
                                       IN  P_THRD_FUNC pThrdFunc, \
                                       IN  PVOID pThrdFuncArg, \
                                       IN  USHORT_T stack_size, \
                                       IN  TRD_PRI pri, \
                                       IN  UCHAR_T* thrd_name)
{
  THRD_PARAM_S thrd_param;
  thrd_param.stackDepth = stack_size;
  thrd_param.priority = pri;
  memcpy (thrd_param.thrdname, thrd_name, strlen (thrd_name) );
  return CreateAndStart (pThrdHandle, NULL, NULL, pThrdFunc, pThrdFuncArg, &thrd_param);
}

VOID tuya_light_print_port_init (VOID)
{
  ;
}


UCHAR_T* tuya_light_ty_get_enum_str (DP_CNTL_S* dp_cntl, UCHAR_T enum_id)
{
  if ( dp_cntl == NULL )
  {
    return NULL;
  }

  if ( enum_id >= dp_cntl->prop.prop_enum.cnt )
  {
    return NULL;
  }

  return dp_cntl->prop.prop_enum.pp_enum[enum_id];
}

STATIC VOID device_cb (IN CONST TY_RECV_OBJ_DP_S* dp)
{
  BOOL op_led = FALSE;
  BOOL op_temp = FALSE;

  if (NULL == dp)
  {
    PR_ERR ("dp error");
    return;
  }

  UCHAR_T nxt = dp->dps_cnt;
  PR_DEBUG ("dp_cnt:%d", nxt);

  for (UCHAR_T i=0; i<nxt; i++)
  {
    op_temp = light_light_dp_proc (& (dp->dps[i]) );

    if (op_temp==TRUE)
    {
      op_led = TRUE;
    }
  }

  PR_DEBUG ("op_led=%d", op_led);

  if (op_led == TRUE)
  {
    light_dp_data_autosave();
    light_dp_upload();
  }
}

STATIC VOID reset_cb (GW_RESET_TYPE_E type)
{
  light_dp_data_default_set(); /* reset & save into flash */
}
#if _IS_OEM
  OPERATE_RET tuya_light_smart_frame_init (CHAR_T* product_key, CHAR_T* sw_ver)
#else
  OPERATE_RET tuya_light_smart_frame_init (CHAR_T* sw_ver)
#endif
{
  OPERATE_RET op_ret;
  TY_IOT_CBS_S wf_cbs =
  {
    NULL, \
    NULL, \
    reset_cb, \
    device_cb, \
    NULL, \
    NULL, \
    NULL,
  };
#if _IS_OEM
  /* 注册tuya sdk中的sdk */
  op_ret = tuya_iot_wf_soc_dev_init_param (light_get_wifi_cfg(), WF_START_SMART_FIRST, &wf_cbs, product_key, product_key,
                                           sw_ver);
#else
  /* 注册tuya sdk中的sdk */
  op_ret = tuya_iot_wf_soc_dev_init_param (light_get_wifi_cfg(), WF_START_SMART_FIRST, &wf_cbs, PRODECT_KEY, PRODECT_KEY,
                                           sw_ver);
#endif

  if (OPRT_OK != op_ret)
  {
    PR_ERR ("tuya_iot_wf_soc_dev_init err:%d", op_ret);
    return OPRT_COM_ERROR;
  }

  op_ret = tuya_light_reg_get_wf_nw_stat_cb();

  if (OPRT_OK != op_ret)
  {
    PR_ERR ("tuya_light_reg_get_wf_nw_stat_cb err:%d", op_ret);
    return op_ret;
  }
}

DEV_CNTL_N_S* tuya_light_get_dev_cntl (VOID)
{
  DEV_CNTL_N_S* dev_cntl;
  dev_cntl = get_gw_cntl()->dev;
  return dev_cntl;
}

OPERATE_RET tuya_light_dp_report (CHAR_T* out)
{
  return sf_obj_dp_report_async (get_gw_cntl()->gw_if.id, out, FALSE);
}

STATIC VOID key_process (TY_GPIO_PORT_E port, PUSH_KEY_TYPE_E type, INT_T cnt)
{
  tuya_light_key_process (port, type, cnt);
}

OPERATE_RET tuya_light_button_reg (IN UINT_T gpio_no, IN BOOL_T is_high, IN  UINT_T long_key_time,
                                   KEY_CALLBACK callback)
{
  OPERATE_RET ret;
  KEY_USER_DEF_S button;
  button.port = gpio_no;
  button.low_level_detect = !is_high;
  button.call_back           = callback;
  button.long_key_time       = long_key_time;
  button.seq_key_detect_time = 0;
  button.lp_tp  =  LP_ONCE_TRIG;
  ret = reg_proc_key (&button);
  return ret;
}
INT_T tuya_light_get_free_heap_size (VOID)
{
  return SysGetHeapSize();
}


extern VOID tuya_light_dp_data_default_set (VOID);

STATIC VOID set_wf_gw_status (IN CONST GW_WIFI_NW_STAT_E stat)
{
  wifi_stat = stat;
  PR_DEBUG ("wifi_stat = %d", wifi_stat);
  tuya_light_wf_stat_cb (wifi_stat); /* 根据wifi状态来处理--显示式样等 */
  return;
}

OPERATE_RET tuya_light_reg_get_wf_nw_stat_cb (VOID)
{
  OPERATE_RET op_ret;
  op_ret = tuya_iot_reg_get_wf_nw_stat_cb (set_wf_gw_status);

  if (OPRT_OK != op_ret)
  {
    PR_ERR ("tuya_iot_reg_get_wf_nw_stat_cb err:%02x", op_ret);
    return op_ret;
  }
}

TUYA_GW_WIFI_NW_STAT_E tuya_light_get_wf_gw_status (VOID)
{
  /*STATIC UCHAR_T check_times = 0;
  if(check_times < 5){
    check_times++;
    return STAT_LOW_POWER;
  }*/
  return wifi_stat;
}

BYTE_T tuya_light_get_gw_status (VOID)
{
  PR_DEBUG ("wifi_stat = %d", wifi_stat);

  if ( (STAT_UNPROVISION == wifi_stat) || (STAT_AP_STA_UNCFG == wifi_stat) )
  {
    return TUYA_LIGHT_UN_ACTIVE;
  }

  return TUYA_LIGHT_ACTIVE;
}
BOOL_T tuya_light_get_gw_mq_conn_stat (VOID)
{
  if (wifi_stat == STAT_CLOUD_CONN || wifi_stat == STAT_AP_CLOUD_CONN)
  {
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

VOID tuya_light_dev_reset (VOID)
{
  PR_DEBUG ("tuya_iot_wf_gw_unactive\n");
  //gw_wifi_reset(WRT_AUTO);
  tuya_iot_wf_gw_fast_unactive (light_get_wifi_cfg(), WF_START_SMART_FIRST);
}

OPERATE_RET tuya_light_gpio_init (UINT_T gpio, BYTE_T type)
{
  if (type == TUYA_GPIO_IN)
  {
    return tuya_gpio_inout_set (gpio, TRUE);
  }
  else if (type == TUYA_GPIO_OUT)
  {
    return tuya_gpio_inout_set (gpio, FALSE);
  }
}
INT_T tuya_light_gpio_read (UINT_T gpio)
{
  return tuya_gpio_read (gpio);
}

OPERATE_RET tuya_light_gpio_ctl (UINT_T gpio, BYTE_T high)
{
  if (high == TUYA_GPIO_LOW)
  {
    return tuya_gpio_write (gpio, FALSE );
  }
  else if (high == TUYA_GPIO_HIGH)
  {
    return tuya_gpio_write (gpio, TRUE);
  }
}

OPERATE_RET tuya_ws_db_user_param_read (OUT BYTE_T** buf, OUT UINT_T* len)
{
  return ws_db_user_param_read (buf, len);
}


