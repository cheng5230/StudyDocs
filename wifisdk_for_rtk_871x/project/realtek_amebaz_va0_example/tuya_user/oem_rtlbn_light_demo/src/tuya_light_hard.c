
#include "tuya_adapter_platform.h"
#include "tuya_device.h"
#include "tuya_common_light.h"
#include "tuya_light_hard.h"
#include "tuya_light_lib.h"

#define CHAN_NUM 5

#define LIGHT_MAX_CTL_VAL 512
#define PWM_MAX 1000

#define DEFAULT_BRIGNT_PERCENT 50 //初始化亮度 50%
#define DEFAULT_TEMP_PERCENT 100  //初始色温 	 全冷 100%

#define BRIGHT_LMT_MAX 100 //实际最高亮度为100%
#define BRIGHT_LMT_MIN 10  //实际最低亮度为10%

CONST CHAR_T *light_color_mode[] = {"rgbwc", "rgbc", "rgb", "cw", "c"}; /* 字符串数组 */
CONST CHAR_T color_count[] = {5, 4, 3, 2, 1};
CONST CHAR_T *wfcfg[] = {"spcl", "old"};
CONST CHAR_T *light_driver_mode[] = {"pwm", "i2c-sm726", "i2c-sm2135"};
CONST CHAR_T *cw_driver_type[] = {"cw", "cct"};

CONST CHAR_T *color_table[] = {"r", "g", "b", "c", "w"};
CONST CHAR_T *iic_cfg_table[] = {"iicr", "iicg", "iicb", "iicc", "iicw"};
CONST CHAR_T *pin_level_table[] = {"low", "high"};

CONST UCHAR_T lowpower_pin_table[] = {LOW_POWER_0, LOW_POWER_1, LOW_POWER_2, LOW_POWER_3, LOW_POWER_4};
//根据模块类型默认定义
CONST UCHAR_T iic_scl_pin[] = {I2C_SCL_0, I2C_SCL_1, I2C_SCL_2, I2C_SCL_3, I2C_SCL_4};
CONST UCHAR_T iic_sda_pin[] = {I2C_SDA_0, I2C_SDA_1, I2C_SDA_2, I2C_SDA_3, I2C_SDA_4};

//下为原SOC最通用gamma数值
//RED 0.80-100%
STATIC CONST BYTE gamma_red[] = {
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
    236, 238, 239, 240, 241, 243, 244, 245, 246, 248, 249, 250, 251, 253, 254, 255};

//GREEN 0.6-70%
STATIC CONST BYTE gamma_green[] = {
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
    166, 167, 168, 169, 170, 172, 173, 174, 175, 176, 177, 179};

//BLUE 0.6-75%
STATIC CONST BYTE gamma_blue[] = {
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
    188, 189, 190, 191};

STATIC HW_TABLE_S g_hw_table;

STATIC TIMER_ID wf_config_timer;
STATIC VOID wf_config_timer_cb(UINT timerID, PVOID pTimerArg);

STATIC VOID tuya_light_gpio_pin_config(UINT_T io_info[])
{
    UCHAR i, index, cnt;
    char pin_name[] = {'r', 'g', 'b', 'c', 'w'};
    UINT pwm_gpio_info[6] = {PWM_0_OUT_IO_NUM, PWM_1_OUT_IO_NUM, PWM_2_OUT_IO_NUM, PWM_3_OUT_IO_NUM, PWM_4_OUT_IO_NUM, PWM_5_OUT_IO_NUM};

    cnt = 0;
    if (g_hw_table.driver_mode == DRV_MODE_IIC_SMx726)
    {
        i = 3;
    }
    else if (g_hw_table.light_mode >= LIGHT_MODE_CW)
    {
        i = 3;
    }
    else
    {
        i = 0;
    }

    for (; i < 6; i++)
    {
        for (index = 0; index < 6; index++)
        {
            if (pwm_gpio_info[index] == g_hw_table.light_pin[i].pin_num)
            {
                break;
            }
        }
        if (index >= 6)
            continue;

        PR_DEBUG("pin:%c num:%d", pin_name[i], pwm_gpio_info[index]);
        io_info[cnt] = pwm_gpio_info[index];
        cnt++;
        if (cnt >= g_hw_table.pwm_channel_num)
            break;
    }
}

/******************************************************************************
 * FunctionName : hw_config_def_sets
 * Description  : default 配置设定，要实现不同的灯，更改该函数的中配置
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID hw_config_def_set(VOID)
{
    //功能配置
    g_hw_table.module_type = MODULE_TYPE_02;
    g_hw_table.wf_init_cfg = WF_CFG_OLD;
    g_hw_table.light_mode = LIGHT_MODE_RGBCW;
    g_hw_table.power_onoff_mode = POWER_ONOFF_MODE_SHADE;
    //
    g_hw_table.driver_mode = DRV_MODE_PWM_IO;
    g_hw_table.cw_type = CW_TYPE;
    //
    g_hw_table.pwm_frequency = 1000;
    g_hw_table.cw_max_power = 100;
    //
    g_hw_table.bright_max_precent = 100;
    g_hw_table.bright_min_precent = 10;
    g_hw_table.color_max_precent = 100;
    g_hw_table.color_min_precent = 10;
    //
    g_hw_table.dev_memory = TRUE;
    g_hw_table.cw_rgb_mutex = TRUE;

    //配网配置
    g_hw_table.reset_dev_mode = DRV_RESET_BY_POWER_ON_TIME_COUNT;
    g_hw_table.reset_count = 3;
    //
    g_hw_table.netconfig_color = CONFIG_COLOR_C;
    g_hw_table.netconfig_bright = 50;
    //
    g_hw_table.def_light_color = CONFIG_COLOR_C;
    g_hw_table.def_bright_precent = 100;
    g_hw_table.def_temp_precent = 100;

    //通道配置
    g_hw_table.pwm_channel_num = color_count[g_hw_table.light_mode];
    g_hw_table.light_pin[0].pin_num = PWM_0_OUT_IO_NUM; //R
    g_hw_table.light_pin[0].level_sta = HIGH_LEVEL;
    g_hw_table.light_pin[1].pin_num = PWM_1_OUT_IO_NUM; //G
    g_hw_table.light_pin[1].level_sta = HIGH_LEVEL;
    g_hw_table.light_pin[2].pin_num = PWM_2_OUT_IO_NUM; //B
    g_hw_table.light_pin[2].level_sta = HIGH_LEVEL;
    g_hw_table.light_pin[3].pin_num = PWM_3_OUT_IO_NUM; //C
    g_hw_table.light_pin[3].level_sta = HIGH_LEVEL;
    g_hw_table.light_pin[4].pin_num = PWM_4_OUT_IO_NUM; //W
    g_hw_table.light_pin[4].level_sta = HIGH_LEVEL;

    //IIC配置
    g_hw_table.lowpower_pin = NONE;
}

/******************************************************************************
 * FunctionName : tuya_light_device_hw_init
 * Description  : 灯硬件配置初始化
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_device_hw_init(VOID)
{
    OPERATE_RET op_ret;
    USHORT_T pwm_frequecy;
    UCHAR_T pwm_channel_num;
    UINT_T pwm_val = 0;
    UINT_T io_info[5]; //默认引脚配置顺序

    hw_config_def_set(); /* 装载default 配置 */

    pwm_frequecy = 1000 * 1000 / g_hw_table.pwm_frequency;
    pwm_channel_num = g_hw_table.pwm_channel_num;

    if ((g_hw_table.driver_mode == DRV_MODE_PWM_IO) || (g_hw_table.driver_mode == DRV_MODE_IIC_SMx726 && g_hw_table.light_mode <= LIGHT_MODE_RGB))
    {
        tuya_light_gpio_pin_config(io_info);
        tuya_light_pwm_init(pwm_frequecy, &pwm_val, pwm_channel_num, io_info);
    }

    tuya_light_hw_timer_init();

    op_ret = sys_add_timer(wf_config_timer_cb, NULL, &wf_config_timer);
    if (OPRT_OK != op_ret)
    {
        PR_ERR("wf_direct_timer_cb add error:%d", op_ret);
        return;
    }

    //配置为CCT/CW模式
    tuya_light_cw_type_set((LIGHT_CW_TYPE_E)g_hw_table.cw_type);

    //配网设备重配网方式--只能是开关次数配网！
    tuya_light_reset_mode_set(DRV_RESET_BY_POWER_ON_TIME_COUNT);

    //灯光断电记忆功能配置（不影响情景模式中的配置）
    tuya_light_memory_flag_set(g_hw_table.dev_memory);

    //CW混光最大输出功率
    tuya_light_cw_max_power_set(g_hw_table.cw_max_power);

    //配置灯光库调光级数
    tuya_light_ctrl_precision_set(LIGHT_MAX_CTL_VAL);
}

STATIC LIGHT_COLOR_TYPE_E tuya_light_color_type_get(VOID)
{
    switch (g_hw_table.light_mode)
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

STATIC NET_LIGHT_TYPE_E tuya_light_net_light_type_get(VOID)
{
    switch (g_hw_table.netconfig_color)
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

/******************************************************************************
 * FunctionName : hw_config_def_sets
 * Description  : default 配置设定
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_config_param_set(LIGHT_DEFAULT_CONFIG_S *config)
{
    config->wf_cfg = g_hw_table.wf_init_cfg;

    config->wf_rst_cnt = g_hw_table.reset_count;

    config->color_type = tuya_light_color_type_get();

    config->light_drv_mode = g_hw_table.driver_mode;

    config->net_light_type = tuya_light_net_light_type_get();

    config->def_light_color = g_hw_table.def_light_color;
    config->def_bright_precent = g_hw_table.def_bright_precent;
    config->def_temp_precent = g_hw_table.def_temp_precent;

    config->bright_max_precent = g_hw_table.bright_max_precent;
    config->bright_min_precent = g_hw_table.bright_min_precent;

    config->color_bright_max_precent = g_hw_table.color_max_precent;
    config->color_bright_min_precent = g_hw_table.color_min_precent;

    config->power_onoff_type = g_hw_table.power_onoff_mode;

    config->whihe_color_mutex = g_hw_table.cw_rgb_mutex;
}

//获取完RGB下发数值之后使用校正
VOID tuya_light_gamma_adjust(USHORT red, USHORT green, USHORT blue, USHORT *adjust_red, USHORT *adjust_green, USHORT *adjust_blue)
{
    //取值分级和实际输出步进分级转换（主要用于gamma只能取255，但需求1023的变化分级）
#if GAMMA_ENABLE
    *adjust_red = gamma_red[red * 255 / LIGHT_MAX_CTL_VAL] * LIGHT_MAX_CTL_VAL / 255;
    *adjust_green = gamma_green[green * 255 / LIGHT_MAX_CTL_VAL] * LIGHT_MAX_CTL_VAL / 255;
    *adjust_blue = gamma_blue[blue * 255 / LIGHT_MAX_CTL_VAL] * LIGHT_MAX_CTL_VAL / 255;
#else
    *adjust_red = red;
    *adjust_green = green;
    *adjust_blue = blue;
#endif
}

STATIC VOID tuya_set_wf_sta_light(UINT time)
{

    sys_start_timer(wf_config_timer, time, TIMER_CYCLE);
}

STATIC tuya_set_default_light(CONIFG_COLOR_E light_color, USHORT light_bright)
{
    switch (light_color)
    {
    case CONFIG_COLOR_R:
        if (light_color == LIGHT_MODE_CW)
        {
            if (g_hw_table.cw_type == CW_TYPE)
            {
                tuya_light_send_data(0, 0, 0, light_bright, 0);
            }
            else if (g_hw_table.cw_type == CCT_TYPE)
            {
                tuya_light_send_data(0, 0, 0, light_bright, LIGHT_MAX_CTL_VAL);
            }
        }
        else if (g_hw_table.light_mode == LIGHT_MODE_C)
        {
            tuya_light_send_data(0, 0, 0, light_bright, 0);
        }
        else
        {
            tuya_light_send_data(light_bright, 0, 0, 0, 0);
        }
        break;

    case CONFIG_COLOR_G:
        if (g_hw_table.light_mode == LIGHT_MODE_CW)
        {
            if (g_hw_table.cw_type == CW_TYPE)
            {
                tuya_light_send_data(0, 0, 0, light_bright, 0);
            }
            else if (g_hw_table.cw_type == CCT_TYPE)
            {
                tuya_light_send_data(0, 0, 0, light_bright, LIGHT_MAX_CTL_VAL);
            }
        }
        else if (g_hw_table.light_mode == LIGHT_MODE_C)
        {
            tuya_light_send_data(0, 0, 0, light_bright, 0);
        }
        else
        {
            tuya_light_send_data(0, light_bright, 0, 0, 0);
        }
        break;

    case CONFIG_COLOR_B:
        if (g_hw_table.light_mode == LIGHT_MODE_CW)
        {
            if (g_hw_table.cw_type == CW_TYPE)
            {
                tuya_light_send_data(0, 0, 0, light_bright, 0);
            }
            else if (g_hw_table.cw_type == CCT_TYPE)
            {
                tuya_light_send_data(0, 0, 0, light_bright, LIGHT_MAX_CTL_VAL);
            }
        }
        else if (g_hw_table.light_mode == LIGHT_MODE_C)
        {
            tuya_light_send_data(0, 0, 0, light_bright, 0);
        }
        else
        {
            tuya_light_send_data(0, 0, light_bright, 0, 0);
        }
        break;

    case CONFIG_COLOR_C:
        if (g_hw_table.light_mode == LIGHT_MODE_RGB)
        {
            tuya_light_send_data(light_bright, 0, 0, 0, 0);
        }
        else
        {
            if (g_hw_table.cw_type == CW_TYPE)
            {
                tuya_light_send_data(0, 0, 0, light_bright, 0);
            }
            else if (g_hw_table.cw_type == CCT_TYPE)
            {
                tuya_light_send_data(0, 0, 0, light_bright, LIGHT_MAX_CTL_VAL);
            }
        }
        break;

    case CONFIG_COLOR_W:
        if (g_hw_table.light_mode == LIGHT_MODE_RGB)
        {
            tuya_light_send_data(light_bright, 0, 0, 0, 0);
        }
        else if (g_hw_table.light_mode == LIGHT_MODE_RGBC || LIGHT_MODE_C == g_hw_table.light_mode)
        {
            tuya_light_send_data(0, 0, 0, light_bright, 0);
        }
        else
        {
            if (g_hw_table.cw_type == CW_TYPE)
            {
                tuya_light_send_data(0, 0, 0, 0, light_bright);
            }
            else if (g_hw_table.cw_type == CCT_TYPE)
            {
                tuya_light_send_data(0, 0, 0, light_bright, 0);
            }
        }
        break;

    default:
        break;
    }
}

STATIC VOID tuya_set_wf_lowpower_light(VOID)
{

    USHORT lowpower_bright = LIGHT_MAX_CTL_VAL / 100 * g_hw_table.def_bright_precent;
    tuya_set_default_light(g_hw_table.def_light_color, lowpower_bright);
}

VOID light_lowpower_enable(VOID)
{
    tuya_light_enter_lowpower_mode();
}

VOID light_lowpower_disable(VOID)
{
    tuya_light_exit_lowpower_mode();
}

VOID tuya_light_config_stat(CONF_STAT_E stat)
{
    PR_DEBUG("stat:%d", stat);
    STATIC BOOL_T config_flag = FALSE;

    switch (stat)
    {
    case CONF_STAT_SMARTCONFIG:
        PR_NOTICE("wf config EZ mode");
        config_flag = TRUE;
        tuya_set_wf_sta_light(250);
        break;

    case CONF_STAT_APCONFIG:
        PR_NOTICE("wf config AP mode");
        config_flag = TRUE;
        tuya_set_wf_sta_light(1500);
        break;

    case CONF_STAT_LOWPOWER:
        //关闭配网指示灯
        if (IsThisSysTimerRun(wf_config_timer))
        {
            sys_stop_timer(wf_config_timer);
        }

        tuya_set_wf_lowpower_light();
        PR_NOTICE("wf config LowPower mode");
        break;

    case CONF_STAT_UNCONNECT:
    default:
        //关闭配网指示灯
        if (IsThisSysTimerRun(wf_config_timer))
        {
            sys_stop_timer(wf_config_timer);
        }

        if (config_flag == TRUE)
        {
            config_flag = FALSE;
            light_init_stat_set();
        }
        break;
    }
}

STATIC VOID wf_config_timer_cb(UINT timerID, PVOID pTimerArg)
{
    STATIC BYTE_T cnt = 0;

    USHORT netconfig_bright = LIGHT_MAX_CTL_VAL / 100 * g_hw_table.netconfig_bright;

    if (cnt % 2 == 0)
    {
        tuya_set_default_light(g_hw_table.netconfig_color, netconfig_bright);
    }
    else
    {
        if (g_hw_table.cw_type == CCT_TYPE && g_hw_table.netconfig_color == CONFIG_COLOR_C)
        {
            tuya_light_send_data(0, 0, 0, 0, LIGHT_MAX_CTL_VAL);
        }
        else
        {
            tuya_light_send_data(0, 0, 0, 0, 0);
        }
    }

    cnt++;
}

STATIC VOID tuya_light_pwm_output(USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value, USHORT_T WW_value)
{
    switch (g_hw_table.light_mode)
    {
    case LIGHT_MODE_RGBCW:
    case LIGHT_MODE_RGBC:
    case LIGHT_MODE_RGB:
        pwm_set_duty(R_value, 0);
        pwm_set_duty(G_value, 1);
        pwm_set_duty(B_value, 2);
        if (g_hw_table.light_mode <= LIGHT_MODE_RGBC)
        {
            pwm_set_duty(CW_value, 3);
            if (g_hw_table.light_mode == LIGHT_MODE_RGBCW)
            {

                pwm_set_duty(WW_value, 4);
            }
        }
        break;

    case LIGHT_MODE_CW:
    case LIGHT_MODE_C:
        pwm_set_duty(CW_value, 0);
        if (g_hw_table.light_mode == LIGHT_MODE_CW)
        {
            pwm_set_duty(WW_value, 1);
        }
        break;

    default:
        break;
    }

    //PR_DEBUG("R=%d G=%d B=%d C=%d W=%d", R_value, G_value, B_value, CW_value, WW_value);
}

STATIC VOID tuya_light_device_output_precision_set(USHORT_T *R_value, USHORT_T *G_value, USHORT_T *B_value, USHORT_T *CW_value, USHORT_T *WW_value)
{
    *R_value = *R_value * PWM_MAX / LIGHT_MAX_CTL_VAL;
    *G_value = *G_value * PWM_MAX / LIGHT_MAX_CTL_VAL;
    *B_value = *B_value * PWM_MAX / LIGHT_MAX_CTL_VAL;
    *CW_value = *CW_value * PWM_MAX / LIGHT_MAX_CTL_VAL;
    *WW_value = *WW_value * PWM_MAX / LIGHT_MAX_CTL_VAL;
}

VOID tuya_light_send_pwm_data(USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value, USHORT_T WW_value)
{
    tuya_light_device_output_precision_set(&R_value, &G_value, &B_value, &CW_value, &WW_value);

    if ((g_hw_table.driver_mode == DRV_MODE_PWM_IO) || (g_hw_table.driver_mode == DRV_MODE_IIC_SMx726))
    {

        if (g_hw_table.driver_mode == DRV_MODE_PWM_IO)
        {
            if (g_hw_table.light_pin[0].level_sta == LOW_LEVEL)
            {
                R_value = PWM_MAX - R_value;
            }

            if (g_hw_table.light_pin[1].level_sta == LOW_LEVEL)
            {
                G_value = PWM_MAX - G_value;
            }

            if (g_hw_table.light_pin[2].level_sta == LOW_LEVEL)
            {
                B_value = PWM_MAX - B_value;
            }
        }

        if (g_hw_table.light_pin[3].level_sta == LOW_LEVEL)
        {
            CW_value = PWM_MAX - CW_value;
        }

        if (g_hw_table.light_pin[4].level_sta == LOW_LEVEL)
        {
            WW_value = PWM_MAX - WW_value;
        }
    }

    switch (g_hw_table.driver_mode)
    {
    case DRV_MODE_PWM_IO:
        tuya_light_pwm_output(R_value, G_value, B_value, CW_value, WW_value);
        break;

    case DRV_MODE_IIC_SMx726:
        break;

    case DRV_MODE_IIC_SM2135:
        break;

    case DRV_MODE_PWM_9231:
        break;

    default:
        break;
    }
}

VOID tuya_light_pwm_stop_sta_set(VOID)
{
    UCHAR_T i;

    for (i = 0; i < 5; i++)
    {
        PR_DEBUG("hw_table->light_pin[%d].pin_num:%d", i, g_hw_table.light_pin[i].pin_num);
        if (g_hw_table.light_pin[i].pin_num != NONE)
        {
            PR_NOTICE("hw_table->light_pin[i].level_sta:%d", g_hw_table.light_pin[i].level_sta);
            if (g_hw_table.light_pin[i].level_sta == LOW_LEVEL)
            {
                tuya_light_gpio_init(g_hw_table.light_pin[i].pin_num, TUYA_GPIO_OUT);
                tuya_light_gpio_ctl(g_hw_table.light_pin[i].pin_num, 1);
            }
        }
    }
}
