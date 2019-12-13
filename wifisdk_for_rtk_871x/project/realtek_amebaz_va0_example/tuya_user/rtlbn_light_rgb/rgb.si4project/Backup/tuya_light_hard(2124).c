#include ".compile_usr_cfg.h"
#include "tuya_device.h"
#include "tuya_common_light.h"
#include "tuya_light_hard.h"
#include "tuya_light_lib.h"
#include "tuya_hw_table.h"

#define CHAN_NUM 5

#define LIGHT_MAX_CTL_VAL 1023
#define PWM_MAX 1023

#define IIC_MAX 255
#define IIC_DATA_SEND_CAL(val) (val * IIC_MAX / PWM_MAX)
#if USER_DEFINE_LIGHT_STRIP_LED_CONFIG
  STATIC UCHAR __rgbcw_r_index = 0;
  STATIC UCHAR __rgbcw_g_index = 1;
  STATIC UCHAR __rgbcw_b_index = 2;
  STATIC UCHAR __rgbcw_c_index = 3;
  STATIC UCHAR __rgbcw_w_index = 4;
  STATIC USHORT_T __r_value = LIGHT_MAX_CTL_VAL + 100;
  STATIC USHORT_T __g_value = LIGHT_MAX_CTL_VAL + 100;
  STATIC USHORT_T __b_value = LIGHT_MAX_CTL_VAL + 100;
  STATIC USHORT_T __cw_value = LIGHT_MAX_CTL_VAL + 100;
  STATIC USHORT_T __ww_value = LIGHT_MAX_CTL_VAL + 100;
#endif
STATIC TIMER_ID wf_config_timer;
STATIC VOID wf_config_timer_cb (UINT timerID, PVOID pTimerArg);
STATIC VOID tuya_light_pwm_output (USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value,
                                   USHORT_T WW_value);


NET_LIGHT_TYPE_E tuya_light_net_light_type_get (VOID)
{
  HW_TABLE_S* hw_table = get_hw_table();

  switch (hw_table->netconfig_color)
  {
    case CONFIG_COLOR_R:
    case CONFIG_COLOR_G:
    case CONFIG_COLOR_B:
      return COLOR_NET_LIGHT;
      break;

    case CONFIG_COLOR_C:
    case CONFIG_COLOR_W:
      return WHITE_NET_LIGHT;
      break;

    default:
      return WHITE_NET_LIGHT;
      break;
  }
}

VOID tuya_light_gpio_pin_config (UINT_T io_info[])
{
  HW_TABLE_S* hw_table = get_hw_table();
  UCHAR i, index, cnt;
  char pin_name[] = {'r', 'g', 'b', 'c', 'w'};
  UINT pwm_gpio_info[6] = {PWM_0_OUT_IO_NUM, PWM_1_OUT_IO_NUM, PWM_2_OUT_IO_NUM, PWM_3_OUT_IO_NUM, PWM_4_OUT_IO_NUM, PWM_5_OUT_IO_NUM};
  cnt = 0;

  if (hw_table->driver_mode == DRV_MODE_IIC_SMx726 )
  {
    i = 3;
  }
  else if (hw_table->light_mode >= LIGHT_MODE_CW)
  {
    i = 3;
  }
  else
  {
    i = 0;
  }

  for (; i < 6; i ++)
  {
    for (index = 0; index < 6; index ++)
    {
      if (pwm_gpio_info[index] == hw_table->light_pin[i].pin_num)
      {
        break;
      }
    }

    if (index >= 6)
    {
      continue;
    }

    //PR_DEBUG("pin:%c num:%d", pin_name[i], pwm_gpio_info[index]);
    io_info[cnt]= pwm_gpio_info[index];
    cnt ++;

    if (cnt >= hw_table->pwm_channel_num)
    {
      break;
    }
  }
}


VOID tuya_light_device_pwm_hw_init (VOID)
{
  HW_TABLE_S* hw_table = get_hw_table();
  USHORT_T pwm_frequecy = 1000 * 1000 / hw_table->pwm_frequency;
  UCHAR_T  pwm_channel_num = hw_table->pwm_channel_num;
  UINT_T pwm_val = 0;
  //默认引脚配置顺序
  UINT_T io_info[5];

  if ( (hw_table->driver_mode == DRV_MODE_PWM_IO) || (hw_table->driver_mode == DRV_MODE_IIC_SMx726 &&
                                                      hw_table->light_mode <= LIGHT_MODE_RGB) )
  {
    tuya_light_gpio_pin_config (io_info);
    tuya_light_pwm_init (pwm_frequecy, &pwm_val, pwm_channel_num, io_info);

    if (hw_table->cw_type == CW_TYPE) //CW添加白光互补输出
    {
      if ( (hw_table->driver_mode == DRV_MODE_IIC_SMx726 && hw_table->light_mode == LIGHT_MODE_RGBCW) || \
           (hw_table->driver_mode == DRV_MODE_PWM_IO && hw_table->light_mode == LIGHT_MODE_CW) )
      {
        tuya_light_pwm_channel_reverse_set (1);
      }
      else if (hw_table->driver_mode == DRV_MODE_PWM_IO && hw_table->light_mode == LIGHT_MODE_RGBCW)
      {
        tuya_light_pwm_channel_reverse_set (4);
      }
    }
  }

  if (hw_table->driver_mode == DRV_MODE_IIC_SMx726 || hw_table->driver_mode == DRV_MODE_IIC_SM2135)
  {
    i2c_gpio_init (hw_table->i2c_cfg.scl_pin, hw_table->i2c_cfg.sda_pin);
  }
}

VOID tuya_light_device_pwm_mute( )
{
  UINT_T io_info[5];
  HW_TABLE_S* hw_table = get_hw_table();
  UCHAR_T  pwm_channel_num = hw_table->pwm_channel_num;
  tuya_light_gpio_pin_config (io_info);
  tuya_light_set_pwm_mute (pwm_channel_num, io_info);
}

VOID tuya_light_device_one_pwm_hw_init (UINT_T pin_name)
{
  HW_TABLE_S* hw_table = get_hw_table();
  USHORT_T pwm_frequecy = 1000 * 1000 / hw_table->pwm_frequency;
  UINT_T pwm_val = 0;
  //默认引脚配置顺序
  UINT_T io_info[5];
  io_info[0] = pin_name;
  tuya_light_pwm_init (pwm_frequecy, &pwm_val, 1, io_info);
}

VOID tuya_light_device_one_pwm_mute (UINT_T pin_name)
{
  UINT_T io_info[5];
  io_info[0] = pin_name;
  tuya_light_set_pwm_mute (1, io_info);
}


VOID tuya_light_device_hw_init (VOID)
{
  PR_DEBUG ("**** %s ****", __FUNCTION__);
  OPERATE_RET op_ret;
  HW_TABLE_S* hw_table = get_hw_table();
  tuya_light_device_pwm_hw_init();
  tuya_light_hw_timer_init();
  op_ret = sys_add_timer (wf_config_timer_cb, NULL, &wf_config_timer);

  if (OPRT_OK != op_ret)
  {
    PR_ERR ("wf_direct_timer_cb add error:%d", op_ret);
    return ;
  }

  //配置为CCT模式
  tuya_light_cw_type_set ( (LIGHT_CW_TYPE_E) hw_table->cw_type);
  //配网设备重配网方式
  tuya_light_reset_mode_set ( (LIGHT_RESET_NETWORK_STA_E) hw_table->reset_dev_mode);
  //灯光断电记忆功能配置（不影响情景模式中的配置）
  tuya_light_memory_flag_set (hw_table->dev_memory);
  //CW混光最大输出功率
  tuya_light_cw_max_power_set (hw_table->cw_max_power);
  //配置灯光库调光级数
  tuya_light_ctrl_precision_set (LIGHT_MAX_CTL_VAL);
}

OPERATE_RET tuya_light_key_init (KEY_CALLBACK callback)
{
  OPERATE_RET op_ret;
  HW_TABLE_S* hw_table = get_hw_table();

  if (hw_table->key.id == -1)   //取消设置
  {
    PR_DEBUG ("light key gpio null!");
    return OPRT_COM_ERROR;
  }
  else
  {
    PR_DEBUG ("light key gpio = %d", hw_table->key.id);
  }

  // key process init
  op_ret = key_init (NULL, 0, 25);

  if (OPRT_OK  != op_ret)
  {
    return op_ret;
  }

  // register key to process
  op_ret = tuya_light_button_reg (hw_table->key.id, hw_table->key.level, hw_table->key.times * 1000, callback);

  if (OPRT_OK  != op_ret)
  {
    return op_ret;
  }

  return OPRT_OK;
}

//下为原SOC最通用gamma数值
//RED 0.80-100%
STATIC CONST BYTE gamma_red[] =
{
  0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7, 8, 9, 9, 10, 11, 11, 12, 13, 13,
  14, 15, 15, 16, 17, 18, 18, 19, 20, 21, 21, 22, 23, 24, 24, 25, 26, 27, 28,
  28, 29, 30, 31, 32, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42, 43, 44,
  44, 45, 46, 47, 48, 49, 50, 51, 52, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
  62, 63, 64, 65, 66, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98,
  99, 100, 101, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
  115, 116, 117, 118, 119, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130,
  131, 132, 134, 135, 136, 137, 138, 139, 140, 141, 142, 144, 145, 146, 147,
  148, 149, 150, 151, 152, 154, 155, 156, 157, 158, 159, 160, 162, 163, 164,
  165, 166, 167, 168, 170, 171, 172, 173, 174, 175, 177, 178, 179, 180, 181,
  182, 184, 185, 186, 187, 188, 189, 191, 192, 193, 194, 195, 196, 198, 199,
  200, 201, 202, 204, 205, 206, 207, 208, 210, 211, 212, 213, 214, 216, 217,
  218, 219, 220, 222, 223, 224, 225, 227, 228, 229, 230, 231, 233, 234, 235,
  236, 238, 239, 240, 241, 243, 244, 245, 246, 248, 249, 250, 251, 253, 254, 255
};


//GREEN 0.6-70%
STATIC CONST BYTE gamma_green[] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4,
  4, 5, 5, 5, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12,
  13, 13, 13, 14, 14, 15, 15, 16, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21,
  21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 31,
  31, 32, 33, 33, 34, 34, 35, 36, 36, 37, 38, 38, 39, 39, 40, 41, 41, 42, 43,
  43, 44, 45, 45, 46, 47, 47, 48, 49, 49, 50, 51, 52, 52, 53, 54, 54, 55, 56,
  57, 57, 58, 59, 60, 60, 61, 62, 63, 63, 64, 65, 66, 66, 67, 68, 69, 70, 70,
  71, 72, 73, 74, 75, 75, 76, 77, 78, 79, 80, 80, 81, 82, 83, 84, 85, 86, 86,
  87, 88, 89, 90, 91, 92, 93, 94, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103,
  104, 105, 106, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
  118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132,
  133, 134, 135, 136, 137, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
  149, 150, 151, 152, 154, 155, 156, 157, 158, 159, 160, 161, 162, 164, 165,
  166, 167, 168, 169, 170, 172, 173, 174, 175, 176, 177, 179
};


//BLUE 0.6-75%
STATIC CONST BYTE gamma_blue[] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 5,
  5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12, 12, 13, 13,
  14, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22,
  23, 23, 24, 24, 25, 25, 26, 27, 27, 28, 28, 29, 29, 30, 31, 31, 32, 32, 33,
  34, 34, 35, 36, 36, 37, 38, 38, 39, 40, 40, 41, 42, 42, 43, 44, 44, 45, 46,
  46, 47, 48, 49, 49, 50, 51, 51, 52, 53, 54, 54, 55, 56, 57, 58, 58, 59, 60,
  61, 61, 62, 63, 64, 65, 65, 66, 67, 68, 69, 70, 70, 71, 72, 73, 74, 75, 76,
  76, 77, 78, 79, 80, 81, 82, 83, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,
  94, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
  109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122,
  123, 124, 125, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137,
  138, 139, 141, 142, 143, 144, 145, 146, 147, 148, 150, 151, 152, 153,
  154, 155, 156, 158, 159, 160, 161, 162, 163, 165, 166, 167, 168, 169,
  170, 172, 173, 174, 175, 176, 178, 179, 180, 181, 183, 184, 185, 186,
  188, 189, 190, 191
};


//获取完RGB下发数值之后使用校正
VOID tuya_light_gamma_adjust (USHORT red, USHORT green, USHORT blue, USHORT* adjust_red, USHORT* adjust_green,
                              USHORT* adjust_blue)
{
  //取值分级和实际输出步进分级转换（主要用于gamma只能取255，但需求1023的变化分级）
#if GAMMA_ENABLE
#if 0
  red = red * GAMMA_PRECISION / PWM_MAX;
  green = green * GAMMA_PRECISION / PWM_MAX;
  blue = blue * GAMMA_PRECISION / PWM_MAX;
  PR_DEBUG ("color data befor gamma: r = %d,g = %d,b = %d", red, green, blue);
  gamma_get_by_json (&red, &green, &blue);
  PR_DEBUG ("color data after gamma: r = %d,g = %d,b = %d", red, green, blue);
  *adjust_red = red * PWM_MAX / GAMMA_PRECISION;
  *adjust_green = green * PWM_MAX / GAMMA_PRECISION;
  *adjust_blue = blue * PWM_MAX / GAMMA_PRECISION;
#else
  *adjust_red = gamma_red[red * 255 / LIGHT_MAX_CTL_VAL] * LIGHT_MAX_CTL_VAL / 255;
  *adjust_green = gamma_green[green * 255 / LIGHT_MAX_CTL_VAL] * LIGHT_MAX_CTL_VAL / 255;
  *adjust_blue = gamma_blue[blue * 255 / LIGHT_MAX_CTL_VAL] * LIGHT_MAX_CTL_VAL / 255;
#endif
#else
  *adjust_red = red;
  *adjust_green = green;
  *adjust_blue = blue;
#endif
}

#define DEFAULT_BRIGNT_PERCENT      50            //初始化亮度 50%
#define DEFAULT_TEMP_PERCENT        100               //初始色温   全冷 100%

#define BRIGHT_LMT_MAX              100               //实际最高亮度为100%
#define BRIGHT_LMT_MIN              10                //实际最低亮度为10%


VOID tuya_light_color_power_ctl (COLOR_POWER_CTL_E ctl)
{
  HW_TABLE_S* hw_table = get_hw_table();

  if ( (hw_table->lowpower_pin != NONE) && (hw_table->driver_mode == DRV_MODE_IIC_SM2135 ||
                                            hw_table->driver_mode == DRV_MODE_IIC_SMx726) )
  {
    //PR_DEBUG("set low pin %d status %d", hw_table->lowpower_pin,ctl);
    tuya_light_gpio_init (hw_table->lowpower_pin, TUYA_GPIO_OUT);
    tuya_light_gpio_ctl (hw_table->lowpower_pin, ctl);
  }
}

LIGHT_COLOR_TYPE_E tuya_light_color_type_get (VOID)
{
  HW_TABLE_S* hw_table = get_hw_table();

  switch (hw_table->light_mode)
  {
    case LIGHT_MODE_C:
      return LIGHT_COLOR_C;
      break;

    case LIGHT_MODE_CW:
      return LIGHT_COLOR_CW;
      break;

    case LIGHT_MODE_RGB:
      return LIGHT_COLOR_RGB;
      break;

    case LIGHT_MODE_RGBC:
      return LIGHT_COLOR_RGBC;
      break;

    case LIGHT_MODE_RGBCW:
      return LIGHT_COLOR_RGBCW;
      break;

    default:
      break;
  }
}


VOID tuya_light_config_param_set (LIGHT_DEFAULT_CONFIG_S* config)
{
  HW_TABLE_S* hw_table = get_hw_table();
  config->wf_cfg = (WIFI_CFG_DEFAULT_E) hw_table->wf_init_cfg;
  config->wf_rst_cnt = hw_table->reset_count;
  config->color_type = tuya_light_color_type_get();
  config->light_drv_mode = hw_table->driver_mode;
  config->net_light_type = tuya_light_net_light_type_get();
  config->def_light_color = hw_table->def_light_color;
  config->def_bright_precent = hw_table->def_bright_precent;
  config->def_temp_precent = hw_table->def_temp_precent;
  config->bright_max_precent = hw_table->bright_max_precent;
  config->bright_min_precent = hw_table->bright_min_precent;
  config->color_bright_max_precent = hw_table->color_max_precent;
  config->color_bright_min_precent = hw_table->color_min_precent;
  config->power_onoff_type = hw_table->power_onoff_mode;
  config->whihe_color_mutex = hw_table->cw_rgb_mutex;
}


STATIC VOID tuya_set_wf_sta_light (UINT       time)
{
  sys_start_timer (wf_config_timer, time, TIMER_CYCLE);
}

STATIC tuya_set_default_light (CONIFG_COLOR_E light_color, USHORT light_bright)
{
  HW_TABLE_S* hw_table = get_hw_table();

  switch (light_color)
  {
    case CONFIG_COLOR_R:
      if (light_color == LIGHT_MODE_CW )
      {
        if (hw_table->cw_type == CW_TYPE)
        {
          tuya_light_send_data (0, 0, 0, light_bright, 0);
        }
        else if (hw_table->cw_type == CCT_TYPE)
        {
          tuya_light_send_data (0, 0, 0, light_bright, LIGHT_MAX_CTL_VAL);
        }
      }
      else if (hw_table->light_mode == LIGHT_MODE_C)
      {
        tuya_light_send_data (0, 0, 0, light_bright, 0);
      }
      else
      {
        tuya_light_send_data (light_bright, 0, 0, 0, 0);
      }

      break;

    case CONFIG_COLOR_G:
      if (hw_table->light_mode == LIGHT_MODE_CW )
      {
        if (hw_table->cw_type == CW_TYPE)
        {
          tuya_light_send_data (0, 0, 0, light_bright, 0);
        }
        else if (hw_table->cw_type == CCT_TYPE)
        {
          tuya_light_send_data (0, 0, 0, light_bright, LIGHT_MAX_CTL_VAL);
        }
      }
      else if (hw_table->light_mode == LIGHT_MODE_C)
      {
        tuya_light_send_data (0, 0, 0, light_bright, 0);
      }
      else
      {
        tuya_light_send_data (0, light_bright, 0, 0, 0);
      }

      break;

    case CONFIG_COLOR_B:
      if (hw_table->light_mode == LIGHT_MODE_CW )
      {
        if (hw_table->cw_type == CW_TYPE)
        {
          tuya_light_send_data (0, 0, 0, light_bright, 0);
        }
        else if (hw_table->cw_type == CCT_TYPE)
        {
          tuya_light_send_data (0, 0, 0, light_bright, LIGHT_MAX_CTL_VAL);
        }
      }
      else if (hw_table->light_mode == LIGHT_MODE_C)
      {
        tuya_light_send_data (0, 0, 0, light_bright, 0);
      }
      else
      {
        tuya_light_send_data (0, 0, light_bright, 0, 0);
      }

      break;

    case CONFIG_COLOR_C:
      if (hw_table->light_mode == LIGHT_MODE_RGB )
      {
        tuya_light_send_data (light_bright, 0, 0, 0, 0);
      }
      else
      {
        if (hw_table->cw_type == CW_TYPE)
        {
          tuya_light_send_data (0, 0, 0, light_bright, 0);
        }
        else if (hw_table->cw_type == CCT_TYPE)
        {
          tuya_light_send_data (0, 0, 0, light_bright, LIGHT_MAX_CTL_VAL);
        }
      }

      break;

    case CONFIG_COLOR_W:
      if (hw_table->light_mode == LIGHT_MODE_RGB )
      {
        tuya_light_send_data (light_bright, 0, 0, 0, 0);
      }
      else if (hw_table->light_mode == LIGHT_MODE_RGBC)
      {
        tuya_light_send_data (0, 0, 0, light_bright, 0);
      }
      else
      {
        if (hw_table->cw_type == CW_TYPE)
        {
          tuya_light_send_data (0, 0, 0, 0, light_bright);
        }
        else if (hw_table->cw_type == CCT_TYPE)
        {
          tuya_light_send_data (0, 0, 0, light_bright, 0);
        }
      }

      break;

    default:
      break;
  }
}

STATIC VOID tuya_set_wf_lowpower_light (VOID)
{
  HW_TABLE_S* hw_table = get_hw_table();
  USHORT lowpower_bright = LIGHT_MAX_CTL_VAL / 100 * hw_table->def_bright_precent;
  tuya_set_default_light (hw_table->def_light_color, lowpower_bright);
}

VOID light_lowpower_enable (VOID)
{
  tuya_light_enter_lowpower_mode();
}

VOID light_lowpower_disable (VOID)
{
  tuya_light_exit_lowpower_mode();
}

#if USER_DEFINE_LIGHT_STRIP_LED_CONFIG
VOID tuya_light_set_rgbcw_order (UCHAR* rgbcw_order)
{
  if (rgbcw_order == NULL)
  {
    return;
  }

  int i=0;

  for (i=0; rgbcw_order[i] != '\0' && i<6; ++i)
  {
    switch (rgbcw_order[i])
    {
      case 'r':
      case 'R':
        __rgbcw_r_index = i;
        PR_DEBUG ("__rgbcw_r_index:%d", __rgbcw_r_index);
        break;

      case 'g':
      case 'G':
        __rgbcw_g_index = i;
        PR_DEBUG ("__rgbcw_g_index:%d", __rgbcw_g_index);
        break;

      case 'b':
      case 'B':
        __rgbcw_b_index = i;
        PR_DEBUG ("__rgbcw_b_index:%d", __rgbcw_b_index);
        break;

      case 'c':
      case 'C':
        __rgbcw_c_index = i;
        PR_DEBUG ("__rgbcw_c_index:%d", __rgbcw_c_index);
        break;

      case 'w':
      case 'W':
        __rgbcw_w_index = i;
        PR_DEBUG ("__rgbcw_w_index:%d", __rgbcw_w_index);
        break;

      default:
        break;
    }
  }

  if (__r_value != (LIGHT_MAX_CTL_VAL + 100) )
  {
    tuya_light_pwm_output (__r_value, __g_value, __b_value, __cw_value, __ww_value);
  }
}
#endif
VOID tuya_light_config_stat (CONF_STAT_E stat)
{
  STATIC BOOL_T defalut = FALSE;
  PR_NOTICE ("---------stat:%d", stat);

  switch (stat)
  {
    case CONF_STAT_SMARTCONFIG:
      PR_NOTICE ("wf config EZ mode");
      defalut = TRUE;
      tuya_set_wf_sta_light (250);
      break;

    case CONF_STAT_APCONFIG:
      PR_NOTICE ("wf config AP mode");
      defalut = TRUE;
      tuya_set_wf_sta_light (1500);
      break;

      #if 0
    case CONF_STAT_CONNECT:
      PR_NOTICE ("wf connect  mode");
      defalut = TRUE;
      if (IsThisSysTimerRun (wf_config_timer) )
      {
        sys_stop_timer (wf_config_timer);
        Wifi_ConFlag = 1;
      }
      light_bright_start();
      break;
    #endif
      
    case CONF_STAT_LOWPOWER:

      //关闭配网指示灯
      if (IsThisSysTimerRun (wf_config_timer) )
      {
        sys_stop_timer (wf_config_timer);
      }

      tuya_set_wf_lowpower_light();
      PR_NOTICE ("wf config LowPower mode");
      break;

    case CONF_STAT_UNCONNECT:
        
    default:
      //关闭配网指示灯
      if (IsThisSysTimerRun (wf_config_timer) )
      {
        sys_stop_timer (wf_config_timer);
        Wifi_ConFlag = 1;
      }

      if (defalut)
      {
        defalut = FALSE;
        light_init_stat_set();
      }
      break;
  }
}

STATIC VOID wf_config_timer_cb (UINT timerID, PVOID pTimerArg)
{
  STATIC BYTE_T cnt = 0;
  HW_TABLE_S* hw_table = get_hw_table();
  USHORT netconfig_bright = LIGHT_MAX_CTL_VAL / 100 * hw_table->netconfig_bright;
  if (cnt % 2 == 0)
  {
    //PR_DEBUG("wf config LowPower mode=%d %d",hw_table->netconfig_color,netconfig_bright);
    tuya_set_default_light (hw_table->netconfig_color, netconfig_bright);
  }
  else
  {
    if (hw_table->cw_type == CCT_TYPE && hw_table->netconfig_color == CONFIG_COLOR_C)
    {
      tuya_light_send_data (0, 0, 0, 0, LIGHT_MAX_CTL_VAL);
    }
    else
    {
      tuya_light_send_data (0, 0, 0, 0, 0);
    }
  }
  cnt ++;
}

VOID tuya_set_wf_config_default_on (VOID)
{
  HW_TABLE_S* hw_table = get_hw_table();
  USHORT netconfig_bright = LIGHT_MAX_CTL_VAL / 100 * hw_table->netconfig_bright;
  tuya_set_default_light (hw_table->netconfig_color, netconfig_bright);
}

VOID tuya_set_wf_config_default_off (VOID)
{
  HW_TABLE_S* hw_table = get_hw_table();

  if (hw_table->cw_type == CCT_TYPE && hw_table->netconfig_color == CONFIG_COLOR_C)
  {
    tuya_light_send_data (0, 0, 0, 0, LIGHT_MAX_CTL_VAL);
  }
  else
  {
    tuya_light_send_data (0, 0, 0, 0, 0);
  }
}


UINT light_key_id_get (VOID)
{
  HW_TABLE_S* hw_table = get_hw_table();
  return hw_table->key.id;
}


#if USER_DEFINE_LIGHT_RGBCW_FAKE_DATA
STATIC VOID __use_rgb_gamma (USHORT_T* R_value, USHORT_T* G_value, USHORT_T* B_value, USHORT_T* CW_value,
                             USHORT_T* WW_value)
{
  //PR_DEBUG("R_O:%d G_O:%d, B_O:%d\n", __user_define_rgb_gamm[*R_value*255/LIGHT_MAX_CTL_VAL][0], __user_define_rgb_gamm[*G_value*255/LIGHT_MAX_CTL_VAL][1], __user_define_rgb_gamm[*B_value*255/LIGHT_MAX_CTL_VAL][2]);
  *R_value = __user_define_rgb_gamm[*R_value*255/LIGHT_MAX_CTL_VAL][0]*LIGHT_MAX_CTL_VAL/255;
  *G_value = __user_define_rgb_gamm[*G_value*255/LIGHT_MAX_CTL_VAL][1]*LIGHT_MAX_CTL_VAL/255;
  *B_value = __user_define_rgb_gamm[*B_value*255/LIGHT_MAX_CTL_VAL][2]*LIGHT_MAX_CTL_VAL/255;
  //PR_DEBUG("R_N=%d G_N=%d B_N=%d", *R_value, *G_value, *B_value);
}
#endif
STATIC VOID __change_pwm_if_needed (USHORT_T* R_value, USHORT_T* G_value, USHORT_T* B_value, USHORT_T* CW_value,
                                    USHORT_T* WW_value)
{
#define WARM_R    PWM_MAX   //189,118,0;248,147,29
#define WARM_G    140
#define WARM_B    0

  if (light_work_mode_get() == 1) //color mode
  {
#if USER_DEFINE_LIGHT_RGBCW_FAKE_DATA

    switch (USER_DEFINE_LIGHT_TYPE)
    {
      case USER_LIGTH_MODE_RGBCW_FAKE_CUS:
        __use_rgb_gamma (R_value, G_value, B_value, CW_value, WW_value);
        PR_DEBUG ("R=%d G=%d B=%d C=%d W=%d", *R_value, *G_value, *B_value, *CW_value, *WW_value);
        break;

      default:
        break;
    }

#endif
    return;
  }

  if ( (light_work_mode_get() == 2) || (light_work_mode_get() == 3) ) //color mode or music
  {
    if (*R_value != 0 || *G_value != 0 || *B_value != 0)
    {
#if USER_DEFINE_LIGHT_RGBCW_FAKE_DATA

      switch (USER_DEFINE_LIGHT_TYPE)
      {
        case USER_LIGTH_MODE_RGBCW_FAKE_CUS:
          __use_rgb_gamma (R_value, G_value, B_value, CW_value, WW_value);
          //PR_DEBUG("R=%d G=%d B=%d C=%d W=%d", *R_value, *G_value, *B_value, *CW_value, *WW_value);
          break;

        default:
          break;
      }

#endif
      return;
    }

    if (*CW_value == 0 && *WW_value == 0)
    {
      return;
    }
  }

  switch (USER_DEFINE_LIGHT_TYPE)
  {
    case USER_LIGTH_MODE_RGBC_FAKE:
      *R_value = *G_value = *CW_value;
      *B_value = *CW_value * 8 / 10;
      //PR_DEBUG("R=%d G=%d B=%d C=%d W=%d", *R_value, *G_value, *B_value, *CW_value, *WW_value);
      break;

    case USER_LIGTH_MODE_RGBW_FAKE:
      *R_value = *WW_value * WARM_R / PWM_MAX;
      *G_value = *WW_value * WARM_G / PWM_MAX;
      *B_value = *WW_value * WARM_B / PWM_MAX;
      //PR_DEBUG("R=%d G=%d B=%d C=%d W=%d", *R_value, *G_value, *B_value, *CW_value, *WW_value);
      break;

    case USER_LIGTH_MODE_RGBCW_FAKEC:
      *R_value = *G_value = *CW_value;
      *B_value = *CW_value * 8 / 10;
      //根据客户的需求，在假五路时，暖光承担所有的亮度功能，调节色温条时，不改变暖光的亮度。
      u8 ww_tmp = ( (*CW_value+*WW_value) > PWM_MAX? PWM_MAX: (*CW_value+*WW_value) );

      //当ww_tmp>=920时，需要时，需要调整RGB+W不超过最大负载。
      if (ww_tmp >= 920)
      {
        ww_tmp -= *CW_value/10;
      }

      *CW_value = ww_tmp;
      //PR_DEBUG("R=%d G=%d B=%d C=%d W=%d", *R_value, *G_value, *B_value, *CW_value, *WW_value);
      break;

    case USER_LIGTH_MODE_RGBCW_FAKEW:
      *R_value = *WW_value * WARM_R / PWM_MAX;
      *G_value = *WW_value * WARM_G / PWM_MAX;
      *B_value = *WW_value * WARM_B / PWM_MAX;
      //根据客户的需求，在假五路时，冷光承担所有的亮度功能，调节色温条时，不改变冷光的亮度。
      u8 cw_tmp = ( (*CW_value+*WW_value) > PWM_MAX? PWM_MAX: (*CW_value+*WW_value) );

      //当cw_tmp>=920时，需要时，需要调整RGB+C不超过最大负载。
      if (cw_tmp >= 920)
      {
        cw_tmp -= *WW_value/10;
      }

      *CW_value = cw_tmp;
      //PR_DEBUG("R=%d G=%d B=%d C=%d W=%d", *R_value, *G_value, *B_value, *CW_value, *WW_value);
      break;

    case USER_LIGTH_MODE_RGBCW_FAKEFC:
      //rgb simulate C and put w in c channel.
      *R_value = *G_value = *CW_value;
      *B_value = *CW_value * 8 / 10;
      *CW_value = *WW_value;
      break;

    //rgb simulate W.
    case USER_LIGTH_MODE_RGBCW_FAKEFW:
      *R_value = *WW_value * WARM_R / PWM_MAX;
      *G_value = *WW_value * WARM_G / PWM_MAX;
      *B_value = *WW_value * WARM_B / PWM_MAX;
      break;

    case USER_LIGTH_MODE_RGBCW_FAKE_CUS:
#if USER_DEFINE_LIGHT_RGBCW_FAKE_DATA
      {
#define CUS_COLOR_STEP_NUM 54
        USHORT_T bri = (*CW_value + *WW_value);
        USHORT_T tmpr = (*CW_value*CUS_COLOR_STEP_NUM) / (*WW_value + *CW_value);
        *R_value = __user_define_fake_data[tmpr][0]*bri/255;
        *G_value = __user_define_fake_data[tmpr][1]*bri/255;
        *B_value = __user_define_fake_data[tmpr][2]*bri/255;
        *CW_value = __user_define_fake_data[tmpr][3]*bri/255;
        //PR_NOTICE("bri=%d, tmpr=%d, R=%d G=%d B=%d C=%d W=%d", bri, tmpr, *R_value, *G_value, *B_value, *CW_value, *WW_value);
        break;
      }

#endif

    default:
      break;
  }

  //PR_DEBUG("R=%d G=%d B=%d C=%d W=%d", *R_value, *G_value, *B_value, *CW_value, *WW_value);
}
#if USER_DEFINE_LIGHT_STRIP_LED_CONFIG
STATIC VOID __change_rgbcw_order_if_needed (USHORT_T* R_value, USHORT_T* G_value, USHORT_T* B_value, USHORT_T* CW_value,
                                            USHORT_T* WW_value)
{
  HW_TABLE_S* hw_table = get_hw_table();

  if (hw_table->light_mode > LIGHT_MODE_RGB )
  {
    return;
  }

  if (__rgbcw_r_index == 0 && __rgbcw_g_index==1 && __rgbcw_b_index==2 && __rgbcw_c_index==3 && __rgbcw_w_index==4)
  {
    //PR_DEBUG("default order do nothing\n");
    return;
  }

  //PR_DEBUG("before change order R=%d G=%d B=%d C=%d W=%d", *R_value, *G_value, *B_value, *CW_value, *WW_value);
  USHORT_T tmp_value[5];
  tmp_value[__rgbcw_r_index] = *R_value;
  tmp_value[__rgbcw_g_index] = *G_value;
  tmp_value[__rgbcw_b_index] = *B_value;
  tmp_value[__rgbcw_c_index] = *CW_value;
  tmp_value[__rgbcw_w_index] = *WW_value;
  *R_value = tmp_value[0];
  *G_value = tmp_value[1];
  *B_value = tmp_value[2];
  *CW_value = tmp_value[3];
  *WW_value = tmp_value[4];
  //PR_DEBUG("after change order R=%d G=%d B=%d C=%d W=%d", *R_value, *G_value, *B_value, *CW_value, *WW_value);
}
#endif

VOID __light_pwm_set_duty (UINT_T duty, UCHAR_T channel)
{
  UINT pwm_gpio_info[6] = {PWM_0_OUT_IO_NUM, PWM_1_OUT_IO_NUM, PWM_2_OUT_IO_NUM, PWM_3_OUT_IO_NUM, PWM_4_OUT_IO_NUM, PWM_5_OUT_IO_NUM};
  static char pwm_status[6] = {1, 1, 1, 1, 1, 1};

  if (duty == 0)
  {
    if (pwm_status[channel] == 0)
    {
      //PR_NOTICE ("channel %d is already gpio mode", channel);
      return;
    }

    pwm_status[channel] = 0;
    //PR_NOTICE ("channel %d is change to gpio mode", channel);
    tuya_light_device_one_pwm_mute (pwm_gpio_info[channel]);
    return;
  }

  if (pwm_status[channel] == 0)
  {
    pwm_status[channel] = 1;
    //PR_NOTICE ("channel %d is change to pwm mode", channel);
    tuya_light_device_one_pwm_hw_init (pwm_gpio_info[channel]);
  }

  pwm_set_duty (duty, channel);
}

STATIC VOID tuya_light_pwm_output (USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value,
                                   USHORT_T WW_value)
{
  HW_TABLE_S* hw_table = get_hw_table();
  static UCHAR_T is_pwm_init = 1;
#if USER_DEFINE_LIGHT_STRIP_LED_CONFIG
  __r_value = R_value;
  __g_value = G_value;
  __b_value = B_value;
  __cw_value = CW_value;
  __ww_value = WW_value;
  __change_rgbcw_order_if_needed (&R_value, &G_value, &B_value, &CW_value, &WW_value);
#endif
  PR_DEBUG ("11 R=%d G=%d B=%d C=%d W=%d", R_value, G_value, B_value, CW_value, WW_value);
#if 0

  if (R_value == 0 && G_value == 0 && B_value == 0 && CW_value == 0 && WW_value == 0)
  {
    is_pwm_init = 0;
    tuya_light_device_pwm_mute();
    return;
  }

  if (is_pwm_init == 0)
  {
    tuya_light_device_pwm_hw_init( );
    is_pwm_init = 1;
  }

#endif
  switch (hw_table->light_mode)
  {
    case LIGHT_MODE_RGBCW:
    case LIGHT_MODE_RGBC:
    case LIGHT_MODE_RGB:
      __change_pwm_if_needed (&R_value, &G_value, &B_value, &CW_value, &WW_value);
      PR_DEBUG ("22 R=%d G=%d B=%d C=%d W=%d", R_value, G_value, B_value, CW_value, WW_value);
      __light_pwm_set_duty (R_value, 0);
      __light_pwm_set_duty (G_value, 1);
      __light_pwm_set_duty (B_value, 2);

      if (hw_table->light_mode <= LIGHT_MODE_RGBC)
      {
        __light_pwm_set_duty (CW_value, 3);

        if (hw_table->light_mode == LIGHT_MODE_RGBCW)
        {
          __light_pwm_set_duty (WW_value, 4);
        }
      }

      break;

    case LIGHT_MODE_CW:
    case LIGHT_MODE_C:
      __light_pwm_set_duty (CW_value, 0);

      if (hw_table->light_mode == LIGHT_MODE_CW)
      {
        __light_pwm_set_duty (WW_value, 1);
      }

      break;

    default:
      break;
  }

  //PR_DEBUG("R=%d G=%d B=%d C=%d W=%d", R_value, G_value, B_value, CW_value, WW_value);
}

STATIC VOID tuya_prod_light_pwm_output (USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value,
                                        USHORT_T WW_value)
{
  HW_TABLE_S* hw_table = get_hw_table();
  static UCHAR_T is_pwm_init = 1;

  //PR_DEBUG("11 R=%d G=%d B=%d C=%d W=%d", R_value, G_value, B_value, CW_value, WW_value);
#if 0
  if (R_value == 0 && G_value == 0 && B_value == 0 && CW_value == 0 && WW_value == 0)
  {
    is_pwm_init = 0;
    tuya_light_device_pwm_mute();
    return;
  }

  if (is_pwm_init == 0)
  {
    tuya_light_device_pwm_hw_init( );
    is_pwm_init = 1;
  }
#endif
  switch (hw_table->light_mode)
  {
    case LIGHT_MODE_RGBCW:
    case LIGHT_MODE_RGBC:
    case LIGHT_MODE_RGB:
      PR_DEBUG ("22 R=%d G=%d B=%d C=%d W=%d", R_value, G_value, B_value, CW_value, WW_value);
      __light_pwm_set_duty (R_value, 0);
      __light_pwm_set_duty (G_value, 1);
      __light_pwm_set_duty (B_value, 2);

      if (hw_table->light_mode <= LIGHT_MODE_RGBC)
      {
        __light_pwm_set_duty (CW_value, 3);

        if (hw_table->light_mode == LIGHT_MODE_RGBCW)
        {
          __light_pwm_set_duty (WW_value, 4);
        }
      }

      break;

    case LIGHT_MODE_CW:
    case LIGHT_MODE_C:
      __light_pwm_set_duty (CW_value, 0);

      if (hw_table->light_mode == LIGHT_MODE_CW)
      {
        __light_pwm_set_duty (WW_value, 1);
      }

      break;

    default:
      break;
  }

  //PR_DEBUG("R=%d G=%d B=%d C=%d W=%d", R_value, G_value, B_value, CW_value, WW_value);
}


STATIC VOID tuya_light_iic_sm726_output (USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value,
                                         USHORT_T WW_value)
{
  HW_TABLE_S* hw_table = get_hw_table();
  UCHAR_T sm16726b_value[3] = {0xff, 0xff, 0xff};
  STATIC USHORT_T last_val[5] = {0};

  if (hw_table->light_mode <= LIGHT_MODE_RGB)
  {
    sm16726b_value[hw_table->i2c_cfg.out_pin_num[0]] = IIC_DATA_SEND_CAL (R_value);
    sm16726b_value[hw_table->i2c_cfg.out_pin_num[1]] = IIC_DATA_SEND_CAL (G_value);
    sm16726b_value[hw_table->i2c_cfg.out_pin_num[2]] = IIC_DATA_SEND_CAL (B_value);

    if (hw_table->light_mode <= LIGHT_MODE_RGBC)
    {
      if (last_val[3] != CW_value)
      {
        pwm_set_duty (CW_value, 0);
        last_val[3] = CW_value;
        PR_DEBUG ("CW=%d", CW_value);
      }

      if (hw_table->light_mode == LIGHT_MODE_RGBCW)
      {
        if (last_val[3] != CW_value)
        {
          pwm_set_duty (WW_value, 1);
          last_val[4] = WW_value;
          PR_DEBUG ("WW=%d", WW_value);
        }
      }
    }

    if (last_val[0] != sm16726b_value[hw_table->i2c_cfg.out_pin_num[0]] || \
        last_val[1] != sm16726b_value[hw_table->i2c_cfg.out_pin_num[1]] || \
        last_val[2] != sm16726b_value[hw_table->i2c_cfg.out_pin_num[2]])
    {
      SM16726B_WritePage (sm16726b_value, 3);
      last_val[0] = sm16726b_value[hw_table->i2c_cfg.out_pin_num[0]];
      last_val[1] = sm16726b_value[hw_table->i2c_cfg.out_pin_num[1]];
      last_val[2] = sm16726b_value[hw_table->i2c_cfg.out_pin_num[2]];
#if 1
      PR_DEBUG ("R[%d]=%d G[%d]=%d B[%d]=%d", hw_table->i2c_cfg.out_pin_num[0],
                sm16726b_value[hw_table->i2c_cfg.out_pin_num[0]], \
                hw_table->i2c_cfg.out_pin_num[1], sm16726b_value[hw_table->i2c_cfg.out_pin_num[1]], \
                hw_table->i2c_cfg.out_pin_num[2], sm16726b_value[hw_table->i2c_cfg.out_pin_num[2]]);
#endif
    }
  }
  else
  {
    PR_ERR ("device driver err!!");
  }
}

STATIC VOID tuya_light_iic_sm2135_output (USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value,
                                          USHORT_T WW_value)
{
  HW_TABLE_S* hw_table = get_hw_table();
  UCHAR_T SM2135_value[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  STATIC USHORT_T last_val[5] = {0};
  STATIC LIGHT_CURRENT_DATA_SEND_MODE_E last_final_data_mode = NULL_DATA_SEND;

  if (hw_table->light_mode <= LIGHT_MODE_RGB)
  {
    SM2135_value[hw_table->i2c_cfg.out_pin_num[0] + 2] = IIC_DATA_SEND_CAL (R_value);
    SM2135_value[hw_table->i2c_cfg.out_pin_num[1] + 2] = IIC_DATA_SEND_CAL (G_value);
    SM2135_value[hw_table->i2c_cfg.out_pin_num[2] + 2] = IIC_DATA_SEND_CAL (B_value);
  }

  if (hw_table->light_mode != LIGHT_MODE_RGB)
  {
    SM2135_value[hw_table->i2c_cfg.out_pin_num[3] + 2] = IIC_DATA_SEND_CAL (CW_value);
  }

  if (hw_table->light_mode == LIGHT_MODE_RGBCW || hw_table->light_mode == LIGHT_MODE_CW)
  {
    SM2135_value[hw_table->i2c_cfg.out_pin_num[4] + 2] = IIC_DATA_SEND_CAL (WW_value);
  }

  SM2135_value[0] = ( ( ( (hw_table->i2c_cfg.sm2135_rgb_current - 10) / 5) << 4) | ( ( (
                              hw_table->i2c_cfg.sm2135_cw_current - 10) / 5) ) );
  //  PR_DEBUG("sm2135 current = %x", SM2135_value[0]);
  LIGHT_CURRENT_DATA_SEND_MODE_E final_data_mode = light_send_final_data_mode_get();

  if (final_data_mode != MIX_DATA_SEND)
  {
    if (final_data_mode != NULL_DATA_SEND)
    {
      last_final_data_mode = final_data_mode;
    }

    //sm2135同一时间只允许输出一种灯光
    if (last_final_data_mode == COLOR_DATA_SEND)
    {
      if ( last_val[0] != SM2135_value[hw_table->i2c_cfg.out_pin_num[0] + 2] || \
           last_val[1] != SM2135_value[hw_table->i2c_cfg.out_pin_num[1] + 2] || \
           last_val[2] != SM2135_value[hw_table->i2c_cfg.out_pin_num[2] + 2])
      {
        SM2135_value[1] = 0x00; //输出彩光模式
        SM2315_WritePage (SM2135_value, SM2135_Device_Addr, 5);
        last_val[0] = SM2135_value[hw_table->i2c_cfg.out_pin_num[0] + 2];
        last_val[1] = SM2135_value[hw_table->i2c_cfg.out_pin_num[1] + 2];
        last_val[2] = SM2135_value[hw_table->i2c_cfg.out_pin_num[2] + 2];
#if 1 //output debug
        PR_DEBUG ("<%d> R[%d]=%d G[%d]=%d B[%d]=%d", last_final_data_mode, \
                  hw_table->i2c_cfg.out_pin_num[0], SM2135_value[hw_table->i2c_cfg.out_pin_num[0] + 2], \
                  hw_table->i2c_cfg.out_pin_num[1], SM2135_value[hw_table->i2c_cfg.out_pin_num[1] + 2], \
                  hw_table->i2c_cfg.out_pin_num[2], SM2135_value[hw_table->i2c_cfg.out_pin_num[2] + 2]);
#endif
      }
    }
    else if (last_final_data_mode == WHITE_DATA_SEND)
    {
      if ( last_val[3] != SM2135_value[hw_table->i2c_cfg.out_pin_num[3] + 2] || \
           last_val[4] != SM2135_value[hw_table->i2c_cfg.out_pin_num[4] + 2])
      {
        SM2135_value[1] = 0x80; //输出白光模式
        SM2315_WritePage (SM2135_value, SM2135_Device_Addr, 2);
        SM2315_WritePage (&SM2135_value[5], SM2135_Device_Addr+5, 2);
        last_val[3] = SM2135_value[hw_table->i2c_cfg.out_pin_num[3] + 2];
        last_val[4] = SM2135_value[hw_table->i2c_cfg.out_pin_num[4] + 2];
#if 1 //output debug
        PR_DEBUG ("<%d> C[%d]=%d W[%d]=%d", last_final_data_mode, \
                  hw_table->i2c_cfg.out_pin_num[3], SM2135_value[hw_table->i2c_cfg.out_pin_num[3] + 2], \
                  hw_table->i2c_cfg.out_pin_num[4], SM2135_value[hw_table->i2c_cfg.out_pin_num[4] + 2]);
#endif
      }
    }
  }
  else
  {
    PR_ERR ("Mixed white and colored output is not supported !!!");
  }
}

STATIC VOID tuya_light_device_output_precision_set (USHORT_T* R_value, USHORT_T* G_value, USHORT_T* B_value,
                                                    USHORT_T* CW_value, USHORT_T* WW_value)
{
  *R_value  = *R_value  * PWM_MAX / LIGHT_MAX_CTL_VAL;
  *G_value  = *G_value  * PWM_MAX / LIGHT_MAX_CTL_VAL;
  *B_value  = *B_value  * PWM_MAX / LIGHT_MAX_CTL_VAL;
  *CW_value = *CW_value * PWM_MAX / LIGHT_MAX_CTL_VAL;
  *WW_value = *WW_value * PWM_MAX / LIGHT_MAX_CTL_VAL;
}

VOID tuya_prod_light_send_pwm_data (USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value,
                                    USHORT_T WW_value)
{
  HW_TABLE_S* hw_table = get_hw_table();
  tuya_light_device_output_precision_set (&R_value, &G_value, &B_value, &CW_value, &WW_value);

  if ( (hw_table->driver_mode == DRV_MODE_PWM_IO) || (hw_table->driver_mode == DRV_MODE_IIC_SMx726) )
  {
    if (hw_table->driver_mode == DRV_MODE_PWM_IO)
    {
      if (hw_table->light_pin[0].level_sta == LOW_LEVEL)
      {
        R_value = PWM_MAX - R_value;
      }

      if (hw_table->light_pin[1].level_sta == LOW_LEVEL)
      {
        G_value = PWM_MAX - G_value;
      }

      if (hw_table->light_pin[2].level_sta == LOW_LEVEL)
      {
        B_value = PWM_MAX - B_value;
      }
    }

    if (hw_table->light_pin[3].level_sta == LOW_LEVEL)
    {
      CW_value = PWM_MAX - CW_value;
    }

    if (hw_table->light_pin[4].level_sta == LOW_LEVEL)
    {
      WW_value = PWM_MAX - WW_value;
    }

    //互补
    if (hw_table->cw_type == CW_TYPE) //CW添加白光互补输出
    {
      WW_value = tuya_light_pwm_set_hubu (WW_value);
    }
  }

  switch (hw_table->driver_mode)
  {
    case DRV_MODE_PWM_IO:
      tuya_prod_light_pwm_output (R_value, G_value, B_value, CW_value, WW_value);
      break;

    case DRV_MODE_IIC_SMx726:
      tuya_light_iic_sm726_output (R_value, G_value, B_value, CW_value, WW_value);
      break;

    case DRV_MODE_IIC_SM2135:
      tuya_light_iic_sm2135_output (R_value, G_value, B_value, CW_value, WW_value);
      break;

    case DRV_MODE_PWM_9231:
      break;

    default:
      break;
  }
}


VOID tuya_light_send_pwm_data (USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value,
                               USHORT_T WW_value)
{
  HW_TABLE_S* hw_table = get_hw_table();
  tuya_light_device_output_precision_set (&R_value, &G_value, &B_value, &CW_value, &WW_value);

  if ( (hw_table->driver_mode == DRV_MODE_PWM_IO) || (hw_table->driver_mode == DRV_MODE_IIC_SMx726) )
  {
    if (hw_table->driver_mode == DRV_MODE_PWM_IO)
    {
      if (hw_table->light_pin[0].level_sta == LOW_LEVEL)
      {
        R_value = PWM_MAX - R_value;
      }

      if (hw_table->light_pin[1].level_sta == LOW_LEVEL)
      {
        G_value = PWM_MAX - G_value;
      }

      if (hw_table->light_pin[2].level_sta == LOW_LEVEL)
      {
        B_value = PWM_MAX - B_value;
      }
    }

    if (hw_table->light_pin[3].level_sta == LOW_LEVEL)
    {
      CW_value = PWM_MAX - CW_value;
    }

    if (hw_table->light_pin[4].level_sta == LOW_LEVEL)
    {
      WW_value = PWM_MAX - WW_value;
    }

    //互补
    if (hw_table->cw_type == CW_TYPE) //CW添加白光互补输出
    {
      WW_value = tuya_light_pwm_set_hubu (WW_value);
    }
  }

  switch (hw_table->driver_mode)
  {
    case DRV_MODE_PWM_IO:
      tuya_light_pwm_output (R_value, G_value, B_value, CW_value, WW_value);
      break;

    case DRV_MODE_IIC_SMx726:
      tuya_light_iic_sm726_output (R_value, G_value, B_value, CW_value, WW_value);
      break;

    case DRV_MODE_IIC_SM2135:
      tuya_light_iic_sm2135_output (R_value, G_value, B_value, CW_value, WW_value);
      break;

    case DRV_MODE_PWM_9231:
      break;

    default:
      break;
  }
}

VOID tuya_light_pwm_stop_sta_set (VOID)
{
  HW_TABLE_S* hw_table = get_hw_table();
  UCHAR_T i;

  for (i = 0; i < hw_table->pwm_channel_num; i ++)
  {
    if (hw_table->light_pin[i].pin_num != NONE)
    {
      if (hw_table->light_pin[i].level_sta == LOW_LEVEL)
      {
        tuya_light_gpio_init (hw_table->light_pin[i].pin_num, TUYA_GPIO_OUT);
        tuya_light_gpio_ctl (hw_table->light_pin[i].pin_num, 1);
      }
    }
  }
}


