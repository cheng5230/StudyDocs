#ifndef MBED_SPI_EXT_API_H
#define MBED_SPI_EXT_API_H

#define MAX_NUM_SEGMENTS 10
#define NUM_COLORS        3 /* number of colors per segment */
#define MAX_CUSTOM_MODES  4

 typedef uint16_t (*mode_ptr)(void);

 typedef struct Segment { // 20 bytes
	uint16_t start;
	uint16_t stop;
	uint16_t speed;
	uint8_t  mode;
	uint8_t  options;
	uint32_t colors[NUM_COLORS];
 }segment;

// segment runtime parameters
 typedef struct Segment_runtime { // 16 bytes
	unsigned long next_time;
	uint32_t counter_mode_step;
	uint32_t counter_mode_call;
	uint8_t aux_param;	 // auxilary param (usually stores a color_wheel index)
	uint8_t aux_param2;  // auxilary param (usually stores bitwise options)
	uint16_t aux_param3; // auxilary param (usually stores a segment index)
 }segment_runtime;

#define PWM_1            _PA_23 
#define WS2812_GPIO      _PA_23 
#define USE_GPIO        0
#define USE_SPI         1

// SPI1 (S2)
#define SPI1_MOSI  PA_23
#define SPI1_MISO  PA_22
#define SPI1_SCLK  PA_18
#define SPI1_CS    PA_19
#define SHOW_R  0x1
#define SHOW_G  0x2
#define SHOW_B  0X3
#define SHOW_W  0x4

#define SPI1_COLCK 10000000
#define PIXEL_MAX  100      

#define DEFAULT_BRIGHTNESS (uint8_t)50
#define DEFAULT_MODE       (uint8_t)0
#define DEFAULT_SPEED      (uint16_t)1000
#define DEFAULT_COLOR      (uint32_t)0xFF0000

#if defined(ESP8266) || defined(ESP32)
  //#pragma message("Compiling for ESP")
  #define SPEED_MIN (uint16_t)2
#else
  //#pragma message("Compiling for Arduino")
  #define SPEED_MIN (uint16_t)10
#endif
#define SPEED_MAX (uint16_t)65535

#define BRIGHTNESS_MIN (uint8_t)0
#define BRIGHTNESS_MAX (uint8_t)255

/* each segment uses 36 bytes of SRAM memory, so if you're application fails because of
  insufficient memory, decreasing MAX_NUM_SEGMENTS may help */
extern segment _segments[MAX_NUM_SEGMENTS];				  // SRAM footprint: 20 bytes per element 
extern segment_runtime _segment_runtimes[MAX_NUM_SEGMENTS]; // SRAM footprint: 16 bytes per element

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
#define DARK(c)    (uint32_t)((c >> 4) & 0x0f0f0f0f)

// segment options
// bit    7: reverse animation
// bits 4-6: fade rate (0-7)
// bit    3: gamma correction
// bits 1-2: size
// bits   0: TBD
#define NO_OPTIONS   (uint8_t)0x00
#define REVERSE      (uint8_t)0x80
#define IS_REVERSE   ((SEGMENT.options & (REVERSE))== REVERSE)
#define FADE_XFAST   (uint8_t)0x10
#define FADE_FAST    (uint8_t)0x20
#define FADE_MEDIUM  (uint8_t)0x30
#define FADE_SLOW    (uint8_t)0x40
#define FADE_XSLOW   (uint8_t)0x50
#define FADE_XXSLOW  (uint8_t)0x60
#define FADE_GLACIAL (uint8_t)0x70
#define FADE_RATE    ((SEGMENT.options >> 4) & 7)
#define GAMMA        (uint8_t)0x08
#define IS_GAMMA     ((SEGMENT.options & GAMMA) == GAMMA)
#define SIZE_SMALL   (uint8_t)0x00
#define SIZE_MEDIUM  (uint8_t)0x02
#define SIZE_LARGE   (uint8_t)0x04
#define SIZE_XLARGE  (uint8_t)0x06
#define SIZE_OPTION  ((SEGMENT.options >> 1) & 3)

// segment runtime options (aux_param2)
#define FRAME     (uint8_t)0x80
#define SET_FRAME (SEGMENT_RUNTIME.aux_param2 |=  FRAME)
#define CLR_FRAME (SEGMENT_RUNTIME.aux_param2 &= ~FRAME)
#define CYCLE     (uint8_t)0x40
#define SET_CYCLE (SEGMENT_RUNTIME.aux_param2 |=  CYCLE)
#define CLR_CYCLE (SEGMENT_RUNTIME.aux_param2 &= ~CYCLE)
#define MODE_COUNT (sizeof(_names)/sizeof(_names[0]))

#define FX_MODE_STATIC                   0
#define FX_MODE_BLINK                    1
#define FX_MODE_BREATH                   2
#define FX_MODE_COLOR_WIPE               3
#define FX_MODE_COLOR_WIPE_INV           4 
#define FX_MODE_COLOR_WIPE_REV           5
#define FX_MODE_COLOR_WIPE_REV_INV       6
#define FX_MODE_COLOR_WIPE_RANDOM        7
#define FX_MODE_RANDOM_COLOR             8
#define FX_MODE_SINGLE_DYNAMIC           9
#define FX_MODE_MULTI_DYNAMIC           10
#define FX_MODE_RAINBOW                 11
#define FX_MODE_RAINBOW_CYCLE           12
#define FX_MODE_SCAN                    13
#define FX_MODE_DUAL_SCAN               14
#define FX_MODE_FADE                    15
#define FX_MODE_THEATER_CHASE           16
#define FX_MODE_THEATER_CHASE_RAINBOW   17
#define FX_MODE_RUNNING_LIGHTS          18
#define FX_MODE_TWINKLE                 19
#define FX_MODE_TWINKLE_RANDOM          20
#define FX_MODE_TWINKLE_FADE            21
#define FX_MODE_TWINKLE_FADE_RANDOM     22
#define FX_MODE_SPARKLE                 23
#define FX_MODE_FLASH_SPARKLE           24
#define FX_MODE_HYPER_SPARKLE           25
#define FX_MODE_STROBE                  26
#define FX_MODE_STROBE_RAINBOW          27
#define FX_MODE_MULTI_STROBE            28
#define FX_MODE_BLINK_RAINBOW           29
#define FX_MODE_CHASE_WHITE             30
#define FX_MODE_CHASE_COLOR             31
#define FX_MODE_CHASE_RANDOM            32
#define FX_MODE_CHASE_RAINBOW           33
#define FX_MODE_CHASE_FLASH             34
#define FX_MODE_CHASE_FLASH_RANDOM      35
#define FX_MODE_CHASE_RAINBOW_WHITE     36
#define FX_MODE_CHASE_BLACKOUT          37
#define FX_MODE_CHASE_BLACKOUT_RAINBOW  38
#define FX_MODE_COLOR_SWEEP_RANDOM      39
#define FX_MODE_RUNNING_COLOR           40
#define FX_MODE_RUNNING_RED_BLUE        41
#define FX_MODE_RUNNING_RANDOM          42
#define FX_MODE_LARSON_SCANNER          43
#define FX_MODE_COMET                   44
#define FX_MODE_FIREWORKS               45
#define FX_MODE_FIREWORKS_RANDOM        46
#define FX_MODE_MERRY_CHRISTMAS         47
#define FX_MODE_FIRE_FLICKER            48
#define FX_MODE_FIRE_FLICKER_SOFT       49
#define FX_MODE_FIRE_FLICKER_INTENSE    50
#define FX_MODE_CIRCUS_COMBUSTUS        51
#define FX_MODE_HALLOWEEN               52
#define FX_MODE_BICOLOR_CHASE           53
#define FX_MODE_TRICOLOR_CHASE          54
#define FX_MODE_ICU                     55
#define FX_MODE_CUSTOM                  56  // keep this for backward compatiblity
#define FX_MODE_CUSTOM_0                56  // custom modes need to go at the end
#define FX_MODE_CUSTOM_1                57
#define FX_MODE_CUSTOM_2                58
#define FX_MODE_CUSTOM_3                59

void WS2812b_Init(void);
void WS2812B_Test(void);



#endif

