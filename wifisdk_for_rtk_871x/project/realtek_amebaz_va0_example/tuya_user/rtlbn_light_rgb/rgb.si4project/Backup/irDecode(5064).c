#include "irDecode.h"
#include "device.h"
#include "serial_api.h"
#include "serial_ex_api.h"

STATIC volatile uint32_t gs_time_125us = 0;

STATIC uint16_t lastIrCnt;
STATIC uint32_t irCmd;
// STATIC IRCODE last_irType;
STATIC IRCODE irType;
STATIC volatile uint32_t timer_cnt = 1;
IRCMD cur_ircmd;
int8_t cur_irType;

gpio_t gpio_test_ir;
//STATIC TIMER_ID ir_timer;

////remote control
IRDEAL irdeal;
#define UART_TX PA_23 //UART0 TX
#define UART_RX PA_18 //UART0 RX 
#define SRX_BUF_SZ 16
char rx_buf[SRX_BUF_SZ]= {0};
volatile uint32_t rx_done=0;
serial_t    sobj_remote;
STATIC SEM_HANDLE g_remote_control_semp;

SFT_TIMER LightGraTimer;

uint16_t ir_temp[128];
uint8_t ir_cnt = 0;

STATIC uint8_t isInRange (uint16_t n, uint16_t target, uint8_t range)
{
  if ( (n > (target - range) ) && (n < (target + range) ) )
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

STATIC IRCODE IrDecodePh (uint16_t cnt)
{
  if (isInRange (cnt, 110, 15) ) //5
  {
    return IRCODESTART;
  }
  else if (isInRange (cnt, 88, 6) )
  {
    return IRCODEREPEAT;
  }
  else if (isInRange (cnt, 18, 4) ) //3
  {
    return IRCODE1;
  }
  else if (isInRange (cnt, 9, 4) ) //2
  {
    return IRCODE0;
  }

  return IRCODEERROR;
}


STATIC void IrCmdDeal (VOID)
{
  uint8_t* p;
  UCHAR i;
  p = (uint8_t*) &irCmd;
  #if 1 // always lost bit code, and fail. So skip this check func.
  if(p[3]!=0x0)
	{ 
		irCmd = 0;
		irType = IRCODEERROR;
		// PR_DEBUG_RAW("IR CODE ERR!\r\n");
		return;
	}
  #else
  if (p[2]!=0xf7 || p[3]!=0x0) // IR REMOTE PROVIDE BY AILY&ALLEN
  {
    // if(p[2]!=0xff || p[3]!=0x0){ // SPECIAL IR REMOTE
    irCmd = 0;
    irType = IRCODEERROR;
    // PR_DEBUG_RAW("IR CODE ERR!\r\n");
    return;
  }
  #endif

  #if 0
  if ( (uint8_t) (p[0] & 0xff) != (uint8_t) (~p[1] & 0xff) )
  {
    // PR_DEBUG_RAW("p[0]=0x%02x, != ~p[1]=0x%02x\r\n", p[0], ~p[1]);
    irCmd = 0;
    irType = IRCODEERROR;
    return;
  }
  #endif
  
  /***** 解码自适应 ******/
	if(p[2] == 0xff)
	{
        cur_ircmd = p[0];
	}
	else if (p[2] == 0xf7)
	{
	    cur_ircmd = p[1];
	}
  
  cur_irType = irType;//IRCODESTART;
  PostSemaphore (irdeal.ir_cmddeal_sem);
  irType = IRCODEERROR;
  irCmd = 0;
}

STATIC void IrDecode (uint16_t cnt)
{
  IRCODE code;
  STATIC uint8_t irDecodeSecondByte;
#if 0//纯打印
  STATIC uint32_t x = 0;
  STATIC UCHAR ir_cmd_ = 0x00;
  STATIC UCHAR ir_temp_[7];
  ir_temp[ir_cnt] = cnt;
  ir_cnt ++;

  if (ir_cnt >= 32) //大于这个最少按两次
  {
    //直接数据全局打印
    uint8_t i;
    PR_DEBUG_RAW ("ir%d: ", x++);
    ir_temp_[5] = 1;

    for (i = 0; i < 48; i ++)
    {
      if (i%8 == 0)
      {
        PR_DEBUG_RAW ("\r\nnum:%d  ", i/8);
      }

      PR_DEBUG_RAW ("%04d ", ir_temp[i]);
    }

    PR_DEBUG_RAW ("\r\n");
    ir_cnt = 0;

    /*下面为解析数据*/
    for (i=0; i<48; i++) //二进制
    {
      if (isInRange (ir_temp[i], 18, 2) )
      {
        ir_cmd_ = (ir_cmd_<<1) | 0x01;
        PR_DEBUG_RAW ("1");
      }
      else if (isInRange (ir_temp[i], 9, 2) )
      {
        ir_cmd_ = ir_cmd_<<1;
        PR_DEBUG_RAW ("0");
      }
      else if (isInRange (ir_temp[i], 108, 5) )
      {
        PR_DEBUG_RAW ("S");  //头
      }
      else if (isInRange (ir_temp[i], 90, 5) )
      {
        PR_DEBUG_RAW ("P");  //重复码1
      }
      else if (isInRange (ir_temp[i], 145, 5) ) //经检验，重复码2 = CODE0
      {
        PR_DEBUG_RAW ("p");  //重复码2
      }
      else
      {
        PR_DEBUG_RAW ("e"); //错误码
      }

      //
      if (i%8 == 7)
      {
        ir_temp_[i/7 - 1] = ir_cmd_;
        PR_DEBUG_RAW ("   analysis num:%d \r\n", i/7 - 1);
      }
    }

    PR_DEBUG_RAW ("\r\n");
    memset (ir_temp, 0, 48);

    for (i=0; i<6; i++) //十六进制
    {
      PR_DEBUG_RAW ("code num:%d -> 0X%02x\r\n", i, ir_temp_[i]);
    }
  }

#else//解码
  //  STATIC IRCODE CmdType = IRCODEERROR;
  if (irType == IRCODESTART)
  {
    //注意头码容易和重复码靠近
    switch (IrDecodePh (cnt) )
    {
      case IRCODE1:
        irCmd = (irCmd<<1) | 0x01;
        break;

      case IRCODE0:
        irCmd = (irCmd<<1);
        break;

      default:
        irType = IRCODEERROR;
        irCmd = 0;
        ir_cnt = 0;
        return;
        break;
    }

    ir_cnt++;
    if (ir_cnt >= 32)
    {
      // PR_DEBUG_RAW("if(ir_cnt >= 32) irType:%d\r\n", irType);
      IrCmdDeal();
      ir_cnt = 0;
      irType = IRCODEERROR;
    }

    return;
  }

  if (irType == IRCODEREPEAT)
  {
    cur_irType = IRCODEREPEAT;
    PostSemaphore (irdeal.ir_cmddeal_sem);
    irType = IRCODEERROR;
  }

  irType = IrDecodePh (cnt);
#endif
}


STATIC void ir_test_timer_handler (uint32_t id)
{
  timer_cnt ++;

  if (timer_cnt > TIMER_CNT_MAX)
  {
    timer_cnt = 0;
    ir_cnt = 0;
    irCmd = 0;
    cur_ircmd = 0xff; // invalid code!
  }
}

STATIC void gpio_interrupt (uint32_t id, gpio_irq_event event)
{
  if (id == IR_GPIO_NUM)
  {
    // PR_NOTICE("gpio_interrupt, timer_cnt=%d", timer_cnt);
    //获取计数
    IrDecode (timer_cnt);
    timer_cnt = 0;
  }
}



//*****************************************************
//******************红外线程***************************
STATIC IR_CALLBACK __ir_callback = NULL;

STATIC VOID IrCmdDealThread (PVOID pArg)
{
  static int i = 0;

  while (1)
  {
    WaitSemaphore (irdeal.ir_cmddeal_sem);

    if (__ir_callback != NULL)
    {
      __ir_callback (cur_ircmd, cur_irType);
    }
  }
}


//********************************************************
//******************红外相关初始化************************
STATIC void gpio_intr_init (VOID)
{
#if 1//IR中断
  uint32_t gpio_irq_id = IR_GPIO_NUM;
  gpio_irq_init (&ir_irq, IR_GPIO_NUM, gpio_interrupt, (uint32_t) gpio_irq_id);
  gpio_irq_set (&ir_irq, IRQ_FALL, 1);
  gpio_irq_enable (&ir_irq);
  irType = IRCODEERROR;
  // last_irType = IRCODEERROR;
#else//测试使用
  gpio_init (&test_gpio_x, PA_12);
  gpio_dir (&test_gpio_x, PIN_OUTPUT);   // Direction: Output
  gpio_mode (&test_gpio_x, PullNone);    // No pull
  gpio_write (&test_gpio_x, 0); //测试正常可用,接高点亮依据亮度看现象
#endif
}
STATIC void TimerInit (VOID)
{
  gtimer_init (&ir_timer, TIMER3);
  gtimer_start_periodical (&ir_timer, 100, (void*) ir_test_timer_handler, NULL); //100us
}

STATIC VOID UtilInit (IR_CALLBACK callback)
{
  OPERATE_RET op_ret;
  op_ret = CreateAndInitSemaphore (&irdeal.ir_cmddeal_sem, 0, 1);

  if (OPRT_OK != op_ret)
  {
    PR_ERR ("Create Semp err:%d", op_ret);
    return ;
  }

  op_ret = PostSemaphore (irdeal.ir_cmddeal_sem);

  if (OPRT_OK != op_ret)
  {
    PR_ERR ("PostSemp err:%d", op_ret);
    return ;
  }

  THRD_PARAM_S thrd_param;
  thrd_param.stackDepth = 1024+512;
  thrd_param.priority = TRD_PRIO_2;
  thrd_param.thrdname = "ir_task";
  op_ret = CreateAndStart (&irdeal.ir_thread, NULL, NULL, IrCmdDealThread, NULL, &thrd_param);

  if (op_ret != OPRT_OK)
  {
    PR_ERR ("Create ir_thread err:%d", op_ret);
    return ;
  }

  __ir_callback = callback;
  PR_DEBUG ("UtilInit Finish!");
}

void uart_recv_string_done (uint32_t id)
{
  serial_t*    sobj = (void*) id;
  PR_INFO ("PostSemp success\n");
  PostSemaphore (g_remote_control_semp);
}

void light_strip_infrared_init (IR_CALLBACK callback)
{
  gpio_intr_init();
  TimerInit();
  UtilInit (callback);
}

