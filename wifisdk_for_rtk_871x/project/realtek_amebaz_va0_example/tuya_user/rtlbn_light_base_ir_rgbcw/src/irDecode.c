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


STATIC void IrCmdDeal(VOID)
{
	uint8_t *p;
	UCHAR i;
	p = (uint8_t *)&irCmd;
	
	//PR_NOTICE("p[0]=0x%02x, p[1]=0x%02x,p[2]=0x%02x, p[3]=0x%02x\r\n", p[0],p[1],p[2], p[3]);
#if 1 // always lost bit code, and fail. So skip this check func.
	if(p[2]!=0xdd || p[3]!=0x55){ // IR REMOTE PROVIDE BY AILY&ALLEN
		irCmd = 0;
		irType = IRCODEERROR;
		return;
	}
#endif

	if ((uint8_t)(p[0] & 0xff) != (uint8_t)(~p[1] & 0xff)) 
    {
		PR_NOTICE("p[0]=%d, != ~p[1]=%d\r\n", p[0], ~p[1]);
		irCmd = 0;
		irType = IRCODEERROR;
		return;
	}

	cur_ircmd = p[1];
	cur_irType = irType;//IRCODESTART;
	
	PostSemaphore (irdeal.ir_cmddeal_sem);
	irType = IRCODEERROR;
	irCmd = 0;
}


STATIC void IrDecode(uint16_t cnt)
{
	IRCODE code;
    STATIC uint8_t irDecodeSecondByte;

	//÷ÿ∏¥¬Î¥¶¿Ì
	if(irType == IRCODEREPEAT)
    {
		cur_irType = IRCODEREPEAT;
		PostSemaphore (irdeal.ir_cmddeal_sem);
		irType = IRCODEERROR;
		return;
	}


	//Ω‚¬Îø™ ºŒª,»•≥˝¿¨ª¯÷µ
	if(irType == IRCODEERROR){
		irType = IRCODESTARTHEAD;
		return;
	}

	//Ω‚¬Î:Õ∑¬ÎªÚ’ﬂ÷ÿ∏¥¬Î
	
	if(irType == IRCODESTARTHEAD){
		irType = IrDecode_Start(cnt);
	}
	else if(irType == IRCODESTART)
    {
		switch(IrDecode_Data(cnt))
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
		if(ir_cnt >= 32)
        {
			//printf("if(ir_cnt >= 32) irType:%d\r\n", irType);
			IrCmdDeal();
			ir_cnt = 0;
			irType = IRCODEERROR;
		}
		return;
	}
	
}


STATIC void ir_test_timer_handler (uint32_t id)
{
  timer_cnt ++;

  if (timer_cnt > TIMER_CNT_MAX)
  {
    timer_cnt = 0;
    ir_cnt = 0;
    irCmd = 0;
	irType = IRCODEERROR;
    cur_ircmd = 0xff; // invalid code!
  }
}

STATIC void gpio_interrupt (uint32_t id, gpio_irq_event event)
{
  if (id == IR_GPIO_NUM)
  {
    // PR_NOTICE("gpio_interrupt, timer_cnt=%d", timer_cnt);
    //Ëé∑ÂèñËÆ°Êï∞
    IrDecode (timer_cnt);
    timer_cnt = 0;
  }
}



//*****************************************************
//******************Á∫¢Â§ñÁ∫øÁ®ã***************************
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
//******************Á∫¢Â§ñÁõ∏ÂÖ≥ÂàùÂßãÂåñ************************
STATIC void gpio_intr_init (VOID)
{
#if 1//IR‰∏≠Êñ≠
  uint32_t gpio_irq_id = IR_GPIO_NUM;
  gpio_irq_init (&ir_irq, IR_GPIO_NUM, gpio_interrupt, (uint32_t) gpio_irq_id);
  gpio_irq_set (&ir_irq, IRQ_FALL, 1);
  gpio_irq_enable (&ir_irq);
  irType = IRCODEERROR;
  // last_irType = IRCODEERROR;
#else//ÊµãËØï‰ΩøÁî®
  gpio_init (&test_gpio_x, PA_12);
  gpio_dir (&test_gpio_x, PIN_OUTPUT);   // Direction: Output
  gpio_mode (&test_gpio_x, PullNone);    // No pull
  gpio_write (&test_gpio_x, 0); //ÊµãËØïÊ≠£Â∏∏ÂèØÁî®,Êé•È´òÁÇπ‰∫Æ‰æùÊçÆ‰∫ÆÂ∫¶ÁúãÁé∞Ë±°
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

