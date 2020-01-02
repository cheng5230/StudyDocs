#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include <example_entry.h>
#include <sys_api.h>
#include "gpio_api.h"
#include "analogin_api.h"
#include "flash_api.h"
#include "osdep_service.h"
#include "device_lock.h"
#include "timer_api.h"
#include "rtc_api.h"
#include "wait_api.h"
#include "device.h"
#include "gpio_api.h"   // mbed
#include "gpio_irq_api.h"   // mbed
#include "diag.h"
#include "us_ticker_api.h"
#include "serial_api.h"
#include "ws2812b.h"   // mbed


#define GPIO_LED_PIN       _PA_5
#define GPIO_PUSHBT_PIN    _PA_12
#define GPIO_LED_PIN1      _PA_23
#define GPIO_LED_PIN2      _PA_12
#define FLASH_APP_BASE     0xFF000

#define GPIO_OUT_PIN        _PA_5
#define GPIO_IRQ_PIN        _PA_12

#define UART_TX    PA_23
#define UART_RX    PA_18

// SPI0 (S0)
#define SPI0_MOSI  PA_4
#define SPI0_MISO  PA_3
#define SPI0_SCLK  PA_1
#define SPI0_CS    PA_2


float period[8] = {1.0/523, 1.0/587, 1.0/659, 1.0/698, 1.0/784, 1.0/880, 1.0/988, 1.0/1047};

gpio_t gpio_out;
gpio_irq_t gpio_irq;
volatile char irq_rise;

gtimer_t my_timer1;
gtimer_t my_timer4;
gpio_t gpio_led1;
gpio_t gpio_led4;
volatile uint32_t time4_expired=0;
u8 gpio_flag;




extern void console_init(void);

void uart_send_string(serial_t *sobj, char *pstr)
{
    unsigned int i=0;

    while (*(pstr+i) != 0) {
        serial_putc(sobj, *(pstr+i));
        i++;
    }
}

void tim4_capture_ISR(u32 data)
{
	u32 value = TIM4->CCMRx[0] & 0xFFFF;
	if(time4_expired==0)
	{
		gpio_write(&gpio_led4, 0);
		time4_expired = 1;
	}
	else
	{
		gpio_write(&gpio_led4, 1);
		time4_expired = 0;
	}
	//DBG_8195A("Pulse number: %d, %s\n", value, (value == 3276 || value == 3277) ? "success" : "fail");
	RTIM_INTClear(TIMx[4]);
}

void tim4_capture_pulse_num()
{
	RTIM_TimeBaseInitTypeDef  TIM_InitStruct_temp;
	TIM_CCInitTypeDef TIM_CCInitStruct;
		
	RTIM_TimeBaseStructInit(&TIM_InitStruct_temp);
	TIM_InitStruct_temp.TIM_Idx = 4;
	TIM_InitStruct_temp.TIM_Prescaler = 19;
	TIM_InitStruct_temp.TIM_Period = 19; //interrupt every 10us --> ((19+1)*(19+1))/(400000)
	RTIM_TimeBaseInit(TIM4, &TIM_InitStruct_temp, TIMx_irq[4], (IRQ_FUN) tim4_capture_ISR, 0);

	RTIM_CCStructInit(&TIM_CCInitStruct);
	TIM_CCInitStruct.TIM_ICPulseMode = TIM_CCMode_PulseNumber;
	RTIM_CCxInit(TIM4, &TIM_CCInitStruct, 0);

	RTIM_INTConfig(TIM4, TIM_IT_CC0, ENABLE);
	
	RTIM_CCxCmd(TIM4, 0, TIM_CCx_Enable);
	RTIM_Cmd(TIM4, ENABLE);

}

void timer4_timeout_handler(uint32_t id)
{
    //time4_expired = 1;
    
	gpio_t *gpio_led = (gpio_t *)id;
	if(gpio_flag)
	{
		gpio_flag = 0;
		gpio_write(gpio_led,0);
	}
	else
	{
		gpio_flag = 1;
		gpio_write(gpio_led,1);
	}
    //gpio_write(gpio_led, !gpio_read(gpio_led));  
	//DBG_8195A("---------------------------\n");
}


void timer1_timeout_handler(uint32_t id)
{
    gpio_t *gpio_led = (gpio_t *)id;
	if(gpio_flag)
	{
		gpio_flag = 0;
		gpio_write(gpio_led,0);
	}
	else
	{
		gpio_flag = 1;
		gpio_write(gpio_led,1);
	}
    //gpio_write(gpio_led, !gpio_read(gpio_led));  
	DBG_8195A("1111111111111111111\n");
}


void GPIO_CallBack(void)
{
	serial_t    sobj;
	char test_data[2] = {0xff,0x00};
	
    #if 0
	// Init LED control pin
    gpio_init(&gpio_led1, GPIO_LED_PIN1);
    gpio_dir(&gpio_led1, PIN_OUTPUT);    // Direction: Output
    gpio_mode(&gpio_led1, PullUp);      // No pull

	u8 port_num = PORT_NUM(gpio_led1.pin);
    u8 pin_num  = PIN_NUM(gpio_led1.pin);
	u32 RegValue;
	
    while(1)
    {
		if(gpio_flag)
		{
			gpio_flag = 0;
			//gpio_direct_write(&gpio_led1,0);
			
			RegValue =	GPIO->PORT[port_num].DR;
			RegValue |= (1 << pin_num);
            GPIO->PORT[port_num].DR = RegValue;
			delay_100ns(2);  // 300ns
			RegValue =  GPIO->PORT[port_num].DR;
			RegValue &= ~(1 << pin_num);
            GPIO->PORT[port_num].DR = RegValue;
			delay_100ns(6);  // 600ns
		}
		else
		{
			gpio_flag = 1;
			//gpio_direct_write(&gpio_led1,1);
			
			RegValue =	GPIO->PORT[port_num].DR;
			RegValue |= (1 << pin_num);
            GPIO->PORT[port_num].DR = RegValue;
			delay_100ns(6);   // 600ns
			RegValue =  GPIO->PORT[port_num].DR;
			RegValue &= ~(1 << pin_num);
            GPIO->PORT[port_num].DR = RegValue;
			delay_100ns(2);  // 300ns
		}
	}
	
    //gpio_init(&gpio_led4, GPIO_LED_PIN2);
    //gpio_dir(&gpio_led4, PIN_OUTPUT);    // Direction: Output
    //gpio_mode(&gpio_led4, PullNone);     // No pull
    // Initial a periodical timer
    gtimer_init(&my_timer1, TIMER3);
    gtimer_start_periodical(&my_timer1, 50, (void*)timer1_timeout_handler, (uint32_t)&gpio_led1);

    // Initial a one-shout timer and re-trigger it in while loop
    //time4_expired = 0;
    //gtimer_init(&my_timer4, TIMER4);
	//gtimer_start_periodical(&my_timer4, 1, (void*)timer4_timeout_handler, (uint32_t)&gpio_led4);
    //tim4_capture_pulse_num();
    vTaskDelete(NULL);
    #endif
	
	#if 0
	serial_init(&sobj,UART_TX,UART_RX);
    serial_baud(&sobj,6000000);
    serial_format(&sobj, 7, ParityNone, 0);
	while(1)
	{
		serial_putc(&sobj, test_data[0]);
		//serial_putc(&sobj, test_data[0]);
		//wait(1);
		serial_putc(&sobj, test_data[1]);
		//serial_putc(&sobj, test_data[1]);
		//wait(1);
		//serial_send_stream_dma(&sobj,&test_data[0],1);
	}
	#endif
	
	#if 0
	WS2812b_Init();
	while(1)
	{
		WS2812B_Test();
	}
	#endif

}

void GPIO_task(void)
{
	if(xTaskCreate( (TaskFunction_t)GPIO_CallBack, "GPIO CALL BACK", (2048/4), NULL, (tskIDLE_PRIORITY + 1), NULL)!= pdPASS) {
			DBG_8195A("Cannot create GPIO CALL BACK task\n\r");
	}
}

void adc_isr(void *Data)
{
	u32 buf[30];
	u32 isr = 0;
	u32 i = 0;

	isr = ADC_GetISR();
	if (isr & BIT_ADC_FIFO_THRESHOLD) {
		for(i = 0; i < 30; i++) {
			buf[i] = (u32)ADC_Read();
		}
	}

	ADC_INTClear();
	DBG_8195A("0x%08x, 0x%08x\n", buf[0], buf[1]);
}


VOID adc_one_shot (VOID)
{
	ADC_InitTypeDef ADCInitStruct;
	
	/* ADC Interrupt Initialization */
	InterruptRegister((IRQ_FUN)&adc_isr, ADC_IRQ, (u32)NULL, 5);
	InterruptEn(ADC_IRQ, 5);

	/* To release ADC delta sigma clock gating */
	PLL2_Set(BIT_SYS_SYSPLL_CK_ADC_EN, ENABLE);

	/* Turn on ADC active clock */
	RCC_PeriphClockCmd(APBPeriph_ADC, APBPeriph_ADC_CLOCK, ENABLE);

	ADC_InitStruct(&ADCInitStruct);
	ADCInitStruct.ADC_BurstSz = 8;
	ADCInitStruct.ADC_OneShotTD = 8; /* means 4 times */
	ADC_Init(&ADCInitStruct);
	ADC_SetOneShot(ENABLE, 100, ADCInitStruct.ADC_OneShotTD); /* 100 will task 200ms */

	ADC_INTClear();
	ADC_Cmd(ENABLE);
	
	vTaskDelete(NULL);
}

void AdC_task(void)
{
	if(xTaskCreate( (TaskFunction_t)adc_one_shot, "ADC ONE SHOT DEMO", (2048/4), NULL, (tskIDLE_PRIORITY + 1), NULL)!= pdPASS) {
			DBG_8195A("Cannot create ADC one shot demo task\n\r");
	}
}

static void flash_test_task(void *param)
{
    flash_t         flash;
    uint32_t        address = FLASH_APP_BASE;

#if 1
    uint32_t        val32_to_write = 0x13572468;
    uint32_t        val32_to_read;
    int loop = 0;
    int result = 0;
    
    for(loop = 0; loop < 10; loop++)
    {
		device_mutex_lock(RT_DEV_LOCK_FLASH);
        flash_read_word(&flash, address, &val32_to_read);
        DBG_8195A("Read Data 0x%x\n", val32_to_read);
        flash_erase_sector(&flash, address);
        flash_write_word(&flash, address, val32_to_write);
        flash_read_word(&flash, address, &val32_to_read);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);

        DBG_8195A("Read Data 0x%x\n", val32_to_read);

        // verify result
        result = (val32_to_write == val32_to_read) ? 1 : 0;
        //printf("\r\nResult is %s\r\n", (result) ? "success" : "fail");
        DBG_8195A("\r\nResult is %s\r\n", (result) ? "success" : "fail");
        result = 0;
    }

#else
    int VERIFY_SIZE = 256;
    int SECTOR_SIZE = 16;
    
    uint8_t writedata[VERIFY_SIZE];
    uint8_t readdata[VERIFY_SIZE];
    uint8_t verifydata = 0;
    int loop = 0;
    int index = 0;
    int sectorindex = 0;
    int result = 0;
    int resultsector = 0;
    int testloop = 0;
    
    for(testloop = 0; testloop < 1; testloop++){
        address = FLASH_APP_BASE;
        for(sectorindex = 0; sectorindex < 0x300; sectorindex++){
            result = 0;
            //address += SECTOR_SIZE;
			device_mutex_lock(RT_DEV_LOCK_FLASH);
            flash_erase_sector(&flash, address);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
            //DBG_8195A("Address = %x \n", address);
            for(loop = 0; loop < SECTOR_SIZE; loop++){
                for(index = 0; index < VERIFY_SIZE; index++)
                {
                    writedata[index] = verifydata + index;
                }
				device_mutex_lock(RT_DEV_LOCK_FLASH);
                flash_stream_write(&flash, address, VERIFY_SIZE, &writedata);
                flash_stream_read(&flash, address, VERIFY_SIZE, &readdata);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				
                for(index = 0; index < VERIFY_SIZE; index++)
                {
                    //DBG_8195A("Address = %x, Writedata = %x, Readdata = %x \n",address,writedata[index],readdata[index]);
                    if(readdata[index] != writedata[index]){
                        DBG_8195A("Error: Loop = %d, Address = %x, Writedata = %x, Readdata = %x \n",testloop,address,writedata[index],readdata[index]);
                    }
                    else{
                        result++;
                        //DBG_8195A(ANSI_COLOR_BLUE"Correct: Loop = %d, Address = %x, Writedata = %x, Readdata = %x \n"ANSI_COLOR_RESET,testloop,address,writedata[index],readdata[index]);
                    }
                }
                address += VERIFY_SIZE;
            }
            if(result == VERIFY_SIZE * SECTOR_SIZE){
                //DBG_8195A("Sector %d Success \n", sectorindex);
                resultsector++;
            }
        }
        if(resultsector == 0x300){
            DBG_8195A("Test Loop %d Success \n", testloop);    
        }
        resultsector = 0;
        verifydata++;
    }
    //DBG_8195A("%d Sector Success \n", resultsector);

    DBG_8195A("Test Done");

#endif
    vTaskDelete(NULL);
}

void Flash_task(void)
{
	if(xTaskCreate(flash_test_task, ((const char*)"flash_test_task"), 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
			printf("\n\r%s xTaskCreate(flash_test_task) failed", __FUNCTION__);
}

void rtc_test_task(void)
{
	time_t seconds;
    struct tm *timeinfo;

	while(1)
	{
		seconds = rtc_read();
		timeinfo = localtime(&seconds);

		DBG_8195A("Time as seconds since January 1, 1970 = %d\n", seconds);

		DBG_8195A("Time as a basic string = %s", ctime(&seconds));

		DBG_8195A("Time as a custom formatted string = %d-%d-%d %d:%d:%d\n", 
		timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour,
		timeinfo->tm_min,timeinfo->tm_sec);
	
		vTaskDelay(1000);
		//wait(1.0);
	}
}

void RTC_task(void)
{
    rtc_init();
    rtc_write(1256729737);  // Set RTC time to Wed, 28 Oct 2009 11:35:37
    
    if(xTaskCreate(rtc_test_task, ((const char*)"rtc_test_task"), 1024/2, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
			printf("\n\r%s xTaskCreate(rtc_test_task) failed", __FUNCTION__);
}

void gpio_demo_irq_handler (uint32_t id, gpio_irq_event event)
{
    static unsigned int rise_time;
    static unsigned int fall_time;

    if (irq_rise) {
        rise_time = us_ticker_read();
        // Changed as Falling Edge Trigger
        gpio_irq_set_event(&gpio_irq, IRQ_FALL);
        irq_rise = 0;
    } else {
        fall_time = us_ticker_read();
        // Changed as Rising Edge Trigger
        gpio_irq_set_event(&gpio_irq, IRQ_RISE);
        irq_rise = 1;

        printf("%d\n", (fall_time-rise_time));
    }
}


void rgbw_test_task(void)
{
	while(1)
	{
		wait_ms(500);
	    gpio_write(&gpio_out, 1);
	    wait_us(1000);
	    gpio_write(&gpio_out, 0);
	}
}

void RGBW_task(void)
{
	// Init LED control pin
	gpio_init(&gpio_out, GPIO_OUT_PIN);
	gpio_dir(&gpio_out, PIN_OUTPUT);	// Direction: Output
	gpio_mode(&gpio_out, PullNone); 	// No pull
	gpio_write(&gpio_out, 0);

	// Initial Push Button pin as interrupt source
	gpio_irq_init(&gpio_irq, GPIO_IRQ_PIN, gpio_demo_irq_handler, (uint32_t)(&gpio_irq));
	gpio_irq_set(&gpio_irq, IRQ_RISE, 1);	// Falling Edge Trigger
	irq_rise = 1;
	gpio_irq_pull_ctrl(&gpio_irq, PullNone);
	gpio_irq_enable(&gpio_irq);

    if(xTaskCreate(rgbw_test_task, ((const char*)"rgbw_test_task"), 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
			printf("\n\r%s xTaskCreate(rgbw_test_task) failed", __FUNCTION__);
}


/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void)
{
	/* Initialize log uart and at command service */
	//console_init();	
	ReRegisterPlatformLogUart();

	/* pre-processor of application example */
	pre_example_entry();

	/* wlan intialization */
#if defined(CONFIG_WIFI_NORMAL) && defined(CONFIG_NETWORK)
	wlan_network();
#endif

	/* Execute application example */
	//example_entry();
    GPIO_task();
	//AdC_task();
	//Flash_task();
	//RTC_task();
	//RGBW_task();
	
    /*Enable Schedule, Start Kernel*/
#if defined(CONFIG_KERNEL) && !TASK_SCHEDULER_DISABLED
	#ifdef PLATFORM_FREERTOS
	vTaskStartScheduler();
	#endif
#else
	RtlConsolTaskRom(NULL);
#endif
}

