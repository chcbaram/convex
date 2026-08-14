#include "lv_port_disp.h"
#include "lcd.h"
#include "lcd/st7789.h"


/*********************
 *      DEFINES
 *********************/


#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
static void flush_wait_cb(lv_display_t * disp);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_display_t * disp;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_callback(void)
{
  lv_display_flush_ready(disp);
}

void lv_port_disp_init(void)
{
  /*-------------------------
   * Initialize your display
   * -----------------------*/
  disp_init();

  /*------------------------------------
   * Create a display and set a flush_cb
   * -----------------------------------*/
  disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  lv_display_set_flush_cb(disp, disp_flush);
  lv_display_set_flush_wait_cb(disp, flush_wait_cb);

  lv_display_set_buffers(disp, lcdGetFrameBuffer(), NULL, LCD_WIDTH*LCD_HEIGHT*2, LV_DISPLAY_RENDER_MODE_DIRECT);
  // st7789SetCallBack(lv_port_callback);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
  /*You code here*/
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
  disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
  disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
  if(disp_flush_enabled)
  {
    // 프레임버퍼에 직접 그리므로(DIRECT) 화면 전체를 한 번만 보내면 된다.
    // 한 프레임에서 바뀐 영역이 여러 개면 이 함수가 그 수만큼 불리는데,
    // 그때마다 보내면 전체 전송이 여러 번 일어나 그만큼 느려진다.
    // 객체가 많은 화면(스펙트럼 앱)이 GIF 보다 느렸던 이유다.
    if (lv_display_flush_is_last(disp_drv) != true)
    {
      return;
    }

    if (lcdDrawAvailable())
    {
      lcdRequestDraw();
    }
  }
}

static void flush_wait_cb(lv_display_t * disp)
{
  uint32_t pre_time = millis();


  while(millis()-pre_time < 50)
  {
    if (lcdDrawAvailable())
      break;

    osThreadYield();
  }
}