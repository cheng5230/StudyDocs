#include ".compile_usr_cfg.h"
#include "tuya_hw_table.h"
#include "tuya_device.h"
#include "tuya_light_lib.h"

STATIC HW_TABLE_S g_hw_table;

CONST CHAR_T* light_color_mode[] = {"rgbwc", "rgbc", "rgb", "cw", "c"};
CONST CHAR_T* wfcfg[] = {"spcl", "old"};

CONST CHAR_T* light_driver_mode[] = {"pwm", "i2c-sm726", "i2c-sm2135"};
CONST CHAR_T* cw_driver_type[] = {"cw", "cct"};


CONST CHAR_T* color_table[] = {"r", "g", "b", "c", "w"};
CONST CHAR_T* iic_cfg_table[] = {"iicr", "iicg", "iicb", "iicc", "iicw"};
CONST CHAR_T* pin_level_table[] = {"low", "high"};
CONST CHAR_T color_count[] = {5, 4, 3, 2, 1};

CONST UCHAR_T lowpower_pin_table[] = {LOW_POWER_0, LOW_POWER_1, LOW_POWER_2, LOW_POWER_3, LOW_POWER_4};//根据模块类型默认定义
CONST UCHAR_T iic_scl_pin[] = {I2C_SCL_0, I2C_SCL_1, I2C_SCL_2, I2C_SCL_3, I2C_SCL_4};
CONST UCHAR_T iic_sda_pin[] = {I2C_SDA_0, I2C_SDA_1, I2C_SDA_2, I2C_SDA_3, I2C_SDA_4};

STATIC char json_version[] = "0.0.2";


/******************************************************************************
 * FunctionName : user_cjson_get_valueint
 * Description  : 从json配置中读取value值（枚举 & cnt）
 * Parameters   :  *root             json
                   *key_name        字符标识符
                   max              上限值
                   min              下限值
                   dug_line         debug 行号
 * Returns      :
                    NONE 读取失败

*******************************************************************************/
STATIC INT_T user_cjson_get_valueint (cJSON* root, CONST CHAR_T* key_name, INT_T max, INT_T min, INT_T dug_line)
{
  cJSON* json = NULL, *child = NULL;
  json = cJSON_GetObjectItem (root, key_name);

  if (json != NULL)
  {
    child = cJSON_GetObjectItem (json, "value");

    if (child != NULL)
    {
      if (child->valueint <= max || child->valueint >= min)
      {
        if ( cJSON_Number == child->type )
        {
          return child->valueint;
        }
        else if ( (cJSON_True == child->type)  || (cJSON_False == child->type) )
        {
          return child->type;
        }
      }

      PR_ERR ("value is illegality : %d", child->valueint);
    }
  }

  //  PR_NOTICE("%s is null, debug:%d", key_name, dug_line);
  return NONE;
}


/******************************************************************************
 * FunctionName : user_cjson_get_pin_valueint
 * Description  : 从json配置中读取灯的IO口
 * Parameters   :  *root             json
                   *key_name        字符标识符
                   *value           IO口&高低电平（value）
                   dug_line         debug 行号
 * Returns      : none
*******************************************************************************/
STATIC OPERATE_RET user_cjson_get_pin_valueint (cJSON* root, CONST CHAR_T* key_name, INT_T* value, INT_T dug_line)
{
  cJSON* json = NULL;
  INT_T data_temp = NONE;
  json = cJSON_GetObjectItem (root, key_name);

  if (json != NULL)
  {
    data_temp = user_cjson_get_valueint (json, "pin", 30, 0,
                                         dug_line);     /* max io -> 30 min io pin -> 0*/ // 为啥要这样？？？

    if (data_temp != NONE)
    {
      value[0] = data_temp;
    }
    else
    {
      data_temp = user_cjson_get_valueint (root, key_name, 30, 0, dug_line);

      if (data_temp != NONE)
      {
        value[0] = data_temp;
      }
      else
      {
        goto ERR;
      }
    }

    data_temp = user_cjson_get_valueint (json, "lv", TRUE, FALSE, dug_line);

    if (data_temp != NONE)
    {
      value[1] = data_temp;
    }

    return OPRT_OK;
  }

ERR:
  //  PR_NOTICE("%s is null, debug:%d", key_name, dug_line);
  return OPRT_COM_ERROR;
}

/******************************************************************************
 * FunctionName : user_cjson_get_valuestring
 * Description  : 从json配置中读取字符串
 * Parameters   : *root             json
                   *key_name        字符标识符
                   *buf             返回string
                   dug_line         debug 行号
 * Returns      : none
*******************************************************************************/
STATIC OPERATE_RET user_cjson_get_valuestring (cJSON* root, CONST CHAR_T* key_name, char* buf, INT_T dug_line)
{
  cJSON* json = NULL, *child = NULL;
  json = cJSON_GetObjectItem (root, key_name);

  if (json != NULL)
  {
    child = cJSON_GetObjectItem (json, "value");

    if (child != NULL)
    {
      strcpy (buf, child->valuestring);
      return OPRT_OK;
    }
  }

  //  PR_NOTICE("%s is null, debug:%d", key_name, dug_line);
  return OPRT_COM_ERROR;
}

STATIC SHORT Json_versions_analysis (char* Json_ver)
{
  char analysis[5], i;

  for (i = 0; i < 5; i++)
  {
    if (i % 2 == 0)
    {
      analysis[i] = * (Json_ver + i) - 0x30;
    }
  }

  return (analysis[0]*100 + analysis[2]*10 + analysis[4]);
}

/******************************************************************************
 * FunctionName : uesr_param_get_by_json_old
 * Description  : 配置设定(兼容旧版json)
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC OPERATE_RET uesr_param_get_by_json_old (HW_TABLE_S* hw_table, CHAR_T* cfg_json_string)
{
  OPERATE_RET opt;
  UCHAR_T i = 0;
  cJSON* root = NULL;
  PR_NOTICE ("Resolution by old way");
  root = cJSON_Parse (cfg_json_string);

  if (root == NULL)
  {
    PR_ERR ("JSON root node parse ERR!!!");
    return OPRT_COM_ERROR;
  }

  cJSON* head = NULL;
  head = cJSON_GetObjectItem (root, "light");

  if (NULL == head)
  {
    PR_ERR ("invalid param");
    return OPRT_COM_ERROR;
  }

  UCHAR_T buf[20] = {0};
  INT_T  rev_value, rev_pin_value[2] = {0};
  cJSON* unit_key = NULL;
  /* color_mode 灯光类型（1-5路灯） */
  memset (buf, 0, SIZEOF (buf) );
  opt = user_cjson_get_valuestring (head, "cmod", buf, __LINE__);

  if (OPRT_OK == opt)
  {
    for (i = 0; i < sizeof (light_color_mode) / sizeof (light_color_mode[0]);
         i ++)     /* 遍历出灯的类型（RGB、RGBCW...） */
    {
      if (strcmp (buf, light_color_mode[i]) == 0)
      {
        hw_table->light_mode = i;
        break;
      }
    }
  }

  hw_table->pwm_channel_num = color_count[hw_table->light_mode];      /* pwm通道数 */

  if (LIGHT_MODE_RGB == hw_table->light_mode )
  {
    hw_table->netconfig_color = CONFIG_COLOR_R;             /* 配网的时候，闪烁的led设定 */
  }
  else
  {
    hw_table->netconfig_color = CONFIG_COLOR_C;
  }

  /* rstcnt */
  rev_value = user_cjson_get_valueint (head, "rstcnt", 10, 3, __LINE__);      /* 设定连续开关重启的次数 */

  if (rev_value != NONE)
  {
    hw_table->reset_count = rev_value;
  }

  /* rstbr */
  rev_value = user_cjson_get_valueint (head, "rstbr", 100, 1, __LINE__);      /* 重启的默认亮度 */

  if (rev_value != NONE)
  {
    hw_table->netconfig_bright = rev_value;
  }

  unit_key = cJSON_GetObjectItem (head, "color");             /* light pin */

  if (NULL == unit_key)
  {
    PR_ERR ("invalid param");
    return OPRT_COM_ERROR;
  }

  for (i = 0; i < sizeof (color_table) / sizeof (color_table[0]); i ++)
  {
    memset (rev_pin_value, 0, SIZEOF (rev_pin_value) );
    opt = user_cjson_get_pin_valueint (unit_key, color_table[i], rev_pin_value, __LINE__);

    if (opt != OPRT_OK)
    {
      hw_table->light_pin[i].pin_num = NONE;
      PR_DEBUG ("%s pin is void", color_table[i]);
      continue;
    }

    hw_table->light_pin[i].pin_num = rev_pin_value[0];
  }

  if (root)
  {
    cJSON_Delete (root);
  }

  return OPRT_OK;
}

/******************************************************************************
 * FunctionName : uesr_param_get_by_json
 * Description  : 配置设定(json 配置的入口)
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC OPERATE_RET uesr_param_get_by_json (HW_TABLE_S* hw_table, CHAR_T* cfg_json_string)
{
  OPERATE_RET opt;
  UCHAR_T i = 0;
  cJSON* root = NULL;
  root = cJSON_Parse (cfg_json_string);

  if (root == NULL)
  {
    PR_ERR ("json Parse err!!!");
    goto ERR;
  }

  cJSON* head = NULL;
  head = cJSON_GetObjectItem (root, "light");

  if (head == NULL)
  {
    PR_ERR ("ERR!!! Can't find the head of json");
    goto ERR;
  }

  CHAR_T buf[20] = {0};
  INT_T  rev_value, rev_pin_value[2] = {0};
  cJSON* unit_key = NULL, *value_key = NULL;
  /**************************************
        1、json版本解析
  **************************************/
  opt = user_cjson_get_valuestring (head, "Jsonver", json_version, __LINE__);

  if (opt != OPRT_OK)
  {
    if (root)
    {
      cJSON_Delete (root);
    }

    /*******旧版解析方式*********/
    opt = uesr_param_get_by_json_old (hw_table, cfg_json_string);
    return opt;
    /****************************/
  }

  /**************************************
        2、灯光配置部分
  **************************************/
  unit_key = cJSON_GetObjectItem (head, "ProdFunc");
  /* color_mode */
  memset (buf, 0, SIZEOF (buf) );
  opt = user_cjson_get_valuestring (unit_key, "cmod", buf, __LINE__);

  if (opt == OPRT_OK)
  {
    for (i = 0; i < sizeof (light_color_mode) / sizeof (light_color_mode[0]); i ++)
    {
      if (strcmp (buf, light_color_mode[i]) == 0)
      {
        hw_table->light_mode = i;
        break;
      }
    }
  }

  /* wf_inif_mode */
  memset (buf, 0, SIZEOF (buf) );
  opt = user_cjson_get_valuestring (unit_key, "wfcfg", buf, __LINE__);

  if (opt == OPRT_OK)
  {
    for (i = 0; i < sizeof (wfcfg) / sizeof (wfcfg[0]); i ++)
    {
      if (strcmp (buf, wfcfg[i]) == 0)
      {
        hw_table->wf_init_cfg = i;
        break;
      }
    }
  }

  /* driver_mode  */
  rev_value = user_cjson_get_valueint (unit_key, "dmod", 2, 0, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->driver_mode = rev_value;
  }

  /* cw driver type */
  if ( (DRV_MODE_PWM_IO == hw_table->driver_mode && (LIGHT_MODE_RGBCW == hw_table->light_mode ||
                                                     LIGHT_MODE_CW == hw_table->light_mode ) ) || (DRV_MODE_IIC_SMx726 == hw_table->driver_mode  &&
                                                         LIGHT_MODE_RGBCW == hw_table->light_mode ) )
  {
    rev_value = user_cjson_get_valueint (unit_key, "cwtype", TRUE, FALSE, __LINE__);

    if (rev_value != NONE)
    {
      hw_table->cw_type = rev_value;
    }
  }

  /* bright max */
  rev_value = user_cjson_get_valueint (unit_key, "brightmax", 100, 1, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->bright_max_precent = rev_value;
  }

  /* bright min */
  rev_value = user_cjson_get_valueint (unit_key, "brightmin", 100, 1, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->bright_min_precent = rev_value;
  }

  /* color max */
  rev_value = user_cjson_get_valueint (unit_key, "colormax", 100, 1, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->color_max_precent = rev_value;
  }

  /* color min */
  rev_value = user_cjson_get_valueint (unit_key, "colormin", 100, 1, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->color_min_precent = rev_value;
  }

  /* pwm frequency */
  rev_value = user_cjson_get_valueint (unit_key, "pwmhz", 20000, 100, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->pwm_frequency = rev_value;
  }

  /* Power Memory mode */
  rev_value = user_cjson_get_valueint (unit_key, "pmemory", TRUE, FALSE, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->dev_memory = rev_value;
  }

  /* Power onoff mode */
  rev_value = user_cjson_get_valueint (unit_key, "onoffmode", TRUE, FALSE, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->power_onoff_mode = rev_value;
  }

  /* cw max power */
  rev_value = user_cjson_get_valueint (unit_key, "cwmaxp", 200, 100, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->cw_max_power = rev_value;
  }

  /* cw rgb ctrl mutex */
  rev_value = user_cjson_get_valueint (unit_key, "fk", TRUE, FALSE, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->cw_rgb_mutex = rev_value;
  }

  /*****************************************
        3、驱动配置部分
  *****************************************/

  if ( (hw_table->driver_mode == DRV_MODE_PWM_IO) || ( (hw_table->driver_mode == DRV_MODE_IIC_SMx726) &&
                                                       ( (hw_table->light_mode == LIGHT_MODE_RGBC) || (hw_table->light_mode == LIGHT_MODE_RGBCW) ) ) )
  {
    hw_table->pwm_channel_num = color_count[hw_table->light_mode];

    if (hw_table->driver_mode == DRV_MODE_IIC_SMx726)
    {
      hw_table->pwm_channel_num = hw_table->pwm_channel_num - 3;
    }

    /* color */
    unit_key = cJSON_GetObjectItem (head, "color");

    if (NULL != unit_key)
    {
      for (i = 0; i < sizeof (color_table) / sizeof (color_table[0]); i ++)
      {
        if ( (hw_table->driver_mode == DRV_MODE_IIC_SMx726) && (i < 3) )
        {
          hw_table->light_pin[i].pin_num = NONE;
          continue;
        }

        memset (rev_pin_value, 0, SIZEOF (rev_pin_value) );
        opt = user_cjson_get_pin_valueint (unit_key, color_table[i], rev_pin_value, __LINE__);

        if (opt != OPRT_OK)
        {
          hw_table->light_pin[i].pin_num = NONE;
          continue;
        }

        hw_table->light_pin[i].pin_num = rev_pin_value[0];
        hw_table->light_pin[i].level_sta = rev_pin_value[1];
      }
    }
    else
    {
      PR_DEBUG ("Json color is void");
    }
  }

  if (hw_table->driver_mode == DRV_MODE_IIC_SMx726 || hw_table->driver_mode == DRV_MODE_IIC_SM2135)
  {
    //如果正确读取json就按照需求配置
    unit_key = cJSON_GetObjectItem (head, "iic");

    if (NULL != unit_key)
    {
      memset (rev_pin_value, 0, SIZEOF (rev_pin_value) );

      if (OPRT_OK == user_cjson_get_pin_valueint (unit_key, "ctrl", rev_pin_value, __LINE__) )
      {
        hw_table->lowpower_pin = rev_pin_value[0];
      }

      //IIC singal pin
      memset (rev_pin_value, 0, SIZEOF (rev_pin_value) );

      if (OPRT_OK == user_cjson_get_pin_valueint (unit_key, "iicscl", rev_pin_value, __LINE__) )
      {
        hw_table->i2c_cfg.scl_pin = rev_pin_value[0];
      }

      //IIC data pin
      memset (rev_pin_value, 0, SIZEOF (rev_pin_value) );

      if (OPRT_OK == user_cjson_get_pin_valueint (unit_key, "iicsda", rev_pin_value, __LINE__) )
      {
        hw_table->i2c_cfg.sda_pin = rev_pin_value[0];
      }

      //IIC out pin
      for (i = 0; i < sizeof (iic_cfg_table) / sizeof (iic_cfg_table[0]); i ++)
      {
        memset (rev_pin_value, 0, SIZEOF (rev_pin_value) );
        opt = user_cjson_get_pin_valueint (unit_key, iic_cfg_table[i], rev_pin_value, __LINE__);

        if (opt != OPRT_OK)
        {
          hw_table->i2c_cfg.out_pin_num[i] = NONE;
          continue;
        }

        hw_table->i2c_cfg.out_pin_num[i] = rev_pin_value[0];
      }

      if (hw_table->driver_mode == DRV_MODE_IIC_SM2135)  //补全缺省
      {
        switch (hw_table->light_mode)
        {
          case LIGHT_MODE_RGBC:
            if (hw_table->i2c_cfg.out_pin_num[3] == 3)
            {
              hw_table->i2c_cfg.out_pin_num[4] = 4;
            }
            else if (hw_table->i2c_cfg.out_pin_num[3] == 4)
            {
              hw_table->i2c_cfg.out_pin_num[4] = 3;
            }

            break;

          case LIGHT_MODE_RGB:
            hw_table->i2c_cfg.out_pin_num[3] = 3;
            hw_table->i2c_cfg.out_pin_num[4] = 4;
            break;

          case LIGHT_MODE_CW:
          case LIGHT_MODE_C:
            hw_table->i2c_cfg.out_pin_num[0] = 0;
            hw_table->i2c_cfg.out_pin_num[1] = 1;
            hw_table->i2c_cfg.out_pin_num[2] = 2;

            if (hw_table->light_mode == LIGHT_MODE_C)
            {
              if (hw_table->i2c_cfg.out_pin_num[3] == 3)
              {
                hw_table->i2c_cfg.out_pin_num[4] = 4;
              }
              else if (hw_table->i2c_cfg.out_pin_num[3] == 4)
              {
                hw_table->i2c_cfg.out_pin_num[4] = 3;
              }
            }

            break;

          default:
            break;
        }

        //output current of white light
        rev_value = user_cjson_get_valueint (unit_key, "wampere", 60, 10, __LINE__);

        if (rev_value != NONE)
        {
          hw_table->i2c_cfg.sm2135_cw_current = rev_value;
        }

        //output current of color light
        rev_value = user_cjson_get_valueint (unit_key, "campere", 45, 10, __LINE__);

        if (rev_value != NONE)
        {
          hw_table->i2c_cfg.sm2135_rgb_current = rev_value;
        }
      }
    }
    else
    {
      PR_DEBUG ("Json iic is null");
    }
  }

  //低功耗控制脚json兼容，现在忽略驱动模式可配置
  //外部优先级大于iic内部
  memset (rev_pin_value, 0, SIZEOF (rev_pin_value) );

  if (OPRT_OK == user_cjson_get_pin_valueint (head, "ctrl", rev_pin_value, __LINE__) )
  {
    hw_table->lowpower_pin = rev_pin_value[0];
  }

  /*****************************************
        4、按键配置部分
  *****************************************/
  /* light key */
  memset (rev_pin_value, 0, SIZEOF (rev_pin_value) );

  if (OPRT_OK == user_cjson_get_pin_valueint (head, "lkey", rev_pin_value, __LINE__) )
  {
    hw_table->key.id = rev_pin_value[0];
    hw_table->key.level = rev_pin_value[1];
    //兼容旧版 -- 2018.12.17
    unit_key = cJSON_GetObjectItem (head, "lkey");
    rev_value = user_cjson_get_valueint (unit_key, "time", 20, 3, __LINE__);

    if (rev_value != NONE)
    {
      hw_table->key.times = rev_value;
    }
  }

  //pin与lv参数在平台上绑定死，新版单独将time取出单独定义 -- 2018.12.17
  rev_value = user_cjson_get_valueint (head, "ktime", 20, 3, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->key.times = rev_value;
  }

  rev_value = user_cjson_get_valueint (head, "kfl", 255, 0, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->key.long_key_fun = rev_value;
  }

  rev_value = user_cjson_get_valueint (head, "kfn", 255, 0, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->key.normal_key_fun = rev_value;
  }

  /*****************************************
        5、配网部分
  *****************************************/
  unit_key = cJSON_GetObjectItem (head, "connection"); //other部分上传后默认归类到json根目录("light")下
  /* light reset mode*/
  rev_value = user_cjson_get_valueint (unit_key, "rstmode", 2, 0, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->reset_dev_mode = rev_value;
  }

  /* rstbr */
  rev_value = user_cjson_get_valueint (unit_key, "rstbr", 100, 1, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->netconfig_bright = rev_value;
  }

  /* rstcor */
  if (hw_table->light_mode == LIGHT_MODE_RGB)
  {
    hw_table->netconfig_color = CONFIG_COLOR_R;
  }
  else
  {
    hw_table->netconfig_color = CONFIG_COLOR_C;
  }

  memset (buf, 0, SIZEOF (buf) );
  opt = user_cjson_get_valuestring (unit_key, "rstcor", buf, __LINE__);

  if (opt == OPRT_OK)
  {
    for (i = 0; i < sizeof (color_table) / sizeof (color_table[0]); i ++)
    {
      if (strcmp (buf, color_table[i]) == 0)
      {
        hw_table->netconfig_color = i;
        break;
      }
    }
  }

  /* defcolor */
  if (hw_table->light_mode == LIGHT_MODE_RGB)
  {
    hw_table->def_light_color = CONFIG_COLOR_R;
  }
  else
  {
    hw_table->def_light_color = CONFIG_COLOR_C;
  }

  memset (buf, 0, SIZEOF (buf) );
  opt = user_cjson_get_valuestring (unit_key, "defcolor", buf, __LINE__);

  if (opt == OPRT_OK)
  {
    for (i = 0; i < sizeof (color_table) / sizeof (color_table[0]); i ++)
    {
      if (strcmp (buf, color_table[i]) == 0)
      {
        break;
      }
    }

    hw_table->def_light_color = i;

    if (hw_table->light_mode == LIGHT_MODE_RGBC)
    {
      if (hw_table->def_light_color == CONFIG_COLOR_W)
      {
        hw_table->def_light_color = CONFIG_COLOR_C;
      }
    }
    else if (hw_table->light_mode == LIGHT_MODE_RGB)
    {
      if (hw_table->def_light_color == CONFIG_COLOR_W || hw_table->def_light_color == CONFIG_COLOR_C)
      {
        hw_table->def_light_color = CONFIG_COLOR_R;
      }
    }
    else if (hw_table->light_mode == LIGHT_MODE_CW)
    {
      if (hw_table->def_light_color != CONFIG_COLOR_W && hw_table->def_light_color != CONFIG_COLOR_C)
      {
        hw_table->def_light_color = CONFIG_COLOR_C;
      }
    }
    else if (hw_table->light_mode == LIGHT_MODE_C)
    {
      hw_table->def_light_color = CONFIG_COLOR_C;
    }

    //暖灯
    if (hw_table->def_light_color == 4)
    {
      hw_table->def_temp_precent = 0;
    }
  }

  /* default bright */
  rev_value = user_cjson_get_valueint (unit_key, "defbright", 100, 1, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->def_bright_precent = rev_value;
  }

  /* default temperture */
  rev_value = user_cjson_get_valueint (unit_key, "deftemp", 100, 1, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->def_temp_precent = rev_value;
  }

  /* rstcnt */
#if _IS_OEM
  rev_value = user_cjson_get_valueint (head, "rstnum", 20, 3, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->reset_count = rev_value;
  }

#else
  unit_key = cJSON_GetObjectItem (head, "other");
  rev_value = user_cjson_get_valueint (unit_key, "rstnum", 20, 3, __LINE__);

  if (rev_value != NONE)
  {
    hw_table->reset_count = rev_value;
  }

#endif

  if (root)
  {
    cJSON_Delete (root);
  }

  return OPRT_OK;
ERR:

  if (root)
  {
    cJSON_Delete (root);
  }

  return OPRT_COM_ERROR;
}


/******************************************************************************
 * FunctionName : hw_config_def_printf
 * Description  : 打印json配置的信息
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
VOID hw_config_def_printf (VOID)
{
  HW_TABLE_S* hw_table = get_hw_table();
  UCHAR i;
  char* light_driver_mode[3] = {"pwm", "i2c-sm726", "i2c-sm2135"};
  char* cw_driver_type[2] = {"cw", "cct"};
  char* pin_level_table[2] = {"low", "high"};
  char* onoff_mode[2] = {"shade", "direct"};
  PR_NOTICE ("== light config parms ==");
  PR_NOTICE ("Json versions : %s", json_version);
  UCHAR version = Json_versions_analysis (json_version);
  PR_DEBUG ("version = %d", version);

  if (version == 0)   //旧版本
  {
    PR_NOTICE ("color_mode:%s", light_color_mode[hw_table->light_mode]);
    PR_NOTICE ("driver_mode:%s", light_driver_mode[hw_table->driver_mode]);
    PR_NOTICE ("pwm_channel_num:%d", hw_table->pwm_channel_num);
    //
    PR_NOTICE ("reset_count:%d", hw_table->reset_count);
    PR_NOTICE ("netconfig_bright:%d%%", hw_table->netconfig_bright);
    PR_NOTICE ("netconfig_color:%s", color_table[hw_table->netconfig_color]);

    //
    for (i = 0; i < 5; i ++)
    {
      if (hw_table->light_pin[i].pin_num != NONE)
      {
        PR_NOTICE ("%s %d", color_table[i], hw_table->light_pin[i].pin_num);
      }
    }
  }
  else     //新版本
  {
    //灯光配置部分
    //  PR_NOTICE("module type = %s",module_type[hw_table->module_type]);
    PR_NOTICE ("wf inif mode = %s", wfcfg[hw_table->wf_init_cfg - WF_CFG_SPCL]);
    PR_NOTICE ("light type = %s", light_color_mode[hw_table->light_mode]);
    PR_NOTICE ("app onoff mode = %s", onoff_mode[hw_table->power_onoff_mode]);
    PR_NOTICE ("light driver mode = %s", light_driver_mode[hw_table->driver_mode]);
    PR_NOTICE ("cw driver type = %s", cw_driver_type[hw_table->cw_type]);
    PR_NOTICE ("light pwm frequency = %dHz", hw_table->pwm_frequency);
    PR_NOTICE ("white light max power = %d%%", hw_table->cw_max_power);
    PR_NOTICE ("white light max bright = %d%%", hw_table->bright_max_precent);
    PR_NOTICE ("white light min bright = %d%%", hw_table->bright_min_precent);
    PR_NOTICE ("color light max bright = %d%%", hw_table->color_max_precent);
    PR_NOTICE ("color light min bright = %d%%", hw_table->color_min_precent);
    PR_NOTICE ("power memory = %d", hw_table->dev_memory);
    PR_NOTICE ("white color ctrl mutex = %d", hw_table->cw_rgb_mutex);

    //通道配置部分

    if (hw_table->lowpower_pin != NONE)
    {
      PR_NOTICE ("color power ctrl pin = %d", hw_table->lowpower_pin);
    }

    PR_NOTICE ("pwm_channel_num : %d", hw_table->pwm_channel_num);

    if (hw_table->driver_mode == DRV_MODE_PWM_IO || hw_table->driver_mode == DRV_MODE_IIC_SMx726)
    {
      for (i = 0; i < 5; i ++)
      {
        if (hw_table->light_pin[i].pin_num != NONE)
        {
          PR_NOTICE ("pwm pin[%s] = %d\tlevel = %s", color_table[i], hw_table->light_pin[i].pin_num,
                     pin_level_table[hw_table->light_pin[i].level_sta]);
        }
      }
    }

    if (hw_table->driver_mode == DRV_MODE_IIC_SM2135 || hw_table->driver_mode == DRV_MODE_IIC_SMx726)
    {
      PR_NOTICE ("iic scl pin = %d", hw_table->i2c_cfg.scl_pin);
      PR_NOTICE ("iic sda pin = %d", hw_table->i2c_cfg.sda_pin);

      for (i = 0; i < 5; i ++)
      {
        if (hw_table->i2c_cfg.out_pin_num[i] != NONE)
        {
          PR_DEBUG ("iic %s out pin = %d", color_table[i], hw_table->i2c_cfg.out_pin_num[i]);
        }
      }

      if (hw_table->driver_mode == DRV_MODE_IIC_SM2135)
      {
        PR_NOTICE ("sm2135 output current of white light = %dmA", hw_table->i2c_cfg.sm2135_cw_current);
        PR_NOTICE ("sm2135 output current of color light = %dmA", hw_table->i2c_cfg.sm2135_rgb_current);
      }
    }

    //按键配置部分
    if (hw_table->key.id != NONE)
    {
      PR_NOTICE ("light key pin = %d", hw_table->key.id);
      PR_NOTICE ("light key level = %s", pin_level_table[hw_table->key.level]);
      PR_NOTICE ("time of long key = %ds", hw_table->key.times);
      PR_NOTICE ("function of long key = %x", hw_table->key.long_key_fun);
      PR_NOTICE ("function of normal key = %x", hw_table->key.normal_key_fun);
    }

    //配网部分
    PR_NOTICE ("reset wifi mode = %d", hw_table->reset_dev_mode);
    PR_NOTICE ("reset count = %d", hw_table->reset_count);
    PR_NOTICE ("net config light color = %s", color_table[hw_table->netconfig_color]);
    PR_NOTICE ("net config light bright = %d%%", hw_table->netconfig_bright);
    PR_NOTICE ("default color = %s", color_table[hw_table->def_light_color]);
    PR_NOTICE ("default bright = %d%%", hw_table->def_bright_precent);
    PR_NOTICE ("default temperture = %d%%", hw_table->def_temp_precent);
  }
}

STATIC LIGHT_MODE_E __light_usr_mode_get()
{
  //    PR_DEBUG("PRE_DEFINE_LIGHT_TYPE = %d",PRE_DEFINE_LIGHT_TYPE);
  switch (USER_DEFINE_LIGHT_TYPE)
  {
    case USER_LIGTH_MODE_C:
      return LIGHT_MODE_C;

    case USER_LIGTH_MODE_CW:
      return LIGHT_MODE_CW;

    case USER_LIGTH_MODE_RGB:
      return LIGHT_MODE_RGB;

    case USER_LIGTH_MODE_RGBC:
    case USER_LIGTH_MODE_RGBC_FAKE:
    case USER_LIGTH_MODE_RGBW_FAKE:
      return LIGHT_MODE_RGBC;

    case USER_LIGTH_MODE_RGBCW:
    case USER_LIGTH_MODE_RGBCW_FAKEC:
    case USER_LIGTH_MODE_RGBCW_FAKEW:
    case USER_LIGTH_MODE_RGBCW_FAKEFC:
    case USER_LIGTH_MODE_RGBCW_FAKEFW:
    case USER_LIGTH_MODE_RGBCW_FAKE_CUS:
      return LIGHT_MODE_RGBCW;

    default:
      return LIGHT_MODE_RGBC;
  }
}



/******************************************************************************
 * FunctionName : hw_config_def_sets
 * Description  : default 配置设定
                    默认设定为5路灯,
                    CW
                    pwm驱动
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
STATIC VOID hw_config_def_set (VOID)
{
  //功能配置
  g_hw_table.module_type = MODULE_TYPE_03;
  g_hw_table.wf_init_cfg = USER_DEFINE_WIFI_POWER_MODE;
  g_hw_table.light_mode = __light_usr_mode_get() ;
  PR_DEBUG ("light_mode = %d", g_hw_table.light_mode);
  g_hw_table.power_onoff_mode = USER_DEFINE_POWER_ON_OFF_MODE;
  //
  g_hw_table.driver_mode = USER_DEFINE_LIGHT_DRIVER_MODE;
  g_hw_table.cw_type = USER_DEFINE_LIGHT_CW_TYPE;
  //
  g_hw_table.pwm_frequency = 20000;
  g_hw_table.cw_max_power  = 100;
  //
  g_hw_table.bright_max_precent = 100;
  g_hw_table.bright_min_precent = 10;
  g_hw_table.color_max_precent = 100;
  g_hw_table.color_min_precent = 10;
  //
  g_hw_table.dev_memory = TRUE;
#ifdef USER_DEFINE_CW_RGB_MUTEX  
  g_hw_table.cw_rgb_mutex = USER_DEFINE_CW_RGB_MUTEX;
#else
  g_hw_table.cw_rgb_mutex = TRUE;
#endif
  //配网配置
  g_hw_table.reset_dev_mode = DRV_RESET_BY_ANYWAY;
  g_hw_table.reset_count = USER_DEFINE_RESET_COUNT;
  //
  g_hw_table.netconfig_color = USER_DEFINE_NETCONFIG_COLOR;
  g_hw_table.netconfig_bright = 80;
  //
  g_hw_table.def_light_color = USER_DEFINE_DEFAULT_LIGHT_COLOR;
  g_hw_table.def_bright_precent = 100;
  g_hw_table.def_temp_precent = 100;

  if (g_hw_table.def_light_color == CONFIG_COLOR_W)
  {
    g_hw_table.def_temp_precent = 0;
  }

  //通道配置
  g_hw_table.pwm_channel_num = color_count[g_hw_table.light_mode];
  g_hw_table.light_pin[0].pin_num = 0xFF;   //R
  g_hw_table.light_pin[0].level_sta = HIGH_LEVEL;
  g_hw_table.light_pin[1].pin_num = 0xFF;   //G
  g_hw_table.light_pin[1].level_sta = HIGH_LEVEL;
  g_hw_table.light_pin[2].pin_num = 0xFF;   //B
  g_hw_table.light_pin[2].level_sta = HIGH_LEVEL;
  g_hw_table.light_pin[3].pin_num = 0xFF;   //C
  g_hw_table.light_pin[3].level_sta = HIGH_LEVEL;
  g_hw_table.light_pin[4].pin_num = 0xFF;   //W
  g_hw_table.light_pin[4].level_sta = HIGH_LEVEL;

  switch(g_hw_table.light_mode)
  {
    case LIGHT_MODE_RGBCW:
      g_hw_table.light_pin[4].pin_num = PWM_4_OUT_IO_NUM;   //W
      PR_DEBUG ("LIGHT_MODE_RGBCW config W pin:%d", g_hw_table.light_pin[4].pin_num);
    case LIGHT_MODE_RGBC:
      g_hw_table.light_pin[3].pin_num = PWM_3_OUT_IO_NUM;   //C
      PR_DEBUG ("LIGHT_MODE_RGBC config C pin:%d", g_hw_table.light_pin[3].pin_num);
    case LIGHT_MODE_RGB:
      g_hw_table.light_pin[0].pin_num = PWM_0_OUT_IO_NUM;   //R
      g_hw_table.light_pin[1].pin_num = PWM_1_OUT_IO_NUM;   //G
      g_hw_table.light_pin[2].pin_num = PWM_2_OUT_IO_NUM;   //B
      PR_DEBUG("LIGHT_MODE_RGB config r pin:%d", g_hw_table.light_pin[0].pin_num);
      PR_DEBUG("LIGHT_MODE_RGB config g pin:%d", g_hw_table.light_pin[1].pin_num);
      PR_DEBUG("LIGHT_MODE_RGB config b pin:%d", g_hw_table.light_pin[2].pin_num);
      break;
     case LIGHT_MODE_CW:
       g_hw_table.light_pin[4].pin_num = PWM_4_OUT_IO_NUM;   //W
       PR_DEBUG ("LIGHT_MODE_CW config W pin:%d", g_hw_table.light_pin[4].pin_num);
     case LIGHT_MODE_C:
      g_hw_table.light_pin[3].pin_num = PWM_3_OUT_IO_NUM;   //C
      PR_DEBUG("LIGHT_MODE_C config C pin:%d", g_hw_table.light_pin[3].pin_num);
      break;
     default:
      break;
  }
  
  //IIC配置
  g_hw_table.lowpower_pin = NONE;
  //按键配置
  g_hw_table.key.level = LOW_LEVEL;
  g_hw_table.key.id = KEY_BUTTON;
  g_hw_table.key.times = USER_DEFINE_KEY_RESET_TIMES;       //5S
  g_hw_table.key.long_key_fun = USER_DEFINE_LONG_KEY_FUNCTION;
  g_hw_table.key.normal_key_fun = USER_DEFINE_NORMAL_KEY_FUNCTION;
}

/******************************************************************************
 * FunctionName : json_config_init
 * Description  : json配置
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
OPERATE_RET tuya_light_config_param_get (VOID)
{
  //默认参数
  hw_config_def_set();
#if 0//不使用json配置可不编译此段
  OPERATE_RET op_ret;
  UINT len = 0;
  CHAR* pConfig = NULL;
#if _IS_OEM
  PR_DEBUG ("INIT JSON CONFIG FROM FLASH");
  op_ret = tuya_ws_db_user_param_read (&pConfig, &len);
  PR_DEBUG ("get user param over");

  if (OPRT_OK != op_ret)
  {
    PR_ERR ("ws_db_get_user_param err:%d", op_ret);
    return op_ret;
  }

  len = strlen (pConfig);
  PR_DEBUG ("config len:%d, config data:%s", len, pConfig);

  if (uesr_param_get_by_json (&g_hw_table, pConfig) == OPRT_COM_ERROR)
  {
    PR_ERR ("hw_set_by_json error.");
    Free (pConfig);
    return OPRT_INVALID_PARM;
  }

#else
  PR_DEBUG ("INIT JSON CONFIG FROM FIRMWARE");
  len = strlen (CFG_JS);
  PR_DEBUG ("config len:%d, config data:%s", len, CFG_JS);

  if (uesr_param_get_by_json (&g_hw_table, CFG_JS) == OPRT_COM_ERROR)
  {
    PR_ERR ("hw_set_by_json error.");
    Free (pConfig);
    return OPRT_INVALID_PARM;
  }

#endif
  Free (pConfig);
#endif
  return OPRT_OK;
}


HW_TABLE_S* get_hw_table (VOID)
{
  return &g_hw_table;
}

KEY_FUNCTION_E tuya_light_long_key_fun_get (VOID)
{
  return g_hw_table.key.long_key_fun;
}

KEY_FUNCTION_E tuya_light_normal_key_fun_get (VOID)
{
  return g_hw_table.key.normal_key_fun;
}


