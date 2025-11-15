#include "ap.h"
#include "qmk/qmk.h"


void cliUpdate(void);
void lcdUpdate(void);



void apInit(void)
{   
  ledOn(_DEF_LED1);

  moduleInit();
  threadBegin();
  
  logBoot(false);
}

void apMain(void)
{
  uint32_t pre_time;
  bool is_led_on = true;



  pre_time = millis();
  while(1)
  {
    if (is_led_on && millis()-pre_time >= 500)
    {
      is_led_on = false;
      ledOff(_DEF_LED1);
    }
    lcdUpdate();
    delay(5);
  }
}

void lcdUpdate(void)
{
  static uint32_t pre_time = 0;
  static bool key_press_tbl[MATRIX_ROWS][MATRIX_COLS] = {0, };


  for (int rows=0; rows<MATRIX_ROWS; rows++)
  {
    for (int cols=0; cols<MATRIX_COLS; cols++)
    {
      if (keysGetPressed(rows, cols))
        key_press_tbl[rows][cols] = true; 
    }
  }  

  if (lcdDrawAvailable() && millis()-pre_time >= 100)
  {
    pre_time = millis();

    lcdClearBuffer(black);  


    rtc_time_t rtc_time;
    rtc_date_t rtc_date;
    const char *week_str[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    rtcGetTime(&rtc_time);
    rtcGetDate(&rtc_date);

    lcdDrawFillRect(0, 0, LCD_WIDTH, 20, green);
    lcdPrintfRect(0, 0, LCD_WIDTH, 20, black, 16, LCD_ALIGN_H_CENTER|LCD_ALIGN_V_CENTER, 
                  "%02d-%d-%02d %s  %02d:%02d:%02d", 
                  rtc_date.year, rtc_date.month, rtc_date.day, week_str[rtc_date.week],
                  rtc_time.hours, rtc_time.minutes, rtc_time.seconds
                );

    uint16_t box_w;
    uint16_t box_h;

    box_w = LCD_WIDTH/MATRIX_COLS;
    box_h = (LCD_HEIGHT-20)/MATRIX_ROWS;

    for (int rows=0; rows<MATRIX_ROWS; rows++)
    {
      for (int cols=0; cols<MATRIX_COLS; cols++)
      {
        if (key_press_tbl[rows][cols])
        {
          lcdDrawFillRect(0 + box_w * cols, 20 + box_h * rows, box_w, box_h, white);
        }
      }      
    }                
    memset(key_press_tbl, 0, sizeof(key_press_tbl));

    lcdRequestDraw();
  }
}