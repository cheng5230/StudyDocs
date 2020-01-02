#include "spi_api.h"
#include "device.h"
#include "gpio_api.h"   
#include "pwmout_api.h" 
#include <stdint.h>
#include <stdbool.h>
#include "ws2812b.h"
#include "timer_api.h"


#if 0
uint8_t rBuffer[PIXEL_MAX]={0};
uint8_t gBuffer[PIXEL_MAX]={0};
uint8_t bBuffer[PIXEL_MAX]={0};
gtimer_t ws_timer;
volatile uint32_t ws_timercnt;
u8 cur_wscmd;
u8 cur_wsflag;
u8 port_num ;
u8 pin_num ;
u32 RegValue;
volatile uint8_t redtemp;
volatile uint8_t greentemp;
volatile uint8_t bluetemp;
volatile uint8_t rgbstate;
gpio_t gpio_ws2812;
spi_t spi_master;
gpio_t gpio_spi;

void delay_100ns(u8 nns)
{
	while(nns--)
	{
		__NOP;
	}
}

 void PixelUpdate(void)
 {
   #if USE_SPI
   uint8_t rst[50]={0x00};
   spi_master_write_stream(&spi_master, rst, 50);
   #else
   wait_us(60); // 50us以上
   #endif
 }
 

void ws2812_timer_handler(uint32_t id)
 {
	 ws_timercnt ++;
	 if(ws_timercnt > 10)
	 {
		 ws_timercnt = 0;
		 cur_wsflag = 1;
		 cur_wscmd++;
		 if(cur_wscmd>SHOW_W)
		   cur_wscmd = SHOW_R;
	 }
 }


void WSTimerInit(VOID)
 {
	 gtimer_init(&ws_timer, TIMER3);
	 gtimer_start_periodical(&ws_timer, 1000, (void*)ws2812_timer_handler, NULL); //1ms
 }

 void WS2812b_Init(void)
{
	// 使用SPI驱动
	#if USE_SPI
	gpio_init(&gpio_spi, SPI1_MOSI);
	gpio_set(SPI1_MOSI);
    gpio_mode(&gpio_spi, PullNone);     // No pull
    gpio_dir(&gpio_spi, PIN_OUTPUT);    // Direction: Output
	spi_master.spi_idx=MBED_SPI1;
    spi_init(&spi_master, SPI1_MOSI, SPI1_MISO, SPI1_SCLK, SPI1_CS);
	spi_format(&spi_master, 8, 0, 0);
	spi_frequency(&spi_master, SPI1_COLCK);
	// 使用GPIO驱动
    #else
    // Init LED control pin
    gpio_init(&gpio_ws2812, WS2812_GPIO);
    gpio_dir(&gpio_ws2812, PIN_OUTPUT);    // Direction: Output
    gpio_mode(&gpio_ws2812, PullUp);      // No pull
	port_num = PORT_NUM(gpio_ws2812.pin);
    pin_num  = PIN_NUM(gpio_ws2812.pin);
	#endif
	WSTimerInit();
}

void Send_8bits(uint8_t dat)
{
  uint8_t i=0;
  uint8_t CodeOne = 0xfc;
  uint8_t CodeZero = 0xc0;
   
  for (i=0;i<8;i++)
  {
    if(dat&0x80)
    {
		#if USE_SPI
		//spi_master_write_stream_dma(&spi_master, &CodeOne, 1);
	    ssi_write(&spi_master,0xfc);
		#else
		RegValue =	GPIO->PORT[port_num].DR;
		RegValue |= (1 << pin_num);
        GPIO->PORT[port_num].DR = RegValue;
		delay_100ns(6);   // 600ns
		RegValue =  GPIO->PORT[port_num].DR;
		RegValue &= ~(1 << pin_num);
        GPIO->PORT[port_num].DR = RegValue;
		delay_100ns(2);  // 300ns
        #endif
    }
    else
    {
		#if USE_SPI
	    //spi_master_write_stream_dma(&spi_master, &CodeZero, 1);
	    ssi_write(&spi_master,0xc0);
        #else
		RegValue =	GPIO->PORT[port_num].DR;
		RegValue |= (1 << pin_num);
        GPIO->PORT[port_num].DR = RegValue;
		delay_100ns(2);  // 300ns
		RegValue =  GPIO->PORT[port_num].DR;
		RegValue &= ~(1 << pin_num);
        GPIO->PORT[port_num].DR = RegValue;
		delay_100ns(6);  // 600ns
        #endif
    }
    dat=dat<<1;
  }
}

void Send_24bits(uint8_t RData,uint8_t GData,uint8_t BData)
{
  uint8_t i=0;
  uint8_t CodeOne = 0xfc;
  uint8_t CodeZero = 0xc0;
  uint32_t dat;
  
  dat = ((uint32_t)GData << 16) | ((uint32_t)RData <<  8) | BData;
  for (i=0;i<24;i++)
  {
    if(dat&0x800000)
    {
		#if USE_SPI
		//spi_master_write_stream_dma(&spi_master, &CodeOne, 1);
	    ssi_write(&spi_master,0xfc);
		#else
		RegValue =	GPIO->PORT[port_num].DR;
		RegValue |= (1 << pin_num);
        GPIO->PORT[port_num].DR = RegValue;
		delay_100ns(6);   // 600ns
		RegValue =  GPIO->PORT[port_num].DR;
		RegValue &= ~(1 << pin_num);
        GPIO->PORT[port_num].DR = RegValue;
		delay_100ns(2);  // 300ns
        #endif
    }
    else
    {
		#if USE_SPI
	    //spi_master_write_stream_dma(&spi_master, &CodeZero, 1);
	    ssi_write(&spi_master,0xc0);
        #else
		RegValue =	GPIO->PORT[port_num].DR;
		RegValue |= (1 << pin_num);
        GPIO->PORT[port_num].DR = RegValue;
		delay_100ns(2);  // 300ns
		RegValue =  GPIO->PORT[port_num].DR;
		RegValue &= ~(1 << pin_num);
        GPIO->PORT[port_num].DR = RegValue;
		delay_100ns(6);  // 600ns
        #endif
    }
    dat=dat<<1;
  }
}

void setAllPixelColor(uint8_t r, uint8_t g, uint8_t b)
{ 
  uint8_t i=0;
  for(i=0;i<PIXEL_MAX;i++)
  {
    rBuffer[i]=r;
    gBuffer[i]=g;
    bBuffer[i]=b;
  }
  for(i=0;i<PIXEL_MAX;i++)
  {							  
    Send_24bits(rBuffer[i],gBuffer[i],bBuffer[i]);
  }
  PixelUpdate();
}

void SetPixelColor(uint16_t n, uint32_t c)
{	 
  //uint8_t i=0;
   
  rBuffer[n]=(uint8_t)(c>>16);
  gBuffer[n]=(uint8_t)(c>>8);
  bBuffer[n]=(uint8_t)c;

  //for(i=0;i<PIXEL_MAX;i++)
  {							  
    Send_24bits(rBuffer[n],gBuffer[n],bBuffer[n]);
  }
  //PixelUpdate();
}

uint32_t Color(uint8_t r, uint8_t g, uint8_t b)
{
  return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint16_t wait) 
{
  uint16_t i=0;
  for( i=0; i<PIXEL_MAX; i++) 
  {
    SetPixelColor(i, c);
  }
  PixelUpdate();
  wait_ms(wait);
}

uint32_t Wheel(uint8_t WheelPos)
{
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) 
  {
    return Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbow(uint8_t wait)
{
  uint16_t i, j;
  
  for(j=0; j<256; j++) 
  {
    for(i=0; i<PIXEL_MAX; i++)
    {
      SetPixelColor(i, Wheel((i+j) & 255));
    }
    PixelUpdate();
    wait_ms(wait);
  }
}

void rainbowCycle(uint8_t wait) 
{
  uint16_t i, j;
  
  for(j=0; j<256*5; j++) 
  { 
    // 5 cycles of all colors on wheel
    for(i=0; i< PIXEL_MAX; i++) 
    {
      SetPixelColor(i, Wheel(((i * 256 / PIXEL_MAX) + j) & 255));
    }
    PixelUpdate();
    wait_ms (wait);
  }
}

void theaterChase(uint32_t c, uint8_t wait) 
{
  for (int j=0; j<10; j++) 
  { 
    //do 10 cycles of chasing
    for (int q=0; q < 3; q++) 
    {
      for (uint16_t i=0; i < PIXEL_MAX; i=i+1)//turn every one pixel on
      {
        SetPixelColor(i+q, c);    
      }
      PixelUpdate();
      wait_ms(wait);
      
      for (uint16_t i=0; i < PIXEL_MAX; i=i+1) //turn every one pixel off
      {
        SetPixelColor(i+q, 0);        
      }
      PixelUpdate();
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) 
{
  for (int j=0; j < 256; j++) 
  {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++)
    {
      for (uint16_t i=0; i < PIXEL_MAX; i=i+1) //turn every one pixel on
      {
        SetPixelColor(i+q, Wheel( (i+j) % 255));    
      }
      PixelUpdate();
      
      wait_ms(wait);
      
      for (uint16_t i=0; i < PIXEL_MAX; i=i+1)//turn every one pixel off
      {
        SetPixelColor(i+q, 0);        
      }
      PixelUpdate();
    }
  }
}

void WS2812B_Test(void)
{
	// Some example procedures showing how to display to the pixels:
	#if 0
	switch(cur_wscmd)
	{
		case SHOW_R:
			if(cur_wsflag)
			{
            	colorWipe(Color(255, 0, 0), 1);
				cur_wsflag = 0;
			}
			break;
		case SHOW_G:
			if(cur_wsflag)
			{
            	colorWipe(Color(0, 255, 0), 1);
				cur_wsflag = 0;
			}
			break;
        case SHOW_B:
			if(cur_wsflag)
			{
            	colorWipe(Color(0, 0, 255), 1);
				cur_wsflag = 0;
			}
			break;
		case SHOW_W:
			if(cur_wsflag)
			{
            	colorWipe(Color(255, 255, 255), 1);
				cur_wsflag = 0;
			}
			break;
        default:
            break;
	}
	#endif
	if(cur_wsflag)
    {
        cur_wsflag = 0;
        if(rgbstate==0)
        {
          redtemp++;
          if(redtemp==255)
          rgbstate = 1;
        }
        else if(rgbstate==1)
        {
          redtemp--;
          if(redtemp==0)
          rgbstate = 2;
        }
        else if(rgbstate==2)
        {
          greentemp++;
          if(greentemp==255)
          rgbstate = 3;
        }
        else if(rgbstate==3)
        {
          greentemp--;
          if(greentemp==0)
          rgbstate = 4;
        }
        else if(rgbstate==4)
        {
          bluetemp++;
          if(bluetemp==255)
          rgbstate = 5;
        }
        else if(rgbstate==5)
        {
          bluetemp--;
          if(bluetemp==0)
          rgbstate = 6;
        }
		else 
        {
          redtemp=0;
		  greentemp=0;
		  bluetemp=0;
          if(rgbstate++>200)
          rgbstate = 0;
        }
        setAllPixelColor(redtemp,greentemp,bluetemp);
    }
	//colorWipe(Color(255, 0, 0), 1000);
    //colorWipe(Color(0, 255, 0), 1000);
    //colorWipe(Color(0, 0, 255), 1000);
    //colorWipe(Color(255, 255, 255), 1000);
    // Send a theater pixel chase in...
    //theaterChase(Color(127, 127, 127), 50); // White
    //theaterChase(Color(127, 0, 0), 50); // Red
    //theaterChase(Color(0, 127, 0), 50); // Green   
    //theaterChase(Color(0, 0, 127), 50); // Blue   
    //rainbow(20);
    //rainbowCycle(20);
    //theaterChaseRainbow(100);
}
#endif

/********* 变量定义 ***********/
spi_t spi_master;
gpio_t gpio_spi;

segment _segments[MAX_NUM_SEGMENTS];                  // SRAM footprint: 20 bytes per element 
segment_runtime _segment_runtimes[MAX_NUM_SEGMENTS]; // SRAM footprint: 16 bytes per element

// create GLOBAL names to allow WS2812FX to compile with sketches and other libs that store strings
// in PROGMEM (get rid of the "section type conflict with __c" errors once and for all. Amen.)
const char name_0[]  = "Static";
const char name_1[]  = "Blink";
const char name_2[]  = "Breath";
const char name_3[]  = "Color Wipe";
const char name_4[]  = "Color Wipe Inverse";
const char name_5[]  = "Color Wipe Reverse";
const char name_6[]  = "Color Wipe Reverse Inverse";
const char name_7[]  = "Color Wipe Random";
const char name_8[]  = "Random Color";
const char name_9[]  = "Single Dynamic";
const char name_10[]  = "Multi Dynamic";
const char name_11[]  = "Rainbow";
const char name_12[]  = "Rainbow Cycle";
const char name_13[]  = "Scan";
const char name_14[]  = "Dual Scan";
const char name_15[]  = "Fade";
const char name_16[]  = "Theater Chase";
const char name_17[]  = "Theater Chase Rainbow";
const char name_18[]  = "Running Lights";
const char name_19[]  = "Twinkle";
const char name_20[]  = "Twinkle Random";
const char name_21[]  = "Twinkle Fade";
const char name_22[]  = "Twinkle Fade Random";
const char name_23[]  = "Sparkle";
const char name_24[]  = "Flash Sparkle";
const char name_25[]  = "Hyper Sparkle";
const char name_26[]  = "Strobe";
const char name_27[]  = "Strobe Rainbow";
const char name_28[]  = "Multi Strobe";
const char name_29[]  = "Blink Rainbow";
const char name_30[]  = "Chase White";
const char name_31[]  = "Chase Color";
const char name_32[]  = "Chase Random";
const char name_33[]  = "Chase Rainbow";
const char name_34[]  = "Chase Flash";
const char name_35[]  = "Chase Flash Random";
const char name_36[]  = "Chase Rainbow White";
const char name_37[]  = "Chase Blackout";
const char name_38[]  = "Chase Blackout Rainbow";
const char name_39[]  = "Color Sweep Random";
const char name_40[]  = "Running Color";
const char name_41[]  = "Running Red Blue";
const char name_42[]  = "Running Random";
const char name_43[]  = "Larson Scanner";
const char name_44[]  = "Comet";
const char name_45[]  = "Fireworks";
const char name_46[]  = "Fireworks Random";
const char name_47[]  = "Merry Christmas";
const char name_48[]  = "Fire Flicker";
const char name_49[]  = "Fire Flicker (soft)";
const char name_50[]  = "Fire Flicker (intense)";
const char name_51[]  = "Circus Combustus";
const char name_52[]  = "Halloween";
const char name_53[]  = "Bicolor Chase";
const char name_54[]  = "Tricolor Chase";
const char name_55[]  = "ICU";
const char name_56[]  = "Custom 0"; // custom modes need to go at the end
const char name_57[]  = "Custom 1";
const char name_58[]  = "Custom 2";
const char name_59[]  = "Custom 3";

static const char* _names[] = {
  (name_0),
  (name_1),
  (name_2),
  (name_3),
  (name_4),
  (name_5),
  (name_6),
  (name_7),
  (name_8),
  (name_9),
  (name_10),
  (name_11),
  (name_12),
  (name_13),
  (name_14),
  (name_15),
  (name_16),
  (name_17),
  (name_18),
  (name_19),
  (name_20),
  (name_21),
  (name_22),
  (name_23),
  (name_24),
  (name_25),
  (name_26),
  (name_27),
  (name_28),
  (name_29),
  (name_30),
  (name_31),
  (name_32),
  (name_33),
  (name_34),
  (name_35),
  (name_36),
  (name_37),
  (name_38),
  (name_39),
  (name_40),
  (name_41),
  (name_42),
  (name_43),
  (name_44),
  (name_45),
  (name_46),
  (name_47),
  (name_48),
  (name_49),
  (name_50),
  (name_51),
  (name_52),
  (name_53),
  (name_54),
  (name_55),
  (name_56),
  (name_57),
  (name_58),
  (name_59)
};

boolean	 _running;
boolean	 _triggered;
mode_ptr _mode[MODE_COUNT]; // SRAM footprint: 4 bytes per element
uint8_t _segment_index = 0;
uint8_t _num_segments = 1;
uint8_t  brightness;
uint8_t stops;
uint8_t  *pixels;	  ///< Holds LED color values (3 or 4 bytes each)
uint8_t  rOffset;	  ///< Red index within each 3- or 4-byte pixel
uint8_t  gOffset;	  ///< Index of green byte
uint8_t  bOffset;	  ///< Index of blue byte
uint8_t  wOffset;	  ///< Index of white (==rOffset if no white)

void Send_24bits(uint8_t RData,uint8_t GData,uint8_t BData)
{
  uint8_t i=0;
  uint32_t dat;
  dat = ((uint32_t)GData << 16) | ((uint32_t)RData <<  8) | BData;
  for (i=0;i<24;i++)
  {
    if(dat&0x800000)
    {
	    ssi_write(&spi_master,0xfc);
    }
    else
    {
	    ssi_write(&spi_master,0xc0);
    }
    dat=dat<<1;
  }
}

void WS2812FX_init() {

	gpio_init(&gpio_spi, SPI1_MOSI);
	gpio_set(SPI1_MOSI);
	gpio_mode(&gpio_spi, PullNone);     // No pull
	gpio_dir(&gpio_spi, PIN_OUTPUT);    // Direction: Output
	spi_master.spi_idx=MBED_SPI1;
	spi_init(&spi_master, SPI1_MOSI, SPI1_MISO, SPI1_SCLK, SPI1_CS);
	spi_format(&spi_master, 8, 0, 0);
	spi_frequency(&spi_master, SPI1_COLCK);
}

void WS2812FX_service() {

}

void WS2812B_Setup(void)
{

}
uint8_t *getPixels(void) 
{ return pixels; }

uint8_t getNumBytesPerPixel(void) {
  return (wOffset == rOffset) ? 3 : 4; // 3=RGB, 4=RGBW
}

void setPixelColor(uint16_t n, uint32_t c) {
	//rBuffer[n]=(uint8_t)(c>>16);
	//gBuffer[n]=(uint8_t)(c>>8);
	//bBuffer[n]=(uint8_t)c;
	//Send_24bits(rBuffer[n],gBuffer[n],bBuffer[n]);
}

void copyPixels(uint16_t dest, uint16_t src, uint16_t count) {

  uint8_t *pixels = getPixels();
  uint8_t bytesPerPixel = getNumBytesPerPixel(); // 3=RGB, 4=RGBW
  memmove(pixels + (dest * bytesPerPixel), pixels + (src * bytesPerPixel), count * bytesPerPixel);
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
uint32_t color_wheel(uint8_t pos) {
  pos = 255 - pos;
  if(pos < 85) {
    return ((uint32_t)(255 - pos * 3) << 16) | ((uint32_t)(0) << 8) | (pos * 3);
  } else if(pos < 170) {
    pos -= 85;
    return ((uint32_t)(0) << 16) | ((uint32_t)(pos * 3) << 8) | (255 - pos * 3);
  } else {
    pos -= 170;
    return ((uint32_t)(pos * 3) << 16) | ((uint32_t)(255 - pos * 3) << 8) | (0);
  }
}

/*
 * Color wipe function
 * LEDs are turned on (color1) in sequence, then turned off (color2) in sequence.
 * if (bool rev == true) then LEDs are turned off in reverse order
 */
uint16_t color_wipe(uint32_t color1, uint32_t color2, bool rev) {
  if(SEGMENT_RUNTIME.counter_mode_step < SEGMENT_LENGTH) {
    uint32_t led_offset = SEGMENT_RUNTIME.counter_mode_step;
    if (IS_REVERSE)
    {
      setPixelColor(SEGMENT.stop - led_offset, color1);
    } else {
      setPixelColor(SEGMENT.start + led_offset, color1);
    }
  } else {
    uint32_t led_offset = SEGMENT_RUNTIME.counter_mode_step - SEGMENT_LENGTH;
    if((IS_REVERSE && !rev) || (!IS_REVERSE && rev)) {
      setPixelColor(SEGMENT.stop - led_offset, color2);
    } else {
      setPixelColor(SEGMENT.start + led_offset, color2);
    }
  }
  if(SEGMENT_RUNTIME.counter_mode_step % SEGMENT_LENGTH == 0) SET_CYCLE;
  else CLR_CYCLE;

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (SEGMENT_LENGTH * 2);
  return (SEGMENT.speed / (SEGMENT_LENGTH * 2));
}

/*
 * Blink/strobe function
 * Alternate between color1 and color2
 * if(strobe == true) then create a strobe effect
 */
uint16_t blink(uint32_t color1, uint32_t color2, bool strobe) {
  uint32_t color = ((SEGMENT_RUNTIME.counter_mode_call & 1) == 0) ? color1 : color2;
  if(IS_REVERSE) color = (color == color1) ? color2 : color1;
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color);
  }

  if((SEGMENT_RUNTIME.counter_mode_call & 1) == 0) {
    return strobe ? 20 : (SEGMENT.speed / 2);
  } else {
    return strobe ? SEGMENT.speed - 20 : (SEGMENT.speed / 2);
  }
}

/*
 * scan function - runs a block of pixels back and forth.
 */
uint16_t scan(uint32_t color1, uint32_t color2, bool dual) {
  int8_t dir = SEGMENT_RUNTIME.aux_param ? -1 : 1;
  uint8_t size = 1 << SIZE_OPTION;

  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color2);
  }

  for(uint8_t i = 0; i < size; i++) {
    if(IS_REVERSE || dual) {
      setPixelColor(SEGMENT.stop - SEGMENT_RUNTIME.counter_mode_step - i, color1);
    }
    if(!IS_REVERSE || dual) {
      setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.counter_mode_step + i, color1);
    }
  }

  SEGMENT_RUNTIME.counter_mode_step += dir;
  if(SEGMENT_RUNTIME.counter_mode_step == 0) SEGMENT_RUNTIME.aux_param = 0;
  if(SEGMENT_RUNTIME.counter_mode_step >= (SEGMENT_LENGTH - size)) SEGMENT_RUNTIME.aux_param = 1;

  return (SEGMENT.speed / (SEGMENT_LENGTH * 2));
}


/*
 * Tricolor chase function
 */
uint16_t tricolor_chase(uint32_t color1, uint32_t color2, uint32_t color3) {
  uint8_t sizeCnt = 1 << SIZE_OPTION;
  uint16_t index = SEGMENT_RUNTIME.counter_mode_call % (sizeCnt * 3);
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++, index++) {
    index = index % (sizeCnt * 3);

    uint32_t color = color3;
    if(index < sizeCnt) color = color1;
    else if(index < (sizeCnt * 2)) color = color2;

    if(IS_REVERSE) {
      setPixelColor(SEGMENT.start + i, color);
    } else {
      setPixelColor(SEGMENT.stop - i, color);
    }
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * twinkle function
 */
uint16_t twinkle(uint32_t color1, uint32_t color2) {
  if(SEGMENT_RUNTIME.counter_mode_step == 0) {
    for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
      setPixelColor(i, color2);
    }
    uint16_t min_leds = max(1, SEGMENT_LENGTH / 5); // make sure, at least one LED is on
    uint16_t max_leds = max(1, SEGMENT_LENGTH / 2); // make sure, at least one LED is on
    SEGMENT_RUNTIME.counter_mode_step = random(min_leds, max_leds);
  }
  setPixelColor(SEGMENT.start + random16(SEGMENT_LENGTH), color1);

  SEGMENT_RUNTIME.counter_mode_step--;
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * twinkle_fade function
 */
uint16_t twinkle_fade(uint32_t color) {
  fade_out();
  if(random8(3) == 0) {
    uint8_t size = 1 << SIZE_OPTION;
    uint16_t index = SEGMENT.start + random16(SEGMENT_LENGTH - size);
    for(uint8_t i=0; i<size; i++) {
      setPixelColor(index + i, color);
    }
  }
  return (SEGMENT.speed / 8);
}


/*
 * Firing comets from one end.
 */
uint16_t mode_comet(void) {
  fade_out();

  if(IS_REVERSE) {
    setPixelColor(SEGMENT.stop - SEGMENT_RUNTIME.counter_mode_step, SEGMENT.colors[0]);
  } else {
    setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.counter_mode_step, SEGMENT.colors[0]);
  }

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  return (SEGMENT.speed / SEGMENT_LENGTH);
}


/*
 * Fireworks function.
 */
uint16_t fireworks(uint32_t color) {
  fade_out();

// for better performance, manipulate the Adafruit_NeoPixels pixels[] array directly
  uint8_t *pixels = getPixels();
  uint8_t bytesPerPixel = getNumBytesPerPixel(); // 3=RGB, 4=RGBW
  uint16_t startPixel = SEGMENT.start * bytesPerPixel + bytesPerPixel;
  uint16_t stopPixel = SEGMENT.stop * bytesPerPixel ;
  for(uint16_t i=startPixel; i <stopPixel; i++) {
    uint16_t tmpPixel = (pixels[i - bytesPerPixel] >> 2) +
      pixels[i] +
      (pixels[i + bytesPerPixel] >> 2);
    pixels[i] =  tmpPixel > 255 ? 255 : tmpPixel;
  }

  uint8_t size = 2 << SIZE_OPTION;
  if(!_triggered) {
    for(uint16_t i=0; i<max(1, SEGMENT_LENGTH/20); i++) {
      if(random8(10) == 0) {
        uint16_t index = SEGMENT.start + random16(SEGMENT_LENGTH - size);
        for(uint8_t j=0; j<size; j++) {
          setPixelColor(index + j, color);
        }
      }
    }
  } else {
    for(uint16_t i=0; i<max(1, SEGMENT_LENGTH/10); i++) {
      uint16_t index = SEGMENT.start + random16(SEGMENT_LENGTH - size);
      for(uint8_t j=0; j<size; j++) {
        setPixelColor(index + j, color);
      }
    }
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Firework sparks.
 */
uint16_t mode_fireworks(void) {
  uint32_t color = BLACK;
  do { // randomly choose a non-BLACK color from the colors array
    color = SEGMENT.colors[random8(NUM_COLORS)];
  } while (color == BLACK);
  return fireworks(color);
}

/*
 * Random colored firework sparks.
 */
uint16_t mode_fireworks_random(void) {
  return fireworks(color_wheel(random8()));
}


/*
 * Fire flicker function
 */
uint16_t fire_flicker(int rev_intensity) {
  uint8_t w = (SEGMENT.colors[0] >> 24) & 0xFF;
  uint8_t r = (SEGMENT.colors[0] >> 16) & 0xFF;
  uint8_t g = (SEGMENT.colors[0] >>  8) & 0xFF;
  uint8_t b = (SEGMENT.colors[0]        & 0xFF);
  uint8_t lum = max(w, max(r, max(g, b))) / rev_intensity;
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    int flicker = random8(lum);
    r =  max(r - flicker, 0);
    g =  max(g - flicker, 0);
    b =  max(b - flicker, 0);
    setPixelColor(i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Random flickering.
 */
uint16_t mode_fire_flicker(void) {
  return fire_flicker(3);
}

/*
* Random flickering, less intensity.
*/
uint16_t mode_fire_flicker_soft(void) {
  return fire_flicker(6);
}

/*
* Random flickering, more intensity.
*/
uint16_t mode_fire_flicker_intense(void) {
  return fire_flicker(1.7);
}


/*
 * No blinking. Just plain old static light.
 */
uint16_t WS2812FX_mode_static(void) {
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, SEGMENT.colors[0]);
  }
  return 500;
}

/*
 * Normal blinking. 50% on/off time.
 */
uint16_t WS2812FX_mode_blink(void) {
  return blink(SEGMENT.colors[0], SEGMENT.colors[1], false);
}

/*
 * Classic Blink effect. Cycling through the rainbow.
 */
uint16_t WS2812FX_mode_blink_rainbow(void) {
  return blink(color_wheel(SEGMENT_RUNTIME.counter_mode_call & 0xFF), SEGMENT.colors[1], false);
}


/*
 * Classic Strobe effect.
 */
uint16_t WS2812FX_mode_strobe(void) {
  return blink(SEGMENT.colors[0], SEGMENT.colors[1], true);
}


/*
 * Classic Strobe effect. Cycling through the rainbow.
 */
uint16_t WS2812FX_mode_strobe_rainbow(void) {
  return blink(color_wheel(SEGMENT_RUNTIME.counter_mode_call & 0xFF), SEGMENT.colors[1], true);
}


/*
 * Color wipe function
 * LEDs are turned on (color1) in sequence, then turned off (color2) in sequence.
 * if (bool rev == true) then LEDs are turned off in reverse order
 */
uint16_t WS2812FX_color_wipe(uint32_t color1, uint32_t color2, bool rev) {
  if(SEGMENT_RUNTIME.counter_mode_step < SEGMENT_LENGTH) {
    uint32_t led_offset = SEGMENT_RUNTIME.counter_mode_step;
    if(IS_REVERSE) {
      setPixelColor(SEGMENT.stop - led_offset, color1);
    } else {
      setPixelColor(SEGMENT.start + led_offset, color1);
    }
  } else {
    uint32_t led_offset = SEGMENT_RUNTIME.counter_mode_step - SEGMENT_LENGTH;
    if((IS_REVERSE && !rev) || (!IS_REVERSE && rev)) {
      setPixelColor(SEGMENT.stop - led_offset, color2);
    } else {
      setPixelColor(SEGMENT.start + led_offset, color2);
    }
  }

  if(SEGMENT_RUNTIME.counter_mode_step % SEGMENT_LENGTH == 0) SET_CYCLE;
  else CLR_CYCLE;

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (SEGMENT_LENGTH * 2);
  return (SEGMENT.speed / (SEGMENT_LENGTH * 2));
}

/*
 * Lights all LEDs one after another.
 */
uint16_t WS2812FX_mode_color_wipe(void) {
  return color_wipe(SEGMENT.colors[0], SEGMENT.colors[1], false);
}

uint16_t WS2812FX_mode_color_wipe_inv(void) {
  return color_wipe(SEGMENT.colors[1], SEGMENT.colors[0], false);
}

uint16_t WS2812FX_mode_color_wipe_rev(void) {
  return color_wipe(SEGMENT.colors[0], SEGMENT.colors[1], true);
}

uint16_t WS2812FX_mode_color_wipe_rev_inv(void) {
  return color_wipe(SEGMENT.colors[1], SEGMENT.colors[0], true);
}


/*
 * Turns all LEDs after each other to a random color.
 * Then starts over with another color.
 */
uint16_t WS2812FX_mode_color_wipe_random(void) {
  if(SEGMENT_RUNTIME.counter_mode_step % SEGMENT_LENGTH == 0) { // aux_param will store our random color wheel index
    SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param);
  }
  uint32_t color = color_wheel(SEGMENT_RUNTIME.aux_param);
  return color_wipe(color, color, false) * 2;
}


/*
 * Random color introduced alternating from start and end of strip.
 */
uint16_t WS2812FX_mode_color_sweep_random(void) {
  if(SEGMENT_RUNTIME.counter_mode_step % SEGMENT_LENGTH == 0) { // aux_param will store our random color wheel index
    SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param);
  }
  uint32_t color = color_wheel(SEGMENT_RUNTIME.aux_param);
  return color_wipe(color, color, true) * 2;
}


/*
 * Lights all LEDs in one random color up. Then switches them
 * to the next random color.
 */
uint16_t WS2812FX_mode_random_color(void) {
  SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param); // aux_param will store our random color wheel index
  uint32_t color = color_wheel(SEGMENT_RUNTIME.aux_param);

  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color);
  }
  return (SEGMENT.speed);
}


/*
 * Lights every LED in a random color. Changes one random LED after the other
 * to another random color.
 */
uint16_t WS2812FX_mode_single_dynamic(void) {
  if(SEGMENT_RUNTIME.counter_mode_call == 0) {
    for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
      setPixelColor(i, color_wheel(random8()));
    }
  }

  setPixelColor(SEGMENT.start + random16(SEGMENT_LENGTH), color_wheel(random8()));
  return (SEGMENT.speed);
}


/*
 * Lights every LED in a random color. Changes all LED at the same time
 * to new random colors.
 */
uint16_t WS2812FX_mode_multi_dynamic(void) {
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color_wheel(random8()));
  }
  return (SEGMENT.speed);
}


/*
 * Does the "standby-breathing" of well known i-Devices. Fixed Speed.
 * Use mode "fade" if you like to have something similar with a different speed.
 */
uint16_t WS2812FX_mode_breath(void) {
  int lum = SEGMENT_RUNTIME.counter_mode_step;
  if(lum > 255) lum = 511 - lum; // lum = 15 -> 255 -> 15

  uint16_t delay;
  if(lum == 15) delay = 970; // 970 pause before each breath
  else if(lum <=  25) delay = 38; // 19
  else if(lum <=  50) delay = 36; // 18
  else if(lum <=  75) delay = 28; // 14
  else if(lum <= 100) delay = 20; // 10
  else if(lum <= 125) delay = 14; // 7
  else if(lum <= 150) delay = 11; // 5
  else delay = 10; // 4

  uint32_t color = SEGMENT.colors[0];
  uint8_t w = (color >> 24 & 0xFF) * lum / 256;
  uint8_t r = (color >> 16 & 0xFF) * lum / 256;
  uint8_t g = (color >>  8 & 0xFF) * lum / 256;
  uint8_t b = (color       & 0xFF) * lum / 256;
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
  }

  SEGMENT_RUNTIME.counter_mode_step += 2;
  if(SEGMENT_RUNTIME.counter_mode_step > (512-15)) SEGMENT_RUNTIME.counter_mode_step = 15;
  return delay;
}


/*
 * Fades the LEDs between two colors
 */
uint16_t WS2812FX_mode_fade(void) {
  int lum = SEGMENT_RUNTIME.counter_mode_step;
  if(lum > 255) lum = 511 - lum; // lum = 0 -> 255 -> 0

  uint32_t color = color_blend(SEGMENT.colors[0], SEGMENT.colors[1], lum);
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color);
  }

  SEGMENT_RUNTIME.counter_mode_step += 4;
  if(SEGMENT_RUNTIME.counter_mode_step > 511) SEGMENT_RUNTIME.counter_mode_step = 0;
  return (SEGMENT.speed / 128);
}


/*
 * scan function - runs a block of pixels back and forth.
 */
uint16_t WS2812FX_scan(uint32_t color1, uint32_t color2, bool dual) {
  int8_t dir = SEGMENT_RUNTIME.aux_param ? -1 : 1;
  uint8_t size = 1 << SIZE_OPTION;

  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color2);
  }

  for(uint8_t i = 0; i < size; i++) {
    if(IS_REVERSE || dual) {
      setPixelColor(SEGMENT.stop - SEGMENT_RUNTIME.counter_mode_step - i, color1);
    }
    if(!IS_REVERSE || dual) {
      setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.counter_mode_step + i, color1);
    }
  }

  SEGMENT_RUNTIME.counter_mode_step += dir;
  if(SEGMENT_RUNTIME.counter_mode_step == 0) SEGMENT_RUNTIME.aux_param = 0;
  if(SEGMENT_RUNTIME.counter_mode_step >= (SEGMENT_LENGTH - size)) SEGMENT_RUNTIME.aux_param = 1;

  return (SEGMENT.speed / (SEGMENT_LENGTH * 2));
}


/*
 * Runs a block of pixels back and forth.
 */
uint16_t WS2812FX_mode_scan(void) {
  return scan(SEGMENT.colors[0], SEGMENT.colors[1], false);
}


/*
 * Runs two blocks of pixels back and forth in opposite directions.
 */
uint16_t WS2812FX_mode_dual_scan(void) {
  return scan(SEGMENT.colors[0], SEGMENT.colors[1], true);
}


/*
 * Cycles all LEDs at once through a rainbow.
 */
uint16_t WS2812FX_mode_rainbow(void) {
  uint32_t color = color_wheel(SEGMENT_RUNTIME.counter_mode_step);
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color);
  }

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) & 0xFF;
  return (SEGMENT.speed / 256);
}


/*
 * Cycles a rainbow over the entire string of LEDs.
 */
uint16_t WS2812FX_mode_rainbow_cycle(void) {
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
	  uint32_t color = color_wheel(((i * 256 / SEGMENT_LENGTH) + SEGMENT_RUNTIME.counter_mode_step) & 0xFF);
    setPixelColor(SEGMENT.start + i, color);
  }

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) & 0xFF;
  return (SEGMENT.speed / 256);
}


/*
 * Theatre-style crawling lights.
 * Inspired by the Adafruit examples.
 */
uint16_t WS2812FX_mode_theater_chase(void) {
  return tricolor_chase(SEGMENT.colors[0], SEGMENT.colors[1], SEGMENT.colors[1]);
}


/*
 * Theatre-style crawling lights with rainbow effect.
 * Inspired by the Adafruit examples.
 */
uint16_t WS2812FX_mode_theater_chase_rainbow(void) {
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) & 0xFF;
  uint32_t color = color_wheel(SEGMENT_RUNTIME.counter_mode_step);
  return tricolor_chase(color, SEGMENT.colors[1], SEGMENT.colors[1]);
}


/*
 * Running lights effect with smooth sine transition.
 */
uint16_t WS2812FX_mode_running_lights(void) {
  uint8_t w = ((SEGMENT.colors[0] >> 24) & 0xFF);
  uint8_t r = ((SEGMENT.colors[0] >> 16) & 0xFF);
  uint8_t g = ((SEGMENT.colors[0] >>  8) & 0xFF);
  uint8_t b =  (SEGMENT.colors[0]        & 0xFF);

  uint8_t size = 1 << SIZE_OPTION;
  uint8_t sineIncr = max(1, (256 / SEGMENT_LENGTH) * size);
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
    int lum = (int)sine8(((i + SEGMENT_RUNTIME.counter_mode_step) * sineIncr));
    if(IS_REVERSE) {
      r =  (r * lum) / 256;
      g =  (g * lum) / 256;
      b =  (b * lum) / 256;
      setPixelColor(SEGMENT.start + i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
    } else {
      r =  (r * lum) / 256;
      g =  (g * lum) / 256;
      b =  (b * lum) / 256;
      setPixelColor(SEGMENT.stop - i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
    }
  }
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % 256;
  return (SEGMENT.speed / SEGMENT_LENGTH);
}


/*
 * twinkle function
 */
uint16_t WS2812FX_twinkle(uint32_t color1, uint32_t color2) {
  if(SEGMENT_RUNTIME.counter_mode_step == 0) {
    for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
      setPixelColor(i, color2);
    }
    uint16_t min_leds = max(1, SEGMENT_LENGTH / 5); // make sure, at least one LED is on
    uint16_t max_leds = max(1, SEGMENT_LENGTH / 2); // make sure, at least one LED is on
    SEGMENT_RUNTIME.counter_mode_step = random(min_leds, max_leds);
  }

  setPixelColor(SEGMENT.start + random16(SEGMENT_LENGTH), color1);

  SEGMENT_RUNTIME.counter_mode_step--;
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Blink several LEDs on, reset, repeat.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
uint16_t WS2812FX_mode_twinkle(void) {
  return twinkle(SEGMENT.colors[0], SEGMENT.colors[1]);
}

/*
 * Blink several LEDs in random colors on, reset, repeat.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
uint16_t WS2812FX_mode_twinkle_random(void) {
  return twinkle(color_wheel(random8()), SEGMENT.colors[1]);
}


/*
 * fade out functions
 */
//void WS2812FX_fade_out() {
//  return fade_out(SEGMENT.colors[1]);
//}

void WS2812FX_fade_out(uint32_t targetColor) {
  static const uint8_t rateMapH[] = {0, 1, 1, 1, 2, 3, 4, 6};
  static const uint8_t rateMapL[] = {0, 2, 3, 8, 8, 8, 8, 8};

  uint8_t rate  = FADE_RATE;
  uint8_t rateH = rateMapH[rate];
  uint8_t rateL = rateMapL[rate];

  uint32_t color = targetColor;
  int w2 = (color >> 24) & 0xff;
  int r2 = (color >> 16) & 0xff;
  int g2 = (color >>  8) & 0xff;
  int b2 =  color        & 0xff;

  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    color = getPixelColor(i); // current color
    if(rate == 0) { // old fade-to-black algorithm
      setPixelColor(i, (color >> 1) & 0x7F7F7F7F);
    } else { // new fade-to-color algorithm
      int w1 = (color >> 24) & 0xff;
      int r1 = (color >> 16) & 0xff;
      int g1 = (color >>  8) & 0xff;
      int b1 =  color        & 0xff;

      // calculate the color differences between the current and target colors
      int wdelta = w2 - w1;
      int rdelta = r2 - r1;
      int gdelta = g2 - g1;
      int bdelta = b2 - b1;

      // if the current and target colors are almost the same, jump right to the target
      // color, otherwise calculate an intermediate color. (fixes rounding issues)
      wdelta = abs(wdelta) < 3 ? wdelta : (wdelta >> rateH) + (wdelta >> rateL);
      rdelta = abs(rdelta) < 3 ? rdelta : (rdelta >> rateH) + (rdelta >> rateL);
      gdelta = abs(gdelta) < 3 ? gdelta : (gdelta >> rateH) + (gdelta >> rateL);
      bdelta = abs(bdelta) < 3 ? bdelta : (bdelta >> rateH) + (bdelta >> rateL);
      
      rdelta =  r1 + rdelta;
      gdelta =  g1 + gdelta;
      bdelta =  b1 + bdelta;
      setPixelColor(i,((uint32_t)rdelta << 16) | ((uint32_t)gdelta <<  8) | bdelta);
    }
  }
}


/*
 * color blend function
 */
uint32_t WS2812FX_color_blend(uint32_t color1, uint32_t color2, uint8_t blend) {
  if(blend == 0)   return color1;
  if(blend == 255) return color2;

  uint8_t w1 = (color1 >> 24) & 0xff;
  uint8_t r1 = (color1 >> 16) & 0xff;
  uint8_t g1 = (color1 >>  8) & 0xff;
  uint8_t b1 =  color1        & 0xff;

  uint8_t w2 = (color2 >> 24) & 0xff;
  uint8_t r2 = (color2 >> 16) & 0xff;
  uint8_t g2 = (color2 >>  8) & 0xff;
  uint8_t b2 =  color2        & 0xff;

  uint32_t w3 = ((w2 * blend) + (w1 * (255U - blend))) / 256U;
  uint32_t r3 = ((r2 * blend) + (r1 * (255U - blend))) / 256U;
  uint32_t g3 = ((g2 * blend) + (g1 * (255U - blend))) / 256U;
  uint32_t b3 = ((b2 * blend) + (b1 * (255U - blend))) / 256U;

  return ((w3 << 24) | (r3 << 16) | (g3 << 8) | (b3));
}


/*
 * twinkle_fade function
 */
uint16_t WS2812FX_twinkle_fade(uint32_t color) {
  fade_out();

  if(random8(3) == 0) {
    uint8_t size = 1 << SIZE_OPTION;
    uint16_t index = SEGMENT.start + random16(SEGMENT_LENGTH - size);
    for(uint8_t i=0; i<size; i++) {
      setPixelColor(index + i, color);
    }
  }
  return (SEGMENT.speed / 8);
}


/*
 * Blink several LEDs on, fading out.
 */
uint16_t WS2812FX_mode_twinkle_fade(void) {
  return twinkle_fade(SEGMENT.colors[0]);
}


/*
 * Blink several LEDs in random colors on, fading out.
 */
uint16_t WS2812FX_mode_twinkle_fade_random(void) {
  return twinkle_fade(color_wheel(random8()));
}


/*
 * Blinks one LED at a time.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
uint16_t WS2812FX_mode_sparkle(void) {
  uint8_t size = 1 << SIZE_OPTION;
  for(uint8_t i=0; i<size; i++) {
    setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.aux_param3 + i, SEGMENT.colors[1]);
  }
  SEGMENT_RUNTIME.aux_param3 = random16(SEGMENT_LENGTH - size); // aux_param3 stores the random led index
  for(uint8_t i=0; i<size; i++) {
    setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.aux_param3 + i, SEGMENT.colors[0]);
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}


/*
 * Lights all LEDs in the color. Flashes single white pixels randomly.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
uint16_t WS2812FX_mode_flash_sparkle(void) {
  if(SEGMENT_RUNTIME.counter_mode_call == 0) {
    for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
      setPixelColor(i, SEGMENT.colors[0]);
    }
  }

  setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.aux_param3, SEGMENT.colors[0]);

  if(random8(5) == 0) {
    SEGMENT_RUNTIME.aux_param3 = random16(SEGMENT_LENGTH); // aux_param3 stores the random led index
    setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.aux_param3, WHITE);
    return 20;
  } 
  return SEGMENT.speed;
}


/*
 * Like flash sparkle. With more flash.
 * Inspired by www.tweaking4all.com/hardware/arduino/arduino-led-strip-effects/
 */
uint16_t WS2812FX_mode_hyper_sparkle(void) {
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, SEGMENT.colors[0]);
  }

  if(random8(5) < 2) {
    for(uint16_t i=0; i < max(1, SEGMENT_LENGTH/3); i++) {
      setPixelColor(SEGMENT.start + random16(SEGMENT_LENGTH), WHITE);
    }
    return 20;
  }
  return SEGMENT.speed;
}


/*
 * Strobe effect with different strobe count and pause, controlled by speed.
 */
uint16_t WS2812FX_mode_multi_strobe(void) {
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, BLACK);
  }

  uint16_t delay = 200 + ((9 - (SEGMENT.speed % 10)) * 100);
  uint16_t count = 2 * ((SEGMENT.speed / 100) + 1);
  if(SEGMENT_RUNTIME.counter_mode_step < count) {
    if((SEGMENT_RUNTIME.counter_mode_step & 1) == 0) {
      for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
        setPixelColor(i, SEGMENT.colors[0]);
      }
      delay = 20;
    } else {
      delay = 50;
    }
  }
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (count + 1);
  return delay;
}


/*
 * color chase function.
 * color1 = background color
 * color2 and color3 = colors of two adjacent leds
 */

uint16_t WS2812FX_chase(uint32_t color1, uint32_t color2, uint32_t color3) {
  uint8_t size = 1 << SIZE_OPTION;
  for(uint8_t i=0; i<size; i++) {
    uint16_t a = (SEGMENT_RUNTIME.counter_mode_step + i) % SEGMENT_LENGTH;
    uint16_t b = (a + size) % SEGMENT_LENGTH;
    uint16_t c = (b + size) % SEGMENT_LENGTH;
    if(IS_REVERSE) {
      setPixelColor(SEGMENT.stop - a, color1);
      setPixelColor(SEGMENT.stop - b, color2);
      setPixelColor(SEGMENT.stop - c, color3);
    } else {
      setPixelColor(SEGMENT.start + a, color1);
      setPixelColor(SEGMENT.start + b, color2);
      setPixelColor(SEGMENT.start + c, color3);
    }
  }

  if(SEGMENT_RUNTIME.counter_mode_step + (size * 3) == SEGMENT_LENGTH) SET_CYCLE;
  else CLR_CYCLE;

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  return (SEGMENT.speed / SEGMENT_LENGTH);
}


/*
 * Bicolor chase mode
 */
uint16_t WS2812FX_mode_bicolor_chase(void) {
  return chase(SEGMENT.colors[0], SEGMENT.colors[1], SEGMENT.colors[2]);
}


/*
 * White running on _color.
 */
uint16_t WS2812FX_mode_chase_color(void) {
  return chase(SEGMENT.colors[0], WHITE, WHITE);
}


/*
 * Black running on _color.
 */
uint16_t WS2812FX_mode_chase_blackout(void) {
  return chase(SEGMENT.colors[0], BLACK, BLACK);
}


/*
 * _color running on white.
 */
uint16_t WS2812FX_mode_chase_white(void) {
  return chase(WHITE, SEGMENT.colors[0], SEGMENT.colors[0]);
}


/*
 * White running followed by random color.
 */
uint16_t WS2812FX_mode_chase_random(void) {
  if(SEGMENT_RUNTIME.counter_mode_step == 0) {
    SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param);
  }
  return chase(color_wheel(SEGMENT_RUNTIME.aux_param), WHITE, WHITE);
}


/*
 * Rainbow running on white.
 */
uint16_t WS2812FX_mode_chase_rainbow_white(void) {
  uint16_t n = SEGMENT_RUNTIME.counter_mode_step;
  uint16_t m = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  uint32_t color2 = color_wheel(((n * 256 / SEGMENT_LENGTH) + (SEGMENT_RUNTIME.counter_mode_call & 0xFF)) & 0xFF);
  uint32_t color3 = color_wheel(((m * 256 / SEGMENT_LENGTH) + (SEGMENT_RUNTIME.counter_mode_call & 0xFF)) & 0xFF);

  return chase(WHITE, color2, color3);
}


/*
 * White running on rainbow.
 */
uint16_t WS2812FX_mode_chase_rainbow(void) {
  uint8_t color_sep = 256 / SEGMENT_LENGTH;
  uint8_t color_index = SEGMENT_RUNTIME.counter_mode_call & 0xFF;
  uint32_t color = color_wheel(((SEGMENT_RUNTIME.counter_mode_step * color_sep) + color_index) & 0xFF);

  return chase(color, WHITE, WHITE);
}


/*
 * Black running on rainbow.
 */
uint16_t WS2812FX_mode_chase_blackout_rainbow(void) {
  uint8_t color_sep = 256 / SEGMENT_LENGTH;
  uint8_t color_index = SEGMENT_RUNTIME.counter_mode_call & 0xFF;
  uint32_t color = color_wheel(((SEGMENT_RUNTIME.counter_mode_step * color_sep) + color_index) & 0xFF);

  return chase(color, BLACK, BLACK);
}


/*
 * White flashes running on _color.
 */
uint16_t WS2812FX_mode_chase_flash(void) {
  const static uint8_t flash_count = 4;
  uint8_t flash_step = SEGMENT_RUNTIME.counter_mode_call % ((flash_count * 2) + 1);

  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, SEGMENT.colors[0]);
  }

  uint16_t delay = (SEGMENT.speed / SEGMENT_LENGTH);
  if(flash_step < (flash_count * 2)) {
    if(flash_step % 2 == 0) {
      uint16_t n = SEGMENT_RUNTIME.counter_mode_step;
      uint16_t m = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
      if(IS_REVERSE) {
        setPixelColor(SEGMENT.stop - n, WHITE);
        setPixelColor(SEGMENT.stop - m, WHITE);
      } else {
        setPixelColor(SEGMENT.start + n, WHITE);
        setPixelColor(SEGMENT.start + m, WHITE);
      }
      delay = 20;
    } else {
      delay = 30;
    }
  } else {
    SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  }
  return delay;
}


/*
 * White flashes running, followed by random color.
 */
uint16_t WS2812FX_mode_chase_flash_random(void) {
  const static uint8_t flash_count = 4;
  uint8_t flash_step = SEGMENT_RUNTIME.counter_mode_call % ((flash_count * 2) + 1);

  for(uint16_t i=0; i < SEGMENT_RUNTIME.counter_mode_step; i++) {
    setPixelColor(SEGMENT.start + i, color_wheel(SEGMENT_RUNTIME.aux_param));
  }

  uint16_t delay = (SEGMENT.speed / SEGMENT_LENGTH);
  if(flash_step < (flash_count * 2)) {
    uint16_t n = SEGMENT_RUNTIME.counter_mode_step;
    uint16_t m = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
    if(flash_step % 2 == 0) {
      setPixelColor(SEGMENT.start + n, WHITE);
      setPixelColor(SEGMENT.start + m, WHITE);
      delay = 20;
    } else {
      setPixelColor(SEGMENT.start + n, color_wheel(SEGMENT_RUNTIME.aux_param));
      setPixelColor(SEGMENT.start + m, BLACK);
      delay = 30;
    }
  } else {
    SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;

    if(SEGMENT_RUNTIME.counter_mode_step == 0) {
      SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param);
    }
  }
  return delay;
}


/*
 * Alternating pixels running function.
 */
uint16_t WS2812FX_running(uint32_t color1, uint32_t color2) {
  uint8_t size = 4 << SIZE_OPTION;
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
    if((i + SEGMENT_RUNTIME.counter_mode_step) % size < (size / 2)) {
      if(IS_REVERSE) {
        setPixelColor(SEGMENT.start + i, color1);
      } else {
        setPixelColor(SEGMENT.stop - i, color1);
      }
    } else {
      if(IS_REVERSE) {
        setPixelColor(SEGMENT.start + i, color2);
      } else {
        setPixelColor(SEGMENT.stop - i, color2);
      }
    }
  }

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % size;
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Alternating color/white pixels running.
 */
uint16_t WS2812FX_mode_running_color(void) {
  return running(SEGMENT.colors[0], WHITE);
}


/*
 * Alternating red/blue pixels running.
 */
uint16_t WS2812FX_mode_running_red_blue(void) {
  return running(RED, BLUE);
}


/*
 * Alternating red/green pixels running.
 */
uint16_t WS2812FX_mode_merry_christmas(void) {
  return running(RED, GREEN);
}

/*
 * Alternating orange/purple pixels running.
 */
uint16_t WS2812FX_mode_halloween(void) {
  return running(PURPLE, ORANGE);
}


/*
 * Random colored pixels running.
 */
uint16_t WS2812FX_mode_running_random(void) {
  if(IS_REVERSE) {
    copyPixels(SEGMENT.start, SEGMENT.start + 1, SEGMENT_LENGTH - 1);
  } else {
    copyPixels(SEGMENT.start + 1, SEGMENT.start, SEGMENT_LENGTH - 1);
  }

  if(SEGMENT_RUNTIME.counter_mode_step == 0) {
    SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param);
    if(IS_REVERSE) {
      setPixelColor(SEGMENT.stop, color_wheel(SEGMENT_RUNTIME.aux_param));
    } else {
      setPixelColor(SEGMENT.start, color_wheel(SEGMENT_RUNTIME.aux_param));
    }
  }

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (2 << SIZE_OPTION);
  return (SEGMENT.speed / SEGMENT_LENGTH);
}


/*
 * K.I.T.T.
 */
uint16_t WS2812FX_mode_larson_scanner(void) {
  fade_out();

  if(SEGMENT_RUNTIME.counter_mode_step < SEGMENT_LENGTH) {
    if(IS_REVERSE) {
      setPixelColor(SEGMENT.stop - SEGMENT_RUNTIME.counter_mode_step, SEGMENT.colors[0]);
    } else {
      setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.counter_mode_step, SEGMENT.colors[0]);
    }
  } else {
    uint16_t index = (SEGMENT_LENGTH * 2) - SEGMENT_RUNTIME.counter_mode_step - 2;
    if(IS_REVERSE) {
      setPixelColor(SEGMENT.stop - index, SEGMENT.colors[0]);
    } else {
      setPixelColor(SEGMENT.start + index, SEGMENT.colors[0]);
    }
  }

  if(SEGMENT_RUNTIME.counter_mode_step % SEGMENT_LENGTH == 0) SET_CYCLE;
  else CLR_CYCLE;

  SEGMENT_RUNTIME.counter_mode_step++;
  if(SEGMENT_RUNTIME.counter_mode_step >= ((SEGMENT_LENGTH * 2) - 2)) {
    SEGMENT_RUNTIME.counter_mode_step = 0;
  }

  return (SEGMENT.speed / (SEGMENT_LENGTH * 2));
}


/*
 * Firing comets from one end.
 */
uint16_t WS2812FX_mode_comet(void) {
  fade_out();

  if(IS_REVERSE) {
    setPixelColor(SEGMENT.stop - SEGMENT_RUNTIME.counter_mode_step, SEGMENT.colors[0]);
  } else {
    setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.counter_mode_step, SEGMENT.colors[0]);
  }

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  return (SEGMENT.speed / SEGMENT_LENGTH);
}


/*
 * Fireworks function.
 */
uint16_t WS2812FX_fireworks(uint32_t color) {
  fade_out();

// for better performance, manipulate the Adafruit_NeoPixels pixels[] array directly
  uint8_t *pixels = getPixels();
  uint8_t bytesPerPixel = getNumBytesPerPixel(); // 3=RGB, 4=RGBW
  uint16_t startPixel = SEGMENT.start * bytesPerPixel + bytesPerPixel;
  uint16_t stopPixel = SEGMENT.stop * bytesPerPixel ;
  for(uint16_t i=startPixel; i <stopPixel; i++) {
    uint16_t tmpPixel = (pixels[i - bytesPerPixel] >> 2) +
      pixels[i] +
      (pixels[i + bytesPerPixel] >> 2);
    pixels[i] =  tmpPixel > 255 ? 255 : tmpPixel;
  }

  uint8_t size = 2 << SIZE_OPTION;
  if(!_triggered) {
    for(uint16_t i=0; i<max(1, SEGMENT_LENGTH/20); i++) {
      if(random8(10) == 0) {
        uint16_t index = SEGMENT.start + random16(SEGMENT_LENGTH - size);
        for(uint8_t j=0; j<size; j++) {
          setPixelColor(index + j, color);
        }
      }
    }
  } else {
    for(uint16_t i=0; i<max(1, SEGMENT_LENGTH/10); i++) {
      uint16_t index = SEGMENT.start + random16(SEGMENT_LENGTH - size);
      for(uint8_t j=0; j<size; j++) {
        setPixelColor(index + j, color);
      }
    }
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Firework sparks.
 */
uint16_t WS2812FX_mode_fireworks(void) {
  uint32_t color = BLACK;
  do { // randomly choose a non-BLACK color from the colors array
    color = SEGMENT.colors[random8(NUM_COLORS)];
  } while (color == BLACK);
  return fireworks(color);
}

/*
 * Random colored firework sparks.
 */
uint16_t WS2812FX_mode_fireworks_random(void) {
  return fireworks(color_wheel(random8()));
}


/*
 * Fire flicker function
 */
uint16_t WS2812FX_fire_flicker(int rev_intensity) {
  uint8_t w = (SEGMENT.colors[0] >> 24) & 0xFF;
  uint8_t r = (SEGMENT.colors[0] >> 16) & 0xFF;
  uint8_t g = (SEGMENT.colors[0] >>  8) & 0xFF;
  uint8_t b = (SEGMENT.colors[0]        & 0xFF);
  uint8_t lum = max(w, max(r, max(g, b))) / rev_intensity;
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    int flicker = random8(lum);
     r =  max(r - flicker, 0);
     g = max(g - flicker, 0);
     b = max(b - flicker, 0);
     setPixelColor(i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Random flickering.
 */
uint16_t WS2812FX_mode_fire_flicker(void) {
  return fire_flicker(3);
}

/*
* Random flickering, less intensity.
*/
uint16_t WS2812FX_mode_fire_flicker_soft(void) {
  return fire_flicker(6);
}

/*
* Random flickering, more intensity.
*/
uint16_t WS2812FX_mode_fire_flicker_intense(void) {
  return fire_flicker(1.7);
}


/*
 * Tricolor chase function
 */
uint16_t WS2812FX_tricolor_chase(uint32_t color1, uint32_t color2, uint32_t color3) {
  uint8_t sizeCnt = 1 << SIZE_OPTION;
  uint16_t index = SEGMENT_RUNTIME.counter_mode_call % (sizeCnt * 3);
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++, index++) {
    index = index % (sizeCnt * 3);

    uint32_t color = color3;
    if(index < sizeCnt) color = color1;
    else if(index < (sizeCnt * 2)) color = color2;

    if(IS_REVERSE) {
      setPixelColor(SEGMENT.start + i, color);
    } else {
      setPixelColor(SEGMENT.stop - i, color);
    }
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Tricolor chase mode
 */
uint16_t WS2812FX_mode_tricolor_chase(void) {
  return tricolor_chase(SEGMENT.colors[0], SEGMENT.colors[1], SEGMENT.colors[2]);
}


/*
 * Alternating white/red/black pixels running.
 */
uint16_t WS2812FX_mode_circus_combustus(void) {
  return tricolor_chase(RED, WHITE, BLACK);
}

/*
 * ICU mode
 */
uint16_t WS2812FX_mode_icu(void) {
  uint16_t dest = SEGMENT_RUNTIME.counter_mode_step & 0xFFFF;
 
  setPixelColor(SEGMENT.start + dest, SEGMENT.colors[0]);
  setPixelColor(SEGMENT.start + dest + SEGMENT_LENGTH/2, SEGMENT.colors[0]);

  if(SEGMENT_RUNTIME.aux_param3 == dest) { // pause between eye movements
    if(random8(6) == 0) { // blink once in a while
      setPixelColor(SEGMENT.start + dest, BLACK);
      setPixelColor(SEGMENT.start + dest + SEGMENT_LENGTH/2, BLACK);
      return 200;
    }
    SEGMENT_RUNTIME.aux_param3 = random16(SEGMENT_LENGTH/2);
    return 1000 + random16(2000);
  }

  setPixelColor(SEGMENT.start + dest, BLACK);
  setPixelColor(SEGMENT.start + dest + SEGMENT_LENGTH/2, BLACK);

  if(SEGMENT_RUNTIME.aux_param3 > SEGMENT_RUNTIME.counter_mode_step) {
    SEGMENT_RUNTIME.counter_mode_step++;
    dest++;
  } else if (SEGMENT_RUNTIME.aux_param3 < SEGMENT_RUNTIME.counter_mode_step) {
    SEGMENT_RUNTIME.counter_mode_step--;
    dest--;
  }

  setPixelColor(SEGMENT.start + dest, SEGMENT.colors[0]);
  setPixelColor(SEGMENT.start + dest + SEGMENT_LENGTH/2, SEGMENT.colors[0]);

  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Custom modes
 */
uint16_t WS2812FX_mode_custom_0() {
  //return customModes[0]();
}
uint16_t WS2812FX_mode_custom_1() {
  //return customModes[1]();
}
uint16_t WS2812FX_mode_custom_2() {
  //return customModes[2]();
}
uint16_t WS2812FX_mode_custom_3() {
  //return customModes[3]();
}

/*
 * Custom mode helpers
 */
//void WS2812FX_setCustomMode(uint16_t (*p)()) {
  //customModes[0] = p;
//}

//uint8_t WS2812FX_setCustomMode(const __FlashStringHelper* name, uint16_t (*p)()) {
//  static uint8_t custom_mode_index = 0;
//  return setCustomMode(custom_mode_index++, name, p);
//}

uint8_t WS2812FX_setCustomMode(uint8_t index, const char* name, uint16_t (*p)()) {
  if((FX_MODE_CUSTOM_0 + index) < MODE_COUNT) {
    _names[FX_MODE_CUSTOM_0 + index] = name; // store the custom mode name
    //customModes[index] = p; // store the custom mode

    return (FX_MODE_CUSTOM_0 + index);
  }
  return 0;
}

/*
 * Custom show helper
 */
void WS2812FX_setCustomShow(void (*p)()) {
  //customShow = p;
}

void WS2812FX(void)
{
      _mode[FX_MODE_STATIC]                  = &WS2812FX_mode_static;
      _mode[FX_MODE_BLINK]                   = &WS2812FX_mode_blink;
      _mode[FX_MODE_COLOR_WIPE]              = &WS2812FX_mode_color_wipe;
      _mode[FX_MODE_COLOR_WIPE_INV]          = &WS2812FX_mode_color_wipe_inv;
      _mode[FX_MODE_COLOR_WIPE_REV]          = &WS2812FX_mode_color_wipe_rev;
      _mode[FX_MODE_COLOR_WIPE_REV_INV]      = &WS2812FX_mode_color_wipe_rev_inv;
      _mode[FX_MODE_COLOR_WIPE_RANDOM]       = &WS2812FX_mode_color_wipe_random;
      _mode[FX_MODE_RANDOM_COLOR]            = &WS2812FX_mode_random_color;
      _mode[FX_MODE_SINGLE_DYNAMIC]          = &WS2812FX_mode_single_dynamic;
      _mode[FX_MODE_MULTI_DYNAMIC]           = &WS2812FX_mode_multi_dynamic;
      _mode[FX_MODE_RAINBOW]                 = &WS2812FX_mode_rainbow;
      _mode[FX_MODE_RAINBOW_CYCLE]           = &WS2812FX_mode_rainbow_cycle;
      _mode[FX_MODE_SCAN]                    = &WS2812FX_mode_scan;
      _mode[FX_MODE_DUAL_SCAN]               = &WS2812FX_mode_dual_scan;
      _mode[FX_MODE_FADE]                    = &WS2812FX_mode_fade;
      _mode[FX_MODE_THEATER_CHASE]           = &WS2812FX_mode_theater_chase;
      _mode[FX_MODE_THEATER_CHASE_RAINBOW]   = &WS2812FX_mode_theater_chase_rainbow;
      _mode[FX_MODE_TWINKLE]                 = &WS2812FX_mode_twinkle;
      _mode[FX_MODE_TWINKLE_RANDOM]          = &WS2812FX_mode_twinkle_random;
      _mode[FX_MODE_TWINKLE_FADE]            = &WS2812FX_mode_twinkle_fade;
      _mode[FX_MODE_TWINKLE_FADE_RANDOM]     = &WS2812FX_mode_twinkle_fade_random;
      _mode[FX_MODE_SPARKLE]                 = &WS2812FX_mode_sparkle;
      _mode[FX_MODE_FLASH_SPARKLE]           = &WS2812FX_mode_flash_sparkle;
      _mode[FX_MODE_HYPER_SPARKLE]           = &WS2812FX_mode_hyper_sparkle;
      _mode[FX_MODE_STROBE]                  = &WS2812FX_mode_strobe;
      _mode[FX_MODE_STROBE_RAINBOW]          = &WS2812FX_mode_strobe_rainbow;
      _mode[FX_MODE_MULTI_STROBE]            = &WS2812FX_mode_multi_strobe;
      _mode[FX_MODE_BLINK_RAINBOW]           = &WS2812FX_mode_blink_rainbow;
      _mode[FX_MODE_CHASE_WHITE]             = &WS2812FX_mode_chase_white;
      _mode[FX_MODE_CHASE_COLOR]             = &WS2812FX_mode_chase_color;
      _mode[FX_MODE_CHASE_RANDOM]            = &WS2812FX_mode_chase_random;
      _mode[FX_MODE_CHASE_RAINBOW]           = &WS2812FX_mode_chase_rainbow;
      _mode[FX_MODE_CHASE_FLASH]             = &WS2812FX_mode_chase_flash;
      _mode[FX_MODE_CHASE_FLASH_RANDOM]      = &WS2812FX_mode_chase_flash_random;
      _mode[FX_MODE_CHASE_RAINBOW_WHITE]     = &WS2812FX_mode_chase_rainbow_white;
      _mode[FX_MODE_CHASE_BLACKOUT]          = &WS2812FX_mode_chase_blackout;
      _mode[FX_MODE_CHASE_BLACKOUT_RAINBOW]  = &WS2812FX_mode_chase_blackout_rainbow;
      _mode[FX_MODE_COLOR_SWEEP_RANDOM]      = &WS2812FX_mode_color_sweep_random;
      _mode[FX_MODE_RUNNING_COLOR]           = &WS2812FX_mode_running_color;
      _mode[FX_MODE_RUNNING_RED_BLUE]        = &WS2812FX_mode_running_red_blue;
      _mode[FX_MODE_RUNNING_RANDOM]          = &WS2812FX_mode_running_random;
      _mode[FX_MODE_LARSON_SCANNER]          = &WS2812FX_mode_larson_scanner;
      _mode[FX_MODE_COMET]                   = &WS2812FX_mode_comet;
      _mode[FX_MODE_FIREWORKS]               = &WS2812FX_mode_fireworks;
      _mode[FX_MODE_FIREWORKS_RANDOM]        = &WS2812FX_mode_fireworks_random;
      _mode[FX_MODE_MERRY_CHRISTMAS]         = &WS2812FX_mode_merry_christmas;
      _mode[FX_MODE_FIRE_FLICKER]            = &WS2812FX_mode_fire_flicker;
      _mode[FX_MODE_FIRE_FLICKER_SOFT]       = &WS2812FX_mode_fire_flicker_soft;
      _mode[FX_MODE_FIRE_FLICKER_INTENSE]    = &WS2812FX_mode_fire_flicker_intense;
      _mode[FX_MODE_CIRCUS_COMBUSTUS]        = &WS2812FX_mode_circus_combustus;
      _mode[FX_MODE_HALLOWEEN]               = &WS2812FX_mode_halloween;
      _mode[FX_MODE_BICOLOR_CHASE]           = &WS2812FX_mode_bicolor_chase;
      _mode[FX_MODE_TRICOLOR_CHASE]          = &WS2812FX_mode_tricolor_chase;
      _mode[FX_MODE_BREATH]                  = &WS2812FX_mode_breath;
      _mode[FX_MODE_RUNNING_LIGHTS]          = &WS2812FX_mode_running_lights;
      _mode[FX_MODE_ICU]                     = &WS2812FX_mode_icu;
      _mode[FX_MODE_CUSTOM_0]                = &WS2812FX_mode_custom_0;
      _mode[FX_MODE_CUSTOM_1]                = &WS2812FX_mode_custom_1;
      _mode[FX_MODE_CUSTOM_2]                = &WS2812FX_mode_custom_2;
      _mode[FX_MODE_CUSTOM_3]                = &WS2812FX_mode_custom_3;

      brightness = DEFAULT_BRIGHTNESS + 1; // Adafruit_NeoPixel internally offsets brightness by 1
      _running = false;
      _num_segments = 1;
      _segments[0].mode = DEFAULT_MODE;
      _segments[0].colors[0] = DEFAULT_COLOR;
      _segments[0].start = 0;
      _segments[0].stop = stops - 1;
      _segments[0].speed = DEFAULT_SPEED;
      //resetSegmentRuntimes();
}

