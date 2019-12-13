#include "tuya_common_light.h"
#include "tuya_light_lib.h"
#include "tuya_light_hard.h"
#include "uf_file.h"

#define TUYA_LIGH_LIB_VERSION "1.0.2"

#define FASE_SW_CNT_KEY "fsw_cnt_key"
#define LIGHT_DATA_KEY "light_data_key"
//#define FAST_JSON_KEY           "light_fast_json"       /* uf_file json配置的flash文件 */
#define FAST_DATA_KEY "light_fast_data" /* uf_file dp&reset cnt配置的flash文件 */

#define LIGHT_PRECISION 500              //调光精度
#define LIGHT_PRECISION_MAX 10000        //调光精度最大限制
#define LIGHT_PRECISION_MIN 100          //调光精度最低限制
#define LIGHT_PRECISION_BASE_TIME 306900 //调光精度步进计算基准时间

#define HW_TIMER_CYCLE 100       /* 单位:us */
#define NORMAL_DELAY_TIME 614    //hw_arm = us（以1023级，每步300us为准）= (306900/LIGHT_PRECISION)
#define SCENE_DELAY_TIME 100     //情景渐变计算假定延时时间(ms)
#define STANDARD_DELAY_TIMES 300 //无极调光时间(ms)
#define SHADE_THREAD_TIMES 300   //shade线程耗时(us)

#define NORMAL_DP_SAVE_TIME 2500 /* DP数据保存定时器启动时间 单位 MS */
#define LIGHT_RST_CNT_CYCLE 5000 /* 上电计数清除时间 单位 MS */

#define LIGHT_LOW_POWER_ENTER_IMEW 3000 //APP关闭后低功耗进入延时（等待灯光处理完毕）单位 ms
#define LOWPOWER_MODE_DELAY_TIME 1200   //低功耗状态下线程轮询时间 单位 ms

/******旧版本******/
#define BRIGHT_VAL_MAX 1000 //新面板下发最大亮度
#define BRIGHT_VAL_MIN 10   //新面板下发最小亮度

#define COLOR_DATA_LEN 12                                     //颜色数据长度
#define COLOR_DATA_DEFAULT "000003e803e8"                     //新版本默认彩光数据
#define FC_COLOR_DATA_DEFAULT "000003e8000a"                  //彩光&白光同时控制状态下，彩光初始数值

//新版本默认情景数据
#define SCENE_DATA_DEFAULT_RGB "000e0d00002e03e802cc00000000" //彩灯
#define SCENE_DATA_DEFAULT_CW "000e0d00002e03e8000000c80000"  //CW双色
#define SCENE_DATA_DEFAULT_DIM "000e0d00002e03e8000000c803e8" //单色灯

//情景参数
#define SCENE_HEAD_LEN 2                                      //情景头长度
#define SCENE_UNIT_LEN 26                                     //新版灯情景单元长度
#define SCENE_UNIT_NUM_MAX 8                                  //最大情景单元数

#define SCENE_DATA_LEN_MIN (SCENE_HEAD_LEN + SCENE_UNIT_LEN)                      //情景数据最小长度
#define SCENE_DATA_LEN_MAX (SCENE_HEAD_LEN + SCENE_UNIT_LEN * SCENE_UNIT_NUM_MAX) //情景数据最大长度

//音乐灯默认数据
#define MUSIC_INIT_VAL COLOR_DATA_DEFAULT

#ifdef KEY_CHANGE_SCENE_FUN
#define MAX_KEY_SCENE_BUF (SCENE_DATA_LEN_MAX * 8) //按键情景需要的长度
#define KEY_SCENE0_DEF_DATA "000d0d00002e03e802cc00000000"
#define KEY_SCENE1_DEF_DATA "010d0d000084000003e800000000"
#define KEY_SCENE2_DEF_DATA "020d0d00001403e803e800000000"
#define KEY_SCENE3_DEF_DATA "030e0d0000e80383031c00000000"
#define KEY_SCENE4_DEF_DATA "04464602007803e803e800000000464602007803e8000a00000000"
#define KEY_SCENE5_DEF_DATA "05464601000003e803e800000000464601007803e803e80000000046460100f003e803e800000000"
#define KEY_SCENE6_DEF_DATA "06464601000003e803e800000000464601007803e803e800000000"
#define KEY_SCENE7_DEF_DATA "07464602000003e803e800000000464602007803e803e80000000046460200f003e803e800000000464602003d03e803e80000000046460200ae03e803e800000000464602011303e803e800000000"
#else
#define MAX_KEY_SCENE_BUF 0
#endif

#define MAX_FLASH_BUF (512 + MAX_KEY_SCENE_BUF)

typedef enum
{
    WHITE_MODE = 0,
    COLOR_MODE,
    SCENE_MODE,
    MUSIC_MODE,
} SCENE_MODE_E;

typedef struct
{
    BOOL_T power;
    SCENE_MODE_E mode;
    USHORT_T bright;
    USHORT_T col_temp;
    UCHAR_T color[COLOR_DATA_LEN + 1];
    UCHAR_T scene[SCENE_DATA_LEN_MAX + 1];
    UINT_T countdown;
} DP_DATA_S;

typedef enum
{
    SCENE_TYPE_STATIC = 0,
    SCENE_TYPE_JUMP,
    SCENE_TYPE_SHADOW,
} SCENE_TYPE_E;

typedef enum
{
    NEW_SCENE = 0,
    OLD_SCENE_STATIC,
    OLD_SCENE_JUMP,
    OLD_SCENE_SHADOW,
} DP_SCENE_TYPE_E;

typedef enum
{
    CTRL_TYPE_JUMP = 0,
    CTRL_TYPE_SHADE,
} CRTL_TYPE_E;

//新版本最小的范围也超过了255，全部使用USHORT兼容
typedef struct
{
    USHORT_T red;
    USHORT_T green;
    USHORT_T blue;
    USHORT_T white;
    USHORT_T warm;
} BRIGHT_DATA_S;

typedef struct
{
    BRIGHT_DATA_S fin;  //灯光目标值
    BRIGHT_DATA_S curr; //灯光当前值

    SCENE_TYPE_E scene_type;
    UCHAR_T scene_cnt;

    BOOL_T scene_enable; //SCENE使能标志
    BOOL_T scene_new;    //计算新值标志

    BOOL_T shade_new; //新渐变标志
    //	BOOL_T memory;						//断电记忆标志
} LIGHT_HANDLE_S;

typedef struct
{
    UINT_T total;
    UINT_T hw_cnt;
    BOOL_T enable;
} HW_TIMER_S; /* 硬件定时器设定参数 */
STATIC HW_TIMER_S tuya_light_hw_timer;

STATIC UCHAR_T rst_cnt; /* 灯重置次数的计数值 */
STATIC UCHAR_T prod_test_mode;

STATIC DP_DATA_S dp_data;
STATIC LIGHT_HANDLE_S light_handle;

#ifdef KEY_CHANGE_SCENE_FUN
STATIC CHAR_T key_scene_data[8][SCENE_DATA_LEN_MAX + 1];
#else
STATIC CHAR_T *key_scene_data;
#endif

typedef struct
{
    TIMER_ID timer_init_dpdata;
    TIMER_ID timer_wf_stat;
    TIMER_ID timer_data_autosave;
    TIMER_ID timer_countdown;
    TIMER_ID timer_ram_checker;
    TIMER_ID timer_key;
    TIMER_ID lowpower;

    MUTEX_HANDLE mutex;
    MUTEX_HANDLE light_send_mutex;

    SEM_HANDLE sem_shade;
} SYS_HANDLE_S;

STATIC SYS_HANDLE_S sys_handle;

typedef struct
{
    SCENE_TYPE_E type;
    UCHAR_T speed; /* 需要精确数值保证计算 */
    UCHAR_T times; /* 0-100 */
} SCENE_HEAD_S;

BOOL_T fast_config_ok = TRUE; /* 快速启动OK的flag */

/*******************************************************
                内部函数、变量声明
********************************************************/
STATIC OPERATE_RET light_data_write_flash(VOID);
VOID light_dp_data_default_set(VOID);
STATIC VOID light_scene_start(VOID);
STATIC VOID light_scene_stop(VOID);
VOID light_dp_upload(VOID);
STATIC VOID light_shade_start(UINT_T time);
STATIC VOID light_shade_stop(VOID);
STATIC VOID light_bright_start(VOID);
STATIC VOID light_switch_set(BOOL_T stat);
STATIC VOID light_cct_col_temper_keep(UCHAR data);
STATIC USHORT_T light_default_bright_get(VOID);
STATIC USHORT_T light_default_temp_get(VOID);
STATIC VOID light_default_color_get(UCHAR *data);
STATIC UCHAR dp_data_change_for_old_type(USHORT_T val);
//按键
STATIC VOID light_key_scene_data_save(char *str, UCHAR len);
STATIC VOID light_key_scene_data_default_set(VOID);
//系统定时器回调
STATIC VOID lowpower_timer_cb(UINT_T timerID, PVOID_T pTimerArg);

/* 配置相关定义 */
STATIC LIGHT_DEFAULT_CONFIG_S _def_cfg = {0};                             //通用配置项
STATIC LIGHT_CW_TYPE_E _cw_type = LIGHT_CW_PWM;                           //白光输出模式
STATIC LIGHT_RESET_NETWORK_STA_E _reset_dev_mode = LIGHT_RESET_BY_ANYWAY; //模块状态重置方式
STATIC BOOL_T _light_memory;                                              //断电记忆
STATIC UCHAR_T _white_light_max_power = 100;                              //白光最大功率（冷暖混色时）

STATIC BOOL_T lowpower_flag = FALSE; /* 低功耗控制位 */

/* 功能相关定义 */
STATIC UCHAR_T key_trig_num = 0; //按键触发次数
STATIC USHORT_T light_precison = LIGHT_PRECISION;
STATIC UINT_T normal_delay_time = NORMAL_DELAY_TIME;

/*******************************************************
                    外部函数、变量声明
********************************************************/
extern OPERATE_RET light_prod_init(PROD_TEST_TYPE_E type, APP_PROD_CB callback);

/*******************************************************
                    功能函数
********************************************************/
STATIC UCHAR_T __asc2hex(CHAR_T asccode)
{
    UCHAR_T ret;

    if ('0' <= asccode && asccode <= '9')
        ret = asccode - '0';
    else if ('a' <= asccode && asccode <= 'f')
        ret = asccode - 'a' + 10;
    else if ('A' <= asccode && asccode <= 'F')
        ret = asccode - 'A' + 10;
    else
        ret = 0;

    return ret;
}

STATIC CHAR_T __hex2asc(CHAR_T data)
{
    CHAR_T result;
    if ((data >= 0) && (data <= 9)) //变成ascii数字
        result = data + 0x30;
    else if ((data >= 10) && (data <= 15)) //变成ascii小写字母
        result = data + 0x37 + 32;
    else
        result = 0xff;
    return result;
}

//两字符合并为一字节
STATIC UCHAR_T __str2byte(UCHAR_T a, UCHAR_T b)
{
    return (a << 4) | (b & 0xf);
}

//四字符合并为一整型
STATIC UINT_T __str2short(u32_t a, u32_t b, u32_t c, u32_t d)
{
    return (a << 12) | (b << 8) | (c << 4) | (d & 0xf);
}

//绝对值
STATIC INT_T __abs(INT_T value)
{
    return value > 0 ? value : -value;
}

STATIC CHAR_T *dpid2str(CHAR_T *buf, BYTE_T dpid)
{
    memset(buf, 0, SIZEOF(buf));
    snprintf(buf, SIZEOF(buf), "%d", dpid);
}

//获取最大值
STATIC UINT_T __max_value(UINT_T a, UINT_T b, UINT_T c, UINT_T d, UINT_T e)
{
    int x = a > b ? a : b;
    int y = c > d ? c : d;
    int z = x > y ? x : y;
    return z > e ? z : e;
}

//HSV转RGB
STATIC VOID hsv2rgb(FLOAT_T h, FLOAT_T s, FLOAT_T v, USHORT_T *color_r, USHORT_T *color_g, USHORT_T *color_b)
{
    FLOAT_T h60, f;
    UINT_T h60f, hi;

    h60 = h / 60.0;
    h60f = h / 60;

    hi = (int)h60f % 6;
    f = h60 - h60f;

    FLOAT_T p, q, t;

    p = v * (1 - s);
    q = v * (1 - f * s);
    t = v * (1 - (1 - f) * s);

    FLOAT_T r, g, b;

    r = g = b = 0;
    if (hi == 0)
    {
        r = v;
        g = t;
        b = p;
    }
    else if (hi == 1)
    {
        r = q;
        g = v;
        b = p;
    }
    else if (hi == 2)
    {
        r = p;
        g = v;
        b = t;
    }
    else if (hi == 3)
    {
        r = p;
        g = q;
        b = v;
    }
    else if (hi == 4)
    {
        r = t;
        g = p;
        b = v;
    }
    else if (hi == 5)
    {
        r = v;
        g = p;
        b = q;
    }

    r = (r * (FLOAT)light_precison);
    g = (g * (FLOAT)light_precison);
    b = (b * (FLOAT)light_precison);

    r *= 100;
    g *= 100;
    b *= 100;

    *color_r = (r + 50) / 100;
    *color_g = (g + 50) / 100;
    *color_b = (b + 50) / 100;
}

//彩色数据转化为字符
STATIC light_color_data_hex2asc(UCHAR_T *buf, USHORT_T data, BYTE_T len)
{
    if (len == 2)
    {
        buf[0] = __hex2asc((UCHAR)(data >> 4) & 0x0f);
        buf[1] = __hex2asc((UCHAR)data & 0x0f);
    }
    else if (len == 4)
    {
        buf[0] = __hex2asc((UCHAR)(data >> 12) & 0x0f);
        buf[1] = __hex2asc((UCHAR)(data >> 8) & 0x0f);
        buf[2] = __hex2asc((UCHAR)(data >> 4) & 0x0f);
        buf[3] = __hex2asc((UCHAR)data & 0x0f);
    }
}

/******************************************************************************
 * FunctionName : lowpower_timer_cb
 * Description  : 进入低功耗的回调
 * Parameters   : timerID --> timer ID;
                pTimerArg --> 回调参数
 * Returns      : none
*******************************************************************************/
STATIC VOID lowpower_timer_cb(UINT_T timerID, PVOID_T pTimerArg)
{
    if (dp_data.power == FALSE)
    {
        if (FALSE == lowpower_flag)
        {
            light_lowpower_enable();
            lowpower_flag = TRUE;
        }
    }
}

/******************************************************************************
 * FunctionName : tuya_light_wf_stat_cb
 * Description  : wifi 状态的回调的处理
            wifi的状态:
                TUYA_STAT_LOW_POWER     -> STAT_LOW_POWER       -> idle status,use to external config network
                TUYA_STAT_UNPROVISION   -> STAT_UNPROVISION     -> smart config status
                TUYA_STAT_AP_STA_UNCFG  -> STAT_AP_STA_UNCFG    -> ap WIFI config status
                TUYA_STAT_AP_STA_DISC   -> STAT_AP_STA_DISC     -> ap WIFI already config,station disconnect
                TUYA_STAT_AP_STA_CONN   -> STAT_AP_STA_CONN     -> ap station mode,station connect
                TUYA_STAT_STA_DISC      -> STAT_STA_DISC        -> only station mode,disconnect
                TUYA_STAT_STA_CONN      -> STAT_STA_CONN        -> station mode connect
                TUYA_STAT_CLOUD_CONN    -> STAT_CLOUD_CONN      -> cloud connect
                TUYA_STAT_AP_CLOUD_CONN -> STAT_AP_CLOUD_CONN   -> cloud connect and ap start
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_wf_stat_cb(GW_WIFI_NW_STAT_E wf_stat)
{
    OPERATE_RET op_ret;
    STATIC TUYA_GW_WIFI_NW_STAT_E last_wf_stat = 0xff;

    if (last_wf_stat != wf_stat)
    {
        PR_DEBUG("last_wf_stat:%d", last_wf_stat);
        PR_DEBUG("wf_stat:%d", wf_stat);
        PR_DEBUG("size:%d", tuya_light_get_free_heap_size());
        switch (wf_stat)
        {
        case TUYA_STAT_UNPROVISION:
        {
            //light_dp_data_default_set();
            //tuya_light_config_stat(CONF_STAT_SMARTCONFIG);
        }
        break;

        case TUYA_STAT_AP_STA_UNCFG:
        {
            //light_dp_data_default_set();
            //tuya_light_config_stat(CONF_STAT_APCONFIG);
        }
        break;

        case TUYA_STAT_STA_DISC:
        {
            tuya_light_config_stat(CONF_STAT_CONNECT);
            //tuya_light_config_stat(CONF_STAT_UNCONNECT);
        }
        break;

        case TUYA_STAT_LOW_POWER:
        {
            tuya_light_config_stat(CONF_STAT_LOWPOWER);
        }
        break;

        case TUYA_STAT_STA_CONN:
        case TUYA_STAT_AP_STA_CONN:
        {
            tuya_light_config_stat(CONF_STAT_CONNECT);
        }
        break;
        }
        last_wf_stat = wf_stat;
    }
}

/******************************************************************************
 * FunctionName : idu_timer_cb
 * Description  : MQTT连接状态查询及处理回调
 * Parameters   : timerID --> timer ID;
                pTimerArg --> 回调参数
 * Returns      : none
*******************************************************************************/
STATIC VOID idu_timer_cb(UINT_T timerID, PVOID_T pTimerArg)
{
    if (TRUE == tuya_light_get_gw_mq_conn_stat())
    { /* MQTT连接后同步DP数据 */
        light_dp_upload();
        light_lowpower_disable();
        sys_stop_timer(sys_handle.timer_init_dpdata); /* 并关闭该timer */
    }
}

/******************************************************************************
 * FunctionName : tuya_light_hw_timer_cb
 * Description  : 硬件定时器回调函数
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_hw_timer_cb(VOID)
{
    if (tuya_light_hw_timer.enable)
    {
        if (tuya_light_hw_timer.hw_cnt >= tuya_light_hw_timer.total)
        {
            PostSemaphore(sys_handle.sem_shade); /* 释放变化的线程 */
            tuya_light_hw_timer.hw_cnt = 0;
        }
        else
        {
            tuya_light_hw_timer.hw_cnt += HW_TIMER_CYCLE;
        }
    }
}

/******************************************************************************
 * FunctionName : countdown_timeout_cb
 * Description  : 倒计时timer回调
 * Parameters   : timerID --> timer ID;
                pTimerArg --> 回调参数
 * Returns      : none
*******************************************************************************/
STATIC VOID countdown_timeout_cb(UINT_T timerID, PVOID_T pTimerArg)
{
    if (dp_data.countdown > 0)
    {
        dp_data.countdown -= 60;
        PR_DEBUG("left times = %d", dp_data.countdown);
    }
    else
    {
        sys_stop_timer(sys_handle.timer_countdown);
        dp_data.countdown = 0;
    }

    if (dp_data.countdown == 0)
    {
        switch (dp_data.power)
        {
        case true:
            light_switch_set(FALSE);
            break;

        case false:
            light_switch_set(TRUE);
            break;

        default:
            break;
        }
        if (IsThisSysTimerRun(sys_handle.timer_countdown) == TRUE)
        {
            sys_stop_timer(sys_handle.timer_countdown);
        }
    }
    light_dp_upload(); /* 上报DP数据 */
}

/******************************************************************************
 * reset cnt处理
******************************************************************************/
/******************************************************************************
 * FunctionName : light_rst_cnt_tiemout_cb
 * Description  : 灯重置次数操作的回调处理，超过定时时间后的处理操作
 * Parameters   : timerID --> timer ID;
                
 * Returns      : none
*******************************************************************************/
STATIC VOID light_rst_cnt_tiemout_cb(UINT_T timerID, PVOID_T pTimerArg)
{
    light_rst_cnt_write(0);
    PR_DEBUG("set reset cnt -> 0");
}

/******************************************************************************
 * FunctionName : light_rst_cnt_write
 * Description  : 灯的重启次数的写入，直接写入到uf_file 文件中，确保快速的启动
 * Parameters   : val --> 写入口的val值
 * Returns      : none
*******************************************************************************/
OPERATE_RET light_rst_cnt_write(UCHAR_T val)
{
    OPERATE_RET op_ret;
    CHAR_T *out = NULL;
    ty_cJSON *root = NULL;

    root = ty_cJSON_CreateObject();
    if (NULL == root)
    {
        PR_ERR("ty_cJSON_CreateObject error...");
        return OPRT_CR_CJSON_ERR;
    }

    rst_cnt = val; /* 保存重置的次数，主要是清零的操作！！ */

    op_ret = tuya_fast_data_set(); /* 保存重置的次数 */
    if (op_ret != OPRT_OK)
    {
        return op_ret;
    }

    return OPRT_OK;
}

/******************************************************************************
 * FunctionName : tuya_light_rst_count_add
 * Description  : 灯的重启次数+1操作写入flash,启动清除的timer
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_rst_count_add(VOID)
{
    OPERATE_RET op_ret;
    TIMER_ID timer_rst_cnt;

    if (TRUE == tuya_light_dev_poweron())
    {
        PR_NOTICE("rest cnt ++");
        rst_cnt += 1;
        light_rst_cnt_write(rst_cnt);
    }

    op_ret = sys_add_timer(light_rst_cnt_tiemout_cb, NULL, &timer_rst_cnt);
    if (OPRT_OK != op_ret)
    {
        PR_ERR("reset_fsw_cnt timer add err:%d", op_ret);
    }
    else
    {
        sys_start_timer(timer_rst_cnt, LIGHT_RST_CNT_CYCLE, TIMER_ONCE); /* 启动一个timer，定时满足后清除cnt值 */
    }
    PR_NOTICE("current rst count: %d", rst_cnt);
}

/******************************************************************************
 * FunctionName : tuya_light_rst_count_judge
 * Description  : 灯的重启次数的判断，并执行重置的操作
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_rst_count_judge(VOID)
{
    OPERATE_RET op_ret;

    if ((rst_cnt >= _def_cfg.wf_rst_cnt) && (TRUE == fast_config_ok))
    {

        PR_NOTICE("set to reset cnt --> 0");

        //light_rst_cnt_write(0);                           /* 超过重置配网次数，恢复默认值并重置设备 */
        //rst_cnt = 0;                                      /* 恢复默认值,该值会在light_dp_data_default_set中写入flash */
        rst_cnt = 0xFF;

        /* 配置重置的方式为连续点击多次或者任何方式的模式时 */
        if (LIGHT_RESET_BY_ANYWAY == _reset_dev_mode || LIGHT_RESET_BY_POWER_ON_ONLY == _reset_dev_mode)
        {
            light_dp_data_default_set();
            tuya_light_dev_reset();
        }
    }
}

/******************************************************************************
 * 灯光数据处理部分
******************************************************************************/
/******************************************************************************
 * FunctionName : light_val_lmt_get
 * Description  : 白光亮度限制
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC USHORT_T light_val_lmt_get(USHORT_T dp_val)
{
    USHORT_T max = BRIGHT_VAL_MAX * _def_cfg.bright_max_precent / 100;
    USHORT_T min = BRIGHT_VAL_MAX * _def_cfg.bright_min_precent / 100;

    return ((dp_val - BRIGHT_VAL_MIN) * (max - min) / (BRIGHT_VAL_MAX - BRIGHT_VAL_MIN) + min);
}

/******************************************************************************
 * FunctionName : color_val_lmt_get
 * Description  : 彩光亮度限制
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC USHORT_T color_val_lmt_get(USHORT_T dp_val)
{
    USHORT_T max = BRIGHT_VAL_MAX * _def_cfg.color_bright_max_precent / 100;
    USHORT_T min = BRIGHT_VAL_MAX * _def_cfg.color_bright_min_precent / 100;

    return ((dp_val - BRIGHT_VAL_MIN) * (max - min) / (BRIGHT_VAL_MAX - BRIGHT_VAL_MIN) + min);
}

/******************************************************************************
 * FunctionName : light_bright_cw_change
 * Description  : 白光数据计算
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID light_bright_cw_change(USHORT_T bright, USHORT_T temperture, BRIGHT_DATA_S *value)
{
    USHORT_T brt;

    if (bright >= BRIGHT_VAL_MIN)
    { //一般下发状态，白光正常输出时下发数值不会低于最低下发值
        if (_def_cfg.whihe_color_mutex == FALSE && bright == BRIGHT_VAL_MIN)
        { //分控功能下最低下发数值情况处于关闭状态
            brt = 0;
        }
        else
        {
            brt = light_val_lmt_get(bright); //正常范围内计算实际输出范围
        }
    }
    else
    {
        brt = 0; //如果亮度出现下发数据小于最低限制情况（默认只有控制dp发送彩光数据时会清零白光数据），灯光默认关闭
    }

    if (_cw_type == LIGHT_CW_PWM)
    {                                                                                 //CW混光满足最大功率100% -- 200%
        brt = (_white_light_max_power * light_precison / 100) * brt / BRIGHT_VAL_MAX; //灯光最大分级依照最大功率扩大

        value->white = brt * temperture / BRIGHT_VAL_MAX; //依据新的亮度总值进行冷暖输出分配
        value->warm = brt - value->white;

        //如果上式计算中出现单路大于100%输出，此路输出限制在100%，并需要重新计算另外一路保持色温
        //低于最大值的一路根据 （大于最大值的另一路数据 / 最大值） 放大，100用于挺高计算精度
        if (value->white > light_precison)
        {
            value->warm = value->warm * (value->white * 100 / light_precison) / 100;
            value->white = light_precison;
        }

        //同一时间内只会存在一路数据大于最大分级，处理思路同上
        if (value->warm > light_precison)
        {
            value->white = value->white * (value->warm * 100 / light_precison) / 100;
            value->warm = light_precison;
        }
    }
    else if (_cw_type == LIGHT_CW_CCT)
    {
        value->white = light_precison * brt / BRIGHT_VAL_MAX;
        value->warm = light_precison * temperture / BRIGHT_VAL_MAX;
    }
}

/*	白光数据格式转化（字符转数值）	*/
STATIC VOID light_cw_value_get(CHAR_T *buf, BRIGHT_DATA_S *value)
{
    if (buf == NULL)
    {
        PR_ERR("invalid param...");
        return;
    }

    value->white = __str2short(__asc2hex(buf[0]), __asc2hex(buf[1]), __asc2hex(buf[2]), __asc2hex(buf[3]));
    value->warm = __str2short(__asc2hex(buf[4]), __asc2hex(buf[5]), __asc2hex(buf[6]), __asc2hex(buf[7]));

    if (value->white == 0 && _cw_type == LIGHT_CW_PWM)
    {
        value->warm = 0;
    }
    else
    {
        light_bright_cw_change(value->white, value->warm, value);
    }
}

/*	彩光数据格式转化（字符转数值）+ 处理（HSV转RGB）	*/
STATIC VOID light_rgb_value_get(CHAR_T *buf, BRIGHT_DATA_S *value)
{
    USHORT_T h, s, v;
    USHORT_T r, g, b;

    if (buf == NULL)
    {
        PR_ERR("invalid param...");
        return;
    }

    if (LIGHT_CW_CCT == _cw_type)
    {
        if (dp_data.mode == COLOR_MODE || (dp_data.mode == SCENE_MODE && light_handle.fin.white == 0))
        {
            light_handle.fin.warm = light_handle.curr.warm;
        }
    }

    h = __str2short(__asc2hex(buf[0]), __asc2hex(buf[1]), __asc2hex(buf[2]), __asc2hex(buf[3]));
    s = __str2short(__asc2hex(buf[4]), __asc2hex(buf[5]), __asc2hex(buf[6]), __asc2hex(buf[7]));
    v = __str2short(__asc2hex(buf[8]), __asc2hex(buf[9]), __asc2hex(buf[10]), __asc2hex(buf[11]));

    if (_def_cfg.whihe_color_mutex == FALSE)
    {
        if (v <= 10)
        {
            v = 0;
        }
        else
        {
            v = color_val_lmt_get(v);
        }
    }
    else
    {
        if (v >= 10)
            v = color_val_lmt_get(v);
    }

    hsv2rgb((FLOAT)h, (FLOAT)s / 1000.0, (FLOAT)v / 1000.0, &r, &g, &b);

    tuya_light_gamma_adjust(r, g, b, &value->red, &value->green, &value->blue);
}

/*	获取当前灯光渐变最大差值	*/
USHORT_T light_shade_max_value_get(BRIGHT_DATA_S *fin, BRIGHT_DATA_S *curr)
{
    USHORT_T max = __max_value(__abs(fin->red - curr->red), __abs(fin->green - curr->green), __abs(fin->blue - curr->blue),
                               __abs(fin->white - curr->white), __abs(fin->warm - curr->warm));

    return max;
}

/*	根据定义渐变时间计算灯光渐变对应到硬件定时器时间	*/
STATIC UINT light_shade_speed_get(USHORT_T time)
{
    USHORT_T step;
    UINT hw_set_cnt;

    step = light_shade_max_value_get(&light_handle.fin, &light_handle.curr);

    if (step == 0)
        return HW_TIMER_CYCLE;

    hw_set_cnt = 1000 * time / step - SHADE_THREAD_TIMES;

    return hw_set_cnt; //单位us
}

/*********************************************************
            	灯光功能处理函数
**********************************************************/
/*	目标点亮值获取	*/
STATIC VOID light_bright_data_get(VOID)
{
    UCHAR_T i;

    if (_def_cfg.whihe_color_mutex == TRUE)
    {
        memset(&light_handle.fin, 0, SIZEOF(light_handle.fin));

        switch (dp_data.mode)
        {
        case WHITE_MODE:
            light_bright_cw_change(dp_data.bright, dp_data.col_temp, &light_handle.fin);
            PR_DEBUG("light_handle.fin -> cw = %d   ww = %d", light_handle.fin.white, light_handle.fin.warm);
            break;

        case COLOR_MODE:
            light_rgb_value_get(dp_data.color, &light_handle.fin);
            PR_DEBUG("light_handle.fin -> r = %d   g = %d  b = %d", light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue);
            break;

        default:
            break;
        }
    }
    else
    {

        light_bright_cw_change(dp_data.bright, dp_data.col_temp, &light_handle.fin);
        PR_DEBUG("light_handle.fin -> cw = %d   ww = %d", light_handle.fin.white, light_handle.fin.warm);

        light_rgb_value_get(dp_data.color, &light_handle.fin);
        PR_DEBUG("light_handle.fin -> r = %d   g = %d  b = %d", light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue);
    }
}

/*	开始执行灯光功能	*/
STATIC VOID light_bright_start(VOID)
{
    light_bright_data_get();

    switch (dp_data.mode)
    {
    case WHITE_MODE:
    case COLOR_MODE:
        light_scene_stop();
        light_shade_start(normal_delay_time);
        break;

    case SCENE_MODE:
        light_scene_start();
        break;

    case MUSIC_MODE:
        light_scene_stop();
        light_shade_stop();
        break;

    default:
        break;
    }
}

/******************************************************************************
 * FunctionName : light_send_final_data_mode_get
 * Description  : 灯输出模式的获取,
                根据最终的输出,或者配网模式来判断输出的mode
                mode:
                    WHITE_DATA_SEND 
                    COLOR_DATA_SEND
                    MIX_DATA_SEND
                    NULL_DATA_SEND
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LIGHT_CURRENT_DATA_SEND_MODE_E light_send_final_data_mode_get(VOID)
{
    TUYA_GW_WIFI_NW_STAT_E wf_stat = tuya_light_get_wf_gw_status();

    if (TUYA_STAT_UNPROVISION == wf_stat || TUYA_STAT_AP_STA_UNCFG == wf_stat)
    {
        if (WHITE_NET_LIGHT == _def_cfg.net_light_type)
        {
            return WHITE_DATA_SEND;
        }
        else if (COLOR_NET_LIGHT == _def_cfg.net_light_type)
        {
            return COLOR_DATA_SEND;
        }
    }
    else
    {

        if (light_handle.fin.red != 0 || light_handle.fin.green != 0 || light_handle.fin.blue != 0 ||
            light_handle.fin.white != 0 || light_handle.fin.warm != 0)
        {
            if (light_handle.fin.white == 0 && light_handle.fin.warm == 0)
            {
                return COLOR_DATA_SEND;
            }
            else if (light_handle.fin.red == 0 && light_handle.fin.green == 0 && light_handle.fin.blue == 0)
            {
                return WHITE_DATA_SEND;
            }
            else
            {
                return MIX_DATA_SEND;
            }
        }
        else
        {
            return NULL_DATA_SEND;
        }
    }
}

/******************************************************************************
 * FunctionName : tuya_light_color_power_ctl
 * Description  : IIC驱动时，位选位的控制
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_light_color_power_ctl(COLOR_POWER_CTL_E ctl)
{
    HW_TABLE_S *hw_table = get_hw_table();

    if ((hw_table->lowpower_pin != NONE) && (hw_table->driver_mode == DRV_MODE_IIC_SM2135 || hw_table->driver_mode == DRV_MODE_IIC_SMx726))
    {
        tuya_light_gpio_init(hw_table->lowpower_pin, TUYA_GPIO_OUT);
        tuya_light_gpio_ctl(hw_table->lowpower_pin, ctl);
    }
}

/******************************************************************************
 * FunctionName : light_send_data
 * Description  : 灯驱动的输出
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID light_send_data(USHORT_T R_value, USHORT_T G_value, USHORT_T B_value, USHORT_T CW_value, USHORT_T WW_value)
{
    tuya_light_send_data(R_value, G_value, B_value, CW_value, WW_value); /* 发送pwm数据 */
}

/******************************************************************************
 * FunctionName : light_hw_timer_start
 * Description  : 设定硬件定时器的时间并开启
 * Parameters   : time
 * Returns      : none
*******************************************************************************/
STATIC VOID light_hw_timer_start(UINT_T time)
{
    tuya_light_hw_timer.total = time;
    tuya_light_hw_timer.hw_cnt = 0; /* 当前计数值 */
    tuya_light_hw_timer.enable = TRUE;
}

/******************************************************************************
 * FunctionName : light_hw_timer_restart
 * Description  : 重启动硬件定时器
 * Parameters   : time
 * Returns      : none
*******************************************************************************/
STATIC VOID light_hw_timer_restart(VOID)
{
    tuya_light_hw_timer.enable = TRUE;
}

/******************************************************************************
 * FunctionName : light_hw_timer_stop
 * Description  : 停止硬件定时器
 * Parameters   : time
 * Returns      : none
*******************************************************************************/
STATIC VOID light_hw_timer_stop(VOID)
{
    tuya_light_hw_timer.enable = FALSE;
    tuya_light_hw_timer.total = 0;
}

/******************************************************************************
 * FunctionName : light_shade_start
 * Description  : 灯渐变处理开始,设定延迟变化的时间
 * Parameters   : time 延迟变化的时间 单位:us
 * Returns      : none
*******************************************************************************/
STATIC VOID light_shade_start(UINT_T time)
{

    if (time < HW_TIMER_CYCLE) /* 如果设定的时间小于最小的渐变时间 */
    {
        time = HW_TIMER_CYCLE;
    }

    light_handle.shade_new = TRUE;
    PR_DEBUG("light_shade_start :%d", time);
    light_hw_timer_start(time);
}

/******************************************************************************
 * FunctionName : light_shade_stop
 * Description  : 灯渐变处理停止
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID light_shade_stop(VOID)
{
    light_hw_timer_stop();
    PR_DEBUG("light_shade_stop");
}

/******************************************************************************
 * FunctionName : light_thread_shade
 * Description  : 灯渐变处理的线程
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID light_thread_shade(PVOID_T pArg)
{

    INT_T delata_red = 0;
    INT_T delata_green = 0;
    INT_T delata_blue = 0;
    INT_T delata_white = 0;
    INT_T delata_warm = 0;
    USHORT_T max_value;
    STATIC FLOAT_T r_scale;
    STATIC FLOAT_T g_scale;
    STATIC FLOAT_T b_scale;
    STATIC FLOAT_T w_scale;
    STATIC FLOAT_T ww_scale;
    UINT_T RED_GRA_STEP = 1;
    UINT_T GREEN_GRA_STEP = 1;
    UINT_T BLUE_GRA_STEP = 1;
    UINT_T WHITE_GRA_STEP = 1;
    UINT_T WARM_GRA_STEP = 1;

    while (1)
    {
        WaitSemaphore(sys_handle.sem_shade);
        MutexLock(sys_handle.mutex);
        if (memcmp(&light_handle.fin, &light_handle.curr, SIZEOF(BRIGHT_DATA_S)) != 0)
        {
            delata_red = (light_handle.fin.red - light_handle.curr.red);
            delata_green = (light_handle.fin.green - light_handle.curr.green);
            delata_blue = (light_handle.fin.blue - light_handle.curr.blue);
            delata_white = (light_handle.fin.white - light_handle.curr.white);
            delata_warm = (light_handle.fin.warm - light_handle.curr.warm);

            //1、计算步进值（最大差值）
            max_value = light_shade_max_value_get(&light_handle.fin, &light_handle.curr);

            if (light_handle.shade_new == TRUE)
            {

                r_scale = __abs(delata_red) / 1.0 / max_value;
                g_scale = __abs(delata_green) / 1.0 / max_value;
                b_scale = __abs(delata_blue) / 1.0 / max_value;
                w_scale = __abs(delata_white) / 1.0 / max_value;
                ww_scale = __abs(delata_warm) / 1.0 / max_value;
                light_handle.shade_new = FALSE;

            } //按差值对应MAX计算比例

            //2、各路按比例得到实际差值

            if (max_value == __abs(delata_red))
            {
                RED_GRA_STEP = 1;
            }
            else
            {
                RED_GRA_STEP = __abs(delata_red) - max_value * r_scale;
            }

            if (max_value == __abs(delata_green))
            {
                GREEN_GRA_STEP = 1;
            }
            else
            {
                GREEN_GRA_STEP = __abs(delata_green) - max_value * g_scale;
            }

            if (max_value == __abs(delata_blue))
            {
                BLUE_GRA_STEP = 1;
            }
            else
            {
                BLUE_GRA_STEP = __abs(delata_blue) - max_value * b_scale;
            }

            if (max_value == __abs(delata_white))
            {
                WHITE_GRA_STEP = 1;
            }
            else
            {
                WHITE_GRA_STEP = __abs(delata_white) - max_value * w_scale;
            }

            if (max_value == __abs(delata_warm))
            {
                WARM_GRA_STEP = 1;
            }
            else
            {
                WARM_GRA_STEP = __abs(delata_warm) - max_value * ww_scale;
            }

            //3、按计算出的差值调整各路light_bright数值
            if (delata_red != 0)
            {
                if (__abs(delata_red) < RED_GRA_STEP)
                {
                    light_handle.curr.red += delata_red;
                }
                else
                {
                    if (delata_red < 0)
                        light_handle.curr.red -= RED_GRA_STEP;
                    else
                        light_handle.curr.red += RED_GRA_STEP;
                }
            }

            if (delata_green != 0)
            {
                if (__abs(delata_green) < GREEN_GRA_STEP)
                {
                    light_handle.curr.green += delata_green;
                }
                else
                {
                    if (delata_green < 0)
                        light_handle.curr.green -= GREEN_GRA_STEP;
                    else
                        light_handle.curr.green += GREEN_GRA_STEP;
                }
            }

            if (delata_blue != 0)
            {
                if (__abs(delata_blue) < BLUE_GRA_STEP)
                {
                    light_handle.curr.blue += delata_blue;
                }
                else
                {
                    if (delata_blue < 0)
                        light_handle.curr.blue -= BLUE_GRA_STEP;
                    else
                        light_handle.curr.blue += BLUE_GRA_STEP;
                }
            }

            if (delata_white != 0)
            {
                if (__abs(delata_white) < WHITE_GRA_STEP)
                {
                    light_handle.curr.white += delata_white;
                }
                else
                {
                    if (delata_white < 0)
                        light_handle.curr.white -= WHITE_GRA_STEP;
                    else
                        light_handle.curr.white += WHITE_GRA_STEP;
                }
            }

            if (delata_warm != 0)
            {
                if (__abs(delata_warm) < WARM_GRA_STEP)
                {
                    light_handle.curr.warm += delata_warm;
                }
                else
                {
                    if (delata_warm < 0)
                        light_handle.curr.warm -= WARM_GRA_STEP;
                    else
                        light_handle.curr.warm += WARM_GRA_STEP;
                }
            }

            //4、输出
            //PR_DEBUG("light send data %d %d %d %d %d", light_handle.curr.red, light_handle.curr.green, light_handle.curr.blue, light_handle.curr.white, light_handle.curr.warm);
            light_send_data(light_handle.curr.red, light_handle.curr.green, light_handle.curr.blue, light_handle.curr.white, light_handle.curr.warm);
        }
        else
        {
            //结束变化,显示一次目标值
            light_shade_stop();
            PR_DEBUG("light send data %d %d %d %d %d", light_handle.curr.red, light_handle.curr.green, light_handle.curr.blue, light_handle.curr.white, light_handle.curr.warm);

            if (dp_data.power == FALSE)
            { //CCT进入低功耗前需要将全部端口关闭
                light_send_data(0, 0, 0, 0, 0);
            }
            else
            {
                light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue, light_handle.fin.white, light_handle.fin.warm);
            }
        }
        MutexUnLock(sys_handle.mutex);
    }
}

/* 情景数据处理函数 */
STATIC VOID light_scene_data_get(UCHAR_T *buf, SCENE_HEAD_S *head)
{
    head->type = __str2byte(__asc2hex(buf[4]), __asc2hex(buf[5]));
    if (head->type == SCENE_TYPE_SHADOW)
    {
        head->times = 110 - __str2byte(__asc2hex(buf[0]), __asc2hex(buf[1]));
        head->speed = 105 - __str2byte(__asc2hex(buf[2]), __asc2hex(buf[3])); //新版本速度每个单元独立
    }
    else
    {
        head->times = 105 - __str2byte(__asc2hex(buf[0]), __asc2hex(buf[1]));
        head->speed = 105 - __str2byte(__asc2hex(buf[2]), __asc2hex(buf[3])); //新版本速度每个单元独立
    }
    
    light_cw_value_get(buf + 18, &light_handle.fin);
    light_rgb_value_get(buf + 6, &light_handle.fin);
}

/* 开启情景功能 */
VOID light_scene_start(VOID)
{
    SCENE_HEAD_S scene;

    MutexLock(sys_handle.mutex);

    light_handle.scene_cnt = 0;
    light_handle.scene_new = FALSE;

    memset(&scene, 0, SIZEOF(SCENE_HEAD_S));
    light_scene_data_get(&dp_data.scene[0 * SCENE_UNIT_LEN + 2], &scene);
    light_handle.scene_type = scene.type;

    switch (scene.type)
    {
    case SCENE_TYPE_SHADOW:
        //呼吸情景会直接进入第一个单元
        light_handle.scene_cnt++;

        memcpy(&light_handle.curr, &light_handle.fin, SIZEOF(BRIGHT_DATA_S));

        light_handle.scene_enable = TRUE;
        light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue,
                        light_handle.fin.white, light_handle.fin.warm);

        MutexUnLock(sys_handle.mutex);
        break;

    case SCENE_TYPE_JUMP:
        //直接显示
        light_shade_stop();

        light_handle.scene_enable = TRUE;

        MutexUnLock(sys_handle.mutex);
        break;

    case SCENE_TYPE_STATIC:
        MutexUnLock(sys_handle.mutex);
        //静态显示
        light_scene_stop();
        //开启呼吸
        light_shade_start(normal_delay_time);
        break;

    default:
        MutexUnLock(sys_handle.mutex);
        break;
    }
}

/******************************************************************************
 * FunctionName : light_scene_stop
 * Description  : 场景的停止
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID light_scene_stop(VOID)
{
    MutexLock(sys_handle.mutex);
    light_handle.scene_enable = FALSE;
    MutexUnLock(sys_handle.mutex);
}

/******************************************************************************
 * FunctionName : light_thread_scene
 * Description  : 情景线程只处理跳变和渐变模式
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID light_thread_scene(PVOID_T pArg)
{
    SCENE_HEAD_S scene;
    BYTE_T scene_total;
    STATIC UINT_T Last_tick;

    Last_tick = GetSystemTickCount() * GetTickRateMs();
    while (1)
    {
        if ((dp_data.power == FALSE) ||
            (light_handle.scene_enable == FALSE))
        {
            if (FALSE == lowpower_flag)
            {
                SystemSleep(SCENE_DELAY_TIME);
            }
            else
            {
                SystemSleep(LOWPOWER_MODE_DELAY_TIME);
            }
            continue;
        }

        if (light_handle.scene_new == FALSE)
        {

            MutexLock(sys_handle.mutex);

            light_handle.scene_new = TRUE;

            scene_total = strlen(dp_data.scene + 2) / SCENE_UNIT_LEN; //通过总数据长度计算情景个数
            //		PR_DEBUG("Num:%d", scene_total);

            scene_total = strlen(dp_data.scene + 2) / SCENE_UNIT_LEN;

            if (scene_total == 1)
            {
                if (light_handle.scene_cnt == scene_total)
                {
                    light_handle.scene_cnt = 0;

                    memset(&scene, 0, SIZEOF(SCENE_HEAD_S));
                    light_scene_data_get(&dp_data.scene[light_handle.scene_cnt * SCENE_UNIT_LEN + 2], &scene);

                    //目标值为全黑
                    memset(&light_handle.fin, 0, sizeof(BRIGHT_DATA_S));
                    if (_cw_type == LIGHT_CW_CCT)
                    {
                        light_handle.fin.warm = light_handle.curr.warm;
                    }
                }
                else
                {
                    memset(&scene, 0, SIZEOF(SCENE_HEAD_S));
                    light_scene_data_get(&dp_data.scene[light_handle.scene_cnt * SCENE_UNIT_LEN + 2], &scene);

                    light_handle.scene_cnt++;
                }
            }
            else
            {
                memset(&scene, 0, SIZEOF(SCENE_HEAD_S));
                light_scene_data_get(&dp_data.scene[light_handle.scene_cnt * SCENE_UNIT_LEN + 2], &scene);

                light_handle.scene_cnt++;
                if (light_handle.scene_cnt >= scene_total)
                    light_handle.scene_cnt = 0;
            }

            light_handle.scene_type = scene.type;

            if (scene.type == SCENE_TYPE_JUMP)
            {
                light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue, light_handle.fin.white, light_handle.fin.warm);
            }
            else if (scene.type == SCENE_TYPE_SHADOW)
            {
                light_shade_start(light_shade_speed_get(scene.speed * SCENE_DELAY_TIME));
            }

            MutexUnLock(sys_handle.mutex);
        }

        if ((GetSystemTickCount() * GetTickRateMs() - Last_tick) / 100 > scene.times)
        {
            light_handle.scene_new = FALSE;
            Last_tick = GetSystemTickCount() * GetTickRateMs();
        }

        SystemSleep(10);

        SystemSleep(SCENE_DELAY_TIME);
    }
}

/******************************************************************************
 * FunctionName : dev_power_ctl
 * Description  : 灯设备电源开关处理
 * Parameters   : ctrl --> 开关
 * Returns      : none
*******************************************************************************/
STATIC VOID dev_power_ctl(BOOL_T ctrl)
{
    MutexLock(sys_handle.mutex);

    if (TRUE == ctrl)
    {
        dp_data.power = TRUE;
    }
    else
    {
        dp_data.power = FALSE;
    }

    MutexUnLock(sys_handle.mutex);
}

/******************************************************************************
 * FunctionName : dev_lowpower_mode_ctrl
 * Description  : 灯设备低功耗控制，
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID dev_lowpower_mode_ctrl(BOOL_T stat)
{
    if (TRUE == stat)
    {
        if (TRUE == lowpower_flag)
        {
            light_lowpower_disable(); /* 退出低功耗模式 */
            lowpower_flag = FALSE;
        }
    }
    else
    { /* 关的时候，延时关等待灯渐变关闭 */
        sys_start_timer(sys_handle.lowpower, LIGHT_LOW_POWER_ENTER_IMEW, TIMER_ONCE);
    }
}

/******************************************************************************
 * FunctionName : light_switch_set
 * Description  : 灯开关处理
 * Parameters   : stat 
                    --> TRUE    开
                    --> FALSE   关
 * Returns      : none
*******************************************************************************/
STATIC VOID light_switch_set(BOOL_T stat)
{
    dev_lowpower_mode_ctrl(stat);

    if (stat == FALSE)
    {
        dev_power_ctl(FALSE);

        light_scene_stop();
        light_shade_stop();

        memset(&light_handle.fin, 0, sizeof(BRIGHT_DATA_S));

        if (LIGHT_CW_CCT == _cw_type)
        {

            light_handle.fin.warm = light_handle.curr.warm; /* CCT模式保持暖光脚不变 */
        }

        if (POWER_ONOFF_BY_SHADE == _def_cfg.power_onoff_type)
        {
            switch (dp_data.mode)
            {
            case WHITE_MODE:
            case COLOR_MODE:
                light_shade_start(normal_delay_time);
                break;

            case SCENE_MODE:
                if (SCENE_TYPE_JUMP == light_handle.scene_type)
                {
                    light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue, light_handle.fin.white, light_handle.fin.warm);
                }
                else
                {
                    light_shade_start(normal_delay_time);
                }
                break;

            case MUSIC_MODE:
                light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue, light_handle.fin.white, light_handle.fin.warm);
                break;

            default:
                break;
            }
        }
        else if (POWER_ONOFF_BY_DIRECT == _def_cfg.power_onoff_type)
        {
            if ((SCENE_MODE == dp_data.mode) && (SCENE_TYPE_SHADOW == light_handle.scene_type))
            {
                light_shade_start(normal_delay_time);
            }
            else
            {
                light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue, light_handle.fin.white, light_handle.fin.warm);
            }
        }
    }
    else
    {
        dev_power_ctl(TRUE);

        if (POWER_ONOFF_BY_SHADE == _def_cfg.power_onoff_type)
        {
            light_bright_start();
        }
        else if (POWER_ONOFF_BY_DIRECT == _def_cfg.power_onoff_type)
        {
            switch (dp_data.mode)
            {
            case WHITE_MODE:
            case COLOR_MODE:
                light_bright_data_get();
                light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue, light_handle.fin.white, light_handle.fin.warm);
                break;

            case SCENE_MODE:
                light_scene_start();
                break;

            default:
                break;
            }
        }
    }

    if (IsThisSysTimerRun(sys_handle.timer_countdown) == TRUE)
    { /* 如果倒计时在跑，操控开关时会关闭 */
        sys_stop_timer(sys_handle.timer_countdown);
        dp_data.countdown = 0;
    }
}

/******************************************************************************
 * FunctionName : light_dp_upload
 * Description  : dp数据上报同步的处理
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID light_dp_upload(VOID)
{
    OPERATE_RET op_ret = OPRT_OK;
    ty_cJSON *root = NULL;
    CHAR_T *out = NULL;
    DP_CNTL_S *dp_cntl = NULL;
    CHAR_T tmp[4] = {0};

    DEV_CNTL_N_S *dev_cntl = tuya_light_get_dev_cntl();
    if (dev_cntl == NULL)
    {
        return;
    }

    dp_cntl = &dev_cntl->dp[1];

    root = ty_cJSON_CreateObject();
    if (NULL == root)
    {
        PR_ERR("ty_cJSON_CreateObject error...");
        return;
    }

    /* 1-5路固定DP	 开关 + 场景 + 模式 */

    dpid2str(tmp, DPID_SWITCH);
    ty_cJSON_AddBoolToObject(root, tmp, dp_data.power);

    dpid2str(tmp, DPID_MODE);
    ty_cJSON_AddStringToObject(root, tmp, tuya_light_ty_get_enum_str(dp_cntl, (UCHAR)dp_data.mode));

    if (_def_cfg.color_type != LIGHT_COLOR_RGB)
    {
        dpid2str(tmp, DPID_BRIGHT);
        ty_cJSON_AddNumberToObject(root, tmp, dp_data.bright);
    }

    if (_def_cfg.color_type == LIGHT_COLOR_RGBCW || _def_cfg.color_type == LIGHT_COLOR_CW)
    {
        dpid2str(tmp, DPID_TEMPR);
        ty_cJSON_AddNumberToObject(root, tmp, dp_data.col_temp);
    }

    if (_def_cfg.color_type == LIGHT_COLOR_RGBCW ||
        _def_cfg.color_type == LIGHT_COLOR_RGBC ||
        _def_cfg.color_type == LIGHT_COLOR_RGB)
    {
        dpid2str(tmp, DPID_COLOR);
        ty_cJSON_AddStringToObject(root, tmp, dp_data.color);
    }

    dpid2str(tmp, DPID_SCENE);
    ty_cJSON_AddStringToObject(root, tmp, dp_data.scene);

    dpid2str(tmp, DPID_COUNTDOWN);
    ty_cJSON_AddNumberToObject(root, tmp, dp_data.countdown);


    out = ty_cJSON_PrintUnformatted(root);
    ty_cJSON_Delete(root);

    if (NULL == out)
    {
        PR_ERR("ty_cJSON_PrintUnformatted err...");
        return;
    }

    PR_DEBUG("upload: %s", out);
    op_ret = tuya_light_dp_report(out);
    Free(out);
    out = NULL;
    if (OPRT_OK != op_ret)
    {
        PR_ERR("sf_obj_dp_report err:%d", op_ret);
        return;
    }

    return;
}

/*	控制dp处理函数	*/
STATIC VOID light_ctrl_dp_proc(UCHAR *buf)
{
    CRTL_TYPE_E mode;

    mode = *buf - '0';
    //	PR_DEBUG("mode = %d",mode);

    if (_def_cfg.whihe_color_mutex == FALSE && (dp_data.mode == WHITE_MODE || dp_data.mode == COLOR_MODE))
    {
        if (dp_data.mode == WHITE_MODE)
        {
            light_cw_value_get(buf + 13, &light_handle.fin);
        }
        else if (dp_data.mode == COLOR_MODE)
        {
            light_rgb_value_get(buf + 1, &light_handle.fin);
        }
    }
    else
    {
        light_cw_value_get(buf + 13, &light_handle.fin);
        light_rgb_value_get(buf + 1, &light_handle.fin);
    }

    if (mode == CTRL_TYPE_JUMP)
    {
        memcpy(&light_handle.curr, &light_handle.fin, SIZEOF(BRIGHT_DATA_S));
        light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue, light_handle.fin.white, light_handle.fin.warm);
    }
    else if (mode == CTRL_TYPE_SHADE)
    {

        if (dp_data.mode == SCENE_MODE)
        {
            light_scene_stop();
        }

        light_shade_start(light_shade_speed_get(STANDARD_DELAY_TIMES));
    }
}

/*************************************************
            	flash存取处理函数
**************************************************/
/* 定时5秒自动保存 */
STATIC VOID data_autosave_timeout_cb(UINT timerID, PVOID pTimerArg)
{
    PR_DEBUG("%s", __FUNCTION__);

    light_data_write_flash();
}

/******************************************************************************
 * FunctionName : light_dp_data_autosave
 * Description  : 灯dp下发数据保存的回调调用处理
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID light_dp_data_autosave(VOID)
{

    sys_start_timer(sys_handle.timer_data_autosave, NORMAL_DP_SAVE_TIME, TIMER_ONCE);
}

/******************************************************************************
 * FunctionName : light_dp_data_fast_read
 * Description  : 灯dp数据fast的读取
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC OPERATE_RET light_dp_data_fast_read(CHAR_T *buf, DP_DATA_S *data)
{
    OPERATE_RET op_ret;
    ty_cJSON *root = NULL;
    ty_cJSON *child = NULL;
    ty_cJSON *json = NULL;

    PR_DEBUG("read flash:[%s]", buf);

    root = ty_cJSON_Parse(buf);
    if (NULL == root)
    {
        Free(buf);
        buf = NULL;
        goto ERR;
    }
    Free(buf);
    buf = NULL;

    child = ty_cJSON_GetObjectItem(root, "fast_dp_data");
    if (child == NULL)
    {
        PR_ERR("ERR!!! Can't find fast dp data ");
        goto ERR;
    }

    json = ty_cJSON_GetObjectItem(child, "rst");
    if (NULL == json)
    {
        rst_cnt = 0;
    }
    else
    {
        rst_cnt = json->valueint;
    }

    memset(data, 0, SIZEOF(DP_DATA_S));

    json = ty_cJSON_GetObjectItem(child, "switch");
    if (NULL == json)
    {
        data->power = TRUE;
    }
    else
    {
        data->power = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(child, "mode");
    if (NULL == json)
    {
        if (_def_cfg.color_type == LIGHT_COLOR_RGB)
        {
            data->mode = COLOR_MODE;
        }
        else
        {
            data->mode = WHITE_MODE;
        }
    }
    else
    {
        data->mode = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(child, "bright");
    if (NULL == json)
    {
        data->bright = light_default_bright_get();
    }
    else
    {
        data->bright = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(child, "temper");
    if (NULL == json)
    {
        data->col_temp = light_default_temp_get();
    }
    else
    {
        data->col_temp = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(child, "color");
    if (NULL == json)
    {
        if (_def_cfg.whihe_color_mutex == TRUE)
        {
            light_default_color_get(data->color);
        }
        else
        {
            memcpy(data->color, FC_COLOR_DATA_DEFAULT, COLOR_DATA_LEN);
        }
    }
    else
    {
        memcpy(data->color, json->valuestring, SIZEOF(data->color));
    }

    json = ty_cJSON_GetObjectItem(child, "scene");
    if (NULL == json)
    {
        switch (_def_cfg.color_type)
        {
        case LIGHT_COLOR_C:
            memcpy(data->scene, SCENE_DATA_DEFAULT_DIM, SCENE_DATA_LEN_MAX);
            break;

        case LIGHT_COLOR_CW:
            memcpy(data->scene, SCENE_DATA_DEFAULT_CW, SCENE_DATA_LEN_MAX);
            break;

        default:
            memcpy(data->scene, SCENE_DATA_DEFAULT_RGB, SCENE_DATA_LEN_MAX);
            break;
        }
    }
    else
    {
        memcpy(data->scene, json->valuestring, SIZEOF(data->scene));
        PR_DEBUG("dev_get scene data len:%d", strlen(json->valuestring));
    }

#ifdef KEY_CHANGE_SCENE_FUN
    memset(key_scene_data, 0, SIZEOF(char) * (SCENE_DATA_LEN_MAX + 1) * 8);

    json = ty_cJSON_GetObjectItem(child, "scene0");
    if (NULL == json)
    {
        memcpy(key_scene_data[0], KEY_SCENE0_DEF_DATA, strlen(KEY_SCENE0_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[0], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(child, "scene1");
    if (NULL == json)
    {
        memcpy(key_scene_data[1], KEY_SCENE1_DEF_DATA, strlen(KEY_SCENE1_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[1], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(child, "scene2");
    if (NULL == json)
    {
        memcpy(key_scene_data[2], KEY_SCENE2_DEF_DATA, strlen(KEY_SCENE2_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[2], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(child, "scene3");
    if (NULL == json)
    {
        memcpy(key_scene_data[3], KEY_SCENE3_DEF_DATA, strlen(KEY_SCENE3_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[3], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(child, "scene4");
    if (NULL == json)
    {
        memcpy(key_scene_data[4], KEY_SCENE4_DEF_DATA, strlen(KEY_SCENE4_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[4], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(child, "scene5");
    if (NULL == json)
    {
        memcpy(key_scene_data[5], KEY_SCENE5_DEF_DATA, strlen(KEY_SCENE5_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[5], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(child, "scene6");
    if (NULL == json)
    {
        memcpy(key_scene_data[6], KEY_SCENE6_DEF_DATA, strlen(KEY_SCENE6_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[6], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(child, "scene7");
    if (NULL == json)
    {
        memcpy(key_scene_data[7], KEY_SCENE7_DEF_DATA, strlen(KEY_SCENE7_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[7], json->valuestring, strlen(json->valuestring) + 1);
    }
#endif

    ty_cJSON_Delete(root);
    return OPRT_OK;

ERR:
    light_dp_data_fast_default_set();

    return OPRT_COM_ERROR;
}

/******************************************************************************
 * FunctionName : light_dp_data_read
 * Description  : 灯dp数据的读取
 * Parameters   : DP_DATA_S *data
 * Returns      : none
*******************************************************************************/
STATIC OPERATE_RET light_dp_data_read(DP_DATA_S *data)
{
    OPERATE_RET op_ret;
    UCHAR_T *buf = NULL;
    ty_cJSON *root = NULL;

    op_ret = tuya_light_common_flash_read(LIGHT_DATA_KEY, &buf); /* 从KVS中读取dp数据 */
    if (OPRT_OK != op_ret)
    {
        PR_ERR("read light dp data failed %d", op_ret);
        Free(buf);
        buf = NULL;
        goto ERR_EXT;
    }

    PR_NOTICE("read flash:[%s]", buf);

    root = ty_cJSON_Parse(buf);
    if (NULL == root)
    {
        Free(buf);
        buf = NULL;
        return OPRT_CJSON_PARSE_ERR;
    }
    Free(buf);
    buf = NULL;

    ty_cJSON *json = NULL;

    json = ty_cJSON_GetObjectItem(root, "rst");
    if (NULL == json)
    {
        rst_cnt = 0;
    }
    else
    {
        rst_cnt = json->valueint;
    }

    memset(data, 0, SIZEOF(DP_DATA_S));

    json = ty_cJSON_GetObjectItem(root, "switch");
    if (NULL == json)
    {
        data->power = TRUE;
    }
    else
    {
        data->power = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(root, "mode");
    if (NULL == json)
    {
        if (_def_cfg.color_type == LIGHT_COLOR_RGB)
            data->mode = COLOR_MODE;
        else
            data->mode = WHITE_MODE;
    }
    else
    {
        data->mode = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(root, "bright");
    if (NULL == json)
    {
        data->bright = light_default_bright_get();
    }
    else
    {
        data->bright = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(root, "temper");
    if (NULL == json)
    {
        data->col_temp = light_default_temp_get();
    }
    else
    {
        data->col_temp = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(root, "color");
    if (NULL == json)
    {
        if (_def_cfg.whihe_color_mutex == TRUE)
        {
            light_default_color_get(data->color);
        }
        else
        {
            memcpy(data->color, FC_COLOR_DATA_DEFAULT, COLOR_DATA_LEN);
        }
    }
    else
    {
        memcpy(data->color, json->valuestring, SIZEOF(data->color));
    }

    json = ty_cJSON_GetObjectItem(root, "scene");
    if (NULL == json)
    {
        switch (_def_cfg.color_type)
        {
        case LIGHT_COLOR_C:
            memcpy(data->scene, SCENE_DATA_DEFAULT_DIM, SCENE_DATA_LEN_MAX);
            break;

        case LIGHT_COLOR_CW:
            memcpy(data->scene, SCENE_DATA_DEFAULT_CW, SCENE_DATA_LEN_MAX);
            break;

        default:
            memcpy(data->scene, SCENE_DATA_DEFAULT_RGB, SCENE_DATA_LEN_MAX);
            break;
        }
    }
    else
    {
        memcpy(data->scene, json->valuestring, SIZEOF(data->scene));
        PR_DEBUG("dev_get scene data len:%d", strlen(json->valuestring));
    }

#ifdef KEY_CHANGE_SCENE_FUN
    memset(key_scene_data, 0, SIZEOF(char) * (SCENE_DATA_LEN_MAX + 1) * 8);

    json = ty_cJSON_GetObjectItem(root, "scene0");
    if (NULL == json)
    {
        memcpy(key_scene_data[0], KEY_SCENE0_DEF_DATA, strlen(KEY_SCENE0_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[0], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(root, "scene1");
    if (NULL == json)
    {
        memcpy(key_scene_data[1], KEY_SCENE1_DEF_DATA, strlen(KEY_SCENE1_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[1], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(root, "scene2");
    if (NULL == json)
    {
        memcpy(key_scene_data[2], KEY_SCENE2_DEF_DATA, strlen(KEY_SCENE2_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[2], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(root, "scene3");
    if (NULL == json)
    {
        memcpy(key_scene_data[3], KEY_SCENE3_DEF_DATA, strlen(KEY_SCENE3_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[3], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(root, "scene4");
    if (NULL == json)
    {
        memcpy(key_scene_data[4], KEY_SCENE4_DEF_DATA, strlen(KEY_SCENE4_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[4], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(root, "scene5");
    if (NULL == json)
    {
        memcpy(key_scene_data[5], KEY_SCENE5_DEF_DATA, strlen(KEY_SCENE5_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[5], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(root, "scene6");
    if (NULL == json)
    {
        memcpy(key_scene_data[6], KEY_SCENE6_DEF_DATA, strlen(KEY_SCENE6_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[6], json->valuestring, strlen(json->valuestring) + 1);
    }

    json = ty_cJSON_GetObjectItem(root, "scene7");
    if (NULL == json)
    {
        memcpy(key_scene_data[7], KEY_SCENE7_DEF_DATA, strlen(KEY_SCENE7_DEF_DATA) + 1);
    }
    else
    {
        memcpy(key_scene_data[7], json->valuestring, strlen(json->valuestring) + 1);
    }
#endif

    ty_cJSON_Delete(root);
    return OPRT_OK;

ERR_EXT:
    PR_NOTICE("set default data!");
    light_dp_data_default_set();

    return OPRT_COM_ERROR;
}

/******************************************************************************
 * FunctionName : light_data_write_flash
 * Description  : DP点写入到flash中
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC OPERATE_RET light_data_write_flash(VOID)
{
    OPERATE_RET op_ret;
    INT_T i = 0;
    CHAR_T *out = NULL;
    ty_cJSON *root = NULL;

    root = ty_cJSON_CreateObject();
    if (NULL == root)
    {
        PR_ERR("ty_cJSON_CreateObject error...");
        return OPRT_CR_CJSON_ERR;
    }

    /* dp数据 */
    if (_light_memory == TRUE)
    { /* 开启记忆 */
        ty_cJSON_AddBoolToObject(root, "switch", dp_data.power);
        ty_cJSON_AddNumberToObject(root, "mode", dp_data.mode);
        ty_cJSON_AddNumberToObject(root, "bright", dp_data.bright);
        ty_cJSON_AddNumberToObject(root, "temper", dp_data.col_temp);
        ty_cJSON_AddStringToObject(root, "color", dp_data.color);
    }
    else
    { /* 无记忆下默认写入默认值 */
        CHAR color_data[COLOR_DATA_LEN + 1];

        ty_cJSON_AddBoolToObject(root, "switch", TRUE);

        if (_def_cfg.color_type == LIGHT_COLOR_RGB || (_def_cfg.def_light_color < INIT_COLOR_C))
        {
            ty_cJSON_AddNumberToObject(root, "mode", COLOR_MODE);
        }
        else
        {
            ty_cJSON_AddNumberToObject(root, "mode", WHITE_MODE);
        }

        ty_cJSON_AddNumberToObject(root, "bright", light_default_bright_get());
        ty_cJSON_AddNumberToObject(root, "temper", light_default_temp_get());

        if (dp_data.mode == COLOR_MODE)
        {
            light_default_color_get(color_data);
            ty_cJSON_AddStringToObject(root, "color", color_data);
        }
        else
        {
            ty_cJSON_AddStringToObject(root, "color", COLOR_DATA_DEFAULT);
        }
    }
    //情景设定任何模式下保持记忆
    ty_cJSON_AddStringToObject(root, "scene", dp_data.scene);

#ifdef KEY_CHANGE_SCENE_FUN
    ty_cJSON_AddStringToObject(root, "scene0", key_scene_data[0]);
    ty_cJSON_AddStringToObject(root, "scene1", key_scene_data[1]);
    ty_cJSON_AddStringToObject(root, "scene2", key_scene_data[2]);
    ty_cJSON_AddStringToObject(root, "scene3", key_scene_data[3]);
    ty_cJSON_AddStringToObject(root, "scene4", key_scene_data[4]);
    ty_cJSON_AddStringToObject(root, "scene5", key_scene_data[5]);
    ty_cJSON_AddStringToObject(root, "scene6", key_scene_data[6]);
    ty_cJSON_AddStringToObject(root, "scene7", key_scene_data[7]);
#endif

    //	ty_cJSON_AddBoolToObject(root, "memory", _light_memory);//dp控制需要保存，json配置每次上电会更新

    out = ty_cJSON_PrintUnformatted(root);
    ty_cJSON_Delete(root);
    if (NULL == out)
    {
        PR_ERR("ty_cJSON_PrintUnformatted error...");
        return OPRT_MALLOC_FAILED;
    }

    PR_DEBUG("light_dp_data:%s", out);

    op_ret = tuya_light_common_flash_write(LIGHT_DATA_KEY, out, strlen(out));
    Free(out);
    out = NULL;

    if (OPRT_OK != op_ret)
    {
        PR_ERR("tuya_light_flash_write err:%d", op_ret);
        return op_ret;
    }

    op_ret = tuya_fast_data_set(); /* DP数据&cnt保存到uf 文件 */
    if (op_ret != OPRT_OK)
    {
        return op_ret;
    }

    return OPRT_OK;
}

/*****************************************
            dp处理函数
*****************************************/

STATIC USHORT_T light_default_bright_get(VOID)
{
    USHORT_T bright;

    bright = (BRIGHT_VAL_MAX - BRIGHT_VAL_MIN) * _def_cfg.def_bright_precent / 100 + BRIGHT_VAL_MIN;
    bright = bright / 10 * 10; //Rounding down

    return bright;
}

STATIC USHORT_T light_default_temp_get(VOID)
{
    USHORT_T temp;

    temp = BRIGHT_VAL_MAX * _def_cfg.def_temp_precent / 100;
    temp = temp / 10 * 10; //Rounding down

    return temp;
}

STATIC VOID light_default_color_get(UCHAR *data)
{
    USHORT_T h, s, v;

    switch (_def_cfg.def_light_color)
    {
    case INIT_COLOR_R:
        h = 0;
        break;

    case INIT_COLOR_G:
        h = 120;
        break;

    case INIT_COLOR_B:
        h = 240;
        break;

    default:
        h = 0; //默认红色
        break;
    }
    s = 1000;
    v = 1000 / 100 * _def_cfg.def_bright_precent;

    light_color_data_hex2asc(&data[0], h, 4);
    light_color_data_hex2asc(&data[4], s, 4);
    light_color_data_hex2asc(&data[8], v, 4);

    PR_DEBUG("set the def color data is [%s]", dp_data.color);
    return;
}

/******************************************************************************
 * FunctionName : light_dp_data_fast_default_set
 * Description  : 新DP，设定默认的DP设定！！！！ 不保存数据
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID light_dp_data_fast_default_set(VOID)
{

    memset(&dp_data, 0, SIZEOF(DP_DATA_S));

    dp_data.power = TRUE;

    if (LIGHT_COLOR_RGB == _def_cfg.color_type || (_def_cfg.def_light_color < INIT_COLOR_C))
    {
        dp_data.mode = COLOR_MODE;
    }
    else
    {
        dp_data.mode = WHITE_MODE;
    }

    dp_data.bright = light_default_bright_get();
    dp_data.col_temp = light_default_temp_get();

    if (dp_data.mode == COLOR_MODE)
    {
        light_default_color_get(dp_data.color);
    }
    else
    {
        if (_def_cfg.whihe_color_mutex == FALSE)
        {
            memcpy(dp_data.color, FC_COLOR_DATA_DEFAULT, COLOR_DATA_LEN);
        }
        else
        {
            memcpy(dp_data.color, COLOR_DATA_DEFAULT, COLOR_DATA_LEN);
        }
    }

    if (_def_cfg.color_type == LIGHT_COLOR_C)
    {
        memcpy(dp_data.scene, SCENE_DATA_DEFAULT_DIM, SCENE_DATA_LEN_MAX);
    }
    else if (_def_cfg.color_type == LIGHT_COLOR_CW)
    {
        memcpy(dp_data.scene, SCENE_DATA_DEFAULT_CW, SCENE_DATA_LEN_MAX);
    }
    else
    {
        memcpy(dp_data.scene, SCENE_DATA_DEFAULT_RGB, SCENE_DATA_LEN_MAX);
    }

    light_key_scene_data_default_set();
}

/******************************************************************************
 * FunctionName : light_dp_data_default_set
 * Description  : 新DP，设定默认的DP设定！！！！保存到flash
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID light_dp_data_default_set(VOID)
{

    memset(&dp_data, 0, SIZEOF(DP_DATA_S));

    dp_data.power = TRUE;

    if (LIGHT_COLOR_RGB == _def_cfg.color_type || (_def_cfg.def_light_color < INIT_COLOR_C))
    {
        dp_data.mode = COLOR_MODE;
    }
    else
    {
        dp_data.mode = WHITE_MODE;
    }

    dp_data.bright = light_default_bright_get();
    dp_data.col_temp = light_default_temp_get();

    if (dp_data.mode == COLOR_MODE)
    {
        light_default_color_get(dp_data.color);
    }
    else
    {
        if (_def_cfg.whihe_color_mutex == FALSE)
        {
            memcpy(dp_data.color, FC_COLOR_DATA_DEFAULT, COLOR_DATA_LEN);
        }
        else
        {
            memcpy(dp_data.color, COLOR_DATA_DEFAULT, COLOR_DATA_LEN);
        }
    }

    if (_def_cfg.color_type == LIGHT_COLOR_C)
    {
        memcpy(dp_data.scene, SCENE_DATA_DEFAULT_DIM, SCENE_DATA_LEN_MAX);
    }
    else if (_def_cfg.color_type == LIGHT_COLOR_CW)
    {
        memcpy(dp_data.scene, SCENE_DATA_DEFAULT_CW, SCENE_DATA_LEN_MAX);
    }
    else
    {
        memcpy(dp_data.scene, SCENE_DATA_DEFAULT_RGB, SCENE_DATA_LEN_MAX);
    }

    light_key_scene_data_default_set();

    light_data_write_flash();
}

/*	cct模式色温脚保持	*/
STATIC VOID light_cct_col_temper_keep(UCHAR data)
{
    STATIC BOOL key = FALSE; //默认开锁，不会造成影响
    STATIC USHORT_T temp_value;
    STATIC UCHAR last_data = 0;

    if (data == DPID_CONTROL)
    {
        if (dp_data.mode == SCENE_MODE)
        { //在情景模式中，所有灯光的预览效果编辑都会通过调控DP点下发
            if (key == FALSE)
            {
                temp_value = light_handle.curr.warm; //在进入调节之前，记录下色温数值
                key = TRUE;                          //第一次记录完毕之后自锁，
            }
        }
    }
    else
    {
        if (key == TRUE)
        {
            key = FALSE;

            if (last_data == DPID_CONTROL && data == DPID_SCENE)
            { //dpid：28->25，为情景提交
                ;
            }
            else
            {
                light_handle.fin.warm = light_handle.curr.warm = temp_value; //如果不是控制DP，判断上锁情况，如果上锁说明有色温保存，释放色温
            }
        }
    }

    last_data = data;
}

/******************************************************************************
 * FunctionName : light_light_dp_proc
 * Description  : 灯DP点的处理
 * Parameters   : *root下发的json数据 ( 新DP点 )
 * Returns      : flag 
                    TRUE 已经处理
*******************************************************************************/
BOOL_T light_light_dp_proc(TY_OBJ_DP_S *root)
{
    UCHAR_T dpid, type;
    WORD_T len, rawlen;
    UCHAR_T mode;
    BOOL_T flag = FALSE;

    dpid = root->dpid;
    PR_DEBUG("light_light_dp_proc dpid=%d", dpid);
    if (_cw_type == LIGHT_CW_CCT)
    {
        light_cct_col_temper_keep(dpid);
    }

    switch (dpid)
    {
    case DPID_SWITCH:
        if (dp_data.power != root->value.dp_bool)
        {
            flag = TRUE;

            light_switch_set(root->value.dp_bool);
        }
        break;

    case DPID_MODE:
        if (root->type != PROP_ENUM)
        {
            break;
        }
        mode = root->value.dp_enum;
        if (dp_data.mode != mode || mode == SCENE_MODE)
        {
            flag = TRUE;
            dp_data.mode = mode;

            if (dp_data.power == TRUE)
            {
                light_bright_start();
            }
        }
        break;

    case DPID_BRIGHT: //亮度
        if (root->type != PROP_VALUE)
            break;

        if (dpid == DPID_BRIGHT)
        {
            if (root->value.dp_value < BRIGHT_VAL_MIN || root->value.dp_value > BRIGHT_VAL_MAX)
            {
                PR_ERR("the data length is wrong: %d", root->value.dp_value);
                break;
            }
        }

        if (dp_data.bright != root->value.dp_value)
        {
            flag = TRUE;
            dp_data.bright = root->value.dp_value;
            dp_data.mode = WHITE_MODE;

            if (dp_data.power == TRUE)
            {
                light_bright_data_get();
                light_shade_start(normal_delay_time);
            }
        }
        break;

    case DPID_TEMPR: //色温
        if (root->type != PROP_VALUE)
            break;

        if (dpid == DPID_TEMPR)
        {
            if (root->value.dp_value < 0 || root->value.dp_value > BRIGHT_VAL_MAX)
            {
                PR_ERR("the data length is wrong: %d", root->value.dp_value);
                break;
            }
        }

        if (dp_data.col_temp != root->value.dp_value)
        {
            flag = TRUE;
            dp_data.col_temp = root->value.dp_value;
            dp_data.mode = WHITE_MODE;
            if (dp_data.power == TRUE)
            {
                light_bright_data_get();
                light_shade_start(normal_delay_time);
            }
        }
        break;

    case DPID_COLOR: //彩色
        if (root->type != PROP_STR)
            break;

        len = strlen(root->value.dp_str);
        if (len != COLOR_DATA_LEN)
        {
            PR_ERR("the data length is wrong: %d", len);
            break;
        }

        if (strcmp(dp_data.color, root->value.dp_str) != 0)
        {
            flag = TRUE;
            memset(dp_data.color, 0, SIZEOF(dp_data.color));
            memcpy(dp_data.color, root->value.dp_str, len);

            if (dp_data.power == TRUE)
            {
                light_bright_data_get();
                light_shade_start(normal_delay_time);
            }
        }
        break;

    case DPID_SCENE: //新情景
        if (root->type != PROP_STR)
        {
            break;
        }

        BOOL scene_f = FALSE;
        len = strlen(root->value.dp_str);

        if (len < SCENE_DATA_LEN_MIN || len > SCENE_DATA_LEN_MAX)
        {
            PR_ERR("the data length is wrong: %d", len);
        }
        else
        {
            scene_f = TRUE;
        }

        if (scene_f == TRUE)
        {
            flag = TRUE;
            memset(dp_data.scene, 0, SIZEOF(dp_data.scene));
            memcpy(dp_data.scene, root->value.dp_str, len);

            light_key_scene_data_save(dp_data.scene, len);

            if (dp_data.power == TRUE)
            {
                light_scene_start();
            }
        }
        break;

    case DPID_COUNTDOWN:
        if (root->type != PROP_VALUE)
            break;

        if (root->value.dp_value < 0 || root->value.dp_value > 86400)
        {
            PR_ERR("the data size is wrong: %d", root->value.dp_value);
            break;
        }

        if (root->value.dp_value == 0)
        { /* 取消倒计时 */
            if (IsThisSysTimerRun(sys_handle.timer_countdown))
            {
                sys_stop_timer(sys_handle.timer_countdown);
                dp_data.countdown = 0;
                flag = TRUE; /* 重新上报 */
            }
        }

        if (dp_data.countdown != root->value.dp_value)
        {
            flag = TRUE;
            dp_data.countdown = root->value.dp_value;
            sys_start_timer(sys_handle.timer_countdown, 60000, TIMER_CYCLE);
        }
        break;

    case DPID_MUSIC:   //音乐
    case DPID_CONTROL: //实时控制数据
        //实时下发，不保存&&不回复消息
        if (dp_data.power == FALSE)
        {
            break;
        }

        if (root->type != PROP_STR)
            break;

        if (dp_data.power == TRUE)
        {
            light_shade_stop();
            light_ctrl_dp_proc(root->value.dp_str);
        }
        break;

    default:
        break;
    }
    PR_DEBUG("light_light_dp_proc flag=%d", flag);
    return flag;
}

/**************************************************
				按键功能函数
***************************************************/
/*	开启剩余内存监控	*/
VOID light_key_fun_free_heap_check_start(VOID)
{
    sys_start_timer(sys_handle.timer_ram_checker, 300, TIMER_CYCLE);
}

/*	关闭剩余内存监控	*/
VOID light_key_fun_free_heap_check_stop(VOID)
{
    sys_stop_timer(sys_handle.timer_ram_checker);
}

/*	按键重置配网函数	*/
VOID light_key_fun_wifi_reset(VOID)
{
    if (_reset_dev_mode == LIGHT_RESET_BY_ANYWAY || _reset_dev_mode == LIGHT_RESET_BY_LONG_KEY_ONLY)
    {
        light_dp_data_default_set();
        tuya_light_dev_reset();
    }
}

/*	按键开关功能	*/
VOID light_key_fun_power_onoff_ctrl(VOID)
{
    if (TRUE == lowpower_flag)
    {
        lowpower_flag = FALSE;
        light_lowpower_disable();
    }

    if (dp_data.power == FALSE)
    {
        light_switch_set(TRUE);
    }
    else
    {
        light_switch_set(FALSE);
    }

    light_dp_upload();
}

/*	按键七彩切换功能	*/
VOID light_key_fun_7hue_cyclic(VOID) //七彩变化功能
{
    STATIC USHORT_T hue = 0;

    light_color_data_hex2asc(&dp_data.color[0], hue, 4);

    if (hue < 60)
    {
        hue += 30;
    }
    else if (hue < 300)
    {
        hue += 60;
    }
    else
    {
        hue = 0;
    }

    if (dp_data.mode != COLOR_MODE)
    {
        dp_data.mode = COLOR_MODE;
    }

    light_bright_start();
}

/* 按键切换情景功能 */
VOID light_key_fun_scene_change(VOID)
{
#ifdef KEY_CHANGE_SCENE_FUN

    if (dp_data.mode != SCENE_MODE)
    {
        dp_data.mode = SCENE_MODE;
        key_trig_num = 0;
    }

    if (key_trig_num < 8)
    {
        dp_data.power = TRUE;

        memset(dp_data.scene, 0, SIZEOF(dp_data.scene));
        memcpy(dp_data.scene, key_scene_data[key_trig_num], strlen(key_scene_data[key_trig_num]) + 1);

        light_scene_start();

        light_dp_upload();
        light_dp_data_autosave();

        key_trig_num++;
    }
    else
    {
        light_key_fun_power_onoff_ctrl();
        key_trig_num = 0;
    }

#else
    PR_ERR("Not define the scene change of key's function!");
    return;

#endif
}

/* 按键记忆情景功能函数 */
STATIC VOID light_key_scene_data_save(char *str, UCHAR_T len)
{
#ifdef KEY_CHANGE_SCENE_FUN

    UCHAR_T num;

    num = __str2byte(__asc2hex(str[0]), __asc2hex(str[1]));

    memset(key_scene_data[num], 0, strlen(key_scene_data[num]) + 1);
    memcpy(key_scene_data[num], str, len);

    key_trig_num++;

#else
    return;

#endif
}

STATIC VOID light_key_scene_data_default_set(VOID)
{
#ifdef KEY_CHANGE_SCENE_FUN

    memset(&key_scene_data, 0, SIZEOF(DP_DATA_S));
    memcpy(key_scene_data[0], KEY_SCENE0_DEF_DATA, strlen(KEY_SCENE0_DEF_DATA) + 1);
    memcpy(key_scene_data[1], KEY_SCENE1_DEF_DATA, strlen(KEY_SCENE1_DEF_DATA) + 1);
    memcpy(key_scene_data[2], KEY_SCENE2_DEF_DATA, strlen(KEY_SCENE2_DEF_DATA) + 1);
    memcpy(key_scene_data[3], KEY_SCENE3_DEF_DATA, strlen(KEY_SCENE3_DEF_DATA) + 1);
    memcpy(key_scene_data[4], KEY_SCENE4_DEF_DATA, strlen(KEY_SCENE4_DEF_DATA) + 1);
    memcpy(key_scene_data[5], KEY_SCENE5_DEF_DATA, strlen(KEY_SCENE5_DEF_DATA) + 1);
    memcpy(key_scene_data[6], KEY_SCENE6_DEF_DATA, strlen(KEY_SCENE6_DEF_DATA) + 1);
    memcpy(key_scene_data[7], KEY_SCENE7_DEF_DATA, strlen(KEY_SCENE7_DEF_DATA) + 1);

#else
    return;

#endif
}

/*******************************************************
					配置灯光参数
********************************************************/
/* 配置灯光控制精度 */
VOID tuya_light_ctrl_precision_set(USHORT_T val)
{
    if (val < LIGHT_PRECISION_MIN)
    {
        val = LIGHT_PRECISION_MIN;
    }
    else if (val > LIGHT_PRECISION_MAX)
    {
        val = LIGHT_PRECISION_MAX;
    }

    light_precison = val;
    normal_delay_time = LIGHT_PRECISION_BASE_TIME / light_precison;
}

USHORT_T tuya_light_ctrl_precision_get(VOID)
{
    return light_precison;
}

/**************************************************
				初始化函数
***************************************************/
/* 灯光相关线程初始化 */
STATIC OPERATE_RET light_thread_init(VOID)
{
    OPERATE_RET op_ret;
    //
    TUYA_THREAD thread_scene;
    TUYA_THREAD thread_shade;

    op_ret = CreateMutexAndInit(&sys_handle.mutex);
    if (op_ret != OPRT_OK)
    {
        return op_ret;
    }

    op_ret = CreateMutexAndInit(&sys_handle.light_send_mutex);
    if (op_ret != OPRT_OK)
    {
        return op_ret;
    }

    op_ret = CreateAndInitSemaphore(&sys_handle.sem_shade, 0, 1);
    if (OPRT_OK != op_ret)
    {
        return;
    }

    SystemSleep(10);

    op_ret = tuya_light_CreateAndStart(&thread_shade, light_thread_shade, NULL, 1024, TRD_PRIO_2, "gra_task");
    if (op_ret != OPRT_OK)
    {
        return op_ret;
    }

    SystemSleep(10);
    op_ret = tuya_light_CreateAndStart(&thread_scene, light_thread_scene, NULL, 1024, 6, "flash_scene_task");
    if (op_ret != OPRT_OK)
    {
        return op_ret;
    }

    op_ret = sys_add_timer(lowpower_timer_cb, NULL, &sys_handle.lowpower);
    if (OPRT_OK != op_ret)
    {
        return op_ret;
    }

    return OPRT_OK;
}

STATIC OPERATE_RET light_sys_timer_init(VOID)
{
    OPERATE_RET op_ret;

    /* MQTT连接后同步 */
    op_ret = sys_add_timer(idu_timer_cb, NULL, &sys_handle.timer_init_dpdata);
    if (OPRT_OK != op_ret)
    {
        PR_ERR("idu_timer_cb add error:%d", op_ret);
        return op_ret;
    }
    else
    {
        sys_start_timer(sys_handle.timer_init_dpdata, 500, TIMER_CYCLE);
    }

    /* 数据保存 */
    op_ret = sys_add_timer(data_autosave_timeout_cb, NULL, &sys_handle.timer_data_autosave);
    if (OPRT_OK != op_ret)
    {
        PR_ERR("data_autosave_timeout_cb add error:%d", op_ret);
        return op_ret;
    }

    //倒计时
    op_ret = sys_add_timer(countdown_timeout_cb, NULL, &sys_handle.timer_countdown);
    if (OPRT_OK != op_ret)
    {
        return op_ret;
    }

    return OPRT_OK;
}

OPERATE_RET tuya_light_start(VOID)
{
    PR_NOTICE("==== %s ====", __FUNCTION__);

    OPERATE_RET op_ret;

    op_ret = light_sys_timer_init();
    if (op_ret != OPRT_OK)
    {
        PR_ERR("light_sys_timer_init err : %d", op_ret);
        return op_ret;
    }

    return OPRT_OK;
}

/******************************************************************************
 * FunctionName : tuya_light_get_wifi_cfg
 * Description  : wifi模式的设定
                根据oem 配置来设定wifi的联网模式,联网的模式有
                TUYA_GWCM_OLD  		--> WCM_OLD(0)          --> do not have low power mode
                TUYA_GWCM_LOW_POWER --> WCM_LOW_POWER(1)  	--> with low power mode
                TUYA_GWCM_SPCL_MODE --> WCM_SPCL_MODE(2) 	--> special with low power mode
                TUYA_GWCM_OLD_PROD  --> WCM_OLD_CPT(3)    	--> old mode scan

                oem可配置的只有:
                                1. TUYA_GWCM_SPCL_MODE;
                                2. TUYA_GWCM_OLD;
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
TUYA_WF_CFG_MTHD_SEL tuya_light_get_wifi_cfg(VOID)
{
    TUYA_WF_CFG_MTHD_SEL mthd = TUYA_GWCM_SPCL_MODE;
    /* 默认为special模式, 重置了当未重新联网就会重连之前的配置 */

    PR_NOTICE("_def_cfg.wf_cfg %d!", _def_cfg.wf_cfg);
    if (_def_cfg.wf_cfg > WIFI_CFG_OLD_CPT)
    {
        return WIFI_CFG_SPCL_MODE;
    }

    if (WIFI_CFG_OLD_CPT == _def_cfg.wf_cfg)
    {
        PR_NOTICE("old prod mode choose!!!!");
        mthd = TUYA_GWCM_OLD_PROD;
    }
    else if (WIFI_CFG_SPCL_MODE == _def_cfg.wf_cfg)
    {
        PR_NOTICE("specl mode choose!!!!");
        mthd = TUYA_GWCM_SPCL_MODE;
    }
    return mthd;
}

/* 上电初始状态设置 */
VOID light_init_stat_set(VOID)
{
    light_bright_data_get();

    if (_cw_type == LIGHT_CW_CCT)
    {
        if (dp_data.mode != WHITE_MODE)
        {

            light_bright_cw_change(dp_data.bright, dp_data.col_temp, &light_handle.fin);
            light_handle.fin.white = 0;
        }
    }

    PR_DEBUG("power = %d  mode = %d", dp_data.power, dp_data.mode);

    if (dp_data.power == TRUE)
    {

        //直接点亮
        light_handle.curr.red = light_handle.fin.red;
        light_handle.curr.green = light_handle.fin.green;
        light_handle.curr.blue = light_handle.fin.blue;
        light_handle.curr.white = light_handle.fin.white;
        light_handle.curr.warm = light_handle.fin.warm;

        switch (dp_data.mode)
        {
        case WHITE_MODE:
        case COLOR_MODE:
            PR_DEBUG("mode:%d", dp_data.mode);

            light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue, light_handle.fin.white, light_handle.fin.warm);
            PR_NOTICE("start to send pwm");
            break;

        case SCENE_MODE:
            //开启情景模式变化
            light_scene_start();
            break;

        case MUSIC_MODE:
            //默认值
            memset(&light_handle.fin, 0, SIZEOF(light_handle.fin));
            light_rgb_value_get(MUSIC_INIT_VAL, &light_handle.fin);
            light_send_data(light_handle.fin.red, light_handle.fin.green, light_handle.fin.blue, light_handle.fin.white, light_handle.fin.warm);
            break;

        default:
            break;
        }
    }
}

LIGHT_CW_TYPE_E tuya_light_cw_type_get(VOID)
{
    PR_DEBUG("tuya_light_cw_type_get %d", _cw_type);
    return _cw_type;
}

OPERATE_RET tuya_light_cw_type_set(LIGHT_CW_TYPE_E type)
{
    if (type > LIGHT_CW_CCT || type < LIGHT_CW_PWM)
    {
        PR_ERR("tuya_light_cw_type_set err:%d", type);
        return OPRT_INVALID_PARM;
    }

    _cw_type = type;
    return OPRT_OK;
}

OPERATE_RET tuya_light_reset_mode_set(LIGHT_RESET_NETWORK_STA_E mode)
{
    if (mode > LIGHT_RESET_BY_ANYWAY || mode < LIGHT_RESET_BY_POWER_ON_ONLY)
    {
        PR_ERR("tuya_light_reset_mode_set err:%d", mode);
        return OPRT_INVALID_PARM;
    }

    _reset_dev_mode = mode;
    return OPRT_OK;
}

VOID tuya_light_memory_flag_set(BOOL_T status)
{
    _light_memory = status;
}

OPERATE_RET tuya_light_cw_max_power_set(UCHAR_T max_power)
{
    if (max_power > 200 || max_power < 100)
    {
        PR_ERR("tuya_light_cw_type_set err:%d", max_power);
        return OPRT_INVALID_PARM;
    }

    _white_light_max_power = max_power;
    return OPRT_OK;
}

/* 获取灯光配置参数 */
LIGHT_DEFAULT_CONFIG_S *light_default_cfg_get(VOID)
{
    return &_def_cfg;
}

/******************************************************************************
 * FunctionName : light_cfg_param_set
 * Description  : 灯的配置项读取
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC OPERATE_RET light_cfg_param_set(VOID)
{
    tuya_light_config_param_set(&_def_cfg);

    if (_def_cfg.wf_cfg > WIFI_CFG_OLD_CPT)
    {
        PR_ERR("config param wf_cfg err: %d", _def_cfg.wf_cfg);
        _def_cfg.wf_cfg = WIFI_CFG_SPCL_MODE;
    }

    if (_def_cfg.wf_rst_cnt < 3)
    {
        PR_ERR("config param wf_rst_cnt err: %d", _def_cfg.wf_rst_cnt);
        _def_cfg.wf_rst_cnt = 3;
    }

    if ((_def_cfg.color_type < LIGHT_COLOR_C) || (_def_cfg.color_type > LIGHT_COLOR_RGBCW))
    {
        PR_ERR("config param color_type err: %d", _def_cfg.color_type);
        _def_cfg.color_type = LIGHT_COLOR_RGBCW;
    }

    if (_def_cfg.power_onoff_type != POWER_ONOFF_BY_SHADE && _def_cfg.power_onoff_type != POWER_ONOFF_BY_DIRECT)
    {
        PR_ERR("config param power_onoff_type err: %d", _def_cfg.power_onoff_type);
        _def_cfg.power_onoff_type = POWER_ONOFF_BY_SHADE;
    }

    if (_def_cfg.def_light_color < INIT_COLOR_R || _def_cfg.def_light_color > INIT_COLOR_W)
    {
        PR_ERR("config param def_light_color err: %d", _def_cfg.power_onoff_type);
        _def_cfg.def_light_color = INIT_COLOR_C;
    }

    if (_def_cfg.def_bright_precent == 0 || _def_cfg.def_bright_precent > 100)
    {
        PR_ERR("config param def_bright_precent err: %d", _def_cfg.power_onoff_type);
        _def_cfg.def_bright_precent = 50;
    }

    if (_def_cfg.def_temp_precent > 100)
    {
        PR_ERR("config param def_temp_precent err: %d", _def_cfg.power_onoff_type);
        _def_cfg.def_temp_precent = 100;
    }

    if (_def_cfg.bright_max_precent <= _def_cfg.bright_min_precent)
    {
        return OPRT_INVALID_PARM;
    }

    if (_def_cfg.bright_max_precent > 100)
    {
        _def_cfg.bright_max_precent = 100;
        return OPRT_INVALID_PARM;
    }

    if (_def_cfg.bright_min_precent < 1)
    {
        _def_cfg.bright_min_precent = 1;
        return OPRT_INVALID_PARM;
    }

#if 1
    PR_NOTICE("wf_cfg:%d", _def_cfg.wf_cfg);
    PR_NOTICE("wf_rst_cnt:%d", _def_cfg.wf_rst_cnt);
    PR_NOTICE("color_type:%d", _def_cfg.color_type);
    PR_NOTICE("power_onoff_type:%d", _def_cfg.power_onoff_type);
    PR_NOTICE("def_light_color:%d", _def_cfg.def_light_color);
    PR_NOTICE("def_bright_precent:%d", _def_cfg.def_bright_precent);
    PR_NOTICE("def_temp_precent:%d", _def_cfg.def_temp_precent);
    PR_NOTICE("bright_max_precent:%d", _def_cfg.bright_max_precent);
    PR_NOTICE("bright_min_precent:%d", _def_cfg.bright_min_precent);
    PR_NOTICE("white_color_mutex:%d", _def_cfg.whihe_color_mutex);
#endif

    return OPRT_OK;
}

/******************************************************************************
 * FunctionName : tuya_fast_boot_init
 * Description  : 灯的快速启动的初期配置
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID tuya_fast_boot_init(VOID)
{
    OPERATE_RET op_ret;
    uFILE *fp = NULL;
    CHAR_T *buf_r = NULL;
    ty_cJSON *root = NULL;
    ty_cJSON *json = NULL;
    INT_T Num = 0;

    PR_NOTICE("tuya fast boot in");
    op_ret = uf_file_app_init("12345678901234567890123456789012", 32);
    if (OPRT_OK != op_ret)
    {
        PR_ERR("uf_file_app_init err:%d", op_ret);
        return;
    }

    light_hw_timer_stop();
    tuya_light_device_hw_init(); /* 配置初始化 */

    op_ret = light_cfg_param_set(); /* 读取配置到light_lib 中 */
    if (op_ret != OPRT_OK)
    {
        PR_ERR("light_cfg_param_set err : %d", op_ret);
        return;
    }

    op_ret = light_thread_init();
    if (op_ret != OPRT_OK)
    {
        PR_ERR("light_thread_init err : %d", op_ret);
        return;
    }

    /* 读取DP数据&reset cnt */
    fp = ufopen(FAST_DATA_KEY, "r");
    if (NULL == fp) /* 如果无法打开 */
    {
        PR_ERR("cannot open file");
        goto default_set;
    }

    buf_r = (CHAR_T *)Malloc(2048);

    if (NULL == buf_r)
    {
        PR_ERR("malloc failure");
        ufclose(fp);
        goto default_set;
    }

    memset(buf_r, 0, 2048);

    Num = ufread(fp, buf_r, 2048);
    if (Num <= 0)
    {
        Free(buf_r);
        ufclose(fp);
        PR_ERR("uf read error %d", Num);
        goto default_set;
    }
    PR_NOTICE("read buf %s", buf_r);

    light_dp_data_fast_read(buf_r, &dp_data); /* 读取light的DP数据 */
    Free(buf_r);
    op_ret = ufclose(fp);
    if (op_ret != OPRT_OK)
    {
        PR_ERR("close file error");
    }

    if ((FALSE == dp_data.power) && (tuya_light_dev_poweron() == TRUE)) /* 断电上电，强制开 */
    {
        dev_power_ctl(TRUE);
    }

    PR_NOTICE("rest cnt %d", rst_cnt); /* 说明是reset重启 */
    if (rst_cnt == 0xFF)
    {
        rst_cnt = 0;
        return;
    }

    light_init_stat_set(); /* 开始输出 */
    PR_NOTICE("tuya fast boot out");
    return;

default_set:
    light_dp_data_fast_default_set();
    light_init_stat_set(); /* 开始输出 */
    return;
}

/******************************************************************************
 * FunctionName : tuya_light_init
 * Description  : 灯的初期配置
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
OPERATE_RET tuya_light_init(VOID)
{
    OPERATE_RET op_ret;

    PR_NOTICE("==== tuya light lib version:%s ====", TUYA_LIGH_LIB_VERSION);

    light_hw_timer_stop();
    tuya_light_device_hw_init();

    light_dp_data_read(&dp_data);

    if ((FALSE == dp_data.power) && (tuya_light_dev_poweron() == TRUE)) /* 断电上电，强制开 */
    {
        //上电强制开灯
        dev_power_ctl(TRUE);
    }

    op_ret = light_thread_init();
    if (op_ret != OPRT_OK)
    {
        PR_ERR("light_thread_init err : %d", op_ret);
        return op_ret;
    }

    light_init_stat_set(); /*  */

    PR_NOTICE("tuya_light_init end");
    return OPRT_OK;
}

#if 0
/******************************************************************************
 * FunctionName : tuya_fast_json_set
 * Description  : 灯的快速启动的所需json配置数据的flash写入
 * Parameters   : buf --> json配置文件
 * Returns      : none
*******************************************************************************/
OPERATE_RET tuya_fast_json_set(CHAR_T *buf)
{
    OPERATE_RET op_ret;
    CHAR_T *out = NULL;
    ty_cJSON *root = NULL;
    ty_cJSON *child_hw = NULL;
    UINT_T len = 0;
    uFILE *fp = NULL;
    INT_T Num = 0;
    UINT_T offset = 0;

    root = ty_cJSON_CreateObject();
    if (NULL == root)
    {
        PR_ERR("ty_cJSON_CreateObject error...");
        return OPRT_CR_CJSON_ERR;
    }

    child_hw = ty_cJSON_Parse(buf); /* 解析出oem json配置 */
    if (NULL == child_hw)
    {
        PR_ERR("ty_cJSON parse error...");
        return OPRT_CR_CJSON_ERR;
    }

    ty_cJSON_AddItemToObject(root, "hw_config", child_hw);
    Free(buf);

    out = ty_cJSON_PrintUnformatted(root);
    ty_cJSON_Delete(root);

    if (NULL == out)
    {
        PR_ERR("ty_cJSON_PrintUnformatted error...");
        return OPRT_MALLOC_FAILED;
    }
    PR_DEBUG("light_dp_data:%s", out);

    fp = ufopen(FAST_JSON_KEY, "a+");
    if (NULL == fp)
    {
        Free(out);
        out = NULL;
        PR_ERR("write to file error");
        return OPRT_COM_ERROR;
    }
    else
    {
        PR_DEBUG("open file OK");
    }

    offset = ufseek(fp, 0, SEEK_SET);
    if (offset != 0)
    {
        Free(out);
        out = NULL;
        PR_ERR("write to file error");
        return OPRT_COM_ERROR;
    }

    Num = ufwrite(fp, out, strlen(out));
    if (Num <= 0)
    {
        Free(out);
        out = NULL;
        PR_ERR("uf write %d", Num);
        return OPRT_COM_ERROR;
    }
    PR_DEBUG("write result cnt %d", Num);

    op_ret = ufclose(fp);
    if (op_ret != OPRT_OK)
    {
        PR_ERR("close file error");
    }

    Free(out);
    out = NULL;

    return OPRT_OK;
}
#endif

/******************************************************************************
 * FunctionName : tuya_fast_data_set
 * Description  : 灯的快速启动的所需DP数据的flash写入
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
OPERATE_RET tuya_fast_data_set(VOID)
{
    OPERATE_RET op_ret;
    CHAR_T *out = NULL;
    ty_cJSON *root = NULL;
    ty_cJSON *child_dp = NULL;
    CHAR_T *buf = NULL;
    UINT_T len = 0;
    uFILE *fp = NULL;
    INT_T Num = 0;
    CHAR_T color_data[COLOR_DATA_LEN + 1];
    UINT_T offset = 0;

    root = ty_cJSON_CreateObject();
    if (NULL == root)
    {
        PR_ERR("ty_cJSON_CreateObject error...");
        return OPRT_CR_CJSON_ERR;
    }

    child_dp = ty_cJSON_CreateObject();
    if (NULL == child_dp)
    {
        ty_cJSON_Delete(root);
        PR_ERR("ty_cJSON_CreateObject error...");
        return OPRT_CR_CJSON_ERR;
    }

    ty_cJSON_AddNumberToObject(child_dp, "rst", rst_cnt);

    if (_light_memory == TRUE)
    { //开启记忆
        ty_cJSON_AddBoolToObject(child_dp, "switch", dp_data.power);
        ty_cJSON_AddNumberToObject(child_dp, "mode", dp_data.mode);
        ty_cJSON_AddNumberToObject(child_dp, "bright", dp_data.bright);
        ty_cJSON_AddNumberToObject(child_dp, "temper", dp_data.col_temp);
        ty_cJSON_AddStringToObject(child_dp, "color", dp_data.color);
    }
    else
    { //无记忆下默认写入默认值

        ty_cJSON_AddBoolToObject(child_dp, "switch", TRUE);

        if (_def_cfg.color_type == LIGHT_COLOR_RGB || (_def_cfg.def_light_color < INIT_COLOR_C))
        {
            ty_cJSON_AddNumberToObject(child_dp, "mode", COLOR_MODE);
        }
        else
        {
            ty_cJSON_AddNumberToObject(child_dp, "mode", WHITE_MODE);
        }

        ty_cJSON_AddNumberToObject(child_dp, "bright", light_default_bright_get());
        ty_cJSON_AddNumberToObject(child_dp, "temper", light_default_temp_get());

        if (dp_data.mode == COLOR_MODE)
        {
            light_default_color_get(color_data);
            ty_cJSON_AddStringToObject(child_dp, "color", color_data);
        }
        else
        {
            ty_cJSON_AddStringToObject(child_dp, "color", COLOR_DATA_DEFAULT);
        }
    }

    //情景设定任何模式下保持记忆
    ty_cJSON_AddStringToObject(child_dp, "scene", dp_data.scene);

#ifdef KEY_CHANGE_SCENE_FUN
    ty_cJSON_AddStringToObject(child_dp, "scene0", key_scene_data[0]);
    ty_cJSON_AddStringToObject(child_dp, "scene1", key_scene_data[1]);
    ty_cJSON_AddStringToObject(child_dp, "scene2", key_scene_data[2]);
    ty_cJSON_AddStringToObject(child_dp, "scene3", key_scene_data[3]);
    ty_cJSON_AddStringToObject(child_dp, "scene4", key_scene_data[4]);
    ty_cJSON_AddStringToObject(child_dp, "scene5", key_scene_data[5]);
    ty_cJSON_AddStringToObject(child_dp, "scene6", key_scene_data[6]);
    ty_cJSON_AddStringToObject(child_dp, "scene7", key_scene_data[7]);
#endif

    ty_cJSON_AddItemToObject(root, "fast_dp_data", child_dp); /* fast data 存入 */

    out = ty_cJSON_PrintUnformatted(root);
    ty_cJSON_Delete(root);

    if (NULL == out)
    {
        PR_ERR("ty_cJSON_PrintUnformatted error...");
        return OPRT_MALLOC_FAILED;
    }
    PR_DEBUG("light_dp_data:%s", out);

    fp = ufopen(FAST_DATA_KEY, "a+");
    if (NULL == fp)
    {
        PR_ERR("write to file error");
        return OPRT_COM_ERROR;
    }
    else
    {
        PR_DEBUG("open file OK");
    }

    offset = ufseek(fp, 0, SEEK_SET);
    if (offset != 0)
    {
        Free(out);
        out = NULL;
        return OPRT_COM_ERROR;
    }

    Num = ufwrite(fp, out, strlen(out));
    if (Num <= 0)
    {
        PR_ERR("uf write %d", Num);
        Free(out);
        out = NULL;
        return OPRT_COM_ERROR;
    }
    PR_DEBUG("write result cnt %d", Num);

    op_ret = ufclose(fp);
    if (op_ret != OPRT_OK)
    {
        PR_ERR("close file error");
    }

    Free(out);
    out = NULL;

    return OPRT_OK;
}

/******************************************************************************
 * FunctionName : mf_user_callback
 * Description  : 授权时，删除用户数据
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID mf_user_callback(VOID)
{
    OPERATE_RET op_ret;

    op_ret = ufdelete(FAST_DATA_KEY);
}
