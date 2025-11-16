#include "test.h"


static void appInit(void);
static void appMain(void);
static void lcdUpdate(void);
static bool eventReceive(event_t *p_event);

static app_info_t app_info = {
    .name = "KEY TEST",
    .init = appInit,
    .run_func = appMain,
};

static bool is_exit = false;



void appInit(void)
{
  static bool is_first = true;  

  is_exit = false;

  if (is_first)
  {
    is_first = false;
    eventSub(eventReceive);
  }  
}

void appMain(void)
{
  eventPub(EVENT_QMK_ENABLE, 0);
  delay(100);

  while(!is_exit)
  {
    keysUpdate();
    lcdUpdate();
    delay(1);    
  }
  is_exit = false;

  delay(100);
  eventPub(EVENT_QMK_ENABLE, 1);
}

bool eventReceive(event_t *p_event)
{
  if (p_event->code == EVENT_UI_APP_EXIT)
  {
    is_exit = true;
  }

  return true;
}

app_info_t *testGetAppInfo(void)
{
  return &app_info;
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


    lcdDrawFillRect(0, 0, LCD_WIDTH, 20, green);
    lcdPrintfRect(0, 0, LCD_WIDTH, 20, black, 16, LCD_ALIGN_H_LEFT|LCD_ALIGN_V_CENTER, 
                  "MATRIX");

    uint16_t box_w;
    uint16_t box_h;
    int16_t row_sel = -1;
    int16_t col_sel = -1;

    box_w = LCD_WIDTH/MATRIX_COLS;
    box_h = (LCD_HEIGHT-20)/MATRIX_ROWS;

    for (int rows=0; rows<MATRIX_ROWS; rows++)
    {
      for (int cols=0; cols<MATRIX_COLS; cols++)
      {
        if (key_press_tbl[rows][cols])
        {
          row_sel = rows;
          col_sel = cols;
          lcdDrawFillRect(0 + box_w * cols, 20 + box_h * rows, box_w, box_h, white);
        }
      }      
    }                

    if (row_sel >= 0)
    {
      lcdPrintfRect(0, 0, LCD_WIDTH, 20, black, 16, LCD_ALIGN_H_CENTER|LCD_ALIGN_V_CENTER, 
                    "ROW:%02d COL:%02d ", 
                    row_sel,
                    col_sel);
    }

    memset(key_press_tbl, 0, sizeof(key_press_tbl));

    lcdRequestDraw();
  }
}