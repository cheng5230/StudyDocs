/***********************************************************
*  File: light_proc.c 
*  Author: jinhao
*  Date: 2018/10/13
***********************************************************/
#define __DEVICE_GLOBALS
#include "tuya_device.h"
#include "tuya_light_lib.h"
#include "ty_cJSON.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

#define PROD_TEST_DATA_KEY1 "prod_test_key1"
#define PROD_TEST_DATA_KEY2 "prod_test_key2"

//不可更改
#define FUC_BREATH_TIME_TICK 10
#define LOW_RSSI_TIME_TICK 250
#define AGING_TEST_TIME_TICK 60000

//可更改
#define AUZ_TEST_FAIL_TIME_TICK 3000
#define FUC_TEST1_TIME_TICK 1000
#define RESTART_AGING_TIME_TICK 500
#define FUC_TEST2_BREATH_TIME_TICK 1000
#define FUC_TEST1_TIME 120 //120s

#define LIGHT_MAX_DATA 1000
#define LIGHT_MIN_DATA 0

#define LIGHT_MIN_BRIGHT 100

#define PROD_TEST1_SSID "tuya_mdev_test1"
#define PROD_TEST2_SSID "tuya_mdev_test2"

/***********************产测状态灯转换定义************************/

/* 转换思路：
 * 1.用1bit num二进制低5位表示当前要亮的灯来进行参数传参
 * 2.将几路灯的信息依次排进5bit,比如从最低位开始,五路灯RGBCW、四路灯RGBC、三路灯RGB、双色CW、单色C
 * 3.在输入到实际PWM接口时,在prod_send_light_data函数中用light_num_table表进行位置转换
 */

typedef enum
{
    R_NUM = 0,
    G_NUM,
    B_NUM,
    CC_NUM,
    CW_NUM,
    N_NUL = 0xFF,
} LIGHT_NUM_DEF;

STATIC CONST UCHAR light_num_table[5][5] =
    {
        //5种类型灯IO口参数转换表
        //    R      R      B      C       W
        N_NUL, N_NUL, N_NUL, CC_NUM, N_NUL,  //C
        N_NUL, N_NUL, N_NUL, CC_NUM, CW_NUM, //CW
        R_NUM, G_NUM, B_NUM, N_NUL, N_NUL,   //RGB
        R_NUM, G_NUM, B_NUM, CC_NUM, N_NUL,  //RGBC
        R_NUM, G_NUM, B_NUM, CC_NUM, CW_NUM, //RGBCW
};

typedef struct
{
    LIGHT_PROD_PAR prod;

    LIGHT_COLOR_TYPE_E light_max_num;
    LIGHT_CW_TYPE_E cw_type;
} PROD_PAR_S;
STATIC PROD_PAR_S light_prod;

#define light_num2bit(num) (0x01 << num)
//#define     light_num(num)     (light_prod.light_max_num>2?num:(num-CC_NUM))

//传参light_num
#define R_SE_NUM light_num2bit(R_NUM)
#define G_SE_NUM light_num2bit(G_NUM)
#define B_SE_NUM light_num2bit(B_NUM)
#define CC_SE_NUM light_num2bit(CC_NUM)
#define CW_SE_NUM light_num2bit(CW_NUM)

/***********************产测定义************************/

/***********************产测时间定义************************/
CONST AGING_TIME_DEF light_aging_time[5] =
    {
        //5种类型灯产测时间表
        //最后一个参数表示老化结束亮什么类型的灯
        //    C   W  RGB       SUC_NUM
        {40, 0, 0, light_num2bit(CC_NUM)},                          //C
        {20, 20, 0, light_num2bit(CC_NUM) | light_num2bit(CW_NUM)}, //CW
        {0, 0, 40, light_num2bit(G_NUM)},                           //RGB
        {30, 0, 10, light_num2bit(G_NUM)},                          //RGBC
        {20, 20, 10, light_num2bit(G_NUM)},                         //RGBCW
};

#define AGING_TEST_TIME (light_prod.prod.aging_time.cc_time + \
                         light_prod.prod.aging_time.cw_time + \
                         light_prod.prod.aging_time.rgb_time)

#define AGING_TEST_C_TIME light_prod.prod.aging_time.cc_time
#define AGING_TEST_W_TIME light_prod.prod.aging_time.cw_time
#define AGING_TEST_RGB_TIME light_prod.prod.aging_time.rgb_time
#define AGING_SUCCEED_LIGHT light_prod.prod.aging_time.suc_light_num

/***********************************************************
*************************variable define********************
***********************************************************/

TIMER_ID proc_light_timer;
TIMER_ID proc_tick_timers;

typedef struct _APP_DT_ST
{
    UINT len;
    UINT crc32;
} APP_DT_ST;

typedef enum
{
    FUC_TEST1 = 0,
    AGING_TEST,
    FUC_TEST2,
} TEST_MODE_DEF;

typedef struct
{
    UINT aging_tested_time;
    TEST_MODE_DEF test_mode;

    UINT breath_count;
    UINT breath_interval;
    UCHAR light_num;
} TEST_DEF;
STATIC TEST_DEF test_handle;

typedef enum
{
    LIGHT_STANDBY = 0,
    LIGHT_FLASHOVER,
    LIGHT_BREATH,
} LIGHT_MODE_DEF;
STATIC LIGHT_MODE_DEF light_mode = LIGHT_STANDBY;

typedef enum
{
    WIFI_IDLE = 0,
    WIFI_UNAUTHORIZED,
    WIFI_LOW_RSSI,
    WIFI_FUC_TEST1,
    WIFI_RESTART_AGING,
    WIFI_AGING_TEST,
    WIFI_FUC_TEST2,
} PROC_STATUS_DEF;
STATIC PROC_STATUS_DEF light_proc_mode = WIFI_IDLE;

typedef enum
{
    AGING_TEST_CC = 0,
    AGING_TEST_CW,
    AGING_TEST_RGB,
    AGING_TEST_SUCCEED,
} AGING_MODE_DEF; /* 当前的老化的灯的状态 */

STATIC volatile UCHAR flash_over_status = FALSE;
STATIC volatile BOOL all_light_loop = FALSE;
STATIC UCHAR all_lignt_loop_num;
STATIC USHORT loop_cycle_index;
STATIC USHORT light_max_data = LIGHT_MAX_DATA;
STATIC UCHAR fuc_breath_time_tick = FUC_BREATH_TIME_TICK;

STATIC VOID light_aging_test_cb(VOID);
STATIC UCHAR get_light_default_num(VOID);

extern LIGHT_DEFAULT_CONFIG_S *light_default_cfg_get(VOID);
extern LIGHT_CW_TYPE_E tuya_light_cw_type_get(VOID);
extern OPERATE_RET aging_time_write(UINT val);
extern UINT aging_time_read(VOID);

STATIC OPERATE_RET light_prod_test_data_write(VOID)
{
    OPERATE_RET op_ret;
    CHAR *out = NULL;

    ty_cJSON *root_test = NULL;
    root_test = ty_cJSON_CreateObject();
    if (NULL == root_test)
    {
        PR_ERR("json creat failed");
        goto ERR_EXT;
    }

    ty_cJSON_AddNumberToObject(root_test, "test_mode", test_handle.test_mode);
    ty_cJSON_AddNumberToObject(root_test, "aging_time", test_handle.aging_tested_time);

    out = ty_cJSON_PrintUnformatted(root_test);
    ty_cJSON_Delete(root_test);
    if (NULL == out)
    {
        PR_ERR("ty_cJSON_PrintUnformatted err:");
        Free(out);
        goto ERR_EXT;
    }
    PR_DEBUG("write prod test param: %s", out);

    //FLASH1写入
    if (OPRT_OK != tuya_light_common_flash_write(PROD_TEST_DATA_KEY1, out, strlen(out) + 1))
    {
        PR_ERR("prod data write FLASH1 failed!");
        op_ret = OPRT_COM_ERROR;
    }
    else
    {
        PR_DEBUG("prod data write FLASH1 success");
        op_ret = OPRT_OK;
    }

    //FLASH2写入
    if (OPRT_OK != tuya_light_common_flash_write(PROD_TEST_DATA_KEY2, out, strlen(out) + 1))
    {
        PR_ERR("prod data write FLASH2 failed!");
    }
    else
    {
        PR_DEBUG("prod data write FLASH2 success");
        op_ret = OPRT_OK;
    }

    Free(out);

    return op_ret;
ERR_EXT:
    return OPRT_COM_ERROR;
}

/******************************************************************************
 * FunctionName : light_prod_test_data_read
 * Description  : 灯产测数据的读取
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC OPERATE_RET light_prod_test_data_read(VOID)
{
    OPERATE_RET op_ret;
    UCHAR_T mode_psm, mode_app;
    UINT_T time_psm, time_app;
    BYTE flag = 0;
    UCHAR_T *buf = NULL;
    ty_cJSON *root = NULL, *json = NULL;

    //read form KVS
    op_ret = tuya_light_common_flash_read(PROD_TEST_DATA_KEY1, &buf);
    if (OPRT_OK != op_ret)
    {
        PR_ERR("msf_get_single failed");
        Free(buf);
        buf = NULL;
        goto ERR_EXT;
    }
    PR_DEBUG("read flash form FLASH1: %s", buf);

    root = ty_cJSON_Parse(buf);
    if (NULL == root)
    {
        PR_ERR("ty_cJSON Parse ERR!!!");
        goto ERR_EXT;
    }

    json = ty_cJSON_GetObjectItem(root, "test_mode");
    if (NULL != json)
    {
        flag |= (0x01 << 0);
        mode_psm = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(root, "aging_time");
    if (NULL != json)
    {
        flag |= (0x01 << 2);
        time_psm = json->valueint;
    }

    ty_cJSON_Delete(root);
    root = NULL;
    Free(buf);
    buf = NULL;

    //从kvs读取备份
    op_ret = tuya_light_common_flash_read(PROD_TEST_DATA_KEY2, &buf);
    if (op_ret != OPRT_OK)
    {
        PR_NOTICE("FLASH2 get param failed");
        goto ERR_EXT;
    }
    PR_DEBUG("read flash form FLASH2: %s", buf);

    root = ty_cJSON_Parse(buf);
    if (NULL == root)
    {
        Free(buf);
        PR_ERR("ty_cJSON Parse ERR!!!");
        goto ERR_EXT;
    }

    json = ty_cJSON_GetObjectItem(root, "test_mode");
    if (NULL != json)
    {
        flag |= (0x01 << 1);
        mode_app = json->valueint;
    }

    json = ty_cJSON_GetObjectItem(root, "aging_time");
    if (NULL != json)
    {
        flag |= (0x01 << 3);
        time_app = json->valueint;
    }

    ty_cJSON_Delete(root);
    root = NULL;

    //产测标志位判断
    switch (flag & 0x03)
    {
    case 0x00: //两个flash读取均失败
        test_handle.test_mode = FUC_TEST1;
        PR_NOTICE("START NEW PROD TEST!");
        break;

    case 0x01: //直接读取失败
        test_handle.test_mode = mode_psm;
        break;

    case 0x02: //PSM读取失败
        test_handle.test_mode = mode_app + 1;
        if (test_handle.test_mode > FUC_TEST2)
        {
            test_handle.test_mode = FUC_TEST2;
        }
        break;

    case 0x03: //两个flash读取均成功
        if (mode_psm > mode_app)
        {
            test_handle.test_mode = mode_psm;
        }
        else
        {
            test_handle.test_mode = mode_app;
        }
        break;

    default:
        break;
    }

    //老化时间判断
    switch ((flag >> 2) & 0x03)
    {
    case 0x00: //两个flash读取均失败
        test_handle.aging_tested_time = 0;
        PR_NOTICE("Read flash data failed, aging test time reset 0");
        break;

    case 0x01: //直接读取失败
        test_handle.aging_tested_time = time_psm;
        break;

    case 0x02: //PSM读取失败
        test_handle.aging_tested_time = time_app + 1;
        if (test_handle.aging_tested_time > AGING_TEST_TIME)
        {
            test_handle.aging_tested_time = 0;
        }
        break;

    case 0x03: //两个flash读取均成功
        if (time_psm > time_app)
        {
            test_handle.aging_tested_time = time_psm;
        }
        else
        {
            test_handle.aging_tested_time = time_app;
        }
        break;

    default:
        break;
    }

    return OPRT_OK;

ERR_EXT:
    if (buf != NULL)
    {
        Free(buf);
    }

    test_handle.test_mode = FUC_TEST1;
    test_handle.aging_tested_time = 0;

    light_prod_test_data_write();

    return OPRT_COM_ERROR;
}

STATIC UCHAR get_light_default_num(VOID)
{
    UCHAR light_num;

    if (light_prod.light_max_num == LIGHT_COLOR_C || light_prod.light_max_num == LIGHT_COLOR_CW)
    {
        if (light_proc_mode == WIFI_UNAUTHORIZED || light_proc_mode == WIFI_LOW_RSSI)
            light_num = light_num2bit(CC_NUM);
        else
            light_num = CC_NUM;
    }
    else if (light_prod.light_max_num == LIGHT_COLOR_RGB)
    {
        if (light_proc_mode == WIFI_UNAUTHORIZED || light_proc_mode == WIFI_LOW_RSSI)
            light_num = light_num2bit(R_NUM);
        else
            light_num = R_NUM;
    }
    else
    {
        if (light_proc_mode == WIFI_UNAUTHORIZED)
            light_num = light_num2bit(R_NUM);
        else if (light_proc_mode == WIFI_LOW_RSSI)
            light_num = light_num2bit(CC_NUM);
        else
            light_num = R_NUM;
    }

    return light_num;
}

STATIC UCHAR get_light_max_num(VOID)
{
    UCHAR light_num;

    if (light_prod.light_max_num == LIGHT_COLOR_C || light_prod.light_max_num == LIGHT_COLOR_RGBC)
    {
        light_num = CC_NUM;
    }
    else if (light_prod.light_max_num == LIGHT_COLOR_CW || light_prod.light_max_num == LIGHT_COLOR_RGBCW)
    {
        light_num = CW_NUM;
    }
    else
    {
        light_num = B_NUM;
    }

    return light_num;
}

STATIC VOID prod_send_light_data(UCHAR_T light_num, USHORT_T light_data)
{
    USHORT_T light_buf[5];
    UCHAR_T i, j = 0;

    if (light_data > light_max_data)
    {
        light_data = light_max_data;
    }

    memset(light_buf, 0, sizeof(light_buf));

    for (i = 0; i < 5; i++)
    {
        if (light_num_table[light_prod.light_max_num - 1][i] == N_NUL)
        {
            continue;
        }

        if ((light_num >> i) & 0x01)
        {
            if (light_prod.cw_type == LIGHT_CW_CCT)
            {
                if (light_num == CC_SE_NUM)
                {
                    light_buf[CC_NUM] = light_data;
                    light_buf[CW_NUM] = light_max_data;
                    break;
                }
                else if (light_num == CW_SE_NUM)
                {
                    light_buf[CC_NUM] = light_data;
                    light_buf[CW_NUM] = 0;
                    break;
                }
                else if (light_num == AGING_SUCCEED_LIGHT && test_handle.aging_tested_time == AGING_TEST_TIME && test_handle.test_mode == AGING_TEST && light_prod.light_max_num == 2)
                {
                    light_buf[CC_NUM] = light_data;
                    light_buf[CW_NUM] = light_max_data / 2;
                    break;
                }
                else
                {
                    light_buf[light_num_table[light_prod.light_max_num - 1][i]] = light_data;
                }
            }
            else
            {
                light_buf[light_num_table[light_prod.light_max_num - 1][i]] = light_data;
            }
        }
    }

    tuya_light_send_data(light_buf[R_NUM], light_buf[G_NUM], light_buf[B_NUM], light_buf[CC_NUM], light_buf[CW_NUM]);

    PR_DEBUG("r_data:%d,g_data:%d,b_data:%d,cc_data:%d, cw_data:%d", light_buf[0], light_buf[1], light_buf[2], light_buf[3], light_buf[4]);
}

STATIC VOID light_flashover_cb(VOID)
{
    STATIC USHORT cycle_index_count = 0;

    if (all_light_loop == FALSE)
    {
        if (flash_over_status == FALSE)
        {
            if (light_proc_mode == WIFI_UNAUTHORIZED)
                prod_send_light_data(test_handle.light_num, light_max_data / 10);
            else
                prod_send_light_data(test_handle.light_num, light_max_data);

            flash_over_status = TRUE;
        }
        else
        {
            prod_send_light_data(test_handle.light_num, LIGHT_MIN_DATA);
            flash_over_status = FALSE;
        }
        PR_DEBUG("all_light_loop %d flash_over_status %d", all_light_loop, flash_over_status);
    }
    else
    {
        prod_send_light_data(light_num2bit(all_lignt_loop_num), light_max_data);

        if (all_lignt_loop_num == get_light_max_num())
        {
            all_lignt_loop_num = get_light_default_num();

            if (loop_cycle_index != 0)
            {
                cycle_index_count++;
                if (cycle_index_count == loop_cycle_index)
                {
                    sys_stop_timer(proc_light_timer);
                }
            }
        }
        else
        {
            all_lignt_loop_num++;
        }
    }
}

STATIC VOID light_breath_cb(VOID)
{
    STATIC BOOL light_breath = FALSE;
    STATIC BOOL all_light_flag = FALSE;
    STATIC USHORT cycle_index_count = 0;
    UINT breath_step = (light_max_data / (test_handle.breath_interval / fuc_breath_time_tick));

    if (light_breath == FALSE)
    {
        if (test_handle.breath_count <= (light_max_data - breath_step))
        {
            test_handle.breath_count += breath_step;
        }
        else
        {
            test_handle.breath_count = light_max_data;
            light_breath = TRUE;
        }
    }
    else
    {
        if (test_handle.breath_count >= (LIGHT_MIN_DATA + breath_step))
        {
            test_handle.breath_count -= breath_step;
        }
        else
        {
            test_handle.breath_count = LIGHT_MIN_DATA;
            light_breath = FALSE;

            all_light_flag = TRUE;
        }
    }

    if (all_light_loop == FALSE)
    {
        prod_send_light_data(test_handle.light_num, test_handle.breath_count);
    }
    else
    {
        prod_send_light_data(light_num2bit(all_lignt_loop_num), test_handle.breath_count);

        if (all_light_flag == TRUE)
        {
            if (all_lignt_loop_num == get_light_max_num())
            {
                all_lignt_loop_num = get_light_default_num();

                if (loop_cycle_index != 0)
                {
                    cycle_index_count++;
                    if (cycle_index_count == loop_cycle_index)
                    {
                        sys_stop_timer(proc_light_timer);

                        if (light_prod.prod.retest == TRUE)
                        {
                            test_handle.test_mode = FUC_TEST1;
                            //     set_light_test_flag();
                            light_prod_test_data_write();
                            SystemReset();
                        }
                    }
                }
            }
            else
            {
                all_lignt_loop_num++;
            }

            all_light_flag = FALSE;
        }
    }
}

VOID light_flashover_start(UCHAR light_num, UINT interval_time)
{

    light_mode = LIGHT_FLASHOVER;
    flash_over_status = TRUE;
    all_light_loop = FALSE;

    test_handle.light_num = light_num;

    if (light_proc_mode == WIFI_UNAUTHORIZED)
        prod_send_light_data(light_num, light_max_data / 10);
    else
        prod_send_light_data(light_num, light_max_data);

    sys_start_timer(proc_light_timer, interval_time, TIMER_CYCLE);
}

VOID light_breath_start(UCHAR light_num, UINT interval_time)
{

    light_mode = LIGHT_BREATH;
    test_handle.breath_count = LIGHT_MIN_DATA;
    all_light_loop = FALSE;

    test_handle.light_num = light_num;
    test_handle.breath_interval = interval_time;

    prod_send_light_data(light_num, LIGHT_MIN_DATA);
    sys_start_timer(proc_light_timer, fuc_breath_time_tick, TIMER_CYCLE);
}

VOID light_flashover_loop_start(UINT_T interval_time, USHORT_T cycle_index)
{
    if (light_prod.light_max_num == 1)
    {
        light_flashover_start(CC_SE_NUM, interval_time);
        return;
    }

    light_mode = LIGHT_FLASHOVER;
    all_lignt_loop_num = get_light_default_num();
    flash_over_status = TRUE;
    all_light_loop = TRUE;
    loop_cycle_index = cycle_index;

    sys_start_timer(proc_light_timer, interval_time, TIMER_CYCLE);
}

VOID light_breath_loop_start(UINT interval_time, USHORT cycle_index)
{

    light_mode = LIGHT_BREATH;
    all_lignt_loop_num = get_light_default_num();
    all_light_loop = TRUE;

    test_handle.breath_count = LIGHT_MIN_DATA;
    test_handle.breath_interval = interval_time;
    loop_cycle_index = cycle_index;

    sys_start_timer(proc_light_timer, fuc_breath_time_tick, TIMER_CYCLE);
}

VOID light_keep_status(UCHAR_T light_num, USHORT_T light_data)
{
    light_mode = LIGHT_STANDBY;

    if (IsThisSysTimerRun(proc_light_timer) == TRUE)
    {
        sys_stop_timer(proc_light_timer);
    }

    prod_send_light_data(light_num, light_data); // 此处上层传参 已将  light_num2bit 转化过了。
}

STATIC VOID light_prod_timer_cb(UINT timerID, PVOID pTimerArg)
{
    //PR_DEBUG("light_mode:%d", light_mode);

    switch (light_mode)
    {
    case LIGHT_STANDBY:
        break;
    case LIGHT_FLASHOVER:
        light_flashover_cb();
        break;
    case LIGHT_BREATH:
        light_breath_cb();
        break;
    default:
        break;
    }
}

/******************************************************************************
 * FunctionName : aging_test_status
 * Description  : 返回当前灯老化的状态
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC AGING_MODE_DEF aging_test_status(UINT_T time)
{
    if (time < AGING_TEST_C_TIME)
    {
        PR_DEBUG("aging_test_cc_mode:%dmins", AGING_TEST_C_TIME);
        return AGING_TEST_CC;
    }
    else if (time >= AGING_TEST_C_TIME && time < (AGING_TEST_C_TIME + AGING_TEST_W_TIME))
    {
        PR_DEBUG("aging_test_cw_mode:%dmins", AGING_TEST_W_TIME);
        return AGING_TEST_CW;
    }
    else if (time >= (AGING_TEST_C_TIME + AGING_TEST_W_TIME) && time < AGING_TEST_TIME)
    {
        PR_DEBUG("aging_test_rgb_mode:%dmins", AGING_TEST_RGB_TIME);
        return AGING_TEST_RGB;
    }
    else
    {
        PR_DEBUG("aging_test_succeed");
        return AGING_TEST_SUCCEED;
    }
}

STATIC VOID change_aging_test_light(UINT_T time)
{
    PR_DEBUG("aging_test time:%d", time);

    switch (aging_test_status(time))
    {
    case AGING_TEST_CC:
        light_keep_status(CC_SE_NUM, light_max_data);
        break;
    case AGING_TEST_CW:
        light_keep_status(CW_SE_NUM, light_max_data);
        break;
    case AGING_TEST_RGB:
        light_keep_status(R_SE_NUM | G_SE_NUM | B_SE_NUM, light_max_data);
        break;
    case AGING_TEST_SUCCEED:
        light_keep_status(AGING_SUCCEED_LIGHT, (light_max_data / 10));
        test_handle.test_mode = FUC_TEST2;
        //            set_light_test_flag();
        light_prod_test_data_write();
        sys_stop_timer(proc_tick_timers);
        PR_NOTICE("AGING TEST SUCCEED");
        break;
    default:
        break;
    }
}

/******************************************************************************
 * FunctionName : light_tick_timer_cb
 * Description  : 的线程、、、、
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID light_tick_timer_cb(UINT_T timerID, PVOID_T pTimerArg)
{
    STATIC UINT_T tick_count = 0;
    BOOL_T clear_count_flag = FALSE;

    switch (light_proc_mode)
    {
    case WIFI_IDLE:
        break;
    case WIFI_UNAUTHORIZED:
        break;
    case WIFI_LOW_RSSI: /* WIFI信号弱，250ms基准定时器 */
        if (tick_count == 0)
        { //0-12处于间隔1500ms呼吸状态
            light_breath_start(get_light_default_num(), LOW_RSSI_TIME_TICK * 6);
        }
        else if (tick_count == 12)
        { //12-16处于间隔250ms闪烁状态
            light_flashover_start(get_light_default_num(), LOW_RSSI_TIME_TICK);
        }
        else if (tick_count == 16)
        {
            clear_count_flag = TRUE;
        }
        break;
    case WIFI_FUC_TEST1: /* 产测模式1，1000ms基准定时器 */
        if (tick_count == FUC_TEST1_TIME)
        { /* 产测1持续时间120s */
            test_handle.test_mode = AGING_TEST;
            test_handle.aging_tested_time = 0;
            light_prod_test_data_write();
            SystemReset();
            clear_count_flag = TRUE;
        }
        break;
    case WIFI_RESTART_AGING: //老化模式重新上电，250ms基准定时器
        if (light_prod.light_max_num == 1)
        {
            if (tick_count == 10)
            { //单色灯开关循环间隔1s闪烁5次
                tick_count = 0;
                light_aging_test_cb();
            }
            else if (tick_count == 9)
            {
                if (IsThisSysTimerRun(proc_light_timer) == TRUE)
                {
                    sys_stop_timer(proc_light_timer);
                }
            }
        }
        else
        {
            if (tick_count == light_prod.light_max_num * 5)
            { //所有灯循环间隔1s闪烁5次
                tick_count = 0;
                light_aging_test_cb();
            }
            else if (tick_count == light_prod.light_max_num * 5 - 1)
            {
                if (IsThisSysTimerRun(proc_light_timer) == TRUE)
                {
                    sys_stop_timer(proc_light_timer);
                }
            }
        }
        break;
    case WIFI_AGING_TEST: //老化模式，1min基准定时器
        test_handle.aging_tested_time++;
        change_aging_test_light(test_handle.aging_tested_time);
        light_prod_test_data_write();
        break;
    case WIFI_FUC_TEST2:
        break;
    default:
        break;
    }

    if (clear_count_flag == TRUE)
    {
        tick_count = 0;
    }
    else
    {
        tick_count++;
    }
}

STATIC VOID wifi_unauthorized_cb(VOID)
{
    light_proc_mode = WIFI_UNAUTHORIZED;
    light_flashover_start(get_light_default_num(), AUZ_TEST_FAIL_TIME_TICK);
}

STATIC VOID wifi_low_rssi_cb(VOID)
{
    light_proc_mode = WIFI_LOW_RSSI;
    sys_start_timer(proc_tick_timers, LOW_RSSI_TIME_TICK, TIMER_CYCLE);
}

STATIC VOID light_fuc_test1_cb(VOID)
{
    light_proc_mode = WIFI_FUC_TEST1;
    light_flashover_loop_start(FUC_TEST1_TIME_TICK, 0);
    sys_start_timer(proc_tick_timers, FUC_TEST1_TIME_TICK, TIMER_CYCLE);
}

STATIC VOID light_restart_aging_test_cb(VOID)
{
    light_proc_mode = WIFI_RESTART_AGING;
    light_flashover_loop_start(RESTART_AGING_TIME_TICK, 5);
    sys_start_timer(proc_tick_timers, RESTART_AGING_TIME_TICK, TIMER_CYCLE);
}

STATIC VOID light_aging_test_cb(VOID)
{
    light_proc_mode = WIFI_AGING_TEST;

    change_aging_test_light(test_handle.aging_tested_time);

    sys_start_timer(proc_tick_timers, AGING_TEST_TIME_TICK, TIMER_CYCLE);
}

STATIC VOID light_fuc_test2_cb(VOID)
{
    light_proc_mode = WIFI_FUC_TEST2;
    if (light_prod.prod.retest == TRUE)
    {
        light_breath_loop_start(FUC_TEST2_BREATH_TIME_TICK, 10);
    }
    else
    {
        light_breath_loop_start(FUC_TEST2_BREATH_TIME_TICK, 0);
    }
}

/*********************************************************************************************************/

/******************************************************************************
 * FunctionName : light_prod_test
 * Description  : 产测的处理
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID light_prod_test(BOOL_T flag, SCHAR_T rssi)
{
    OPERATE_RET op_ret;

    tuya_light_exit_lowpower_mode();
    tuya_light_config_stat(CONF_STAT_UNCONNECT);

    PR_NOTICE("rssi:%d", rssi);

    //prod thread create and start
    op_ret = sys_add_timer(light_prod_timer_cb, NULL, &proc_light_timer);
    if (OPRT_OK != op_ret)
    {
        return;
    }
    op_ret = sys_add_timer(light_tick_timer_cb, NULL, &proc_tick_timers);
    if (OPRT_OK != op_ret)
    {
        return;
    }

    if (flag == FALSE)
    {
        PR_ERR("WIFI Unauthorized!");
        wifi_unauthorized_cb();
        return;
    }

    if (rssi < -60)
    {
        PR_ERR("RSSI_REE!  rssi:%d", rssi);
        wifi_low_rssi_cb();
        return;
    }

    switch (test_handle.test_mode)
    {
    case FUC_TEST1:
        light_fuc_test1_cb(); /* 进入老化的前的确认 */
        break;
    case AGING_TEST:
        light_restart_aging_test_cb();
        break;
    case FUC_TEST2:
        light_fuc_test2_cb();
        break;
    default:
        break;
    }
    return;
}

/******************************************************************************
 * FunctionName : prod_test
 * Description  : 产测处理
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID prod_test(BOOL_T flag, SCHAR_T rssi)
{
    light_rst_cnt_write(0);
    light_prod_test(flag, rssi);
}

STATIC VOID light_prod_default(VOID)
{
    light_max_data = tuya_light_ctrl_precision_get();
    fuc_breath_time_tick = fuc_breath_time_tick * LIGHT_MAX_DATA / light_max_data;

    light_prod.cw_type = tuya_light_cw_type_get();

    light_prod.light_max_num = light_default_cfg_get()->color_type;
    PR_DEBUG("light_prod.light_max_num:%d", light_prod.light_max_num);

    memcpy(light_prod.prod.test1_ssid, PROD_TEST1_SSID, MAX_TEST_SSID_LEN);
    memcpy(light_prod.prod.test2_ssid, PROD_TEST2_SSID, MAX_TEST_SSID_LEN);
    PR_DEBUG("PROD_TEST1_SSID:%s", light_prod.prod.test1_ssid);
    PR_DEBUG("PROD_TEST2_SSID:%s", light_prod.prod.test2_ssid);

    memcpy(&light_prod.prod.aging_time, &light_aging_time[light_prod.light_max_num - 1], sizeof(AGING_TIME_DEF));

    PR_DEBUG("AGING_TEST_TIME:%d", AGING_TEST_TIME);
    PR_DEBUG("AGING_TEST_C_TIME:%d", AGING_TEST_C_TIME);
    PR_DEBUG("AGING_TEST_W_TIME:%d", AGING_TEST_W_TIME);
    PR_DEBUG("AGING_TEST_RGB_TIME:%d", AGING_TEST_RGB_TIME);
    PR_DEBUG("AGING_SUCCEED_LIGHT:0x%x", AGING_SUCCEED_LIGHT);

    light_prod.prod.retest = FALSE;
}

OPERATE_RET light_prod_init(PROD_TEST_TYPE_E type, APP_PROD_CB callback)
{
    OPERATE_RET op_ret;
    TUYA_WF_CFG_MTHD_SEL mthd;

    mthd = tuya_light_get_wifi_cfg();

    op_ret = light_prod_test_data_read();
    if (op_ret != OPRT_OK)
    {
        PR_NOTICE("START NEW PROD TEST!");
    }
    PR_NOTICE("prod mode = %d, aging time = %d", test_handle.test_mode, test_handle.aging_tested_time);

    light_prod_default();

    if (type == PROD_TEST_TYPE_CUSTOM && callback == NULL)
    {
        PR_ERR("PROD_TEST_TYPE_E error:%d", type);
        return OPRT_INVALID_PARM;
    }
    PR_DEBUG("PROD_TEST_TYPE:%d TUYA_WF_CFG_MTHD_SEL:%d ", type, mthd);
    if (type == PROD_TEST_TYPE_V2)
    {
        if (test_handle.test_mode == FUC_TEST2)
        {
            set_prod_ssid(light_prod.prod.test2_ssid);
        }
        else
        {
            set_prod_ssid(light_prod.prod.test1_ssid);
        }
        app_cfg_set(mthd, callback);
    }
    else if (type == PROD_TEST_TYPE_CUSTOM)
    {
        app_cfg_set(mthd, callback);
    }
    else if (type == PROD_TEST_TYPE_V1)
    {
        app_cfg_set(mthd, callback);
    }

    return OPRT_OK;
}
