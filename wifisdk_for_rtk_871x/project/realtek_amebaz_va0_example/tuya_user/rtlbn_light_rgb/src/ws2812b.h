#ifndef WS2812_H
#define WS2812_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_adapter_platform.h"


/* each segment uses 36 bytes of SRAM memory, so if you're application fails because of
  insufficient memory, decreasing MAX_NUM_SEGMENTS may help */
#define NUM_COLORS        3      /* number of colors per segment */
typedef struct Segment { // 20 bytes
  uint16_t start;
  uint16_t stop;
  uint16_t speed;
  uint8_t  mode;
  uint8_t  options;
  uint32_t colors[NUM_COLORS];
} segment;

// segment runtime parameters
typedef struct Segment_runtime { // 16 bytes
	unsigned long next_time;
	uint32_t counter_mode_step;  // 模式运行步数计数
	uint32_t counter_mode_call; // 模式显示的次数计数
	uint8_t aux_param;	 // auxilary param (usually stores a color_wheel index)
	uint8_t aux_param2;  // auxilary param (usually stores bitwise options)
	uint16_t aux_param3; // auxilary param (usually stores a segment index)
} segment_runtime;


typedef struct{
	uint8_t hue;
	uint8_t val;
	uint8_t sat;
}CHSV;

typedef uint8_t bool;
typedef uint8_t boolean;

//#define true 1
//#define false 0

typedef uint16_t (*mode_ptr)(void);

#define WS2812FX_MODE   1
#define PAOMA_LEDS      3
#define WATER_LEDS       20
#define MODE_RUNS        47
#define TRIPLEDS_DIV     5
#define RANDOM_LEDS      32
#define RUNNING_10S      8000
#define RUNNING_20S      16000
#define RUNNING_30S      24000
#define AutoSwitch_5s    4000
#define AutoSwitch_10s   8000

#define SPEED_MIN (uint16_t)10
#define SPEED_MAX (uint16_t)65535

#define DEFAULT_BRIGHTNESS (uint8_t)200
#define DEFAULT_MODE       (uint8_t)0
#define DEFAULT_SPEED      (uint16_t)1000
#define DEFAULT_COLOR      (uint32_t)0xff0000
#define SPEED_MIN          (uint16_t)10
#define SPEED_MAX          (uint16_t)65535
#define BRIGHTNESS_MIN     (uint8_t)0
#define BRIGHTNESS_MAX     (uint8_t)255

//20~100 Less cooling = taller flames.  More cooling = shorter flames.
#define COOLING  25
//50~200 Higher chance = more roaring fire.  Lower chance = more flickery fire.
#define SPARKING 180

#define SEGMENT          _segments[_segment_index]
#define SEGMENT_RUNTIME  _segment_runtimes[_segment_index]
#define SEGMENT_LENGTH   (SEGMENT.stop - SEGMENT.start + 1)

// some common colors
#define RED        (uint32_t)0xFF0000
#define GREEN      (uint32_t)0x00FF00
#define BLUE       (uint32_t)0x0000FF
#define WHITE      (uint32_t)0xFFFFFF
#define BLACK      (uint32_t)0x000000
#define YELLOW     (uint32_t)0xFFFF00
#define CYAN       (uint32_t)0x00FFFF
#define MAGENTA    (uint32_t)0xFF00FF
#define PURPLE     (uint32_t)0x400080
#define ORANGE     (uint32_t)0xFF3000
#define PINK       (uint32_t)0xFF1493
#define ULTRAWHITE (uint32_t)0xFFFFFFFF
#define DARK(c)    (uint32_t)(((c)>>4)&0x0f0f0f0f)

// segment options
// bit    7: reverse animation
// bits 4-6: fade rate (0-7)
// bit    3: gamma correction
// bits 1-2: size
// bits   0: TBD
#define NO_OPTIONS   (uint8_t)0x00
#define REVERSE      (uint8_t)0x80
#define IS_REVERSE   ((SEGMENT.options & REVERSE) == REVERSE)
#define FADE_XFAST   (uint8_t)0x10
#define FADE_FAST    (uint8_t)0x20
#define FADE_MEDIUM  (uint8_t)0x30
#define FADE_SLOW    (uint8_t)0x40
#define FADE_XSLOW   (uint8_t)0x50
#define FADE_XXSLOW  (uint8_t)0x60
#define FADE_GLACIAL (uint8_t)0x70
#define FADE_RATE    ((uint8_t)(SEGMENT.options >> 4) & 7)
#define GAMMA        (uint8_t)0x08
#define IS_GAMMA     ((SEGMENT.options & GAMMA) == GAMMA)
#define SIZE_SMALL   (uint8_t)0x00
#define SIZE_MEDIUM  (uint8_t)0x02
#define SIZE_LARGE   (uint8_t)0x04
#define SIZE_XLARGE  (uint8_t)0x06
#define SIZE_OPTION  ((uint8_t)(SEGMENT.options >> 1) & 3)
#define TRIFADE_BLACK (uint8_t)0x80
#define COOLING       (uint8_t)55
#define SPARKING      (uint8_t)120

// segment runtime options (aux_param2)
#define FRAME     (uint8_t)0x80
#define SET_FRAME (uint8_t)(SEGMENT_RUNTIME.aux_param2 |=  FRAME)
#define CLR_FRAME (uint8_t)(SEGMENT_RUNTIME.aux_param2 &= (uint8_t)(~FRAME))
#define CYCLE     (uint8_t)0x40
#define SET_CYCLE (uint8_t)(SEGMENT_RUNTIME.aux_param2 |=  CYCLE)
#define CLR_CYCLE (uint8_t)(SEGMENT_RUNTIME.aux_param2 &= (uint8_t)(~CYCLE))

#define MODE_COUNT (sizeof(_names)/sizeof(_names[0]))
#define max(x, y)  ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) < (y) ? (x) : (y))
#define qadd8( i, j) min( ((i) + (j)), 0xFF)
#define qsub8( i, j) max( ((i) - (j)), 0)


#define FX_MODE_STATIC                   0    //  指定的单色colors[0]，          静态显示
#define FX_MODE_BLINK                    1    //  指定的双色colors[0]\[1]，切换显示
#define FX_MODE_BREATH                   2    //  指定的单色colors[0]，          呼吸效果
#define FX_MODE_COLOR_WIPE               3    //  指定的双色，RG-依次点亮，到全部点亮，然后顺向依次灭，到全部灭；循环
#define FX_MODE_COLOR_WIPE_INV           4    //  指定的双色，GR-依次点亮，到全部点亮，然后顺向依次灭，到全部灭；循环
#define FX_MODE_COLOR_WIPE_REV           5    //  指定的双色，R-依次点亮，到全部点亮，G-然后反向依次亮，R依次灭到全灭,G到全部全亮；
#define FX_MODE_COLOR_WIPE_REV_INV       6    //  指定的双色，G-依次点亮，到全部点亮，R-然后反向依次亮，R依次灭到全灭,G到全部全亮；
#define FX_MODE_COLOR_WIPE_RANDOM        7    //  随机颜色，依次点亮，到全部点亮，然后换颜色，顺向填充依次点亮，到全部点亮
#define FX_MODE_RANDOM_COLOR             8    //  七色随机静态变化，R全亮-G全亮替换......
#define FX_MODE_SINGLE_DYNAMIC           9    //  全亮七彩色，然后单色七彩灯在全灯带任意位置随机动态变化   
#define FX_MODE_MULTI_DYNAMIC           10    //  全亮七彩色，七彩色在全灯带的位置动态交替变化  
#define FX_MODE_RAINBOW                 11    //  全亮七彩色，单呼吸依次变化 
#define FX_MODE_RAINBOW_CYCLE           12    //  全亮七彩色，依次变化，七彩追光效果
#define FX_MODE_SCAN                    13    // 指定的单色colors[1]全亮，然后colors[0]单色灯依次来回填充扫描
#define FX_MODE_DUAL_SCAN               14    // 指定的单色colors[1]全亮，然后colors[0]单色灯两头到中间依次来回填充扫描
#define FX_MODE_FADE                    15    // 指定的双色colors[0]/[1]/合成色，单呼吸全亮，然后切换
#define FX_MODE_THEATER_CHASE           16    // 剧场效果，指定的双色colors[0]/[1]/合成色 快闪
#define FX_MODE_THEATER_CHASE_RAINBOW   17    // 剧场七彩效果，指定的双色colors[0]/[1]/合成色 呼吸同时快闪
#define FX_MODE_RUNNING_LIGHTS          18    // 平滑渐入渐出的射光效果
#define FX_MODE_TWINKLE                 19    // 指定的单色colors[1]全亮做为背景，然后colors[0]随机跳动
#define FX_MODE_TWINKLE_RANDOM          20    // 指定的单色colors[1]全亮做为背景，然后七彩色随机跳动
#define FX_MODE_TWINKLE_FADE            21    // 全黑背景色，指定的单色colors[0]随机跳动
#define FX_MODE_TWINKLE_FADE_RANDOM     22   // 全黑背景色， 七色随机跳动
#define FX_MODE_SPARKLE                 23   // 指定的单色colors[1]随机依次点亮直到全亮背景色，然后七色随机跳动
#define FX_MODE_FLASH_SPARKLE           24   // 指定的单色colors[0]全亮背景色，然后单个白光随机跳动
#define FX_MODE_HYPER_SPARKLE           25   // 指定的单色colors[0]全亮背景色，然后多路白光随机跳动
#define FX_MODE_STROBE                  26   // 指定的单色colors[0]背景色,频闪，同时colors[1]夹杂闪烁
#define FX_MODE_STROBE_RAINBOW          27   // 指定的单色colors[0]背景色,频闪，同时七色夹杂闪烁
#define FX_MODE_MULTI_STROBE            28   // 指定的单色colors[0]，sos 频闪
#define FX_MODE_BLINK_RAINBOW           29   // 指定的colors[1]与七色进行交替闪烁
#define FX_MODE_CHASE_WHITE             30   // 白色全亮作为背景色--红色填充跑步从头到尾循环跑步
#define FX_MODE_CHASE_COLOR             31   // 红色全亮作为背景色--白色填充跑步从头到尾循环跑步
#define FX_MODE_CHASE_RANDOM            32   // 随机色变换跑步，单色依次点亮，到全部点亮
#define FX_MODE_CHASE_RAINBOW           33   // 七色依次变化跑步，七彩色依次点亮，到全部点亮
#define FX_MODE_CHASE_FLASH             34   // 指定的colors[1]全亮作为背景色，两颗白色闪烁同时进行顺向跑步
#define FX_MODE_CHASE_FLASH_RANDOM      35   // 两颗白色闪烁同时进行顺向跑步，然后依次填充七色依次点亮直到全亮，然后换色重复
#define FX_MODE_CHASE_RAINBOW_WHITE     36   // 白色全亮作为背景色，然后七色（两颗灯）依次顺向重头到尾进行跑步
#define FX_MODE_CHASE_BLACKOUT          37   // 指定的colors[1]全亮作为背景色，然后黑色（两颗灯）填充依次顺向重头到尾进行跑步
#define FX_MODE_CHASE_BLACKOUT_RAINBOW  38   //七色灯全亮，然后七色灯替换依次跑步
#define FX_MODE_COLOR_SWEEP_RANDOM      39   //单色灯依次点亮，全部点亮，然后反向换色，依次点亮颜色，重复来回点亮跑步
#define FX_MODE_RUNNING_COLOR           40   // 交替颜色/白色交替快闪
#define FX_MODE_RUNNING_RED_BLUE        41   // 交替红色/蓝色交替快闪
#define FX_MODE_RUNNING_RANDOM          42   // 随机像素运行交替快闪
#define FX_MODE_LARSON_SCANNER          43   // 指定的colors[1]色，来回扫描
#define FX_MODE_COMET                   44   // 指定的colors[1]色，来回扫描.同上效果一样
#define FX_MODE_FIREWORKS               45  // 烟花烟火
#define FX_MODE_FIREWORKS_RANDOM        46  // 随机烟火
#define FX_MODE_MERRY_CHRISTMAS         47  // 圣诞效果
#define FX_MODE_FIRE_FLICKER            48  // 火焰闪烁效果。 在强风中
#define FX_MODE_FIRE_FLICKER_SOFT       49  //火焰闪烁效果。 运行较慢/柔和
#define FX_MODE_FIRE_FLICKER_INTENSE    50  // 火焰闪烁效果
#define FX_MODE_CIRCUS_COMBUSTUS        51  // 白色/七彩色组合闪烁效果
#define FX_MODE_HALLOWEEN               52  // 七彩色色/七彩色组合闪烁效果
#define FX_MODE_BICOLOR_CHASE           53  //指定的colors[0]作为背景色，然后colors[1]从头到尾跑步
#define FX_MODE_TRICOLOR_CHASE          54  //三色彩色像素交替三色像素运行
#define FX_MODE_ICU                     55  // 三段运行跑步/停止。重复
#define FX_MODE_CUSTOM_BWAVEFADE1       56  // 呼吸波1 : 每个灯依次点亮红，直到全部点亮;全部灯进入吸状态（逐渐变暗)
#define FX_MODE_CUSTOM_BWAVEFADE2       57  // 呼吸波2 : 每个灯依次点亮红，直到全部点亮;全部灯进入吸状态（逐渐变暗)
#define FX_MODE_CUSTOM_BWAVEFADE3       58  // 呼吸波3 : 每个灯依次点亮红，同时呼吸，直到全部点亮;然后切换颜色
#define FX_MODE_CUSTOM_SPOTLIGHT        59  // 追光效果 : 固定光追光, 红绿蓝黄紫色，全灯带依次追光
#define FX_MODE_CUSTOM_FIXEDTWINKLE     60  // 固定闪烁 : 奇数灯闪烁，切换偶数灯闪烁，切换颜色
#define FX_MODE_CUSTOM_ALTERTWINKLE     61  // 交替闪烁 : 奇数灯闪烁，切换偶数灯闪烁，流光颜色变化
#define FX_MODE_CUSTOM_ALTERFADE        62  // 奇数灯闪烁，切换偶数灯闪烁，流光颜色变化并且呼吸
#define FX_MODE_CUSTOM_RANDOMTWINKLE    63  // 随机闪烁 : 以时间作为随机数，超过32颗灯，则重复选择
#define FX_MODE_CUSTOM_AUTOSWITCH       64  // 自动变幻 : 在流光模式和随机闪烁模式之间切换，5s计时
#define FX_MODE_CUSTOM_WATERDROP        65  // 水滴效果 : 20个灯亮度不一，然后依次走动
#define FX_MODE_CUSTOM_GLITTER          66  // 彩虹闪烁效果
#define FX_MODE_CUSTOM_CONFETTI         67  // 撞击效果

extern const uint8_t  ModeRUN[MODE_RUNS]; 
extern boolean  _triggered;
extern uint8_t ModeRunIndex;
extern uint16_t ModeRunTimes;
extern uint16_t UsrH,UsrS,UsrV;
extern uint8_t UserIndex;
extern uint8_t StremberCnts;

void WS2812FX_Init(void);
void start(uint8_t index);
void WS2812FX_service(uint8_t type , uint8_t times,uint8_t scene_num,uint8_t mode);
uint16_t WS2812FX_mode_static(void);
uint16_t WS2812FX_mode_blink(void);
uint16_t WS2812FX_mode_breath(void);
uint16_t WS2812FX_mode_color_wipe(void);
uint16_t WS2812FX_mode_color_wipe_inv(void);
uint16_t WS2812FX_mode_color_wipe_rev(void);
uint16_t WS2812FX_mode_color_wipe_rev_inv(void);
uint16_t WS2812FX_mode_color_wipe_random(void);
uint16_t WS2812FX_mode_random_color(void);
uint16_t WS2812FX_mode_single_dynamic(void);
uint16_t WS2812FX_mode_multi_dynamic(void);
uint16_t WS2812FX_mode_rainbow_cycle(void);
uint16_t WS2812FX_mode_dual_scan(void);
uint16_t WS2812FX_mode_fade(void);
uint16_t WS2812FX_mode_theater_chase(void);
uint16_t WS2812FX_mode_theater_chase_rainbow(void);
uint16_t WS2812FX_mode_twinkle(void);
uint16_t WS2812FX_mode_twinkle_random(void);
uint16_t WS2812FX_mode_twinkle_fade(void);
uint16_t WS2812FX_mode_twinkle_fade_random(void);
uint16_t WS2812FX_mode_sparkle(void);
uint16_t WS2812FX_mode_flash_sparkle(void);
uint16_t WS2812FX_mode_hyper_sparkle(void);
uint16_t WS2812FX_mode_strobe(void);
uint16_t WS2812FX_mode_strobe_rainbow(void);
uint16_t WS2812FX_mode_multi_strobe(void);
uint16_t WS2812FX_mode_blink_rainbow(void);
uint16_t WS2812FX_mode_chase_white(void);
uint16_t WS2812FX_mode_chase_color(void);
uint16_t WS2812FX_mode_chase_random(void);
uint16_t WS2812FX_mode_chase_rainbow(void);
uint16_t WS2812FX_mode_chase_flash(void);
uint16_t WS2812FX_mode_chase_flash_random(void);
uint16_t WS2812FX_mode_chase_rainbow_white(void);
uint16_t WS2812FX_mode_chase_blackout(void);
uint16_t WS2812FX_mode_chase_blackout_rainbow(void);
uint16_t WS2812FX_mode_color_sweep_random(void);
uint16_t WS2812FX_mode_running_color(void);
uint16_t WS2812FX_mode_running_red_blue(void);
uint16_t WS2812FX_mode_running_random(void);
uint16_t WS2812FX_mode_larson_scanner(void);
uint16_t WS2812FX_mode_comet(void);
uint16_t WS2812FX_mode_fireworks(void);
uint16_t WS2812FX_mode_fireworks_random(void);
uint16_t WS2812FX_mode_merry_christmas(void);
uint16_t WS2812FX_mode_fire_flicker(void);
uint16_t WS2812FX_mode_fire_flicker_soft(void);
uint16_t WS2812FX_mode_fire_flicker_intense(void);
uint16_t WS2812FX_mode_circus_combustus(void);
uint16_t WS2812FX_mode_halloween(void);
uint16_t WS2812FX_mode_bicolor_chase(void);
uint16_t WS2812FX_mode_tricolor_chase(void);
uint16_t WS2812FX_mode_running_lights(void);
uint16_t WS2812FX_mode_icu(void);
uint16_t WS2812FX_mode_custom_BWaveFade1(void);
uint16_t WS2812FX_mode_custom_BWaveFade2(void);
uint16_t WS2812FX_mode_custom_BWaveFade3(void);
uint16_t WS2812FX_mode_custom_SportLight(void);
uint16_t WS2812FX_mode_custom_FixedTwlinkle(void);
uint16_t WS2812FX_mode_custom_AlterTwlinkle(void);
uint16_t WS2812FX_mode_custom_AlterFade(void);
uint16_t WS2812FX_mode_custom_RandomTwinkle(void);
uint16_t WS2812FX_mode_custom_AutoSwitch(void);
uint16_t WS2812FX_mode_custom_WaterDrop(void);
uint16_t WS2812FX_mode_custom_Glitter(void);
uint16_t WS2812FX_mode_custom_Confetti(void);
uint16_t WS2812Fx_Streamber(uint8_t type,uint8_t scene_num,uint8_t times);
uint16_t WS2812Fx_Breath(uint8_t type,uint8_t scene_num,uint8_t times);
uint16_t WS2812Fx_Fire(uint8_t type,uint8_t scene_num,uint8_t times);
uint16_t WS2812Fx_Colorful(uint8_t type,uint8_t scene_num,uint8_t times);
uint16_t WS2812Fx_PaoMa(uint8_t type,uint8_t scene_num,uint8_t times);
uint16_t WS2812Fx_PaoMaNormal(uint8_t type,uint8_t scene_num,uint8_t times);
uint16_t WS2812Fx_Flash(uint8_t type,uint8_t scene_num,uint8_t times);
uint16_t WS2812Fx_FlashNormal(uint8_t type,uint8_t scene_num,uint8_t times);
uint16_t WS2812Fx_Scan(uint8_t type,uint8_t scene_num,uint8_t times);  
uint16_t WS2812Fx_ScanNormal(uint8_t type,uint8_t scene_num,uint8_t times);  
uint16_t WS2812Fx_Trip(uint8_t type,uint8_t scene_num,uint8_t times);
uint16_t WS2812Fx_TripNormal(uint8_t type,uint8_t scene_num,uint8_t times);


#define HSV_MAXSTEPS        51
#define STREMBER_MODES      2
#define STREMBER_RUNCNTS    5
#define BREATH_MODES        5
#define FIRE_MODES          4 //17
#define COLORFUL_MODES      4 //16
#define MYPAOMA_MODES        14
#define MYFLASH_MODES        12
#define MYSCAN_MODES         3
#define MYTRIP_MODES         3

#define PAOMA_MODES        2
#define PAOMA_CNTS         1
#define FLASH_MODES        3
#define FLASH_RUNCNTS      3
#define SCAN_MODES         3
#define SCAN_RUNCNTS       3
#define TRIP_MODES         2 
#define TRIP_RUNCNTS       3


#ifdef __cplusplus
}
#endif


#endif

