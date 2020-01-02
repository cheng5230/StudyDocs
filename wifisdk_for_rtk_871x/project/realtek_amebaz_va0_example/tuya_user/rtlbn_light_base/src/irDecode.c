#include "irDecode.h"
#include "device.h"
#include "serial_api.h"
#include "serial_ex_api.h"


//hw_timer_define
STATIC gtimer_t ir_timer;
//STATIC gtimer_t reset_timer;
STATIC SEM_HANDLE get_switch_sem;
STATIC gpio_irq_t ir_irq;
STATIC gpio_t test_gpio_x;
STATIC gtimer_t test_timer;


STATIC volatile uint32_t gs_time_125us = 0;
STATIC uint16_t lastIrCnt;
STATIC uint32_t irCmd;
// STATIC IRCODE last_irType;
STATIC IRCODE irType;
STATIC volatile uint32_t timer_cnt = 1;
STATIC IRCMD cur_ircmd;
STATIC int8_t cur_irType;
gpio_t gpio_test_ir;
//STATIC TIMER_ID ir_timer;

////remote control
STATIC IRDEAL irdeal;
#define UART_TX PA_23 //UART0 TX
#define UART_RX PA_18 //UART0 RX 
#define SRX_BUF_SZ 16
char rx_buf[SRX_BUF_SZ]= {0};
volatile uint32_t rx_done=0;
serial_t    sobj_remote;
STATIC SEM_HANDLE g_remote_control_semp;

SFT_TIMER LightGraTimer;
uint32_t ir_temp[128];
uint16_t IRCode_temp[33];
uint8_t IR_temp[4];
uint8_t ir_cnt = 0;
uint8_t ir_flag = 0;
uint8_t ir_start = 0;
uint8_t Flag_ir_start = 0;
uint8_t	IR_num = 0;		 
uint8_t ir_repeatcnts = 0;

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

STATIC IRCODE IrDecode_Data(uint16_t cnt)
{
	if(isInRange(cnt,18,4)){  //3
		return IRCODE1;
	}
	else if(isInRange(cnt,9,4)){
		return IRCODE0;
	}
	
	return IRCODEERROR;
}

STATIC IRCODE IrDecode_Start(uint16_t cnt)
{
	if(isInRange(cnt,110,15)){  //5
		return IRCODESTART;
	}
	else if(isInRange(cnt,88,6)){
		return IRCODEREPEAT;
	}
	
	return IRCODEERROR;
}


void IR_Decode(uint16_t IR_time)
{
     /********* 确定起始码*******/
	if(isInRange(IR_time,110,10)) 
	{
		 Flag_ir_start = 1;
		 IR_num = 0;		//清空数据
		 irCmd=0;
	}
	else
	{
		/******** 开始解码 ********/ 
		if((Flag_ir_start==1))
		{
			irCmd<<=1;
			if(isInRange(IR_time,18,4))
			{
				irCmd |= 0x00000001;
			}
			IR_num++;
			if(IR_num>31)
			{
				/******** 解码完成 ********/ 
				IR_temp[0] = (uint8_t)((irCmd>>0)&0xff);
				IR_temp[1] = (uint8_t)((irCmd>>8)&0xff);
				IR_temp[2] = (uint8_t)((irCmd>>16)&0xff);
				IR_temp[3] = (uint8_t)((irCmd>>24)&0xff);
				irCmd=0;
				IR_num=0;
				irType = IRCODESTART;
				Flag_ir_start=0;
			}
		}
		else
		{
			irCmd=0;
			IR_num=0;
		}
	}
}


STATIC void IrCmdDeal (VOID)
{
  uint8_t* p;
  UCHAR i;
 #if 1
	  p = (uint8_t*) &IR_temp;
	  //PR_NOTICE("%d p[0]=0x%02x, p[1]=0x%02x,p[2]=0x%02x, p[3]=0x%02x\r\n",irType, p[0],p[1],p[2], p[3]);
	  if (((p[2] == 0xf7) && (p[3] == 0x00))&&(p[0]+p[1]==0xff))
	  {
	    cur_ircmd = p[1];
	  }
	  else
	  {
		irCmd = 0;
	    irType = IRCODEERROR;
	    return;
	  }
#else
      p = (uint8_t*) &IR_temp;
	  PR_NOTICE("p[0]=0x%02x, p[1]=0x%02x,p[2]=0x%02x, p[3]=0x%02x\r\n", p[0],p[1],p[2], p[3]);
	  if ((uint8_t) (p[2] & 0xff) != (uint8_t) (~p[3] & 0xff))
	  {
		//PR_DEBUG_RAW("p[2]=0x%02x, != ~p[3]=0x%02x\r\n", p[2], ~p[3]);
		irCmd = 0;
	    irType = IRCODEERROR;
	    return;
	  }
	  cur_ircmd = p[1];
 #endif
  cur_irType = irType;
  PostSemaphore (irdeal.ir_cmddeal_sem);
  irType = IRCODEERROR;
  irCmd = 0;
}

STATIC void IrDecode (uint16_t cnt)
{
  uint8_t i;
  ir_flag = 1;
  if(ir_start==0)
  {
	  // 起始码判断
	  if((cnt>108)&&(cnt<118))
      {
		ir_start = 1;
		ir_temp[ir_cnt] = cnt;
	    ir_cnt ++;
	  }
	  // 判断重复码 
	  else if((cnt>85)&&(cnt<95))
	  {
		  IR_num = 0;	  
		  ir_cnt = 0;
		  ir_flag = 0;
		  ir_start = 0;
		  timer_cnt = 0;
	      irCmd=IR_temp[1];
		  #if 0
		  // 电源键 模式加和减 不支持长按
		  if((irCmd==KEY_POWER_ON)||(irCmd==KEY_G_LEVEL_4)||(irCmd==KEY_MODE_FLASH))
		  {
			irCmd = 0;
			irType = IRCODEERROR;
			return;
		  }
		  #else
		  // 长按触发时间 
		  ir_repeatcnts++;
		  if(ir_repeatcnts<4)
		  {
			return;
		  }
		  else
		  {
			ir_repeatcnts = 0;
		  }
		  #endif
		  irType = IRCODEREPEAT;
		  PostSemaphore (irdeal.ir_cmddeal_sem);
		  irType = IRCODEERROR;
		  irCmd = 0;
	  }
  }
  else
  {
	  ir_temp[ir_cnt] = cnt;
	  ir_cnt ++;
	  if(ir_cnt >32)  
	  { 
		MutexLock (irdeal.ir_mutex);
		for (i = 0; i < 33; i ++)
	    {
	      //PR_NOTICE("%d %04d ",i, ir_temp[i]);
	      IR_Decode(ir_temp[i]);
	    }
		ir_repeatcnts = 0;
		ir_start = 0;
	    ir_cnt = 0;
		ir_flag = 0;
		timer_cnt = 0;
		IrCmdDeal();
	  }
	  MutexUnLock (irdeal.ir_mutex);
  }
}

/*************** 100us采样 ***************/
STATIC void ir_test_timer_handler (uint32_t id)
{
  #if 0
  timer_cnt ++;
  if (timer_cnt > TIMER_CNT_MAX)
  {
    timer_cnt = 0;
    ir_cnt = 0;
    irCmd = 0;
	irType = IRCODEERROR;
    cur_ircmd = 0xff;    // invalid code!
  }
  #else
  
  if(ir_flag)
  {
  	 timer_cnt ++;
  }
  #endif
}



STATIC void gpio_interrupt (uint32_t id, gpio_irq_event event)
{
  if (id == IR_GPIO_NUM)
  {
	IrDecode(timer_cnt);
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
//******************红外相关初***********************
STATIC void gpio_intr_init (VOID)
{
   uint32_t gpio_irq_id = IR_GPIO_NUM;
  
  //gpio_init (&test_gpio_x, IR_GPIO_NUM);
  //gpio_dir (&test_gpio_x, PIN_INPUT);      // Direction: Output
  //gpio_mode (&test_gpio_x, PullUp);	      // No pull
  
  gpio_irq_init (&ir_irq, IR_GPIO_NUM, gpio_interrupt, (uint32_t) gpio_irq_id);
  gpio_irq_set (&ir_irq, IRQ_FALL, 1);
  gpio_irq_enable (&ir_irq);
  irType = IRCODEERROR;
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
  
  op_ret = CreateMutexAndInit(&irdeal.ir_mutex);

  if (OPRT_OK != op_ret)
  {
    PR_ERR ("CreateMutexAndInit err ");
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

