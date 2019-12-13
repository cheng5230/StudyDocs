/***********************************************************
*  File: device.c
*  Author: nzy
*  Date: 20150605
***********************************************************/
#define __DEVICE_GLOBALS
#include ".compile_usr_cfg.h"
#include "tuya_device.h"
#include "tuya_light_lib.h"
#include "tuya_common_light.h"
#include "tuya_hw_table.h"
#include "irDecode.h"
#include "uf_file.h"
#include "led_display.h"
#include "gpio_api.h"    
#include "gpio_irq_api.h"   
#include "gpio_irq_api.h" 
#include "ws2812b.h" 


/***********************************************************
*************************micro define***********************
***********************************************************/

#define REG_WRITE(_r,_v)    (*(volatile uint32 *)(_r)) = (_v)
#define REG_READ(_r)        (*(volatile uint32 *)(_r))
#define WDEV_NOW()          REG_READ(0x3ff20c00)
gpio_irq_t gpio_uart;

#define RD_UART gpio_read((gpio_t *)(&gpio_uart)) //GPIO_ReadDataBit(gpio_uart.pin)    

/***********************************************************
*************************function define********************
***********************************************************/
extern  VOID start_test (VOID);
STATIC VOID __userIrCmdDeal (IRCMD cmd, IRCODE irType);

OPERATE_RET set_reset_cnt (INT_T val);

LED_HANDLE wf_light = NULL;

STATIC BOOL recv_debug_flag = FALSE;

VOID tuya_light_pre_device_init(VOID)
{
#if USER_DEBUG_APP
  SetLogManageAttr (LOG_LEVEL_DEBUG);
  PR_DEBUG ("USER SET LOG_LEVEL_DEBUG \n");
#else
  SetLogManageAttr (LOG_LEVEL_NOTICE);
  PR_NOTICE ("USER SET LOG_LEVEL_NOTICE \n");
#endif
#if USER_DEFINE_LOWER_POWER
  tuya_set_lp_mode (TRUE);
#endif
  tuya_light_config_param_get();
  //sys_log_uart_on();
  tuya_light_init();  //light硬件相关初始
  //sys_log_uart_off();
}

#if _IS_OEM
static CHAR_T __product_key[PRODUCT_KEY_LEN+4];
STATIC OPERATE_RET __read_product_key_fast( )
{
  OPERATE_RET op_ret;
  //op_ret = tuya_light_common_flash_read(LIGHT_DATA_KEY, &buf);
  uFILE* file = ufopen (PRODUCT_KEY_DATA_KEY, "r");

  if (NULL == file)
  {
    __product_key[0]='\0';
    PR_ERR ("ufopen failed");
    return OPRT_COM_ERROR;
  }

  INT_T read_len =  ufread (file, __product_key, PRODUCT_KEY_LEN);
  ufclose (file);
  file = NULL;

  if (PRODUCT_KEY_LEN != read_len)
  {
    PR_ERR ("ufread failed read_len%d!=PRODUCT_KEY_LEN", read_len);
    __product_key[0]='\0';
    return OPRT_COM_ERROR;
  }

  __product_key[PRODUCT_KEY_LEN]='\0';
  PR_NOTICE ("read flash: product_key[%s]", __product_key);
  return OPRT_OK;
}
#endif

static void __product_key_init()
{
#if _IS_OEM
  memset (__product_key, 0x0, PRODUCT_KEY_LEN+4);

  if (__read_product_key_fast( ) != OPRT_OK)
  {
    __product_key[0]='\0';
  }

  if (__product_key[0] != '\0')
  {
    PR_NOTICE ("====Got PID from flash[%s]====\r\n", __product_key);
    return;
  }

  memcpy (__product_key, PRODECT_KEY, PRODUCT_KEY_LEN);
  __product_key[PRODUCT_KEY_LEN] = '\0';
  PR_NOTICE ("====doesn't got PID from flash, use pre define Key[%s]====\r\n", __product_key);
  tuya_light_oem_set (TRUE);
#endif
}


VOID tuya_light_app_init (VOID)
{
  OPERATE_RET op_ret;
  tuya_light_lib_app_init( );
  sys_log_uart_on();
  //sys_log_uart_off();
  PR_NOTICE ("%s:s:%s:%s", APP_BIN_NAME, USER_SW_VER, __DATE__, __TIME__);
  PR_NOTICE ("%s", tuya_iot_get_sdk_info());
#if USER_DEBUG_APP
  hw_config_def_printf();
#endif
  __product_key_init();
  light_prod_init (PROD_TEST_TYPE_V2, prod_test);
}

STATIC OPERATE_RET device_differ_init (VOID)
{
  OPERATE_RET op_ret;
  op_ret = tuya_light_key_init (tuya_light_key_process);

  if (op_ret != OPRT_OK)
  {
    PR_ERR ("tuya_light_key_init ERR:%d", op_ret);
  }

  light_strip_infrared_init (__userIrCmdDeal);
  return OPRT_OK;
}

extern OPERATE_RET spi_test();
extern void spi_DmaInit(void);

void bit_delay (void)  //8.7us
{
  volatile int time;
  time = 120;
  while (time--);
}

unsigned char rx_data_buf[512] = {0};


//接收函数
void rxbyte (void)
{
  int  i;
  unsigned char readbuffer;
  static int j = 0;

  if (RD_UART==0)
  {
    DelayNop(1000);

    if (RD_UART==0)
    {      
      readbuffer=0;
      DelayNop(2000);

      for (i=0; i<8; i++)
      {
        readbuffer>>=1;

        if (RD_UART==1)
        {
          readbuffer|=0x80;
        }
        else if (RD_UART==0)
        {
          ;
        }
        DelayNop(2540);
      }
      uart_receive_input(readbuffer);
      #if 0
      if(j < 499){
        rx_data_buf[j] = readbuffer;    
        j++;
      }
      else{
         j = 0;
         for(i = 0;i < 500;i++){
           PR_NOTICE("buf[%d] =  %d\r\n",i,rx_data_buf[i]);
         }
      }
      #endif
      
    }
    else
    {
    }
  }
}


void gpio_uart_irq_handler(){

  //PR_NOTICE("get in irq_handler step 1\n");
  gpio_irq_disable(&gpio_uart);
  //PR_NOTICE("get in irq_handler step 2\n");
  rxbyte();
  gpio_irq_enable(&gpio_uart);
}

gpio_t gpio_led;

void gpio_uart_mode_init(){

  gpio_irq_init(&gpio_uart, PA_12, gpio_uart_irq_handler, NULL);
  gpio_irq_set(&gpio_uart, IRQ_FALL, 1);   // Falling Edge Trigger
  gpio_irq_enable(&gpio_uart); 
  static int num = 0;

#if 0
 gpio_init(&gpio_led, PA_12);
 gpio_dir(&gpio_led, PIN_OUTPUT);    // Direction: Output
 gpio_mode(&gpio_led, PullNone);     // No pull

 while(1){
  
  if(num){
      gpio_write(&gpio_led,0);
      num = 0;
  }
  else{
      gpio_write(&gpio_led,1);
      num = 1;
  }
  
  DelayNop(1000);      //41US  DelayNop(2536)
 }
#endif
  
  PR_NOTICE("gpio_uart_mode_init success\n");
}

void uart_server_init(){
  OPERATE_RET op_ret = OPRT_OK; 

  wifi_protocol_init();

  #if 0
  THRD_HANDLE thread_uart_server;
  op_ret = tuya_light_CreateAndStart(&thread_uart_server, _uart_server_thread, NULL,1024,TRD_PRIO_2,"uart_server_task");
  if(op_ret != OPRT_OK) {
    PR_ERR("light_gpio_init err:%d",op_ret);
    return op_ret;
  }
  PR_NOTICE("\n tuya_light_CreateAndStart %d \n", op_ret);
  #endif
}

VOID tuya_light_device_init (VOID)
{
  tuya_light_print_port_init();
#if _IS_OEM
  tuya_light_smart_frame_init (__product_key, USER_SW_VER);
#else
  tuya_light_smart_frame_init (USER_SW_VER);
#endif
  tuya_light_start();                            /*必须在云服务启动后调*/
  device_differ_init();

  //display_init(DISPLAY_BUF_MAX_PIXEL);
  ///spi_test();
  spi_DmaInit();
  gpio_uart_mode_init();
  uart_server_init();
  
  INT_T size = SysGetHeapSize();
  PR_DEBUG("init ok  free_mem_size:%d", size);
  return ;
}
VOID mf_user_callback (VOID)
{
}

/*****************************************
    按键控制部分
*****************************************/
VOID normal_key_function_cb (VOID)
{
  KEY_FUNCTION_E fun = tuya_light_normal_key_fun_get();
  
#if (USER_DEFINE_LIGHT_TYPE == USER_LIGTH_MODE_C || USER_DEFINE_LIGHT_TYPE == USER_LIGTH_MODE_CW)
  fun = KEY_FUN_POWER_CTRL;
#endif

  TUYA_GW_WIFI_NW_STAT_E wf_stat = tuya_light_get_wf_gw_status(); //处于配网状态下，功能暂时屏
  if (wf_stat == STAT_UNPROVISION || wf_stat == STAT_AP_STA_UNCFG)
  {
    return;
  }

  PR_DEBUG("normal key fun = [%x]", fun);
  light_IrCrtl.IR_Mode = 0;
  
  switch (fun)
  {
    case KEY_FUN_POWER_CTRL :
      light_key_fun_power_onoff_ctrl();
      break;

    case KEY_FUN_RAINBOW_CHANGE :
      light_key_fun_7hue_cyclic();
      break;

    case KEY_FUN_SCENE_CHANGE :
      light_key_fun_scene_change();
      break;

    case KEY_FUN_POWER_RAINBOW_SCENE :
      light_key_fun_combo_change();
      break;

    default:
      break;
  }
}

VOID long_key_function_cb (VOID)
{
  KEY_FUNCTION_E fun = tuya_light_long_key_fun_get();

  if (KEY_FUN_RESET_WIFI != fun)
  {
    TUYA_GW_WIFI_NW_STAT_E wf_stat = tuya_light_get_wf_gw_status();

    if (wf_stat == STAT_UNPROVISION || wf_stat == STAT_AP_STA_UNCFG)
    {
      return;
    }
  }

  PR_DEBUG ("long key fun = [%x]", fun);

  switch (fun)
  {
    case KEY_FUN_RESET_WIFI :
      light_key_fun_wifi_reset();
      break;

    default:
      break;
  }
}


VOID seq_key_function_cb (UCHAR cnt)
{
  STATIC UCHAR key_flag = 0;

  if (cnt == 15)
  {
    recv_debug_flag = 1 - recv_debug_flag;
  }

  if (cnt == 10)
  {
    key_flag = 1 - key_flag;

    if (key_flag)
    {
      light_key_fun_free_heap_check_start();
    }
    else
    {
      light_key_fun_free_heap_check_stop();
    }
  }
}


VOID tuya_light_key_process (INT_T gpio_no, PUSH_KEY_TYPE_E type, INT_T cnt)
{
  PR_DEBUG ("gpio_no: %d", gpio_no);
  PR_DEBUG ("type: %d", type);
  PR_DEBUG ("cnt: %d", cnt);
  UCHAR_T temp[16];

  if ( (INT) light_key_id_get() == gpio_no)
  {
    if (LONG_KEY == type)
    {
      long_key_function_cb();
    }
    else if (NORMAL_KEY == type)
    {
      normal_key_function_cb();
    }
    else if (SEQ_KEY == type)
    {
      //  seq_key_function_cb(cnt);
    }
  }
}

STATIC VOID __userIrCmdDeal (IRCMD cmd, IRCODE irType)
{
  STATIC IRCMD_new last_cmd = -1;
  static uint8_t I = 0;

  // uint8_t index;
  if (irType == IRCODESTART || irType == IRCODEREPEAT)
  {
    if (irType == IRCODESTART)
    {
      last_cmd = cmd;
      PR_DEBUG ("NEW CMD:%x\r\n", last_cmd);
      I = 0;
    }
    else
    {
      I++;
      PR_DEBUG ("REPEAT%d CMD:%x\r\n", I, last_cmd);

      switch (last_cmd)
      {
        case KEY_POWER_ON:
          if (I >= 48)
          {
            PR_DEBUG ("===>ir remote KEY_POWER_ON long press\r\n");
            long_key_function_cb();
            //wifi reset.
          }

          return;

        case KEY_BRIGHTNESS_UP: //0x00,
        case KEY_BRIGHTNESS_DOWN: //0x80,

        // break;
        case -1:
          I = 0;

        default:
          return;
      }
    }
  }

  TUYA_GW_WIFI_NW_STAT_E wf_stat = tuya_light_get_wf_gw_status();

  if ( (wf_stat == STAT_UNPROVISION) || (wf_stat == STAT_AP_STA_UNCFG))
  {
    return;
  }
   
  if(light_IrCrtl.IR_Mode)
  {
    if(light_IrCrtl.IR_Flag==0)
    {
       if((last_cmd==KEY_B_LEVEL_4)||(last_cmd==KEY_W_LEVEL_5)||(last_cmd==KEY_MODE_STROBE)||(last_cmd==KEY_B_LEVEL_3)||(last_cmd==KEY_B_LEVEL_5))
       {

       }
       else
       {
          light_IrCrtl.IR_Mode = 0;
       }
    }
  }

  /*手动模式加和减处理*/
  if((last_cmd==KEY_G_LEVEL_4)||(last_cmd==KEY_MODE_FLASH))
  {
      light_IrCrtl.ManulFlag = 1;
  }
  else
  {
      /*速度加和减不处理*/
      if((last_cmd==KEY_B_LEVEL_5)||(last_cmd==KEY_B_LEVEL_3))
      {

      }
      else
      {
         light_IrCrtl.ManulFlag = 0;
      }
  }
      
  light_IrCrtl.IR_INCFlag = 1;
  switch (last_cmd)
  #if USER_IRREMOTE
  {
    /******** 开关按键 *****/
    case KEY_POWER_ON:
        
        if(_light_threadFlag == FALSE)
        light_ir_fun_power_on_ctrl();
        else
        light_ir_fun_power_off_ctrl();
      break;
        
    /******** 白光功能 *****/
    case KEY_POWER_OFF:
      light_ir_key_fun_white();
      break;

    /******** 绿光功能 *****/
    case KEY_BRIGHTNESS_DOWN:
      //120
      light_ir_key_fun_color (120);
      break;
    
    /******** 红光功能 *****/
    case KEY_BRIGHTNESS_UP:
      //0
      light_ir_key_fun_color (0);
      break;

    /******** 蓝光功能 *****/
    case KEY_R_LEVEL_5:
      //240
      light_ir_key_fun_color (240);
      break;
    
    /******** 黄光功能 *****/
    case KEY_R_LEVEL_4:
      //60
      light_ir_key_fun_color (60);
      break;
    
    /******** 青光功能 *****/
    case KEY_R_LEVEL_3:
      //180
      light_ir_key_fun_color (180);
      break;
    
    /******** 橙光功能 *****/
    case KEY_G_LEVEL_5:
      //30
      light_ir_key_fun_color (30);
      break;
    
    /******** 粉光功能 *****/
    case KEY_G_LEVEL_3:
      //300
      light_ir_key_fun_color (300);
      break;

   /********  手动模式处理 **************/
    // 手动模式功能自加
    case KEY_MODE_FLASH:
        __scene_cnt = 0;
        __mode_cnts = 0;
        UserIndex++;
        if(dp_data.mode != SCENE_MODE)
        {
           UserIndex = 0;
           key_trig_num = 0;
           light_IrCrtl.ModeTotals = STREMBER_MODES;
           light_ir_fun_scene_change(key_trig_num);
        }
        else
        {
            if(UserIndex>=light_IrCrtl.ModeTotals)
            {
                UserIndex = 0;
                key_trig_num++;
                if(key_trig_num>3)
                  key_trig_num = 0;
                
                if(key_trig_num==SCENE_TYPE_BREATH)
                {
                   light_IrCrtl.ModeTotals = BREATH_MODES;
                }
                else if(key_trig_num==SCENE_TYPE_RAIN_DROP)
                {
                   light_IrCrtl.ModeTotals = FIRE_MODES; 
                }
                else if(key_trig_num==SCENE_TYPE_COLORFULL)
                {
                   light_IrCrtl.ModeTotals = COLORFUL_MODES;
                }
                else
                {
                  light_IrCrtl.ModeTotals = STREMBER_MODES;
                }
                light_ir_fun_scene_change(key_trig_num);
            }
        }
        PR_NOTICE("index1 ++++ %d %d %d",key_trig_num,UserIndex,light_IrCrtl.ModeTotals);
      break;
      
    // 手动模式功能减
    case KEY_G_LEVEL_4:
        __scene_cnt = 0;
        __mode_cnts = 0;
        /*****模式处理*****/
        if(dp_data.mode != SCENE_MODE)
        {
           UserIndex = 0;
           key_trig_num = 0;
           light_IrCrtl.ModeTotals = STREMBER_MODES;
           light_ir_fun_scene_change(key_trig_num);
        }
        else
        {
            if(UserIndex)
            {
               UserIndex--;
            }
            else
            {
               if(key_trig_num)
               key_trig_num--;
               else
               key_trig_num = 3;
               
               if(key_trig_num==SCENE_TYPE_BREATH)
               {
                 light_IrCrtl.ModeTotals = BREATH_MODES;
               }
               else if(key_trig_num==SCENE_TYPE_RAIN_DROP)
               {
                 light_IrCrtl.ModeTotals = FIRE_MODES; 
               }
               else if(key_trig_num==SCENE_TYPE_COLORFULL)
               {
                 light_IrCrtl.ModeTotals = COLORFUL_MODES;
               }
               else
               {
                light_IrCrtl.ModeTotals = STREMBER_MODES;
               }
               UserIndex = light_IrCrtl.ModeTotals;
               light_ir_fun_scene_change(key_trig_num);
            }
        }
        PR_NOTICE("index1 ---- %d %d %d ",key_trig_num,UserIndex,light_IrCrtl.ModeTotals);
      break;
      
     // 速度加
    case KEY_B_LEVEL_5:
      PR_NOTICE("+++++++ %d %d",key_trig_num,light_IrCrtl.Speedtimes);
      if(key_trig_num<4)
      {
        if(light_IrCrtl.Speedtimes<96)
        light_IrCrtl.Speedtimes = light_IrCrtl.Speedtimes+5;
        else
          light_IrCrtl.Speedtimes = 100;
        /********全局速度控制*******/
        light_IrCrtl.ManulTimes = light_IrCrtl.Speedtimes;
        light_IrCrtl.SpeedFlag = 1;
        light_scene_SetDataPara(light_IrCrtl.Speedtimes,light_IrCrtl.Speedtimes);
      }
      break;
    // 速度减
   case KEY_B_LEVEL_3:
       PR_NOTICE("----- %d %d",key_trig_num,light_IrCrtl.Speedtimes);
      if(key_trig_num<4)
      {
        if(light_IrCrtl.Speedtimes>44)
        light_IrCrtl.Speedtimes = light_IrCrtl.Speedtimes-5;
        else
          light_IrCrtl.Speedtimes = 40;
        /********全局速度控制*******/
        light_IrCrtl.ManulTimes = light_IrCrtl.Speedtimes;
        light_IrCrtl.SpeedFlag = 1;
        light_scene_SetDataPara(light_IrCrtl.Speedtimes,light_IrCrtl.Speedtimes);
      }
      break;  
   // 自动运行手动4种模式
   case KEY_B_LEVEL_4:
      if(light_IrCrtl.IR_Mode==0)
      {
        light_IrCrtl.IR_Mode = 1;
        light_IrCrtl.IR_Cnts = 0;
        light_IrCrtl.IR_Runtimes = 0;
        light_ir_fun_scene_change(0);
      }
      break;
   // 亮度减
    case KEY_W_LEVEL_5:
      light_ir_fun_bright_down();
      break;
    
   //亮度加
    case KEY_MODE_STROBE:
      light_ir_fun_bright_up();
      break;
    
    /********   手动模式 ***************/
    // 手动模式1
    case KEY_R_LEVEL_2:
      light_ir_fun_scene_change (0);
      break;
    
    // 手动模式2
    case KEY_G_LEVEL_2:
      light_ir_fun_scene_change (1);
      break;
    
    // 手动模式3
    case KEY_B_LEVEL_2:
      light_ir_fun_scene_change (2);
      break;

    // 手动模式4
    case KEY_MODE_FADE:
      light_ir_fun_scene_change (3);
      break;
    /*********** 音乐模式 **************/
    // 音乐模式1
    case KEY_R_LEVEL_1:
      light_ir_fun_scene_change (4);
      break;

    // 音乐模式2
    case KEY_G_LEVEL_1:
      light_ir_fun_scene_change (5);
      break;
   
    // 音乐模式3
    case KEY_B_LEVEL_1:
      light_ir_fun_scene_change (6);
      break;

     // 音乐模式4   
    case KEY_MODE_SMOOTH: 
      light_ir_fun_scene_change (7);
      break;
    
    default:
      break;
  }
  #else
  {
    case KEY_POWER_ON:
    case XKEY_POWER_ON:
      light_ir_fun_power_on_ctrl();
      break;

    case KEY_POWER_OFF:
    case XKEY_POWER_OFF:
      light_ir_fun_power_off_ctrl();
      break;

    case KEY_BRIGHTNESS_UP:
    case XKEY_BRIGHTNESS_UP:
      //    
      light_ir_fun_bright_up();
      break;

    case KEY_BRIGHTNESS_DOWN:
    case XKEY_BRIGHTNESS_DOWN:
      //
      light_ir_fun_bright_down();
      break;

    case KEY_R_LEVEL_5:
    case XKEY_R_LEVEL_5:
      //0
      light_ir_key_fun_color (0);
      break;

    case KEY_R_LEVEL_4:
    case XKEY_R_LEVEL_4:
      //15
      light_ir_key_fun_color (15);
      break;

    case KEY_R_LEVEL_3:
    case XKEY_R_LEVEL_3:
      //30
      light_ir_key_fun_color (30);
      break;

    case KEY_R_LEVEL_2:
    case XKEY_R_LEVEL_2:
      //45
      light_ir_key_fun_color (45);
      break;

    case KEY_R_LEVEL_1:
    case XKEY_R_LEVEL_1:
      //60
      light_ir_key_fun_color (60);
      break;

    case KEY_G_LEVEL_5:
     case XKEY_G_LEVEL_5:
      //120
      light_ir_key_fun_color (120);
      break;

    case KEY_G_LEVEL_4:
      case XKEY_G_LEVEL_4:
      //135
      light_ir_key_fun_color (135);
      break;

    case KEY_G_LEVEL_2:
      case XKEY_G_LEVEL_2:
      //150
      light_ir_key_fun_color (150);
      break;

    case KEY_G_LEVEL_3:
     case XKEY_G_LEVEL_3:
      //165
      light_ir_key_fun_color (160);
      break;

    case KEY_G_LEVEL_1:
       case XKEY_G_LEVEL_1:
      //180
      light_ir_key_fun_color (180);
      break;

    case KEY_B_LEVEL_5:
      case XKEY_B_LEVEL_5:
      //240
      light_ir_key_fun_color (240);
      break;

    case KEY_B_LEVEL_4:
      case XKEY_B_LEVEL_4:
      //255
      light_ir_key_fun_color (255);
      break;

    case KEY_B_LEVEL_3:
      case XKEY_B_LEVEL_3:
      //270
      light_ir_key_fun_color (270);
      break;

    case KEY_B_LEVEL_2:
      case XKEY_B_LEVEL_2:
      //285
      light_ir_key_fun_color (285);
      break;

    case KEY_B_LEVEL_1:
      case XKEY_B_LEVEL_1:
      //300
      light_ir_key_fun_color (300);
      break;

    case KEY_W_LEVEL_5:
      case XKEY_W_LEVEL_5:
      light_ir_key_fun_white();
      break;

    case KEY_MODE_SMOOTH:
      case XKEY_MODE_SMOOTH:
      light_ir_fun_scene_change (0);
      break;

    case KEY_MODE_FLASH:
      case XKEY_MODE_FLASH:
      light_ir_fun_scene_change (4);
      break;

    case KEY_MODE_STROBE:
       case XKEY_MODE_STROBE:
      light_ir_fun_scene_change (5);
      break;

    case KEY_MODE_FADE:
       case XKEY_MODE_FADE:
      light_ir_fun_scene_change (1);
      break;

    default:
      break;
  }
  #endif
}


