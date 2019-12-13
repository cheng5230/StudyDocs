#include ".compile_usr_cfg.h"
#include "tuya_light_lib.h"
#include "tuya_light_hard.h"
#include "uf_file.h"

#include "spi_api.h"
#include "spi_ex_api.h"
#include "gpio_api.h"
#include "device.h"
#include "uni_mutex.h"
#include "led_display.h"
#include "ws2812b.h"
#include "mcu_api.h"
#include "tuya_light_lib.h"


segment _segments[DISPLAY_BUF_MAX_PIXEL];                  // SRAM footprint: 20 bytes per element 
segment_runtime _segment_runtimes[DISPLAY_BUF_MAX_PIXEL]; // SRAM footprint: 16 bytes per element

/* A PROGMEM (flash mem) table containing 8-bit unsigned sine wave (0-255).
   Copy & paste this snippet into a Python REPL to regenerate:
import math
for x in range(256):
    print("{:3},".format(int((math.sin(x/128.0*math.pi)+1.0)*127.5+0.5))),
    if x&15 == 15: print
*/
static const uint8_t  _NeoPixelSineTable[256] = {
  128,131,134,137,140,143,146,149,152,155,158,162,165,167,170,173,
  176,179,182,185,188,190,193,196,198,201,203,206,208,211,213,215,
  218,220,222,224,226,228,230,232,234,235,237,238,240,241,243,244,
  245,246,248,249,250,250,251,252,253,253,254,254,254,255,255,255,
  255,255,255,255,254,254,254,253,253,252,251,250,250,249,248,246,
  245,244,243,241,240,238,237,235,234,232,230,228,226,224,222,220,
  218,215,213,211,208,206,203,201,198,196,193,190,188,185,182,179,
  176,173,170,167,165,162,158,155,152,149,146,143,140,137,134,131,
  128,124,121,118,115,112,109,106,103,100, 97, 93, 90, 88, 85, 82,
   79, 76, 73, 70, 67, 65, 62, 59, 57, 54, 52, 49, 47, 44, 42, 40,
   37, 35, 33, 31, 29, 27, 25, 23, 21, 20, 18, 17, 15, 14, 12, 11,
   10,  9,  7,  6,  5,  5,  4,  3,  2,  2,  1,  1,  1,  0,  0,  0,
    0,  0,  0,  0,  1,  1,  1,  2,  2,  3,  4,  5,  5,  6,  7,  9,
   10, 11, 12, 14, 15, 17, 18, 20, 21, 23, 25, 27, 29, 31, 33, 35,
   37, 40, 42, 44, 47, 49, 52, 54, 57, 59, 62, 65, 67, 70, 73, 76,
   79, 82, 85, 88, 90, 93, 97,100,103,106,109,112,115,118,121,124};

/* Similar to above, but for an 8-bit gamma-correction table.
   Copy & paste this snippet into a Python REPL to regenerate:
import math
gamma=2.6
for x in range(256):
    print("{:3},".format(int(math.pow((x)/255.0,gamma)*255.0+0.5))),
    if x&15 == 15: print
*/
static const uint8_t  _NeoPixelGammaTable[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,
    3,  3,  4,  4,  4,  4,  5,  5,  5,  5,  5,  6,  6,  6,  6,  7,
    7,  7,  8,  8,  8,  9,  9,  9, 10, 10, 10, 11, 11, 11, 12, 12,
   13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20,
   20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29,
   30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 38, 38, 39, 40, 41, 42,
   42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
   58, 59, 60, 61, 62, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73, 75,
   76, 77, 78, 80, 81, 82, 84, 85, 86, 88, 89, 90, 92, 93, 94, 96,
   97, 99,100,102,103,105,106,108,109,111,112,114,115,117,119,120,
  122,124,125,127,129,130,132,134,136,137,139,141,143,145,146,148,
  150,152,154,156,158,160,162,164,166,168,170,172,174,176,178,180,
  182,184,186,188,191,193,195,197,199,202,204,206,209,211,213,215,
  218,220,223,225,227,230,232,235,237,240,242,245,247,250,252,255};

  
uint16_t UsrH,UsrS,UsrV;
uint16_t CurrH,CurrS,CurrV;
uint16_t GoalH,GoalS,GoalV;
uint8_t StremberCnts=1;
uint8_t StremberSteps;
uint8_t StremberFlag;
static uint32_t UsrColors;
static uint8_t chasecolorFlag;
static const uint8_t  WaterFlux[WATER_LEDS] = {80,80,80,80,80,80,80,80,200,200,200,200,200,200,255,255,255,255,255,255};
const uint8_t  ModeRUN[MODE_RUNS] = {FX_MODE_COLOR_WIPE,FX_MODE_COLOR_WIPE_INV,FX_MODE_COLOR_WIPE_REV,FX_MODE_COLOR_WIPE_REV_INV,FX_MODE_CHASE_WHITE,FX_MODE_CHASE_COLOR,FX_MODE_CHASE_RANDOM,FX_MODE_CHASE_RAINBOW,FX_MODE_CHASE_FLASH,
                                     FX_MODE_CHASE_FLASH_RANDOM,FX_MODE_CHASE_BLACKOUT,FX_MODE_CHASE_BLACKOUT_RAINBOW,FX_MODE_COLOR_SWEEP_RANDOM,FX_MODE_RUNNING_COLOR,FX_MODE_RUNNING_RANDOM,FX_MODE_CUSTOM_SPOTLIGHT,FX_MODE_CUSTOM_WATERDROP,
                                     FX_MODE_RANDOM_COLOR,FX_MODE_MULTI_DYNAMIC,FX_MODE_RAINBOW,FX_MODE_RAINBOW_CYCLE,FX_MODE_BREATH,FX_MODE_CUSTOM_BWAVEFADE1,FX_MODE_CUSTOM_BWAVEFADE2,FX_MODE_CUSTOM_BWAVEFADE3,FX_MODE_SCAN,FX_MODE_DUAL_SCAN,
                                     FX_MODE_ICU,FX_MODE_THEATER_CHASE,FX_MODE_THEATER_CHASE_RAINBOW,FX_MODE_SPARKLE,FX_MODE_STROBE,FX_MODE_MULTI_STROBE,FX_MODE_BLINK_RAINBOW,FX_MODE_RUNNING_LIGHTS,FX_MODE_CIRCUS_COMBUSTUS,FX_MODE_HALLOWEEN,
                                     FX_MODE_TRICOLOR_CHASE,FX_MODE_CUSTOM_FIXEDTWINKLE,FX_MODE_CUSTOM_ALTERTWINKLE,FX_MODE_CUSTOM_ALTERFADE,FX_MODE_CUSTOM_RANDOMTWINKLE,FX_MODE_CUSTOM_AUTOSWITCH,FX_MODE_CUSTOM_CONFETTI,
                                     FX_MODE_MERRY_CHRISTMAS,FX_MODE_FIRE_FLICKER_INTENSE,FX_MODE_FIRE_FLICKER_SOFT};

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
const char name_56[]  = "BWAVEFADE1"; 
const char name_57[]  = "BWAVEFADE2";
const char name_58[]  = "BWAVEFADE3";
const char name_59[]  = "SPOTLIGHT";
const char name_60[]  = "FIXEDTWINKLE";
const char name_61[]  = "ALTERTWINKLE";
const char name_62[]  = "ALTERFADE";
const char name_63[]  = "RANDOMTWINKLE";
const char name_64[]  = "AUTOSWITCH";
const char name_65[]  = "WATERDROP";
const char name_66[]  = "GLITTER";
const char name_67[]  = "CONFETTI";
const char name_68[]  = "Custom";


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
  (name_59),
  (name_60),
  (name_61),
  (name_62),
  (name_63),
  (name_64),
  (name_65),
  (name_66),
  (name_67),
  (name_68)
};

boolean	 _running;
boolean	 _triggered;
mode_ptr _mode[MODE_COUNT]; // SRAM footprint: 4 bytes per element
uint8_t _segment_index = 0;
uint8_t _num_segments = 1;
uint8_t  brightness;
uint8_t  *pixels;	  ///< Holds LED color values (3 or 4 bytes each)
uint8_t  rOffset;	  ///< Red index within each 3- or 4-byte pixel
uint8_t  gOffset;	  ///< Index of green byte
uint8_t  bOffset;	  ///< Index of blue byte
uint8_t  wOffset;	  ///< Index of white (==rOffset if no white)
uint8_t  rgbBuffer[DISPLAY_BUF_MAX_PIXEL*3]={0};
uint16_t _rand16seed;
uint16_t numLEDs =DISPLAY_BUF_MAX_PIXEL ;	  ///< Number of RGB LEDs in strip
volatile uint32_t ws_timercnt;
unsigned long now;
bool doShow;
uint8_t BreathFlag;
uint8_t PaomaFlag;
uint8_t FireFlag;
uint8_t ColorfulFlag;
uint8_t FlashFlag;
uint8_t ScanFlag;
uint8_t TripFlag;
uint8_t ScanFlag;
uint8_t UsrColorFlag;
uint8_t BwaveFlag;
uint8_t WaterFlag;
uint8_t BlinkCnt;
uint16_t Ms_heart;
uint8_t gHue = 0;  
uint8_t ModeRunIndex;
uint8_t UserIndex;
uint8_t UserModeNum;
uint16_t ModeRunTimes;
uint16_t twinkle_random;

int size;


/******************** 1ms 回调***************/
void ws2812_timer_handler(uint8_t type , uint8_t times , uint8_t scene_nums,uint8_t mode)
{
	 ws_timercnt ++;
	 Ms_heart ++;
   if(mcu_VoiceSpeed)
	 ModeRunTimes++;
	 WS2812FX_service(type,times,scene_nums,mode);
}


void WS2812FX_setSegment(uint8_t n, uint16_t start, uint16_t stop, uint8_t mode, const uint32_t colors[], uint16_t speed, uint8_t options) {
  //uint32_t colors[] = {color, 0, 0};
  if(n < (sizeof(_segments) / sizeof(_segments[0]))) 
  {
    if(n + 1 > _num_segments){
		_num_segments = n + 1;
    }
    _segments[n].start = start;
    _segments[n].stop = stop;
    _segments[n].mode = mode;
    _segments[n].speed = speed;
    _segments[n].options = options;
    for(uint8_t i=0; i<NUM_COLORS; i++) {
      _segments[n].colors[i] = colors[i];
    }
  }
}

void show(void)
{  
  uint8_t i;
  uint16_t j;
  for(i=0;i<DISPLAY_BUF_MAX_PIXEL;i++)
  {
      j = i*3;
      update_display_one_pixel(rgbBuffer[j+0],rgbBuffer[j+1],rgbBuffer[j+2], 0, 0, i);
  }
}

void start(uint8_t index) {
	uint8_t i;
	for(i=0;i<DISPLAY_BUF_MAX_PIXEL;i++)
	{
		_segments[i].start = 0;
		_segments[i].stop = 0;
		_segments[i].speed = 0;
		_segments[i].mode = 0;
		_segments[i].options = 0;
		_segments[i].colors[0] = 0;
		_segments[i].colors[1] = 0;
		_segments[i].colors[2] = 0;

    _segment_runtimes[i].next_time = 0;
		_segment_runtimes[i].counter_mode_step = 0;
		_segment_runtimes[i].counter_mode_call = 0;
		_segment_runtimes[i].aux_param = 0;
		_segment_runtimes[i].aux_param2 = 0;
		_segment_runtimes[i].aux_param3 = 0;

    rgbBuffer[i*3]=0;
		rgbBuffer[i*3+1]=0;
		rgbBuffer[i*3+2]=0;
	}

	/************* 变量初始化 ********************/
	brightness = DEFAULT_BRIGHTNESS + 1;
  _triggered = 0;
	_segment_index = 0;
	_running = 1;
	pixels = &rgbBuffer[0];
	size = DISPLAY_BUF_MAX_PIXEL; // calc the size of each segment

	/************* 分段功能模式操作 ********************/
	//size = DISPLAY_BUF_MAX_PIXEL/3; // calc the size of each segment
	//WS2812FX_setSegment(0, 0,       size-1,             FX_MODE_FADE,  RED,   2000, 0);
	//WS2812FX_setSegment(1, size,    size*2-1,           FX_MODE_STROBE,  BLUE,  2000, 1);
	//WS2812FX_setSegment(2, size*2,  DISPLAY_BUF_MAX_PIXEL-1, FX_MODE_CHASE_WHITE, GREEN, 2000, 1);

	/*此模式是在 colors[0] 和 colors[1] 以_segments[n].speed(500)/2 的速度进行闪烁  */
	//WS2812FX_setSegment(0, 0,       size-1,   FX_MODE_BLINK,  (uint32_t[]){RED,BLACK,BLACK},  500, 0);            // 模式1--- 报警闪烁 红色灯交替闪烁
	
	/* 此模式是七色变化:单色全亮，同时 以_segments[n].speed(500)的速度进行颜色切换，颜色变化与colors 数组设置无关  */
	//WS2812FX_setSegment(0, 0,       size-1,   FX_MODE_RANDOM_COLOR, (uint32_t[]){RED,BLACK,BLACK},  250, 0);     // 模式2---   音乐律动 七色全亮交替变化
	
	/* 此模式是全亮七彩色，七彩色在全灯带的位置动态交替变化  ,同时以_segments[n].speed(500) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	//WS2812FX_setSegment(0, 0,       size-1,   FX_MODE_MULTI_DYNAMIC, (uint32_t[]){RED,BLUE,BLUE},  500, 0);       // 模式3-----流光模式  R、G、B每个色系交替增减
	
	/* 此模式是七色变化:单色全呼吸，在最暗进行颜色切换，同时以_segments[n].speed(2000)/128 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	//WS2812FX_setSegment(0, 0,       size-1,   FX_MODE_FADE, (uint32_t[]){RED,BLACK,BLACK},  2000, 0);            // 模式4---   呼吸效果 渐明渐暗全呼吸--七色交替变化

	/* 此模式是七色变化:单色依次点亮直到全亮；切换颜色；同时以_segments[n].speed(500)/(DISPLAY_BUF_MAX_PIXEL * 2) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	//WS2812FX_setSegment(0, 0,       size-1,   FX_MODE_COLOR_WIPE, (uint32_t[]){RED,BLACK,BLACK},  500, 0);        // 模式5---     色颜波1 依次点亮，全亮然后切换颜色

	/* 此模式是七色变化:单色依次点亮直到全亮，反向依次灭，然后全灭；切换颜色；同时以_segments[n].speed(500)/(DISPLAY_BUF_MAX_PIXEL * 2) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	//WS2812FX_setSegment(0, 0,       size-1,   FX_MODE_COLOR_WIPE_REV, (uint32_t[]){RED,BLACK,BLACK},  500, 0);    // 模式6---   色颜波2,依次点亮，然后全亮，反向依次灭，全灭

	/* 此模式是呼吸波1:单色依次点亮直到全亮；呼吸渐灭，直到全灭，然后切换颜色；以_segments[n].speed(1000)/(64) 的速度进行变化，颜色变化与colors 数组设置无关   */
	//WS2812FX_setSegment(0, 0,         size-1,   FX_MODE_CUSTOM_BWAVEFADE1, (uint32_t[]){RED,BLACK,BLACK}, 1000, 0);    // 模式7---  呼吸波1,依次点亮，全亮；然后进行呼吸渐灭，然后切换颜色

	/* 此模式是呼吸波2:单色依次点亮直到全亮；呼吸渐灭，直到全灭，呼吸渐亮，到全亮；然后切换颜色；以_segments[n].speed(1000)/(64) 的速度进行变化，颜色变化与colors 数组设置无关   */
	//WS2812FX_setSegment(0, 0,         size-1,   FX_MODE_CUSTOM_BWAVEFADE2, (uint32_t[]){RED,BLACK,BLACK}, 1000, 0);    // 模式8---  呼吸波2,依次点亮，全亮；然后进行呼吸渐灭渐明，然后切换颜色

	/* 此模式是呼吸波3:单色依次点亮，射波同时进行呼吸渐明渐暗；射波全亮后，全灭；然后切换颜色；以_segments[n].speed(1000)/(64) 的速度进行变化，颜色变化与colors 数组设置无关   */
	//WS2812FX_setSegment(0, 0,         size-1,   FX_MODE_CUSTOM_BWAVEFADE3, (uint32_t[]){RED,BLACK,BLACK}, 1000, 0);    // 模式9---  呼吸波3,依次点亮，全亮；同时进行呼吸渐灭渐明，然后切换颜色

	/* 此模式是七色追光:七色全亮,变换颜色追光切换；同时以_segments[n].speed(1000)/(16) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	 // WS2812FX_setSegment(0, 0,         size-1,  FX_MODE_RAINBOW_CYCLE , (uint32_t[]){RED,BLACK,BLACK},  500, 0);     // 模式10---  追光效果 流光（七色）追逐变换
     
	/* 此模式是固定追光:红黄蓝绿紫固定追光；同时以_segments[n].speed(500)/(256) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	 // WS2812FX_setSegment(0, 0,         size-1, FX_MODE_CUSTOM_SPOTLIGHT  , (uint32_t[]){RED,BLACK,BLACK},  1000, 0); // 模式11---  追光效果 固定光（红黄蓝绿紫）顺序追光

	 /* 此模式是固定闪烁模式:红黄蓝绿紫固定光；同时以_segments[n].speed(1000)/(4) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	  //WS2812FX_setSegment(0, 0, 		size-1,   FX_MODE_CUSTOM_FIXEDTWINKLE, (uint32_t[]){RED,BLACK,BLACK},	1000, 0);	 // 模式12---  固定闪烁

	  /* 此模式是交替闪烁模式:以流光变化；同时以_segments[n].speed(100)/(4) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	  // WS2812FX_setSegment(0, 0, 		size-1,   FX_MODE_CUSTOM_ALTERTWINKLE, (uint32_t[]){RED,BLACK,BLACK},	1000, 0);	 // 模式13---  交替闪烁

	  /* 此模式是交替闪烁模式:以流光/或者红黄蓝绿紫固定光变化且同时呼吸；以_segments[n].speed(500)/(4) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	  // WS2812FX_setSegment(0, 0, 		size-1,   FX_MODE_CUSTOM_ALTERFADE, (uint32_t[]){RED,BLACK,BLACK},	500, 0);	     // 模式14---  呼吸闪烁
	  
	  /* 此模式是随机闪烁模式:以时间最为基准，生成随机数选择对应灯闪烁；以_segments[n].speed(500)/(4) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	  // WS2812FX_setSegment(0, 0, 		size-1,   FX_MODE_CUSTOM_RANDOMTWINKLE, (uint32_t[]){RED,BLACK,BLACK},	500, 0);	     // 模式15---  随机闪烁

	 /* 此模式是自动变换模式 在流光模式和随机闪烁模式来回切换；运行时间10s ;以_segments[n].speed(500)/(4) 的速度进行颜色切换，颜色变化与colors 数组设置无关   */
	 // WS2812FX_setSegment(0, 0, 		size-1,   FX_MODE_CUSTOM_AUTOSWITCH, (uint32_t[]){RED,BLACK,BLACK},	500, 0);	         // 模式16---自动变换
	   
	/* 此模式是水滴效果:20颗灯全亮,三段亮度；同时以_segments[n].speed(1000)/(128) 的速度从头到尾流水，七色依次变化   */
	// WS2812FX_setSegment(0, 0,         size-1,   FX_MODE_CUSTOM_WATERDROP, (uint32_t[]){RED,BLACK,BLACK},  1000, 0); 

	/* 此模式是彩虹三段流水效果；同时以_segments[n].speed(1000)/(256) 的速度从头到尾流水   */
	// WS2812FX_setSegment(0, 0,         size-1,   FX_MODE_CUSTOM_GLITTER, (uint32_t[]){RED,BLACK,BLACK},  500, 0); 

	/* 此模式是七彩多段满天星: 同时以_segments[n].speed(1000)/(128) 的速度从头到尾流水   */
	//WS2812FX_setSegment(0, 0,         size-1,   FX_MODE_CUSTOM_CONFETTI, (uint32_t[]){RED,BLACK,BLACK},  1000, 0); 

	/* 此模式是火焰效果  ---全灯带火焰流动 */
	//WS2812FX_setSegment(0, 0,         size-1,   FX_MODE_FIRE_FLICKER_INTENSE, (uint32_t[]){RED,BLACK,BLACK},  2000, 0); 

	WS2812FX_setSegment(0, 0,         size-1,   index, (uint32_t[]){BLUE,BLACK,BLACK},  2000, 0); 
}

void stop(void) {
  uint8_t i;
  _running = 0;
  for(i=0;i<DISPLAY_BUF_MAX_PIXEL;i++)
  {
  	rgbBuffer[i*3] = 0;
  	rgbBuffer[i*3+1] = 0;
  	rgbBuffer[i*3+2] = 0;
  }
  show();
}

void WS2812FX_Init(void) {
  WS2812FX();
  start(ModeRUN[0]);
}

void WS2812FX_service(uint8_t type , uint8_t times,uint8_t scene_num,uint8_t mode)
{
  uint8_t i;
  uint16_t delay;
	if(_running) 
  {
	    now = ws_timercnt;
	    doShow = 0;
      for(i=0; i < _num_segments; i++) 
      {
        _segment_index = i;
        CLR_FRAME;
        
        if(now > SEGMENT_RUNTIME.next_time) 
        {
          SET_FRAME;
          doShow = 1;
          //delay = (*_mode[SEGMENT.mode])();
          if(mode == SCENE_MODE)
          {
              g_scenemode = type;
              g_dpmode = 1;
              /*********** 场景模式模式切换处理**************/
              /*** (times---65-5) ********/
              if(type == SCENE_TYPE_STATIC)
              {
                /************* 流光效果 **********************/
                delay = WS2812Fx_Streamber(type,scene_num,times);
              }
              else if(type == SCENE_TYPE_BREATH)
              {
                /************* 呼吸效果 **********************/
                delay = WS2812Fx_Breath(type,scene_num,times);
              }
              else if(type == SCENE_TYPE_RAIN_DROP)
              {
                /************* 火焰效果 **********************/
                delay = WS2812Fx_Fire(type,scene_num,times);
              }
              else if(type == SCENE_TYPE_COLORFULL)
              {
                /************* 炫彩效果 **********************/
                delay = WS2812Fx_Colorful(type,scene_num,times);
              }
              else if(type == SCENE_TYPE_MARQUEE)
              {
                 /************* 跑马灯效果 **********************/
                delay = WS2812Fx_PaoMa(type,scene_num,times);
              }
              else if(type == SCENE_TYPE_BLINKING)
              {
                /************* 用户闪烁效果 **********************/
                delay = WS2812Fx_Flash(type,scene_num,times);
              }
              else if(type == SCENE_TYPE_SNOWFLAKE)
              {
                /************* 扫描效果 **********************/
                delay = WS2812Fx_Scan(type,scene_num,times);
              }
              else if(type == SCENE_TYPE_STREAMER_COLOR)
              {
                /************* 跳变效果 **********************/
                delay = WS2812Fx_Trip(type,scene_num,times);
              }
              else 
              {
                light_shade_stop();
              }
          }
          else if(mode == MUSIC_MODE)
          {
              g_scenemode = type;
              g_dpmode = 0;
              if(type == SCENE_TYPE_BLINKING)
              {
                /************* 用户闪烁效果 **********************/
                delay = WS2812Fx_Flash(type,scene_num,times);
              }
              else if(type == SCENE_TYPE_SNOWFLAKE)
              {
                /************* 扫描效果 **********************/
                delay = WS2812Fx_Scan(type,scene_num,times);
              }
              else if(type == SCENE_TYPE_STREAMER_COLOR)
              {
                /************* 跳变效果 **********************/
                delay = WS2812Fx_Trip(type,scene_num,times);
              }
              else
              {
                 /************* 跑马灯效果 **********************/
                delay = WS2812Fx_PaoMa(type,scene_num,times);
              }
          }
          SEGMENT_RUNTIME.next_time = now + max(delay, SPEED_MIN);
          SEGMENT_RUNTIME.counter_mode_call++;
        }
      }
   }
}

/*!
  @brief   Query the color of a previously-set pixel.
  @param   n  Index of pixel to read (0 = first).
  @return  'Packed' 32-bit RGB or WRGB value. Most significant byte is white
           (for RGBW pixels) or 0 (for RGB pixels), next is red, then green,
           and least significant byte is blue.
  @note    If the strip brightness has been changed from the default value
           of 255, the color read from a pixel may not exactly match what
           was previously written with one of the setPixelColor() functions.
           This gets more pronounced at lower brightness levels.
*/
uint32_t getPixelColor(uint16_t n) {
	if(n >= numLEDs){ return 0;} // Out of bounds, return no color.
	uint32_t *p;
    // Is RGB-type device
    p = (uint32_t*) &pixels[n * 3];
    if(brightness) {
      // Stored color was decimated by setBrightness(). Returned value
      // attempts to scale back to an approximation of the original 24-bit
      // value used when setting the pixel color, but there will always be
      // some error -- those bits are simply gone. Issue is most
      // pronounced at low brightness levels.
      return (((uint32_t)(p[0] << 8) / brightness) << 16) |
             (((uint32_t)(p[1] << 8) / brightness) <<  8) |
             ( (uint32_t)(p[2] << 8) / brightness       );
    } else {
      // No brightness adjustment has been made -- return 'raw' color
      return (uint32_t)((uint32_t)p[0] << 16) |
             (uint32_t)((uint32_t)p[1] <<  8) |
              (uint32_t)p[2];
    }  
}

uint8_t *getPixels(void) 
{	
	return pixels; 
}

uint8_t getNumBytesPerPixel(void) 
{
	wOffset = rOffset;
	return (wOffset == rOffset) ? 3 : 4; // 3=RGB, 4=RGBW
}

void setPixelColor(uint16_t n, uint32_t c) {
  
	uint8_t r,g,b;
	if(n < numLEDs) 
  {
		r = (uint8_t)(c >> 16);
		g = (uint8_t)(c >>	8);
		b = (uint8_t)c;
    
		if(brightness) 
    {
       // See notes in setBrightness()
       /** 声音亮度控制 **/
       if((g_scenemode == SCENE_TYPE_STREAMER_COLOR)||(g_scenemode == SCENE_TYPE_SNOWFLAKE))
       {
         r = ((r * mcu_Voicelum) >> 8);
      	 g =((g * mcu_Voicelum) >> 8);
      	 b = ((b * mcu_Voicelum) >> 8);
       }
       else
       {
         if(g_dpmode)
         {
            r = (r * brightness) >> 8;
      		  g = (g * brightness) >> 8;
      	  	b = (b * brightness) >> 8;
         }
         else
         {
             r = ((r * mcu_Voicelum) >> 8);
          	 g =((g * mcu_Voicelum) >> 8);
          	 b = ((b * mcu_Voicelum) >> 8);
         }
      }
    }
    
		rgbBuffer[n*3]=r;
		rgbBuffer[n*3+1]=g;
		rgbBuffer[n*3+2]=b;
	}
}

void copyPixels(uint16_t dest, uint16_t src, uint16_t count) {

  uint8_t *pixels_t = getPixels();
  uint8_t bytesPerPixel = getNumBytesPerPixel(); // 3=RGB, 4=RGBW
  memmove(pixels_t + (dest * bytesPerPixel), pixels_t + (src * bytesPerPixel), count * bytesPerPixel);
}

// fast 8-bit random number generator shamelessly borrowed from FastLED
uint8_t random8(void) {
    _rand16seed = (_rand16seed * 2053) + 13849;
    return (uint8_t)((uint16_t)(_rand16seed + (_rand16seed >> 8)) & 0xFF);
}

// note random8(lim) generates numbers in the range 0 to (lim -1)
uint8_t random8Var(uint8_t lim) {
    uint8_t r = random8();
    r = (r * lim) >> 8;
    return r;
}

uint16_t random16(void) {
    return (uint16_t)random8() * 256 + random8();
}

// note random16(lim) generates numbers in the range 0 to (lim - 1)
uint16_t random16Var(uint16_t lim) {
    uint16_t r = random16();
    r = ((uint32_t)r * lim) >> 16;
    return r;
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
uint32_t color_wheel(uint8_t pos) {
  pos = 255 - pos;
  if(pos < 85) {
    return ((uint32_t)(255 - pos * 3) << 16) | ((uint32_t)(0) << 8) | (uint32_t)(pos * 3);
  } else if(pos < 170) {
    pos -= 85;
    return ((uint32_t)(0) << 16) | ((uint32_t)(pos * 3) << 8) | (uint32_t)(255 - pos * 3);
  } else {
    pos -= 170;
    return ((uint32_t)(pos * 3) << 16) | ((uint32_t)(255 - pos * 3) << 8) | (uint32_t)(0);
  }
}

uint32_t Usercolor_wheel(uint8_t pos) {

  uint16_t h, s, v;
  uint8_t r1,g1,b1;
  uint32_t color;
  
  /******流光呼吸效果**********/
  if(pos<76)
  {
    CurrV = 1000 - pos*2;
    h = CurrH; s = CurrS; v = CurrV;
  }
  else if(pos<127)
  {
    if(StremberSteps==0)
    {
       h = CurrH +  (pos-76)/2;
    }
    else
    {
       h = CurrH +  (pos-76)*StremberSteps;  
    }
    GoalH = h; s = GoalS; v = CurrV;
  }
  else if(pos<204)
  {
     CurrV = 848 + (pos-128)*2;
     h = CurrH; s = CurrS; v = CurrV;
  }
  else 
  {
    if(StremberSteps==0)
    {
       h = GoalH -  (HSV_MAXSTEPS-(255-pos))/2; 
    }
    else
    {
       h = GoalH - (HSV_MAXSTEPS-(255-pos))*StremberSteps; 
    }
    s = GoalS; v = CurrV;
  }
  
  hsv2rgb ( (FLOAT) h, (FLOAT) s/1000.0, (FLOAT) v/1000.0, &r1, &g1, &b1);
  color = (uint32_t)((r1 << 16)) | (uint32_t)((g1 << 8)) | (uint32_t)((b1));
  //PR_NOTICE("%d Hsv= %d %d %d RGB = %d %d %d",pos,h,s,v,r1,g1,b1);    
  return color;
}

/*
 * Color wipe function
 * LEDs are turned on (color1) in sequence, then turned off (color2) in sequence.
 * if (bool rev == 1) then LEDs are turned off in reverse order
 */
uint16_t color_wipe(uint32_t color1, uint32_t color2, bool rev) {

  if(SEGMENT_RUNTIME.counter_mode_step < SEGMENT_LENGTH) 
  {
    uint32_t led_offset = SEGMENT_RUNTIME.counter_mode_step;
    
    if (rev)
    {
      setPixelColor(SEGMENT.stop - led_offset, color1);
    } 
    else 
    {
      setPixelColor(SEGMENT.start + led_offset, color1);
    }
  } 
   else 
   {
    uint32_t led_offset = SEGMENT_RUNTIME.counter_mode_step - SEGMENT_LENGTH;
    if(rev) 
    {
      setPixelColor(SEGMENT.stop - led_offset, color2);
    } 
    else
    {
      setPixelColor(SEGMENT.start + led_offset, color2);
    }
  }
   
  if(SEGMENT_RUNTIME.counter_mode_step % SEGMENT_LENGTH == 0){ SET_CYCLE;}
  else {CLR_CYCLE;}

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (SEGMENT_LENGTH * 2);
  /******************跑马灯效果切换处理**************/
  if(SEGMENT_RUNTIME.counter_mode_step == 0)
  {
      PaomaFlag = 1;
     StremberFlag = 1;
  }
  else
  {
      PaomaFlag = 0;
     StremberFlag = 0;
  }
  return (SEGMENT.speed / (SEGMENT_LENGTH * 2));
}

/*
 * Blink/strobe function
 * Alternate between color1 and color2
 * if(strobe == 1) then create a strobe effect
 */
uint16_t blink(uint32_t color1, uint32_t color2, bool strobe) {

  uint32_t color = ((SEGMENT_RUNTIME.counter_mode_call & 1) == 0) ? color1 : color2;
  
  if(IS_REVERSE) {color = (color == color1) ? color2 : color1;}
  
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color);
  }

  if((SEGMENT_RUNTIME.counter_mode_call & 1) == 0) {
    return strobe ? 20 : (SEGMENT.speed / 2);
  } else {
    return strobe ? SEGMENT.speed : (SEGMENT.speed / 2);
  }
}

/*
 * Returns a new, random wheel index with a minimum distance of 42 from pos.
 */
uint8_t get_random_wheel_index(uint8_t pos) {
  uint8_t r = 0;
  uint8_t x = 0;
  uint8_t y = 0;
  uint8_t d = 0;

  while(d < 42) {
    r = random8();
    x = abs(pos - r);
    y = 255 - x;
    d = min(x, y);
  }
  return r;
}

/*
 * Alternating pixels running function.
 */
uint16_t running(uint32_t color1, uint32_t color2) {
  //uint8_t size_u = 4 << SIZE_OPTION;
  uint8_t size_u;
  
  size_u = SEGMENT_LENGTH>>3;
  
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++) 
  {
    if(((i + SEGMENT_RUNTIME.counter_mode_step) % GROUP_PIXEL_NUM) < (GROUP_PIXEL_NUM/2))
    {
      if(IS_REVERSE) {
        setPixelColor(SEGMENT.start + i, color1);
      } else {
        setPixelColor(SEGMENT.stop - i, color1);
      }
    } 
    else 
    {
      if(IS_REVERSE) {
        setPixelColor(SEGMENT.start + i, color2);
      } else {
        setPixelColor(SEGMENT.stop - i, color2);
      }
    }
  }
  
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  //SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % size_u;
  if(SEGMENT_RUNTIME.counter_mode_step==0)
  {
    return (100);
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

uint16_t chase(uint32_t color1, uint32_t color2, uint32_t color3) {
uint8_t size_u = 1 << SIZE_OPTION;

  if (chasecolorFlag)
  {
      for(uint8_t i=0; i<SEGMENT.stop ; i++) 
      {
         setPixelColor(i, color1);
      }
  }

  for(uint8_t i=0; i<size_u; i++) 
  {
    uint16_t a = (SEGMENT_RUNTIME.counter_mode_step + i) % SEGMENT_LENGTH;
    uint16_t b = (a + size_u) % SEGMENT_LENGTH;
    uint16_t c = (b + size_u) % SEGMENT_LENGTH;

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
  
  if(SEGMENT_RUNTIME.counter_mode_step + (size_u * 3) == SEGMENT_LENGTH) {SET_CYCLE;}
  else {CLR_CYCLE;}

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  
  /*** 跑马灯效果切换处理 ****/
  if(SEGMENT_RUNTIME.counter_mode_call > SEGMENT_LENGTH)
  {
      PaomaFlag = 1;
      StremberFlag = 1;
  }
  else
  {
      PaomaFlag = 0;
      StremberFlag = 0;
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

uint8_t random(uint8_t mins,uint8_t maxs)
{
	return rand()%(maxs - mins) + mins;
}

/*
 * scan function - runs a block of pixels back and forth.
 */
uint16_t scan(uint32_t color1, uint32_t color2, bool dual) 
{
  int8_t dir;
  uint8_t size_u;
  
  if(SEGMENT_RUNTIME.counter_mode_step == 0) {SEGMENT_RUNTIME.aux_param = 0;}
  if(SEGMENT_RUNTIME.counter_mode_step >= (SEGMENT_LENGTH - size_u)) {SEGMENT_RUNTIME.aux_param = 1;}
  
  dir = SEGMENT_RUNTIME.aux_param ? -1 : 1;
  size_u = 1 << SIZE_OPTION;

  //PR_NOTICE("--------- %d %d %d", dir,size_u,SEGMENT_RUNTIME.counter_mode_step);
  
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color2);
  }

  for(uint8_t i = 0; i < size_u; i++) {
    if(IS_REVERSE || dual) {
      setPixelColor(SEGMENT.stop - SEGMENT_RUNTIME.counter_mode_step - i, color1);
    }
    if((!IS_REVERSE) || dual) {
      setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.counter_mode_step + i, color1);
    }
  }
  
  SEGMENT_RUNTIME.counter_mode_step += dir;
  
  if(SEGMENT_RUNTIME.counter_mode_step == 0) {SEGMENT_RUNTIME.aux_param = 0;}
  if(SEGMENT_RUNTIME.counter_mode_step >= (SEGMENT_LENGTH - size_u)) {SEGMENT_RUNTIME.aux_param = 1;}

  return (SEGMENT.speed / (SEGMENT_LENGTH * 2));
}

void fade_out(uint32_t targetColor) {
  static const uint8_t rateMapH[] = {0, 1, 1, 1, 2, 3, 4, 6};
  static const uint8_t rateMapL[] = {0, 2, 3, 8, 8, 8, 8, 8};

  uint8_t rate  = FADE_RATE;
  uint8_t rateH = rateMapH[rate];
  uint8_t rateL = rateMapL[rate];

  uint32_t color = targetColor;
  uint32_t w2 = (color >> 24) & 0xff;
  uint32_t r2 = (color >> 16) & 0xff;
  uint32_t g2 = (color >>  8) & 0xff;
  uint32_t b2 =  color        & 0xff;

  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    color = getPixelColor(i); // current color
		
    if(rate == 0) { // old fade-to-black algorithm
      setPixelColor(i, (color >> 1) & 0x7F7F7F7F);
    } else { // new fade-to-color algorithm
      uint32_t w1 = (color >> 24) & 0xff;
      uint32_t r1 = (color >> 16) & 0xff;
      uint32_t g1 = (color >>  8) & 0xff;
      uint32_t b1 =  color        & 0xff;
      
      // calculate the color differences between the current and target colors
      uint32_t wdelta = w2 - w1;
      uint32_t rdelta = r2 - r1;
      uint32_t gdelta = g2 - g1;
      uint32_t bdelta = b2 - b1;

      // if the current and target colors are almost the same, jump right to the target
      // color, otherwise calculate an intermediate color. (fixes rounding issues)
      wdelta = abs(wdelta) < 3 ? wdelta : (wdelta >> rateH) + (wdelta >> rateL);
      rdelta = abs(rdelta) < 3 ? rdelta : (rdelta >> rateH) + (rdelta >> rateL);
      gdelta = abs(gdelta) < 3 ? gdelta : (gdelta >> rateH) + (gdelta >> rateL);
      bdelta = abs(bdelta) < 3 ? bdelta : (bdelta >> rateH) + (bdelta >> rateL);
      
      setPixelColor(i, (r1 + rdelta)<<16+(g1 + gdelta)<<8+(b1 + bdelta));
    }
  }
}

/*
 * twinkle function
 */
uint16_t twinkle(uint32_t color1, uint32_t color2) {

  if(SEGMENT_RUNTIME.counter_mode_step == 0) 
  {
    for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
      setPixelColor(i, color2);
    }
    uint16_t min_leds = max(1, SEGMENT_LENGTH / 5); // make sure, at least one LED is on
    uint16_t max_leds = max(1, SEGMENT_LENGTH / 2); // make sure, at least one LED is on
    SEGMENT_RUNTIME.counter_mode_step = random(min_leds, max_leds);
  }

  setPixelColor(SEGMENT.start + random16Var(SEGMENT_LENGTH), color1);

  SEGMENT_RUNTIME.counter_mode_step--;
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * twinkle_fade function
 */
uint16_t twinkle_fade(uint32_t color) {
  fade_out(SEGMENT.colors[1]);

  if(random8Var(3) == 0) {
    uint8_t size_u = 1 << SIZE_OPTION;
    uint16_t index = SEGMENT.start + random16Var(SEGMENT_LENGTH - size_u);
    for(uint8_t i=0; i<size_u; i++) {
      setPixelColor(index + i, color);
    }
  }
  return (SEGMENT.speed / 8);
}

/*
 * Tricolor chase function
 */
uint16_t tricolor_chase(uint32_t color1, uint32_t color2, uint32_t color3) {
  uint8_t sizeCnt = 1 << SIZE_OPTION;
  uint16_t index = SEGMENT_RUNTIME.counter_mode_call % (sizeCnt * 3);
  
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
    index = index % (sizeCnt * 3);

    uint32_t color = color3;
    if(index < sizeCnt) {color = color1;}
    else {if(index < (sizeCnt * 2)) {color = color2;}}

    if(IS_REVERSE) {
      setPixelColor(SEGMENT.start + i, color);
    } else {
      setPixelColor(SEGMENT.stop - i, color);
    }
	  index++;
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * color blend function
 */
uint32_t color_blend(uint32_t color1, uint32_t color2, uint8_t blend) {
  if(blend == 0)   {return color1;}
  if(blend == 255) {return color2;}

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
 * Fireworks function.
 */
uint16_t fireworks(uint32_t color) {
  fade_out(SEGMENT.colors[1]);

// for better performance, manipulate the Adafruit_NeoPixels pixels[] array directly
  uint8_t *pixels_u = getPixels();
  uint8_t bytesPerPixel =3;  //getNumBytesPerPixel(); // 3=RGB, 4=RGBW
  uint16_t startPixel = SEGMENT.start * bytesPerPixel + bytesPerPixel;
  uint16_t stopPixel = SEGMENT.stop * bytesPerPixel ;
  for(uint16_t i=startPixel; i <stopPixel; i++) {
    uint16_t tmpPixel = (pixels_u[i - bytesPerPixel] >> 2) +
      pixels_u[i] +
      (pixels_u[i + bytesPerPixel] >> 2);
    pixels_u[i] =  tmpPixel > 255 ? 255 : tmpPixel;
  }

  uint8_t size_u = 2 << SIZE_OPTION;
  if(!FireFlag) 
  {
    FireFlag = 1;
    for(uint16_t i=0; i<max(1, SEGMENT_LENGTH/20); i++) {
      if(random8Var(10) == 0) {
        uint16_t index = SEGMENT.start + random16Var(SEGMENT_LENGTH - size_u);
        for(uint8_t j=0; j<size_u; j++) {
          setPixelColor(index + j, color);
        }
      }
    }
  } 
  else 
  {
    FireFlag = 0;
    for(uint16_t i=0; i<max(1, SEGMENT_LENGTH/10); i++) {
      uint16_t index = SEGMENT.start + random16Var(SEGMENT_LENGTH - size_u);
      for(uint8_t j=0; j<size_u; j++) {
        setPixelColor(index + j, color);
      }
    }
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Fire flicker function
 */
uint16_t fire_flicker(int rev_intensity) {

  uint8_t w = (RED >> 24) & 0xFF;
  uint8_t r = (RED >> 16) & 0xFF;
  uint8_t g = (RED >>  8) & 0xFF;
  uint8_t b = (RED        & 0xFF);
  uint8_t lum = max(w, max(r, max(g, b))) / rev_intensity;
  /*************** 分三组 ***********/

  for(uint16_t i=SEGMENT.start; i < DISPLAY_FIRINGNUMS; i++) {
    int flicker = random8Var(lum);
    r =  max(r - flicker, 0);
    g =  max(g - flicker, 0);
    b =  max(b - flicker, 0);
    setPixelColor(i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
  }
  
  for(uint16_t i=DISPLAY_FIRINGNUMS; i <(DISPLAY_FIRINGNUMS+DISPLAY_FIRINGNUMS); i++) {
    int flicker = random8Var(lum);
    r =  max(r - flicker, 0);
    g =  max(g - flicker, 0);
    b =  max(b - flicker, 0);
    setPixelColor(i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
  }

  for(uint16_t i=(DISPLAY_FIRINGNUMS+DISPLAY_FIRINGNUMS); i <= DISPLAY_BUF_MAX_PIXEL; i++) {
    int flicker = random8Var(lum);
    r =  max(r - flicker, 0);
    g =  max(g - flicker, 0);
    b =  max(b - flicker, 0);
    setPixelColor(i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
  }
  
  return (SEGMENT.speed / SEGMENT_LENGTH);
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
  return blink(SEGMENT.colors[0], SEGMENT.colors[1], 0);
}

/*
 * Classic Blink effect. Cycling through the rainbow.
 */
uint16_t WS2812FX_mode_blink_rainbow(void) {
  static uint32_t colors;
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (SEGMENT_LENGTH);
  if((SEGMENT_RUNTIME.counter_mode_step%GROUP_PIXEL_NUM)==0)
  {
     colors = __GetWs2812FxColors(random8Var(UserModeNum),SCENE_TYPE_BLINKING);
  }
  else
  {
      if(colors==0)
      {
          colors = UsrColors;
      }
  }
  return blink(colors, BLACK, 0);
}

/*
 * Classic Strobe effect.
 */
uint16_t WS2812FX_mode_strobe(void) {

  //GetUserColors(0,0);
  //SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (0xff);
  return blink(UsrColors, BLACK, 1);
}


/*
 * Classic Strobe effect. Cycling through the rainbow.
 */
uint16_t WS2812FX_mode_strobe_rainbow(void) {
  return blink(color_wheel(SEGMENT_RUNTIME.counter_mode_call & 0xFF), SEGMENT.colors[1], 1);
}


/*
 * Color wipe function
 * LEDs are turned on (color1) in sequence, then turned off (color2) in sequence.
 * if (bool rev == 1) then LEDs are turned off in reverse order
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
    if((IS_REVERSE && (!rev)) || ((!IS_REVERSE) && rev)) {
      setPixelColor(SEGMENT.stop - led_offset, color2);
    } else {
      setPixelColor(SEGMENT.start + led_offset, color2);
    }
  }

  if(SEGMENT_RUNTIME.counter_mode_step % SEGMENT_LENGTH == 0) {SET_CYCLE;}
  else {CLR_CYCLE;}

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (SEGMENT_LENGTH * 2);
  return (SEGMENT.speed / (SEGMENT_LENGTH * 2));
}

/*
 * Lights all LEDs one after another.
 */
uint16_t WS2812FX_mode_color_wipe(void) {

  //GetUserColors(0,0);
  uint32_t colors;
  uint8_t r,g,b;
  r = (UsrColors>>16)>>2;
  g = (UsrColors>>8)>>2;
  b = (UsrColors&&0xff)>>2;
  colors = ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
  return color_wipe(UsrColors,colors, 0);
}

uint16_t WS2812FX_mode_color_wipe_inv(void) {
  //GetUserColors(0,0);
  uint32_t colors;
  uint8_t r,g,b;
  r = (UsrColors>>16)>>2;
  g = (UsrColors>>8)>>2;
  b = (UsrColors&&0xff)>>2;
  colors = ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
  return color_wipe(UsrColors, colors, 1);
}

uint16_t WS2812FX_mode_color_wipe_rev(void) {

  //GetUserColors(0,2);  
  return color_wipe(UsrColors, BLACK, 0);
}

uint16_t WS2812FX_mode_color_wipe_rev_inv(void) {
  //GetUserColors(0,2);
  return color_wipe(UsrColors, BLACK, 1);
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
  return color_wipe(color, color, 0) * 2;
}


/*
 * Random color introduced alternating from start and end of strip.
 */
uint16_t WS2812FX_mode_color_sweep_random(void) {
  static uint8_t revse;
  
  if(SEGMENT_RUNTIME.counter_mode_step % SEGMENT_LENGTH == 0) { // aux_param will store our random color wheel index
    SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param);
    if(revse)
    revse = 0;
    else
    revse = 1;
  }
  uint32_t color = color_wheel(SEGMENT_RUNTIME.aux_param);

  return color_wipe(color, color, revse) * 2;
}


/*
 * Lights all LEDs in one random color up. Then switches them
 * to the next random color.
 */
uint16_t WS2812FX_mode_random_color(void) {

  //SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param); // aux_param will store our random color wheel index
  //uint32_t color = color_wheel(SEGMENT_RUNTIME.aux_param);
  //GetUserColors(0,0);
  static uint8_t testcnt = 0;
  static uint32_t colors = 0;
  
  if(SEGMENT_RUNTIME.counter_mode_step == 0)
  {
    testcnt = (++testcnt >= UserModeNum)? 0 : testcnt;
    colors = __GetWs2812FxColors(testcnt,SCENE_TYPE_STREAMER_COLOR);
  }
  else
  {
      if(colors==0)
        colors = UsrColors;
  }
  
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) 
  {
    setPixelColor(i, colors);
  }
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % GROUP_PIXEL_NUM;

  return (SEGMENT.speed / 256);
}


/*
 * Lights every LED in a random color. Changes one random LED after the other
 * to another random color.
 */
uint16_t WS2812FX_mode_single_dynamic(void) {
 #if 0
  if(SEGMENT_RUNTIME.counter_mode_call == 0) {
    for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
      setPixelColor(i, color_wheel(random8()));
    }
  }
  setPixelColor(SEGMENT.start + random16Var(SEGMENT_LENGTH), color_wheel(random8()));
  return (SEGMENT.speed/100);
  #else
    uint16_t i = 0;
    uint8_t temp = 0;
    uint8_t colorcnt = 0;
    
    for(i=0; i <= SEGMENT.stop; i++) 
    {
      temp = (i + SEGMENT_RUNTIME.counter_mode_step)%SEGMENT_LENGTH;
        
      if((temp % GROUP_PIXEL_NUM)==0)
      {
        colorcnt = (++colorcnt >= UserModeNum)? 0 : colorcnt;
        UsrColors = __GetWs2812FxColors(colorcnt,SCENE_TYPE_STREAMER_COLOR);
      }
      
      setPixelColor(temp, UsrColors);
    }
    SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
    
    return (SEGMENT.speed);
   #endif
}


/*
 * Lights every LED in a random color. Changes all LED at the same time
 * to new random colors.
 */
uint16_t WS2812FX_mode_multi_dynamic(void) {
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color_wheel(random8()));
  }
  /**** 运行次数控制 ****/
  if(SEGMENT_RUNTIME.counter_mode_call>=(SEGMENT.stop))
  {
    PaomaFlag = 1;
  }
  else
  {
    PaomaFlag = 0;
  }
  return (SEGMENT.speed/50);
}


/*
 * Does the "standby-breathing" of well known i-Devices. Fixed Speed.
 * Use mode "fade" if you like to have something similar with a different speed.
 */
uint16_t WS2812FX_mode_breath(void) {
  
  uint16_t delay;
  if(SEGMENT_RUNTIME.counter_mode_step==0)
    SEGMENT_RUNTIME.counter_mode_step = 5;
  
  int lum = SEGMENT_RUNTIME.counter_mode_step;
  
  if(lum > 255) {lum = 511 - lum;} // lum = 15 -> 255 -> 15
  if(lum == 5)  {delay = 170;} // 970 pause before each breath
  else if(lum <=  25) {delay = 38;} // 19
  else if(lum <=  50) {delay = 36;} // 18
  else if(lum <=  75) {delay = 28;} // 14
  else if(lum <= 100) {delay = 20;} // 10
  else if(lum <= 125) {delay = 14;} // 7
  else if(lum <= 150) {delay = 11;} // 5
  else {delay = 10;}                // 4

  #if 1
  uint32_t color = UsrColors;
  uint8_t w = (color >> 24 & 0xFF) * lum / 256;
  uint8_t r = (color >> 16 & 0xFF) * lum / 256;
  uint8_t g = (color >>  8 & 0xFF) * lum / 256;
  uint8_t b = (color       & 0xFF) * lum / 256;
  #else
  GetUserColors(1,lum);
  uint8_t w = (UsrColors >> 24 & 0xFF) * lum / 256;
  uint8_t r = (UsrColors >> 16 & 0xFF) * lum / 256;
  uint8_t g = (UsrColors >>  8 & 0xFF) * lum / 256;
  uint8_t b = (UsrColors       & 0xFF) * lum / 256;
  #endif
  
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
  }
  SEGMENT_RUNTIME.counter_mode_step += 2;
  if(SEGMENT_RUNTIME.counter_mode_step > (512-15)) 
  {
    SEGMENT_RUNTIME.counter_mode_step = 5;
    BreathFlag = 1;
  }
  return delay/10;
}


/*
 * Fades the LEDs between two colors
 */
uint16_t WS2812FX_mode_fade(void) {

  #if 1
  int lum = SEGMENT_RUNTIME.counter_mode_step;
  if(lum > 255) lum = 511 - lum; // lum = 0 -> 255 -> 0
  
  uint32_t color = color_blend(SEGMENT.colors[0], SEGMENT.colors[1], lum);
  
  
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, color);
  }

  SEGMENT_RUNTIME.counter_mode_step += 4;
  if(SEGMENT_RUNTIME.counter_mode_step > 511) 
  {
	  SEGMENT_RUNTIME.counter_mode_step = 0;
  }
  #else
  int lum = SEGMENT_RUNTIME.counter_mode_step;
  if(lum > 255) {lum = 511 - lum;} // lum = 0 -> 255 -> 0
  /***************在呼吸最暗的时候，进行颜色的切换----lum=255  *************/
  GetUserColors(1,lum);
  uint32_t color = color_blend(UsrColors, BLACK, lum);
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++)
  {
    setPixelColor(i, color);
  }

  SEGMENT_RUNTIME.counter_mode_step += 4;
  if(SEGMENT_RUNTIME.counter_mode_step > 511) 
  {
    SEGMENT_RUNTIME.counter_mode_step = 0;
  }
  #endif
  
  return (SEGMENT.speed / 128);
}

/*
 * Runs a block of pixels back and forth.
 */
uint16_t WS2812FX_mode_scan(void) {
  //GetUserColors(0, 255);
  uint32_t colors;
  if(colors!=UsrColors)
  {
    colors = UsrColors;
    for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
      setPixelColor(SEGMENT.start + i, BLACK);
    }
  }
  return scan(colors, BLACK, 0);
}


/*
 * Runs two blocks of pixels back and forth in opposite directions.
 */
uint16_t WS2812FX_mode_dual_scan(void) {
  //GetUserColors(0, 255);
  uint32_t colors;
  if(colors!=UsrColors)
  {
    colors = UsrColors;
    for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
      setPixelColor(SEGMENT.start + i, BLACK);
    }
  }
  return scan(colors, BLACK, 1);
}

/*
 * Cycles all LEDs at once through a rainbow.
 */
uint16_t WS2812FX_mode_rainbow(void) {

  uint16_t i = 0;
  uint32_t color = color_wheel(SEGMENT_RUNTIME.counter_mode_step);

  for(i=0; i <= SEGMENT.stop; i++) 
  {
    setPixelColor(i, color);
  }
  
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) & 0xff;

  if(SEGMENT_RUNTIME.counter_mode_step==0)
  {
    BreathFlag = 1;
  }
  else
  {
    BreathFlag = 0;
  }
  return (SEGMENT.speed/100);
}

uint16_t WS2812FX_mode_rainbow_single(void) 
{
    uint8_t pos;
    uint16_t temp;
    
    if(StremberCnts!=__scene_cnt)
    {
      StremberCnts = __scene_cnt;
      __GetWs2812FxColors(StremberCnts,SCENE_TYPE_STATIC);
      CurrH = UsrH; CurrS = UsrS; CurrV = UsrV;
      StremberCnts = (++StremberCnts >= UserModeNum)? 0 : StremberCnts;
      __GetWs2812FxColors(StremberCnts,SCENE_TYPE_STATIC);
      GoalH = UsrH; GoalS = UsrS; GoalV = UsrV;
      StremberCnts = __scene_cnt;
      /****** 步数计算 ******/
      if(CurrH==GoalH)
      {
        StremberSteps = 0;
      }
      else
      {
        if(CurrH>GoalH)
        {
           StremberSteps = (CurrH-GoalH)/HSV_MAXSTEPS;
           temp = GoalH;
           GoalH = CurrH;
           CurrH = temp;
        }
        else
        {
          StremberSteps = (GoalH-CurrH)/HSV_MAXSTEPS;
        }
      }
      PR_NOTICE("----- %d %d %d %d %d %d  %d",StremberCnts,CurrH,CurrS,CurrV,GoalH,GoalS,GoalV);    
    }
    
    for(uint16_t i=0; i < SEGMENT_LENGTH; i++) 
    {
      pos = ((i * 256 / SEGMENT_LENGTH) + SEGMENT_RUNTIME.counter_mode_step) & 0xff;
      uint32_t color = Usercolor_wheel(pos);
      //PR_NOTICE("----- %d %d %x",pos,i,color);    
      setPixelColor(SEGMENT.start + i, color);
    }
    SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) & 0xff;
    
    if(SEGMENT_RUNTIME.counter_mode_step==0)
    {
        StremberFlag = 1;
    }
    else
    {
        StremberFlag = 0;
    }
    //return (SEGMENT.speed);
    return (SEGMENT.speed / 256);
}

/*
 * Cycles a rainbow over the entire string of LEDs.
 */
uint16_t WS2812FX_mode_rainbow_cycle(void) 
{
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
      uint32_t color = color_wheel(((i * 256 / SEGMENT_LENGTH) + SEGMENT_RUNTIME.counter_mode_step) & 0xFF);
      setPixelColor(SEGMENT.start + i, color);
  }

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) & 0xFF;
  if(SEGMENT_RUNTIME.counter_mode_step==0)
  {
      StremberFlag = 1;
  }
  else
  {
      StremberFlag = 0;
  }
  return (SEGMENT.speed / 256);
}


/*
 * Theatre-style crawling lights.
 * Inspired by the Adafruit examples.
 */
uint16_t WS2812FX_mode_theater_chase(void) {

  //GetUserColors(0,0);
  //SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (SEGMENT_LENGTH);
  return tricolor_chase(UsrColors, BLACK,BLACK);
}


/*
 * Theatre-style crawling lights with rainbow effect.
 * Inspired by the Adafruit examples.
 */
uint16_t WS2812FX_mode_theater_chase_rainbow(void) {
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) & 0xFF;
  uint32_t color = color_wheel(SEGMENT_RUNTIME.counter_mode_step);
  return tricolor_chase(color, BLACK,BLACK);
}


/*
 * Running lights effect with smooth sine transition.
 */
uint16_t WS2812FX_mode_running_lights(void) {
  uint8_t r,g,b;

  //GetUserColors(0,0);
  uint8_t wtemp = ((RED >> 24) & 0xFF);
  uint8_t rtemp = ((RED >> 16) & 0xFF);
  uint8_t gtemp = ((RED >>  8) & 0xFF);
  uint8_t btemp =  (RED        & 0xFF);

  uint8_t size_u = 1 << SIZE_OPTION;
  uint8_t sineIncr = max(1, (256 / SEGMENT_LENGTH) * size_u);
  
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
    int lum = (int)_NeoPixelSineTable[(((i + SEGMENT_RUNTIME.counter_mode_step) * sineIncr))];
    if(IS_REVERSE) {
      r =  (rtemp * lum) / 256;
      g =  (gtemp * lum) / 256;
      b =  (btemp * lum) / 256;
      setPixelColor(SEGMENT.start + i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
    } else {
      r =  (rtemp * lum) / 256;
      g =  (gtemp * lum) / 256;
      b =  (btemp * lum) / 256;
      setPixelColor(SEGMENT.stop - i,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
    }
  }
  
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % 256;
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
  
  static uint32_t colors;
  
  if(twinkle_random++>=(SEGMENT.stop/6))
  {
     twinkle_random = 0;
     colors = __GetWs2812FxColors(random8Var(UserModeNum),SCENE_TYPE_BLINKING);
  }
  else
  {
      if(colors == 0)
      {
        colors = UsrColors;
      }
  }
  return twinkle(colors, SEGMENT.colors[1]);
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
  static uint32_t colors;
  uint8_t size_u = 1 << SIZE_OPTION;
  
  for(uint8_t i=0; i<size_u; i++) 
  {
    setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.aux_param3 + i, SEGMENT.colors[1]);
  }
  SEGMENT_RUNTIME.aux_param3 = random16Var(SEGMENT_LENGTH - size_u); // aux_param3 stores the random led index
  /******** 颜色切换处理 **********/
  //GetUserColors(0,255);
  if(colors!=UsrColors)
  {
    colors = UsrColors;
    for(uint8_t i=0; i<SEGMENT_LENGTH; i++) 
    {
       setPixelColor(SEGMENT.start + i, BLACK);
    }
  }
  
  for(uint8_t i=0; i<size_u; i++) 
  {
    setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.aux_param3 + i, colors);
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

  if(random8Var(5) == 0) {
    SEGMENT_RUNTIME.aux_param3 = random16Var(SEGMENT_LENGTH); // aux_param3 stores the random led index
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

  if(random8Var(5) < 2) {
    for(uint16_t i=0; i < max(1, SEGMENT_LENGTH/3); i++) {
      setPixelColor(SEGMENT.start + random16Var(SEGMENT_LENGTH), WHITE);
    }
    return 20;
  }
  return SEGMENT.speed;
}


/*
 * Strobe effect with different strobe count and pause, controlled by speed.
 */
uint16_t WS2812FX_mode_multi_strobe(void) {
  uint16_t delay;
  
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, BLACK);
  }
  //GetUserColors(0,0);
  //uint16_t delay = 200 + ((9 - (SEGMENT.speed % 10)) * 100);
  delay = 50;
  uint16_t count = 2 * ((SEGMENT.speed / 100) + 1);
  if(SEGMENT_RUNTIME.counter_mode_step < count) {
    if((SEGMENT_RUNTIME.counter_mode_step & 1) == 0) {
      for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
        setPixelColor(i, UsrColors);
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
 * Bicolor chase mode
 */
uint16_t WS2812FX_mode_bicolor_chase(void) {
  return chase(SEGMENT.colors[0], SEGMENT.colors[1], SEGMENT.colors[2]);
}


/*
 * White running on _color.
 */
uint16_t WS2812FX_mode_chase_color(void) {
  //GetUserColors(0,0);
  chasecolorFlag = 1;
  return chase(UsrColors, WHITE, WHITE);
}


/*
 * Black running on _color.
 */
uint16_t WS2812FX_mode_chase_blackout(void) {
  //GetUserColors(0,0);

  return chase(UsrColors, BLACK, BLACK);
}


/*
 * _color running on white.
 */
uint16_t WS2812FX_mode_chase_white(void) {
  //GetUserColors(0,0);
  chasecolorFlag = 0;
  return chase(WHITE, UsrColors, UsrColors);
}


/*
 * White running followed by random color.
 */
uint16_t WS2812FX_mode_chase_random(void) {
  static uint32_t colors;
  #if 0
  if(SEGMENT_RUNTIME.counter_mode_step == 0) {
    SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param);
  }
  return chase(color_wheel(SEGMENT_RUNTIME.aux_param), WHITE, WHITE);
  #endif
  chasecolorFlag = 0;
  if(SEGMENT_RUNTIME.counter_mode_step == 0)
  {
     colors = __GetWs2812FxColors(random8Var(UserModeNum),SCENE_TYPE_MARQUEE);
  }
  else
  {
      if(colors == 0)
      {
        colors = UsrColors;
      }
  }
  return chase(colors, WHITE, WHITE);
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
uint16_t WS2812FX_mode_chase_rainbow(void) 
{
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
  
  //GetUserColors(0,0);
  
  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) {
    setPixelColor(i, UsrColors);
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
      delay = 2;
    } else {
      delay = 3;
    }
  } else {
    SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  }
  
  if(SEGMENT_RUNTIME.counter_mode_step == 0)
  {
      PaomaFlag = 1;
  }
  else
  {
      PaomaFlag = 0;
  }
  return delay;
}


/*
 * White flashes running, followed by random color.
 */
uint16_t WS2812FX_mode_chase_flash_random(void) {
  const static uint8_t flash_count = 4;
  uint32_t colors;
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
      delay = 2;
    } else {
      setPixelColor(SEGMENT.start + n, color_wheel(SEGMENT_RUNTIME.aux_param));
      setPixelColor(SEGMENT.start + m, BLACK);
      delay = 3;
    }
  } 
  else 
  {
    SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;

    if(SEGMENT_RUNTIME.counter_mode_step == (SEGMENT_LENGTH-1)) 
    {
      PaomaFlag = 1;
      SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param);
      if(colors == 0)
      {
        colors = UsrColors;
      }
      else
      {
        colors = __GetWs2812FxColors(random8Var(UserModeNum),SCENE_TYPE_MARQUEE);
      }
    }
    else
    {
      PaomaFlag = 0;
    }
  }
  return delay;
}

/*
 * Alternating color/white pixels running.
 */
uint16_t WS2812FX_mode_running_color(void) {
  //GetUserColors(0,0);

  return running(UsrColors, WHITE);
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
  return running(RED, RED/4);
}

/*
 * Alternating orange/purple pixels running.
 */
uint16_t WS2812FX_mode_halloween(void) {
  //GetUserColors(0,0);
  //SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  return running(UsrColors, BLACK);
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

  if(SEGMENT_RUNTIME.counter_mode_step == 0) 
  {
      SEGMENT_RUNTIME.aux_param = get_random_wheel_index(SEGMENT_RUNTIME.aux_param);
      if(IS_REVERSE) {
        setPixelColor(SEGMENT.stop, color_wheel(SEGMENT_RUNTIME.aux_param));
      } else {
        setPixelColor(SEGMENT.start, color_wheel(SEGMENT_RUNTIME.aux_param));
      }
      /**** 运行次数控制 ****/
      if(SEGMENT_RUNTIME.counter_mode_call>=(SEGMENT.stop))
      {
        PaomaFlag = 1;
      }
      else
      {
        PaomaFlag = 0;
      }
  }

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % (2 << SIZE_OPTION);
  
  return (SEGMENT.speed / SEGMENT_LENGTH);
}


/*
 * K.I.T.T.
 */
uint16_t WS2812FX_mode_larson_scanner(void) {
  fade_out(SEGMENT.colors[1]);

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

  if(SEGMENT_RUNTIME.counter_mode_step % SEGMENT_LENGTH == 0) {SET_CYCLE;}
  else {CLR_CYCLE;}

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
  fade_out(SEGMENT.colors[1]);

  if(IS_REVERSE) {
    setPixelColor(SEGMENT.stop - SEGMENT_RUNTIME.counter_mode_step, SEGMENT.colors[0]);
  } else {
    setPixelColor(SEGMENT.start + SEGMENT_RUNTIME.counter_mode_step, SEGMENT.colors[0]);
  }

  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  return (SEGMENT.speed / SEGMENT_LENGTH);
}


/*
 * Firework sparks.
 */
uint16_t WS2812FX_mode_fireworks(void) {
  uint32_t color = BLACK;
  do { // randomly choose a non-BLACK color from the colors array
    color = SEGMENT.colors[random8Var(NUM_COLORS)];
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
 * Random flickering.
 */
uint16_t WS2812FX_mode_fire_flicker(void) {
  return fire_flicker(3);
}

/*
* Random flickering, less intensity.
*/
uint16_t WS2812FX_mode_fire_flicker_soft(void) {
  return fire_flicker(20);
}

/*
* Random flickering, more intensity.
*/
 uint8_t scale8_video( uint8_t i, uint8_t scale)
{
	uint8_t j = (((uint32_t)i * (uint32_t)scale) >> 8) + ((i&&scale)?1:0);
        return j;
}

uint32_t HeatColor( uint8_t temperature)
{
	uint32_t heatcolor;
    uint8_t r,g,b;
	// Scale 'heat' down from 0-255 to 0-191,
	// which can then be easily divided into three
	// equal 'thirds' of 64 units each.
	uint8_t t192 = scale8_video( temperature, 191);

	// calculate a value that ramps up from
	// zero to 255 in each 'third' of the scale.
	uint8_t heatramp = t192 & 0x3F; // 0..63
	heatramp <<= 2; // scale up to 0..252

	// now figure out which third of the spectrum we're in:
	if( t192 & 0x80) {
		// we're in the hottest third
		r = 255; // full red
		g = 255; // full green
		b = heatramp; // ramp up blue

	} else if( t192 & 0x40 ) {
		// we're in the middle third
		r = 255; // full red
		g = heatramp; // ramp up green
		b = 0; // no blue

	} else {
		// we're in the coolest third
		r = heatramp; // ramp up red
		g = 0; // no green
		b = 0; // no blue
	}
    heatcolor = ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
	return heatcolor;
}

uint16_t WS2812FX_mode_fire_flicker_intense(void) {

  // Array of temperature readings at each simulation cell
  static uint8_t heat[DISPLAY_BUF_MAX_PIXEL];
  uint32_t color;

  // Step 1.  Cool down every cell a little
   /**** 分三组火焰 *********/
    for( int i = 0; i < DISPLAY_FIRINGNUMS; i++) {
      heat[i] = qsub8( heat[i],  random8Var((((COOLING * 10) / DISPLAY_FIRINGNUMS) + 2)));
    }
    
    for( int i = DISPLAY_FIRINGNUMS; i < (DISPLAY_FIRINGNUMS+DISPLAY_FIRINGNUMS); i++) {
      heat[i] = qsub8( heat[i],  random8Var((((COOLING * 10) / (DISPLAY_FIRINGNUMS+DISPLAY_FIRINGNUMS)) + 2)));
    }

    for( int i = (DISPLAY_FIRINGNUMS+DISPLAY_FIRINGNUMS); i < DISPLAY_BUF_MAX_PIXEL; i++) {
      heat[i] = qsub8( heat[i],  random8Var((((COOLING * 10) / DISPLAY_BUF_MAX_PIXEL) + 2)));
    }
      
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    /**** 分三组火焰 *********/
    for( int k= DISPLAY_FIRINGNUMS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }

    for( int k= DISPLAY_FIRINGNUMS+DISPLAY_FIRINGNUMS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }

    for( int k= DISPLAY_BUF_MAX_PIXEL - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
      
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
        /**** 分三组火焰 *********/
      if(random8() < SPARKING) {
        int y = random8Var(7);
        heat[y] = qadd8(heat[y], random8Var(95)+160);
      }
  
       if(random8() < SPARKING) {
        int y = random8Var(7+DISPLAY_FIRINGNUMS);
        heat[y] = qadd8(heat[y], random8Var(95)+160);
      }
  
        if(random8() < SPARKING) {
        int y = random8Var(7+DISPLAY_FIRINGNUMS+DISPLAY_FIRINGNUMS);
        heat[y] = qadd8(heat[y], random8Var(95)+160);
      }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < DISPLAY_BUF_MAX_PIXEL; j++) {
	  
      color = HeatColor( heat[j]);
      int pixelnumber;
      //pixelnumber = (DISPLAY_BUF_MAX_PIXEL-1) - j;
      pixelnumber = j;
       
      rgbBuffer[pixelnumber*3]   = (color >> 16)& 0xff;
	    rgbBuffer[pixelnumber*3+1] = (color >> 8)& 0;
	    rgbBuffer[pixelnumber*3+2] = color& 0;
    }
          
	  return (SEGMENT.speed / 128);
}


/*
 * Tricolor chase function
 */
uint16_t WS2812FX_tricolor_chase(uint32_t color1, uint32_t color2, uint32_t color3) {
  uint8_t sizeCnt = 1 << SIZE_OPTION;
  uint16_t index = SEGMENT_RUNTIME.counter_mode_call % (sizeCnt * 3);
  for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
    index = index % (sizeCnt * 3);

    uint32_t color = color3;
    if(index < sizeCnt) {color = color1;}
    else{ if(index < (sizeCnt * 2)) {color = color2;}}

    if(IS_REVERSE) {
      setPixelColor(SEGMENT.start + i, color);
    } else {
      setPixelColor(SEGMENT.stop - i, color);
    }
	 index++;
  }
  return (SEGMENT.speed / SEGMENT_LENGTH);
}

/*
 * Tricolor chase mode
 */
uint16_t WS2812FX_mode_tricolor_chase(void) {
  //GetUserColors(0,0);
  //SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  return tricolor_chase(UsrColors, BLACK, BLACK);
}


/*
 * Alternating white/red/black pixels running.
 */
uint16_t WS2812FX_mode_circus_combustus(void) {
  //GetUserColors(0, 0);
  //SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  return tricolor_chase(UsrColors, WHITE, WHITE);
}

/*
 * ICU mode
 */
uint16_t WS2812FX_mode_icu(void) {

  static  uint32_t colors = 0;
  static  uint8_t modecnts = 0;
  
  uint16_t dest = SEGMENT_RUNTIME.counter_mode_step & 0xFFFF;

  if(colors==0)
  {
    colors = UsrColors;
    for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
      setPixelColor(SEGMENT.start + i, BLACK);
    }
  }
  
  setPixelColor(SEGMENT.start + dest, colors);
  setPixelColor(SEGMENT.start + dest + SEGMENT_LENGTH/2, colors);

  if(SEGMENT_RUNTIME.aux_param3 == dest) 
  { 
    // pause between eye movements
    if(random8Var(6) == 0) { // blink once in a while
      setPixelColor(SEGMENT.start + dest, BLACK);
      setPixelColor(SEGMENT.start + dest + SEGMENT_LENGTH/2, BLACK);
      return 50;
    }
    SEGMENT_RUNTIME.aux_param3 = random16Var(SEGMENT_LENGTH/2);
    modecnts = (++modecnts >= UserModeNum)? 0 : modecnts;
    colors = __GetWs2812FxColors(modecnts,SCENE_TYPE_SNOWFLAKE);
    for(uint16_t i=0; i < SEGMENT_LENGTH; i++) {
      setPixelColor(SEGMENT.start + i, BLACK);
    }
    return random8Var(50);
  }

  setPixelColor(SEGMENT.start + dest, BLACK);
  setPixelColor(SEGMENT.start + dest + SEGMENT_LENGTH/2, BLACK);

  if(SEGMENT_RUNTIME.aux_param3 > SEGMENT_RUNTIME.counter_mode_step)
  {
    SEGMENT_RUNTIME.counter_mode_step++;
    dest++;
  } else{ if (SEGMENT_RUNTIME.aux_param3 < SEGMENT_RUNTIME.counter_mode_step) {
    SEGMENT_RUNTIME.counter_mode_step--;
    dest--;
  }}
   
  setPixelColor(SEGMENT.start + dest,colors);
  setPixelColor(SEGMENT.start + dest + SEGMENT_LENGTH/2,colors);
  
  return (SEGMENT.speed / SEGMENT_LENGTH);
}


/*
 * Custom modes
 * 呼吸波1 : 每个灯依次点亮红，直到全部点亮;全部灯进入吸状态（逐渐变暗),直到全灭;切换
 */
 
uint16_t WS2812FX_mode_custom_BWaveFade1(void){

  /**  第一步 : 射波依次点亮然后全亮 ; 全亮后设置标志1**/
  if(BwaveFlag==0)
  {
    if(SEGMENT_RUNTIME.counter_mode_step>= (SEGMENT_LENGTH)) 
    {
      BwaveFlag = 1;
      BreathFlag = 1;
      SEGMENT_RUNTIME.counter_mode_step = 0;
      //__scene_cnt = (++__scene_cnt >= UserModeNum)? 0 : __scene_cnt;
	  }
	  return color_wipe(UsrColors, BLACK, 0);
  }
  else
  {
      /**  第二步 : 全部呼吸，逐渐变暗，直到全灭 ; 全灭后设置标志0 **/
	  int lum = SEGMENT_RUNTIME.counter_mode_step;
	  if(lum >= 255) 
	  {
	      /***************在呼吸最暗的时候，进行颜色的切换*************/
		  if(lum<260)
		  {
  	    BwaveFlag = 0;
  			lum = 255;
  			SEGMENT_RUNTIME.counter_mode_step = 0;
        //__scene_cnt = (++__scene_cnt >= UserModeNum)? 0 : __scene_cnt;
		  }
		  else
		  {
		  	lum = 511 - lum; // lum = 0（最亮） -> 255 -> 0
		  }
	  }
	  
	  uint32_t color = color_blend(UsrColors, BLACK, lum);
	  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) 
    {
	    setPixelColor(i, color);
	  }
	  
    if(BwaveFlag)
    {
		  SEGMENT_RUNTIME.counter_mode_step += 4;
		  if(SEGMENT_RUNTIME.counter_mode_step > 511) 
		  {
			  SEGMENT_RUNTIME.counter_mode_step = 0;
		  }
    }
	  return (SEGMENT.speed / 64);
  }
}

/*
 * Custom modes
 * 呼吸波2 : 每个灯依次点亮红，直到全部点亮;全部灯进入吸状态（逐渐变暗),直到全灭;切换
 */
uint16_t WS2812FX_mode_custom_BWaveFade2(void){

  static uint32_t colors;
  
  /**  第一步 : 射波依次点亮然后全亮 ; 全亮后设置标志1**/
  if(BwaveFlag==0)
  {
	  if(SEGMENT_RUNTIME.counter_mode_step>= (SEGMENT_LENGTH)) 
    {
  			BwaveFlag = 1;
        BreathFlag = 1;
  			//__scene_cnt = (++__scene_cnt >= UserModeNum)? 0 : __scene_cnt;
  			SEGMENT_RUNTIME.counter_mode_step = 255;
        return (SEGMENT.speed / 64);
	  }
    
    if(colors==0)
      colors = color_blend(UsrColors, BLACK, 251);
	  return color_wipe(colors, BLACK, 0);
  }
  else
  {
      /**  第二步 : 全部呼吸，逐渐变暗，直到全灭 ; 全灭后设置标志0 **/
	  int lum = SEGMENT_RUNTIME.counter_mode_step;
	  if(lum > 255) 
	  {
		  lum = 511 - lum; // lum = 0（最亮） -> 255 -> 0
		  /***************在呼吸最亮的时候，进行颜色的切换*************/
      if(lum < 4) 
      {
    			BwaveFlag = 0;
    			SEGMENT_RUNTIME.counter_mode_step = 0;
          colors = color_blend(UsrColors, BLACK, 251);
    			//__scene_cnt = (++__scene_cnt >= UserModeNum)? 0 : __scene_cnt;
      }
	  }
    
    if(BwaveFlag)
    {
      uint32_t color = color_blend(UsrColors, BLACK, lum);
      
		  for(uint16_t i=SEGMENT.start; i <= SEGMENT.stop; i++) 
      {
		    setPixelColor(i, color);
		  }
		  
		  SEGMENT_RUNTIME.counter_mode_step += 4;
    
		  if(SEGMENT_RUNTIME.counter_mode_step > 511) 
		  {
			  SEGMENT_RUNTIME.counter_mode_step = 0;
		  }
    }
	  return (SEGMENT.speed / 64);
  }
}


/*
 * Custom modes
 * 呼吸波3 : 每个灯依次点亮红，同时呼吸，直到全部点亮;然后切换颜色
 */
uint16_t WS2812FX_mode_custom_BWaveFade3(void){
	/**  全部呼吸，逐渐变暗，直到全灭                **/
	uint16_t i;
	int lum = SEGMENT_RUNTIME.counter_mode_step;
	if(lum > 255) 
	{
	  lum = 511 - lum; // lum = 0（最亮） -> 255 -> 0
	}
	uint32_t color = color_blend(UsrColors, BLACK, lum);
	
	SEGMENT_RUNTIME.counter_mode_step += 8;
	if(SEGMENT_RUNTIME.counter_mode_step > 511) 
	{
		SEGMENT_RUNTIME.counter_mode_step = 0;
	}
  
	/** 射波依次点亮然后全亮,同时进行呼吸,渐明渐暗 **/
  if(SEGMENT_RUNTIME.counter_mode_call>=SEGMENT_LENGTH)
  {
		SEGMENT_RUNTIME.counter_mode_call = 0;
    BreathFlag = 1;
		//__scene_cnt = (++__scene_cnt >= UserModeNum)? 0 : __scene_cnt;
	}
   
	uint32_t led_offset = SEGMENT_RUNTIME.counter_mode_call;
	if (IS_REVERSE)
	{
		  for(i=SEGMENT.stop; i <= (SEGMENT.stop - led_offset); i--) {
    		setPixelColor(i, color);
 		 }
	} 
  else 
  {
	    for(i=SEGMENT.start; i <= (SEGMENT.start + led_offset); i++) {
    		setPixelColor(i, color);
 		 }
	}
    return (SEGMENT.speed / 64);
}

/*
 * Custom modes
 * 追光效果 : 固定光追光, 红绿蓝黄紫色，全灯带依次追光
 */
 void WS2812FX_modeflow(uint16_t *times,uint16_t pos,uint32_t colors)
 {
	uint32_t color;
	uint32_t r1,g1,b1;
	uint16_t i,j,k;
	uint16_t lum;
	uint32_t pos_steps;
	j = (DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV);
	
	if(pos<numLEDs)
	{
		if(*times>pos)
		{
			pos_steps = *times-pos;
			
		    /*淡入效果*/
			if(pos_steps<=j)
			{
				i = pos_steps;
				lum = 100;
				
				/***** 明暗化拖尾效果 *******/
			    color = colors;
			    r1 = (colors >> 16) & 0xff;
			    g1 = (colors >>  8) & 0xff;
			    b1 =  colors        & 0xff;
			   
			    r1 = (r1 * lum) / 256U;
			    g1 = (g1 * lum ) / 256U;
			    b1 = (b1 * lum ) / 256U;
  
				color = ((uint32_t)(r1 << 16) | (uint32_t)(g1 << 8) | (b1));
				
				k = SEGMENT.start+(pos_steps)-1;
				setPixelColor(k,color);
			}
			/*整体追光效果*/
			else if(pos_steps<=SEGMENT_LENGTH)
			{
				for(i=0; i<j; i++) 
				{
					if(IS_REVERSE) 
					{

					} 
					else 
					{
						if(i<j/4){
							lum = 40;
						}
						else if(i<j/2){
							lum = 100;
						}
						else{
							lum = 255;
						}
						/***** 明暗化拖尾效果 *******/
						color = colors;
					    r1 = (colors >> 16) & 0xff;
					    g1 = (colors >>  8) & 0xff;
					    b1 =  colors        & 0xff;
					   
					    r1 = (r1 * lum)  / 256U;
					    g1 = (g1 * lum ) / 256U;
					    b1 = (b1 * lum ) / 256U;
		  
						color =  (uint32_t)(r1 << 16) | (uint32_t)(g1 << 8) | (b1);
					
						k=SEGMENT.start+i+pos_steps-j-1;
						setPixelColor(k,color);
					}
				}
			}
			/*淡出效果*/
		    else if(pos_steps<SEGMENT_LENGTH+j)
		    {
				setPixelColor(pos_steps-j,BLACK);
			}
			/******时间计数复位*****/
			else 
			{
				pos_steps = 0;
				*times = pos;
			}
		}
	}
 } 
 
 uint16_t WS2812FX_mode_custom_SportLight(void){
 
  static uint16_t trip_times[TRIPLEDS_DIV];
	trip_times[0]++;
	trip_times[1]++;
	trip_times[2]++;
	trip_times[3]++;
	trip_times[4]++;
  
  if(trip_times[4]>=(DISPLAY_BUF_MAX_PIXEL+SEGMENT_LENGTH-DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV))
  {
      PaomaFlag = 1;
      trip_times[0] = 0;
      trip_times[1] = 0;
      trip_times[2] = 0;
      trip_times[3] = 0;
      trip_times[4] = 0;
  }
  else
  {
      PaomaFlag = 0;
  }
  
	WS2812FX_modeflow(&trip_times[0],0,RED);
	WS2812FX_modeflow(&trip_times[1],DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV,GREEN);
	WS2812FX_modeflow(&trip_times[2],DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV+DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV,BLUE);
	WS2812FX_modeflow(&trip_times[3],DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV+DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV+DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV,GREEN);
	WS2812FX_modeflow(&trip_times[4],DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV+DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV+DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV+DISPLAY_BUF_MAX_PIXEL/TRIPLEDS_DIV,YELLOW);
	return (SEGMENT.speed/64);
 }
 
 /*
  * Custom modes
  * 固定闪烁 : 奇数灯闪烁，切换偶数灯闪烁，切换颜色
  */
 uint16_t WS2812FX_mode_custom_FixedTwlinkle(void){
 
  uint32_t color;
  uint16_t i;
  
  BlinkCnt++;
  if(BlinkCnt%2)
  {
  	color = UsrColors;
  }
  else
  {
	  color = BLACK;
  }
  
  for(i=0;i<SEGMENT.stop;i++)
  {
    setPixelColor(i,BLACK);
  }
  
  for(i=SEGMENT.start; i <= SEGMENT.stop; i++) {
  	    // 偶数灯闪烁
		if(BlinkCnt<3)
		{
			if((i%2)!=0)
      {
			  setPixelColor(i, color);
			}
		}
	    else 
	    {
  			if(BlinkCnt>3)
  			{
  				BlinkCnt = 0;
  				//__scene_cnt = (++__scene_cnt >= UserModeNum)? 0 : __scene_cnt;
  			}
  			if(i%2==0)
        {
  			  setPixelColor(i, color);
  			}
	    }
  }

   return (SEGMENT.speed/4);
 }

 /*
   * Custom modes
   * 交替闪烁 : 奇数灯闪烁，切换偶数灯闪烁，流光颜色变化
   */
  uint16_t WS2812FX_mode_custom_AlterTwlinkle(void){ 
  
   uint16_t i;
   
   BlinkCnt++;
   for(i=0;i<SEGMENT.stop;i++)
   {
     setPixelColor(i,BLACK);
   }
   
   for(i=SEGMENT.start; i <= SEGMENT.stop; i++) {
		 // 偶数灯闪烁
		 if(BlinkCnt<3)
		 {
			 if(BlinkCnt==1)
			 {
			 	if(i%2!=0)
        {
          //color = __GetWs2812FxColors(random8Var(UserModeNum),SCENE_TYPE_BLINKING);
			 	  setPixelColor(i, UsrColors);
			 	}
			 }
		 }
		 else 
		 {
			 if(BlinkCnt==3)
			 {
				 if(i%2==0)
         {
           //color = __GetWs2812FxColors(random8Var(UserModeNum),SCENE_TYPE_BLINKING);
           setPixelColor(i, UsrColors);
				 }
		 	 }
			 else
       {
			   BlinkCnt = 0;
			 }
		 }
   }
	return (SEGMENT.speed/4);
}

  /*
	* Custom modes
	* 交替闪烁 : 奇数灯闪烁，切换偶数灯闪烁，流光颜色变化并且呼吸
	*/
   uint16_t WS2812FX_mode_custom_AlterFade(void){ 

	uint32_t color;
	uint16_t i;
	
	for(i=0;i<SEGMENT.stop;i++){
	setPixelColor(i,BLACK);
	}
	   /**  全部呼吸，逐渐变暗，直到全灭 **/
	  int lum = SEGMENT_RUNTIME.counter_mode_step;
	  if(lum >= 255) 
	  {
	      /***************在呼吸最暗的时候，进行颜色的切换*************/
		  if(lum<260)
		  {
			  lum = 255;
		  }
		  else
		  {
		  	lum = 511 - lum; // lum = 0（最亮） -> 255 -> 0
		  }
	  }
	  
	  SEGMENT_RUNTIME.counter_mode_step += 4;
	  if(SEGMENT_RUNTIME.counter_mode_step > 511) 
	  {
		SEGMENT_RUNTIME.counter_mode_step = 0;
	  }

	   /**  闪烁时间控制     **/
	  if(SEGMENT_RUNTIME.counter_mode_call>50)
	  {
		SEGMENT_RUNTIME.counter_mode_call = 0;
		BlinkCnt++;
      }
	
	  for(i=SEGMENT.start; i <= SEGMENT.stop; i++) {
		  // 偶数灯闪烁
		  if(BlinkCnt<3)
		  {
			  if(BlinkCnt==1)
			  {
			  	if(i%2!=0)
			  	{
					/*****流光颜色呼吸闪烁***/
					color = color_wheel(random8());
					color = color_blend(color, BLACK, lum);
					setPixelColor(i,color);

			  	}
			  }
		  }
		  else 
		  {
			  if(BlinkCnt==3)
			  {
				  if(i%2==0)
				  {
					/*****流光颜色呼吸闪烁***/
					color = color_wheel(random8());
					color = color_blend(color, BLACK, lum);
					setPixelColor(i,color);

				  }
			  }
			  else{
			  	BlinkCnt = 0;
			  }
		  }
	}
	  
	return (SEGMENT.speed/256);
  
   }
   

   /*
	 * Custom modes
	 * 随机闪烁 : 以时间作为随机数，超过32颗灯，则重复选择
	 */
	uint16_t WS2812FX_mode_custom_RandomTwinkle(void){ 
	   uint16_t i,j,k;
	   BlinkCnt++;
	   for(i=0;i<SEGMENT.stop;i++){
	   setPixelColor(i,BLACK);
	   }
	   
	   k = SEGMENT_LENGTH/RANDOM_LEDS;
	   j = random8Var(RANDOM_LEDS);
		 //GetUserColors(0,0);
     SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
	   for(i=SEGMENT.start; i <= k; i++) {
			 // 偶数灯闪烁
			 if(BlinkCnt<3)
			 {
				 if(BlinkCnt==1)
				 {
				 	if((i%2)!=0){
				 	setPixelColor(i*RANDOM_LEDS+j,UsrColors);
				 	}
				 }
			 }
			 else 
			 {
				 if(BlinkCnt==3)
				 {
					 if((i%2)==0){
					 setPixelColor(i*RANDOM_LEDS+j, UsrColors);
					 }
			 	 }
				 else{
				 BlinkCnt = 0;
				 }
			 }
	   }
		return (SEGMENT.speed/4);
	}

/*
  * Custom modes
  * 自动变幻 : 在流光模式和随机闪烁模式之间切换，10s计时
  */
 uint16_t WS2812FX_mode_custom_AutoSwitch(void){ 
	 uint16_t i,j,k;
	if(Ms_heart<AutoSwitch_5s)
	{
	    /*流光模式------ 10s切换        WS2812FX_mode_multi_dynamic */
		for(i=SEGMENT.start; i <= SEGMENT.stop; i++) {
		  setPixelColor(i, color_wheel(random8()));
		}
		return (SEGMENT.speed);
	}
	else if(Ms_heart<AutoSwitch_10s)
	{
		/*随机跳动模式------ 10s切换        WS2812FX_mode_multi_dynamic */
		  BlinkCnt++;
	    for(i=0;i<SEGMENT.stop;i++){
	      setPixelColor(i,BLACK);
	    }
	    
	    k = SEGMENT_LENGTH/RANDOM_LEDS;
	    j = random8Var(RANDOM_LEDS);
		  //GetUserColors(0,0);
      //SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
      //PR_NOTICE("----%x %d %d %d %d", UsrColors,BlinkCnt,k,j,__scene_cnt);
	    for(i=SEGMENT.start; i <= k; i++) 
      {
  			 // 偶数灯闪烁
  			 if(BlinkCnt<3)
  			 {
  				 if(BlinkCnt==1)
  				 {
    				 	if((i%2)!=0){
    				 	setPixelColor(i*RANDOM_LEDS+j, UsrColors);
  				 	}
  				 }
  			 }
  			 else 
  			 {
  				 if(BlinkCnt==3)
  				 {
  					 if((i%2)==0)
             {
  					   setPixelColor(i*RANDOM_LEDS+j, UsrColors);
  					 }
  			 	 }
  				 else{
  				 BlinkCnt = 0;
  				 }
  			 }
	    }
		return (SEGMENT.speed/4);
	}
	else
	{
    Ms_heart = 0;
    BlinkCnt = 0; 
		return (SEGMENT.speed/256);
	}
 }

/*
 * Custom modes
 * 水滴效果 : 20个灯亮度不一，然后依次走动
 */
uint16_t WS2812FX_mode_custom_WaterDrop(void){

  uint8_t r,g,b;
  uint8_t rtemp,gtemp,btemp;
  uint16_t i;
  uint8_t size_u = 1 << SIZE_OPTION;
  
  SEGMENT_RUNTIME.counter_mode_step++;
  if(SEGMENT_RUNTIME.counter_mode_step>(SEGMENT_LENGTH+SEGMENT_LENGTH-SEGMENT_LENGTH/4))
  {
  	 SEGMENT_RUNTIME.counter_mode_step = 0;
  	 if(WaterFlag)
  	 {
  	    WaterFlag=0;
        PaomaFlag = 1;
     }
  	 else
  	 {
  	    WaterFlag=1;
  	 }
  }
  else
  {
    PaomaFlag = 0;
  }
  
  /*第一步，10科灯七色顺序流水走完*/
  rtemp= ((UsrColors>> 16) & 0xFF);
  gtemp= ((UsrColors >>  8) & 0xFF);
  btemp = (UsrColors      & 0xFF);
  
  for(i=0;i<SEGMENT.stop;i++){
  setPixelColor(i,BLACK);
  }
  
  for(i=WATER_LEDS; i > 0; i--) {
  	
    int lum = (int)WaterFlux[i-1];
	
    if(WaterFlag) 
    {
		r =  (rtemp * lum) / 256;
		g =  (gtemp * lum) / 256;
		b =  (btemp * lum) / 256;
	      
	  setPixelColor(SEGMENT.stop - i-SEGMENT_RUNTIME.counter_mode_step,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.stop - i-(SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/8),((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.stop - i-(SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/4),((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.stop - i-(SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/2),((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.stop - i-(SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/2-SEGMENT_LENGTH/8),((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.stop - i-(SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/2-SEGMENT_LENGTH/4),((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.stop - i-(SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH),((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		  
      } 
	  else 
      {
		r =  (rtemp * lum) / 256;
		g =  (gtemp * lum) / 256;
		b =  (btemp * lum) / 256;
		  
		setPixelColor(SEGMENT.start+i+SEGMENT_RUNTIME.counter_mode_step,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.start+i+SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/8,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.start+i+SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/4,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.start+i+SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/2,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.start+i+SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/2-SEGMENT_LENGTH/8,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.start+i+SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH/2-SEGMENT_LENGTH/4,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		setPixelColor(SEGMENT.start+i+SEGMENT_RUNTIME.counter_mode_step-SEGMENT_LENGTH,((uint32_t)r << 16) | ((uint32_t)g <<  8) | b);
		  
    }
  }
  
  return (SEGMENT.speed/128);
}



void fill_rainbow(  uint8_t * pFirstLED, int numToFill, uint8_t initialhue,uint8_t deltahue )
{
    CHSV hsv;
    hsv.hue = initialhue;
    hsv.val = 255;
    hsv.sat = 240;
    for( int i = 0; i < numToFill; i++){
        pFirstLED[i*3] = hsv.hue;
		pFirstLED[i*3+1] = hsv.val;
	    pFirstLED[i*3+2] = hsv.sat;
        hsv.hue += deltahue;
		hsv.val += deltahue;
		hsv.sat += deltahue;
    }
}

void addGlitter( uint8_t chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
	  rgbBuffer[random8Var(DISPLAY_BUF_MAX_PIXEL)*3]+=(uint8_t)(WHITE >> 16);
	  rgbBuffer[random8Var(DISPLAY_BUF_MAX_PIXEL)*3+1]+=(uint8_t)(WHITE >> 8);
    rgbBuffer[random8Var(DISPLAY_BUF_MAX_PIXEL)*3+2]+=(uint8_t)WHITE;	
  }
}

void nscale8( uint8_t* leds, uint16_t num_leds, uint8_t scale)
{
    for( uint16_t i = 0; i < num_leds; i++) {
		leds[i*3] =((uint32_t)leds[i*3] * (uint32_t)(scale))>>8;
        leds[i*3+1] =((uint32_t)leds[i*3+1] * (uint32_t)(scale))>>8;
        leds[i*3+2] =((uint32_t)leds[i*3+2] * (uint32_t)(scale))>>8;
    }
}

void fadeToBlackBy( uint8_t* leds, uint16_t num_leds, uint8_t fadeBy)
{
    nscale8(leds, num_leds, 255 - fadeBy);
}

void rainbow(void) 
{
  if(Ms_heart>10)
  {
	  Ms_heart = 0;
	  gHue++;
  }
  fill_rainbow(rgbBuffer, DISPLAY_BUF_MAX_PIXEL, gHue, 5);
}

void rainbowWithGlitter(void) 
{
  rainbow();
  addGlitter(80);
}

void confetti(void) 
{
  fadeToBlackBy(rgbBuffer, DISPLAY_BUF_MAX_PIXEL, 10);
  int pos = random8Var(DISPLAY_BUF_MAX_PIXEL);
  rgbBuffer[pos*3] +=gHue + random8Var(64);
  rgbBuffer[pos*3+1] +=200;
  rgbBuffer[pos*3+2] +=255;
}

uint16_t WS2812FX_mode_custom_Glitter(void) 
{
  rainbowWithGlitter();
  return (SEGMENT.speed/256);
}

uint16_t WS2812FX_mode_custom_Confetti(void) 
{
  confetti(); 
  return (SEGMENT.speed/256);
}

/**************** 时间次数切换 ****************/
void  UserSwitch_Ticks(uint8_t scene_num,uint8_t type,uint8_t MaxMode)
{
   if(type==SCENE_TYPE_STATIC)
   {
        if(StremberFlag == 1)
        {
             StremberFlag = 0;
             ModeRunTimes = 0;
        		 Ms_heart = 0;
        		 ws_timercnt = 0;
        		 now = 0;
        		 BwaveFlag = 1;
             WaterFlag = 0;
             BlinkCnt = 0;
        		 SEGMENT_RUNTIME.counter_mode_call = 0;
        		 SEGMENT_RUNTIME.counter_mode_step = 0;
             
             __mode_cnts++;
             __scene_cnt++;
             if(__mode_cnts >= scene_num)
             {
                __mode_cnts = 0;
                
                UserIndex++;
                if(UserIndex>=MaxMode)
                {
                   UserIndex = 0;
                }
             }
             PR_NOTICE("---------strember--------:%d %d \n", UserIndex,__mode_cnts);
         }
   }
   else if(type==SCENE_TYPE_BREATH)
   {
       if(BreathFlag == 1)
       {
           BreathFlag = 0;
           ModeRunTimes = 0;
      		 Ms_heart = 0;
      		 ws_timercnt = 0;
      		 now = 0;
      		 BwaveFlag = 1;
           WaterFlag = 0;
           BlinkCnt = 0;
      		 SEGMENT_RUNTIME.counter_mode_call = 0;
      		 SEGMENT_RUNTIME.counter_mode_step = 0;
           
           __scene_cnt++;
           if(__scene_cnt >= scene_num)
           {
              __scene_cnt = 0;
              
              UserIndex++;
              if(UserIndex>=MaxMode)
              {
                 UserIndex = 0;
              }
           }
           PR_NOTICE("--------breath---------:%d %d \n", UserIndex,__scene_cnt);
       }
   }
   else if(type==SCENE_TYPE_RAIN_DROP)
   {
      if(PaomaFlag)
      {
         PaomaFlag = 0;
    		 Ms_heart = 0;
    		 ws_timercnt = 0;
    		 now = 0;
    		 BwaveFlag = 1;
         WaterFlag = 0;
         BlinkCnt = 0;
    		 SEGMENT_RUNTIME.counter_mode_call = 0;
    		 SEGMENT_RUNTIME.counter_mode_step = 0;
         __scene_cnt++;
         if(__scene_cnt >= scene_num)
         {
            __scene_cnt = 0;
            
            UserIndex++;
            if(UserIndex>=MaxMode)
            {
               UserIndex = 0;
            }
         }
         PR_NOTICE("-------rain----------:%d %d \n", UserIndex,__scene_cnt);
      }
   }
   else if(type==SCENE_TYPE_COLORFULL)
   {
      if(ColorfulFlag)
      {
         ColorfulFlag = 0;
    		 Ms_heart = 0;
    		 ws_timercnt = 0;
    		 now = 0;
    		 BwaveFlag = 1;
         WaterFlag = 0;
         BlinkCnt = 0;
    		 SEGMENT_RUNTIME.counter_mode_call = 0;
    		 SEGMENT_RUNTIME.counter_mode_step = 0;
         __scene_cnt++;
         if(__scene_cnt >= scene_num)
         {
            __scene_cnt = 0;
            
            UserIndex++;
            if(UserIndex>=MaxMode)
            {
               UserIndex = 0;
            }
         }
         PR_NOTICE("--------colorful--------:%d %d \n", UserIndex,__scene_cnt);
      }
   }
   else if(type==SCENE_TYPE_BLINKING)
   {
       if(FlashFlag == 1)
       {
          if(ModeRunTimes>RUNNING_30S)
          {
             FlashFlag = 0;
             ModeRunTimes = 0;
        		 Ms_heart = 0;
        		 ws_timercnt = 0;
        		 now = 0;
        		 BwaveFlag = 1;
             WaterFlag = 0;
             BlinkCnt = 0;
        		 SEGMENT_RUNTIME.counter_mode_call = 0;
        		 SEGMENT_RUNTIME.counter_mode_step = 0;
             
             __mode_cnts++;
             if(__mode_cnts >= scene_num)
             {
                __mode_cnts = 0;
                
                UserIndex++;
                if(UserIndex>=MaxMode)
                {
                   UserIndex = 0;
                }
             }
             PR_NOTICE("-------music flash-------:%d %d \n", UserIndex,__mode_cnts);
          }
       }
   }
   else if(type==SCENE_TYPE_SNOWFLAKE)
   {
      if(ScanFlag)
      {
        if(ModeRunTimes>RUNNING_30S)
        {
           ModeRunTimes = 0;
           ScanFlag = 0;
      		 Ms_heart = 0;
      		 ws_timercnt = 0;
      		 now = 0;
      		 BwaveFlag = 1;
           WaterFlag = 0;
           BlinkCnt = 0;
      		 SEGMENT_RUNTIME.counter_mode_call = 0;
      		 SEGMENT_RUNTIME.counter_mode_step = 0;
           __mode_cnts++;
           if(__mode_cnts >= scene_num)
           {
              __mode_cnts = 0;
              
              UserIndex++;
              if(UserIndex>=MaxMode)
              {
                 UserIndex = 0;
              }
           }
           PR_NOTICE("--------music scan---------:%d %d \n", UserIndex,__mode_cnts);
        }
      }
   }
   else if(type==SCENE_TYPE_STREAMER_COLOR)
   {
      if(TripFlag)
      {
        if(ModeRunTimes>RUNNING_30S)
        {
           ModeRunTimes = 0;
           TripFlag = 0;
      		 Ms_heart = 0;
      		 ws_timercnt = 0;
      		 now = 0;
      		 BwaveFlag = 1;
           WaterFlag = 0;
           BlinkCnt = 0;
      		 SEGMENT_RUNTIME.counter_mode_call = 0;
      		 SEGMENT_RUNTIME.counter_mode_step = 0;
           __mode_cnts++;
           if(__mode_cnts >= scene_num)
           {
              __mode_cnts = 0;
              
              UserIndex++;
              if(UserIndex>=MaxMode)
              {
                 UserIndex = 0;
              }
           }
           PR_NOTICE("-------music show-----:%d %d \n", UserIndex,__mode_cnts);
        }
      }
   }
   else
   {
      if(UsrColorFlag)
      {
        if(ModeRunTimes>RUNNING_30S)
        {
           ModeRunTimes = 0;
           UsrColorFlag = 0;
      		 Ms_heart = 0;
      		 ws_timercnt = 0;
      		 now = 0;
      		 BwaveFlag = 1;
           WaterFlag = 0;
           BlinkCnt = 0;
      		 SEGMENT_RUNTIME.counter_mode_call = 0;
      		 SEGMENT_RUNTIME.counter_mode_step = 0;
           __mode_cnts++;
           if(__mode_cnts >= scene_num)
           {
              __mode_cnts = 0;
              
              UserIndex++;
              if(UserIndex>=MaxMode)
              {
                 UserIndex = 0;
              }
           }
           PR_NOTICE("-------music user-------:%d %d \n", UserIndex,__scene_cnt);
        }
      }
   }

}

/**************** 时间定时效果切换 ****************/
void  UserRun_Switch(uint8_t scene_num,uint8_t type,uint8_t MaxMode)
{
   if(type==SCENE_TYPE_BLINKING)
   {
      if(ModeRunTimes>RUNNING_10S)
      {
         ModeRunTimes = 0;
    		 Ms_heart = 0;
    		 ws_timercnt = 0;
    		 now = 0;
    		 BwaveFlag = 1;
         WaterFlag = 0;
         BlinkCnt = 0;
    		 SEGMENT_RUNTIME.counter_mode_call = 0;
    		 SEGMENT_RUNTIME.counter_mode_step = 0;
         __scene_cnt++;
         if(__scene_cnt >= scene_num)
         {
            __scene_cnt = 0;
            
            UserIndex++;
            if(UserIndex>=MaxMode)
            {
               UserIndex = 0;
            }
         }
         PR_NOTICE("--------------------:%d %d \n", UserIndex,__scene_cnt);
      }
   }
   else if(type==SCENE_TYPE_BREATH)
   {
      if(ModeRunTimes>RUNNING_30S)
      {
         if(BreathFlag == 1)
         {
             BreathFlag = 0;
             ModeRunTimes = 0;
        		 Ms_heart = 0;
        		 ws_timercnt = 0;
        		 now = 0;
        		 BwaveFlag = 1;
             WaterFlag = 0;
             BlinkCnt = 0;
        		 SEGMENT_RUNTIME.counter_mode_call = 0;
        		 SEGMENT_RUNTIME.counter_mode_step = 0;
             
             __scene_cnt++;
             if(__scene_cnt >= scene_num)
             {
                __scene_cnt = 0;
                
                UserIndex++;
                if(UserIndex>=MaxMode)
                {
                   UserIndex = 0;
                }
             }
             PR_NOTICE("--------------------:%d %d \n", UserIndex,__scene_cnt);
         }
      }
   }
   else if(type==SCENE_TYPE_MARQUEE)
   {
      if(ModeRunTimes>RUNNING_10S)
      {
         if(PaomaFlag == 1)
         {
             PaomaFlag = 0;
             ModeRunTimes = 0;
        		 Ms_heart = 0;
        		 ws_timercnt = 0;
        		 now = 0;
        		 BwaveFlag = 1;
             WaterFlag = 0;
             BlinkCnt = 0;
        		 SEGMENT_RUNTIME.counter_mode_call = 0;
        		 SEGMENT_RUNTIME.counter_mode_step = 0;
             
             __scene_cnt++;
             if(__scene_cnt >= scene_num)
             {
                __scene_cnt = 0;
                
                UserIndex++;
                if(UserIndex>=MaxMode)
                {
                   UserIndex = 0;
                }
             }
             PR_NOTICE("--------------------:%d %d \n", UserIndex,__scene_cnt);
         }
      }
   }
   else if(type==SCENE_TYPE_STATIC)
   {
      if(ModeRunTimes>RUNNING_30S)
      {
        if(StremberFlag == 1)
        {
             StremberFlag = 0;
             ModeRunTimes = 0;
        		 Ms_heart = 0;
        		 ws_timercnt = 0;
        		 now = 0;
        		 BwaveFlag = 1;
             WaterFlag = 0;
             BlinkCnt = 0;
        		 SEGMENT_RUNTIME.counter_mode_call = 0;
        		 SEGMENT_RUNTIME.counter_mode_step = 0;
             
             __scene_cnt++;
             if(__scene_cnt >= scene_num)
             {
                __scene_cnt = 0;
                
                UserIndex++;
                if(UserIndex>=MaxMode)
                {
                   UserIndex = 0;
                   StremberCnts=1;
                }
             }
             PR_NOTICE("--------------------:%d %d \n", UserIndex,__scene_cnt);
         }
      }
   }
   else
   {
      if(ModeRunTimes>RUNNING_30S)
      {
         ModeRunTimes = 0;
    		 Ms_heart = 0;
    		 ws_timercnt = 0;
    		 now = 0;
    		 BwaveFlag = 1;
         WaterFlag = 0;
         BlinkCnt = 0;
    		 SEGMENT_RUNTIME.counter_mode_call = 0;
    		 SEGMENT_RUNTIME.counter_mode_step = 0;
         __scene_cnt++;
         if(__scene_cnt >= scene_num)
         {
            __scene_cnt = 0;
            
            UserIndex++;
            if(UserIndex>=MaxMode)
            {
               UserIndex = 0;
            }
         }
         PR_NOTICE("--------------------:%d %d \n", UserIndex,__scene_cnt);
      }
   }
	 //ModeRunIndex++;
	 //if(ModeRunIndex>=MODE_RUNS)
	 //{
	 //  ModeRunIndex = 0;
	 //}
	 //start(ModeRUN[ModeRunIndex]);
   //PR_NOTICE("--------------------:%d %d \n", UserIndex,__scene_cnt);
}

/************************* 跑马灯效果处理*************/
void Usr_Paoma0(void)
{
  uint8_t j,index,size_u = 1;
  uint16_t red,green,blue;
  uint32_t colors;
  /***总灯数52 分26段,每段2个灯***/
  for(j=0;j<SEGMENT_LENGTH/(PAOMA_LEDS*3);j++)
  {
    for(index=0;index<(PAOMA_LEDS*3);index++)
    {
      //setPixelColor((SEGMENT.start + j*PAOMA_LEDS + index + SEGMENT_RUNTIME.counter_mode_step)%SEGMENT_LENGTH, voiceColor[j%3]);
      //PR_NOTICE("----%d %d ",(SEGMENT.start + index + j + SEGMENT_RUNTIME.counter_mode_step)%SEGMENT_LENGTH,index/PAOMA_LEDS);
      setPixelColor((index + j*(PAOMA_LEDS*3) + SEGMENT_RUNTIME.counter_mode_step)%SEGMENT_LENGTH, PaomaColor[index/PAOMA_LEDS]);
    }
  }
  /*** 全灯带左标移动 *****/
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
}

void Usr_Paoma(void)
{
  uint8_t j,index,size_u = 1;
  uint16_t red,green,blue;
  uint32_t colors;
  #if 0
  /***总灯数52 分26段,每段2个灯***/
  for(j=0;j<SEGMENT_LENGTH/(PAOMA_LEDS*3);j++)
  {
    for(index=0;index<(PAOMA_LEDS*3);index++)
    {
      //setPixelColor((SEGMENT.start + j*PAOMA_LEDS + index + SEGMENT_RUNTIME.counter_mode_step)%SEGMENT_LENGTH, voiceColor[j%3]);
      //PR_NOTICE("----%d %d ",(SEGMENT.start + index + j + SEGMENT_RUNTIME.counter_mode_step)%SEGMENT_LENGTH,index/PAOMA_LEDS);
      setPixelColor((index + j*(PAOMA_LEDS*3) + SEGMENT_RUNTIME.counter_mode_step)%SEGMENT_LENGTH, PaomaColor[index/PAOMA_LEDS]);
    }
  }
  /*** 全灯带左标移动 *****/
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % SEGMENT_LENGTH;
  #else
  /********* 全灯带移动 ************/
  for(j=0;j<40;j++)
  {
      index = (j+SEGMENT_RUNTIME.counter_mode_step)%40;
      
      if(index<5)
      {
          red = 255;
          green = 0+(index*51);
          blue = green;
      }
      else if(index<10)
      {
          green = 255;
          red = 255-((index-5)*51);
          blue = red;
      }
      else if(index<15)
      {
          green = 255;
          red = 255-((index-10)*51);
          blue = red;
      }
      else if(index<20)
      {
          green = 255;
          red = 255- ((index-15)*51);
          blue = red;
      }
      else if(index<25)
      {
          green = 255;
          red = ((index-20)*51);
          blue = red;
      }
      else if(index<30)
      {
          red = 255;
          green = 255;
          blue = 255-((index-25)*51);
      }
      else if(index<35)
      {
          red = 255;
          green = 255;
          blue = ((index-30)*51);
      }
      else
      {
          red = 255;
          green = 255-((index-35)*51);
          blue = green;
      }
      colors = ((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue;
  
      setPixelColor(j, colors);
  }
  /*** 全灯带左标移动 *****/
  SEGMENT_RUNTIME.counter_mode_step = (SEGMENT_RUNTIME.counter_mode_step + 1) % 40;
  #endif
}


/************************* 流光效果处理*************/
uint16_t WS2812Fx_Streamber(uint8_t type,uint8_t scene_num,uint8_t times)
{
  /********** 获取用户颜色 *************/
  //UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  UserModeNum = scene_num;
  /********** 自定义效果实现 *************/
  #if 1
  if(UserIndex==0)
  {
    /********** 1.全彩流光----用户色走完切换全彩流光 *************/
    //__scene_cnt = scene_num;
    SEGMENT.speed  =  WS2812FX_mode_rainbow_cycle();
    times = times >>1;
  }
  else
  {
    /********** 2.跑马灯流光----用户色走完切换全彩流光 *************/
    //__scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_color_sweep_random();
    times = times >>1;
  }
  #else
  /********** 2.全彩流光----用户色走完切换全彩流光 *************/
  __scene_cnt = scene_num;
  SEGMENT.speed  =  WS2812FX_mode_rainbow_cycle();
  #endif
  /***** 刷新显示 ******/
  show();
  #if 0
  /**** 时间计数模式----30s切换 ***********/
  UserRun_Switch(scene_num,type,STREMBER_MODES);
  #else
  /**** 时间计数模式----每种模式运行5次 ***********/
  if(light_IrCrtl.ManulFlag)
  {
    times = light_IrCrtl.ManulTimes;
  }
  else
  {
    UserSwitch_Ticks(STREMBER_RUNCNTS,type,STREMBER_MODES);
  }
  #endif
  
  /*****用户设定速度返回设置 ********/
  if(SEGMENT.speed <SPEED_MIN)
  {
     SEGMENT.speed = SPEED_MIN;
  }
  return ((SEGMENT.speed /8) *times);
}

/************************* 呼吸效果处理*************/
uint16_t WS2812Fx_Breath(uint8_t type,uint8_t scene_num,uint8_t times)
{
  #if 0
  /********** 刷新显示 *************/
  if(mcu_VoiceSpeed)
  {
    times = mcu_VoiceSpeed;
    show();
    /**** 时间计数模式显示 ***********/
    UserRun_Switch(scene_num,type,BREATH_MODES);
  }
  else
  {
    times = 1;
  }
  SEGMENT.speed = SPEED_MIN;
  #else
  /********** 获取用户颜色 *************/
  UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  /********** 自定义效果实现 *************/
  if(UserIndex==0)
  {
    SEGMENT.speed = WS2812FX_mode_breath();
    times = times>>1;
  }
  else if(UserIndex==1)
  {
    SEGMENT.speed = WS2812FX_mode_custom_BWaveFade1();
  }
  else if(UserIndex==2)
  {
    SEGMENT.speed = WS2812FX_mode_custom_BWaveFade2();
  }
  else if(UserIndex==3)
  {
    SEGMENT.speed = WS2812FX_mode_custom_BWaveFade3();
  }
  else if(UserIndex==4)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_rainbow();
  }
  /********** 刷新显示 *************/
  show();
  #if 0
  /**** 时间计数模式----30s切换 ***********/
  UserRun_Switch(scene_num,type,BREATH_MODES);
  #else
  /**** 时间计数模式----每种模式运行1次 ***********/
  if(light_IrCrtl.ManulFlag)
  {
    times = light_IrCrtl.ManulTimes;
  }
  else
  {
    UserSwitch_Ticks(scene_num,type,BREATH_MODES);
  }
  #endif
  /*****用户设定速度返回设置 ********/
  if(SEGMENT.speed <SPEED_MIN)
  {
     SEGMENT.speed = SPEED_MIN;
  }
  #endif
  
  return ((SEGMENT.speed /8) *times);
}

/************************* 火焰效果处理*************/
uint16_t WS2812Fx_Fire(uint8_t type,uint8_t scene_num,uint8_t times)
{
  #if 1
  /********** 获取用户颜色 *************/
  UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  UserModeNum = scene_num;
  /********** 自定义效果实现 *************/
  if(UserIndex==0)
  {
    SEGMENT.speed = WS2812FX_mode_color_wipe();
    times = times>>1;
  }
  else if(UserIndex==1)
  {
    SEGMENT.speed = WS2812FX_mode_color_wipe_inv();
    times = times>>1;
  }
  else if(UserIndex==2)
  {
    SEGMENT.speed = WS2812FX_mode_color_wipe_rev();
    times = times>>1;
  }
  else if(UserIndex==3)
  {
    SEGMENT.speed = WS2812FX_mode_color_wipe_rev_inv();
    times = times>>1;
  }
  else if(UserIndex==4)
  {
    SEGMENT.speed = WS2812FX_mode_chase_white();
    times = times>>1;
  }
  else if(UserIndex==5)
  {
    SEGMENT.speed = WS2812FX_mode_chase_color();
    times = times>>1;
  }
  else if(UserIndex==6)
  {
    __scene_cnt = scene_num;  
    SEGMENT.speed = WS2812FX_mode_chase_random();
    times = times>>1;
  }
  else if(UserIndex==7)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_chase_rainbow();
    times = times>>1;
  }
  else if(UserIndex==8)
  {
    SEGMENT.speed = WS2812FX_mode_chase_flash();
    times = times>>1;
  }
  else if(UserIndex==9)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_chase_flash_random();
  }
  else if(UserIndex==10)
  {
    SEGMENT.speed = WS2812FX_mode_chase_blackout();
  }
  else if(UserIndex==11)
  {
    //__scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_chase_blackout_rainbow();
  }
  else if(UserIndex==12)
  {
    //__scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_color_sweep_random();
  }
  else if(UserIndex==13)
  {
    SEGMENT.speed = WS2812FX_mode_custom_WaterDrop();
  }
  else if(UserIndex==14)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_running_random();
  }
  else if(UserIndex==15)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_custom_SportLight();
  }
  else if(UserIndex==16)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_multi_dynamic();
  }
  /********** 刷新显示 *************/
  show();
  #if 0
   /**** 时间计数模式显示 ***********/
  UserRun_Switch(scene_num,type,FIRE_MODES);
  #else
  /**** 时间计数模式----每种模式运行1次 ***********/
  if(light_IrCrtl.ManulFlag)
  {
    times = light_IrCrtl.ManulTimes;
  }
  else
  {
    UserSwitch_Ticks(scene_num,type,FIRE_MODES);
  }
  #endif
  /*****用户设定速度返回设置 ********/
  if(SEGMENT.speed <SPEED_MIN)
  {
     SEGMENT.speed = SPEED_MIN;
  }
  #else
  /********** 获取用户颜色 *************/
  UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  /********** 自定义效果实现 *************/
  if(UserIndex==0)
  {
    SEGMENT.speed = WS2812FX_mode_running_lights();
  }
  else if(UserIndex==1)
  {
    SEGMENT.speed = WS2812FX_mode_merry_christmas();
  }
  else if(UserIndex==2)
  {
    SEGMENT.speed = WS2812FX_mode_fire_flicker_intense();
  }
  else if(UserIndex==3)
  {
    SEGMENT.speed = WS2812FX_mode_fire_flicker_soft();
  }
  /**** 时间计数模式显示 ***********/
   __scene_cnt = scene_num;
  /********** 刷新显示 *************/
  show();
  /**** 时间计数模式显示 ***********/
  UserRun_Switch(scene_num,type,FIRE_MODES);
  if(SEGMENT.speed <SPEED_MIN)
  {
     SEGMENT.speed = SPEED_MIN;
  }
  times = times;
  #endif
  return ((SEGMENT.speed /4) *times);
}

/************************* 炫彩效果处理*************/
uint16_t WS2812Fx_Colorful(uint8_t type,uint8_t scene_num,uint8_t times)
{
  #if 1
  /********** 获取用户颜色 *************/
  UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  UserModeNum = scene_num;
  /********** 自定义效果实现 *************/
  if(UserIndex==0)
  {
    SEGMENT.speed = WS2812FX_mode_theater_chase();
  }
  else if(UserIndex==1)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_twinkle_random();
  }
  else if(UserIndex==2)
  {
    SEGMENT.speed = WS2812FX_mode_sparkle();
  }
  else if(UserIndex==3)
  {
    SEGMENT.speed = WS2812FX_mode_strobe();
  }
  else if(UserIndex==4)
  {
    SEGMENT.speed = WS2812FX_mode_multi_strobe();
  }
  else if(UserIndex==5)
  {
    SEGMENT.speed = WS2812FX_mode_blink_rainbow();
  }
  else if(UserIndex==6)
  {
    SEGMENT.speed = WS2812FX_mode_circus_combustus();
  }
  else if(UserIndex==7)
  {
    SEGMENT.speed = WS2812FX_mode_halloween();
  }
  else if(UserIndex==8)
  {
    SEGMENT.speed = WS2812FX_mode_tricolor_chase();
  }
  else if(UserIndex==9)
  {
    SEGMENT.speed = WS2812FX_mode_custom_FixedTwlinkle();
  }
  else if(UserIndex==10)
  {
    SEGMENT.speed = WS2812FX_mode_custom_AlterTwlinkle();
  }
  else if(UserIndex==11)
  {
    SEGMENT.speed = WS2812FX_mode_custom_RandomTwinkle();
  }
  else if(UserIndex==12)
  {
    SEGMENT.speed = WS2812FX_mode_custom_Confetti();
  }
  else if(UserIndex==13)
  {
    SEGMENT.speed = WS2812FX_mode_scan();
  }
  else if(UserIndex==14)
  {
    SEGMENT.speed = WS2812FX_mode_dual_scan();
  }
  else if(UserIndex==15)
  {
    SEGMENT.speed = WS2812FX_mode_icu();
  }
  /********** 刷新显示 *************/
  show();
  #if 0
  /**** 时间计数模式显示 ***********/
  UserRun_Switch(scene_num,type,COLORFUL_MODES);
  #else
  /**** 时间计数模式----每种模式运行1次 ***********/
  if(SEGMENT_RUNTIME.counter_mode_call >= SEGMENT_LENGTH) 
  {
    ColorfulFlag = 1;
  }
  else
  {
    ColorfulFlag = 0;
  }
  if(light_IrCrtl.ManulFlag)
  {
    times = light_IrCrtl.ManulTimes;
  }
  else
  {
    UserSwitch_Ticks(scene_num,type,COLORFUL_MODES);
  }
  #endif
  SEGMENT.speed = SPEED_MIN<<1;
  #else
  /********** 获取用户颜色 *************/
  UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  /********** 自定义效果实现 *************/
  if(UserIndex==0)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_running_random();
  }
  else if(UserIndex==1)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_custom_SportLight();
  }
  else if(UserIndex==2)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_multi_dynamic();
  }
  else if(UserIndex==3)
  {
    SEGMENT.speed = WS2812FX_mode_custom_AutoSwitch();
  }
  /********** 刷新显示 *************/
  show();
  /**** 时间计数模式显示 ***********/
  UserRun_Switch(scene_num,type,COLORFUL_MODES);
  /*****用户设定速度返回设置 ********/
  if(SEGMENT.speed <SPEED_MIN)
  {
     SEGMENT.speed = SPEED_MIN;
  }
  times = times;
  #endif
  return ((SEGMENT.speed /4) *times);
}


/**** 音乐闪烁 --- 节奏七彩闪烁 声音大小控制亮度 ****/
uint16_t WS2812Fx_PaoMa(uint8_t type,uint8_t scene_num,uint8_t times)
{
  #if 0
  /********** 获取用户颜色 *************/
  UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  UserModeNum = scene_num;
  /********** 自定义效果实现 *************/
  if(UserIndex==0)
  {
    SEGMENT.speed = WS2812FX_mode_color_wipe();
  }
  else if(UserIndex==1)
  {
    SEGMENT.speed = WS2812FX_mode_color_wipe_inv();
  }
  else if(UserIndex==2)
  {
    SEGMENT.speed = WS2812FX_mode_color_wipe_rev();
  }
  else if(UserIndex==3)
  {
    SEGMENT.speed = WS2812FX_mode_color_wipe_rev_inv();
  }
  else if(UserIndex==4)
  {
    SEGMENT.speed = WS2812FX_mode_chase_white();
  }
  else if(UserIndex==5)
  {
    SEGMENT.speed = WS2812FX_mode_chase_color();
  }
  else if(UserIndex==6)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_chase_random();
  }
  else if(UserIndex==7)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_chase_rainbow();
  }
  else if(UserIndex==8)
  {
    SEGMENT.speed = WS2812FX_mode_chase_flash();
  }
  else if(UserIndex==9)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_chase_flash_random();
  }
  else if(UserIndex==10)
  {
    SEGMENT.speed = WS2812FX_mode_chase_blackout();
  }
  else if(UserIndex==11)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_chase_blackout_rainbow();
  }
  else if(UserIndex==12)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_color_sweep_random();
  }
  else if(UserIndex==13)
  {
    SEGMENT.speed = WS2812FX_mode_custom_WaterDrop();
  }
  /********** 刷新显示 *************/
  show();
  /**** 时间计数模式显示 ***********/
  UserRun_Switch(scene_num,type,PAOMA_MODES);
  /*****用户设定速度返回设置 ********/
  if(SEGMENT.speed <SPEED_MIN)
  {
     SEGMENT.speed = SPEED_MIN;
  }
  times = times >>1;
  #else
  /********** 刷新显示 *************/
  if(UserIndex==0)
  {
      g_paomaVoiceFlag = 0;
  }
  else if(UserIndex==1)
  {
     g_paomaVoiceFlag = 1;
  }
  /********** 刷新显示 *************/
  //PR_NOTICE("-----------------:%d %d %d \n", mcu_VoiceSpeed,SEGMENT_RUNTIME.counter_mode_call,ModeRunTimes);
  if(mcu_VoiceSpeed)
  {
    times = mcu_VoiceSpeed;
    /**** 时间计数模式显示 ***********/
    if(SEGMENT_RUNTIME.counter_mode_call > SEGMENT_LENGTH) 
    {
      UsrColorFlag = 1;
    }
    else
    {
      UsrColorFlag = 0;
    }
    UserSwitch_Ticks(1,type,PAOMA_MODES);    // PAOMA_CNTS
  }
  else
  {
    times = 1;
  }
  SEGMENT.speed = SPEED_MIN<<1;
  #endif
  return (SEGMENT.speed *times);
}

/************************* 音乐炫彩 节奏控制流动*************/
uint16_t WS2812Fx_Flash(uint8_t type,uint8_t scene_num,uint8_t times)
{
  #if 1
  /********** 刷新显示 *************/
  if(UserIndex==0)
  {
    Usr_Paoma0();
  }
  else if(UserIndex==1)
  {
    WS2812FX_mode_running_random();
  }
  else if(UserIndex==2)
  {
    WS2812FX_mode_multi_dynamic();
  }
  /********** 刷新显示 *************/
  if(mcu_VoiceSpeed)
  {
    times = mcu_VoiceSpeed;
    #if 0
    /**** 时间计数模式显示 ***********/
    UserRun_Switch(scene_num,type,FLASH_MODES);
    #else
    /**** 时间计数模式----每种模式运行1次 ***********/
    if(SEGMENT_RUNTIME.counter_mode_call > SEGMENT_LENGTH) 
    {
      FlashFlag = 1;
    }
    else
    {
      FlashFlag = 0;
    }
    /**** 时间控制--运行次数控制 ***********/
    show();
    UserSwitch_Ticks(1,type,FLASH_MODES);   // FLASH_RUNCNTS
    #endif
  }
  else
  {
    times = 1;
  }
  SEGMENT.speed = SPEED_MIN<<1;
  #else
  /********** 获取用户颜色 *************/
  UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  UserModeNum = scene_num;
  /********** 自定义效果实现 *************/
  if(UserIndex==0)
  {
    SEGMENT.speed = WS2812FX_mode_theater_chase();
  }
  else if(UserIndex==1)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_twinkle_random();
  }
  else if(UserIndex==2)
  {
    SEGMENT.speed = WS2812FX_mode_sparkle();
  }
  else if(UserIndex==3)
  {
    SEGMENT.speed = WS2812FX_mode_strobe();
  }
  else if(UserIndex==4)
  {
    SEGMENT.speed = WS2812FX_mode_multi_strobe();
  }
  else if(UserIndex==5)
  {
     __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_blink_rainbow();
  }
  else if(UserIndex==6)
  {
    SEGMENT.speed = WS2812FX_mode_circus_combustus();
  }
  else if(UserIndex==7)
  {
    SEGMENT.speed = WS2812FX_mode_halloween();
  }
  else if(UserIndex==8)
  {
    SEGMENT.speed = WS2812FX_mode_tricolor_chase();
  }
  else if(UserIndex==9)
  {
    SEGMENT.speed = WS2812FX_mode_custom_FixedTwlinkle();
  }
  else if(UserIndex==10)
  {
    SEGMENT.speed = WS2812FX_mode_custom_AlterTwlinkle();
  }
  else if(UserIndex==11)
  {
    SEGMENT.speed = WS2812FX_mode_custom_RandomTwinkle();
  }
  else if(UserIndex==12)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_custom_Confetti();
  }
  /********** 刷新显示 *************/
  show();
  /**** 时间计数模式显示 ***********/
  UserRun_Switch(scene_num,type,PAOMA_MODES);
  if(SEGMENT.speed <SPEED_MIN)
  {
     SEGMENT.speed = SPEED_MIN;
  }
  times = times;
  #endif
  return ((SEGMENT.speed /4) *times);
}

/************************* 音乐跑马 *************/
uint16_t WS2812Fx_Scan(uint8_t type,uint8_t scene_num,uint8_t times)
{
  #if 1
  /********** 刷新显示 *************/
  if(UserIndex==0)
  {
    UsrColors = RED;
    SEGMENT.speed = WS2812FX_mode_scan();
  }
  else if(UserIndex==1)
  {
    UsrColors = RED;
    SEGMENT.speed = WS2812FX_mode_dual_scan();
  }
  else if(UserIndex==2)
  {
    SEGMENT.speed = WS2812FX_mode_icu();
  }
  /********** 刷新显示 *************/
  if(mcu_VoiceSpeed)
  {
    times = mcu_VoiceSpeed;
    show();
    #if 0
    /**** 时间计数模式显示 ***********/
    UserRun_Switch(scene_num,type,FLASH_MODES);
    #else
    /**** 时间计数模式----每种模式运行1次 ***********/
    if(SEGMENT_RUNTIME.counter_mode_call > SEGMENT_LENGTH) 
    {
      ScanFlag = 1;
    }
    else
    {
      ScanFlag = 0;
    }
    /**** 时间控制--运行次数控制 ***********/
    UserSwitch_Ticks(1,type,SCAN_MODES);  // SCAN_RUNCNTS
    #endif
  }
  else
  {
    times = 1;
  }
  SEGMENT.speed = SPEED_MIN<<1;
  #else
  /********** 获取用户颜色 *************/
  UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  UserModeNum = scene_num;
  
  /********** 自定义效果实现 *************/
  if(UserIndex==0)
  {
    SEGMENT.speed = WS2812FX_mode_scan();
  }
  else if(UserIndex==1)
  {
    SEGMENT.speed = WS2812FX_mode_dual_scan();
  }
  else if(UserIndex==2)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_icu();
  }
  /********** 刷新显示 *************/
  show();
  /**** 时间计数模式显示 ***********/
  UserRun_Switch(scene_num,type,PAOMA_MODES);
  /*****用户设定速度返回设置 ********/
  if(SEGMENT.speed <SPEED_MIN)
  {
     SEGMENT.speed = SPEED_MIN;
  }
  times = times;
  #endif
  
  return ((SEGMENT.speed /4) *times);
}

/************************* 音乐闪烁 *************/
uint16_t WS2812Fx_Trip(uint8_t type,uint8_t scene_num,uint8_t times)
{
  #if 1
  /********** 刷新显示 *************/
  if(UserIndex==0)
  {
    UsrColors = RED;
    SEGMENT.speed = WS2812FX_mode_theater_chase();
  }
  else if(UserIndex==1)
  {
    SEGMENT.speed = WS2812FX_mode_custom_Confetti();
  }
  /********** 刷新显示 *************/
  if(mcu_VoiceSpeed)
  {
    times = mcu_VoiceSpeed;
    show();
    #if 0
    /**** 时间计数模式显示 ***********/
    UserRun_Switch(scene_num,type,FLASH_MODES);
    #else
    /**** 时间计数模式----每种模式运行1次 ***********/
    if(SEGMENT_RUNTIME.counter_mode_call > SEGMENT_LENGTH) 
    {
      TripFlag = 1;
    }
    else
    {
      TripFlag = 0;
    }
    /**** 时间控制--运行次数控制 ***********/
    UserSwitch_Ticks(1,type,TRIP_MODES);   // TRIP_RUNCNTS
    #endif
  }
  else
  {
    times = 1;
  }
  SEGMENT.speed = SPEED_MIN<<1;
  #else
  /********** 获取用户颜色 *************/
  UsrColors = __GetWs2812FxColors(__scene_cnt,type);
  UserModeNum = scene_num;
  
  /********** 自定义效果实现 *************/
  if(UserIndex==0)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_random_color();
  }
  else if(UserIndex==1)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_single_dynamic();
  }
  else if(UserIndex==2)
  {
    __scene_cnt = scene_num;
    SEGMENT.speed = WS2812FX_mode_rainbow();
  }
  /********** 刷新显示 *************/
  show();
  /**** 时间计数模式显示 ***********/
  UserRun_Switch(scene_num,type,PAOMA_MODES);
  /*****用户设定速度返回设置 ********/
  if(SEGMENT.speed <SPEED_MIN)
  {
     SEGMENT.speed = SPEED_MIN;
  }
  times = times;
  #endif
  return ((SEGMENT.speed /4) *times);
}

void WS2812FX(void)
{
      _mode[FX_MODE_STATIC]                  = &WS2812FX_mode_static;
      _mode[FX_MODE_BLINK]                   = &WS2812FX_mode_blink;
      _mode[FX_MODE_BREATH]                  = &WS2812FX_mode_breath;
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
      _mode[FX_MODE_RUNNING_LIGHTS]          = &WS2812FX_mode_running_lights;
      _mode[FX_MODE_ICU]                     = &WS2812FX_mode_icu;
      _mode[FX_MODE_CUSTOM_BWAVEFADE1]       = &WS2812FX_mode_custom_BWaveFade1;
  	  _mode[FX_MODE_CUSTOM_BWAVEFADE2]       = &WS2812FX_mode_custom_BWaveFade2;
  	  _mode[FX_MODE_CUSTOM_BWAVEFADE3]       = &WS2812FX_mode_custom_BWaveFade3;
  	  _mode[FX_MODE_CUSTOM_SPOTLIGHT]        = &WS2812FX_mode_custom_SportLight;
  	  _mode[FX_MODE_CUSTOM_FIXEDTWINKLE]     = &WS2812FX_mode_custom_FixedTwlinkle;
  	  _mode[FX_MODE_CUSTOM_ALTERTWINKLE]     = &WS2812FX_mode_custom_AlterTwlinkle;
  	  _mode[FX_MODE_CUSTOM_ALTERFADE]        = &WS2812FX_mode_custom_AlterFade;
  	  _mode[FX_MODE_CUSTOM_RANDOMTWINKLE]    = &WS2812FX_mode_custom_RandomTwinkle;
  	  _mode[FX_MODE_CUSTOM_AUTOSWITCH]       = &WS2812FX_mode_custom_AutoSwitch;
  	  _mode[FX_MODE_CUSTOM_WATERDROP]        = &WS2812FX_mode_custom_WaterDrop;
  	  _mode[FX_MODE_CUSTOM_GLITTER]          = &WS2812FX_mode_custom_Glitter;
  	  _mode[FX_MODE_CUSTOM_CONFETTI]         = &WS2812FX_mode_custom_Confetti;
}

