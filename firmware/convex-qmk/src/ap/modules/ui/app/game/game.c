#include "game.h"


static void appInit(void);
static void appMain(void);
static bool eventReceive(event_t *p_event);

static app_info_t app_info = {
    .name = "게임",
    .init = appInit,
    .run_func = appMain,
};

LVGL_IMG_DEF(fighter);
LVGL_IMG_DEF(bullet);
LVGL_IMG_DEF(bomb);

static bool is_exit = false;


void appInit(void)
{
  eventSub(eventReceive);
}

void appMain(void)
{
  typedef struct
  {
    bool is_enable;
    uint8_t type;
    uint16_t color;
    int16_t x;
    int16_t y;
    int16_t size;
    int16_t speed;
  } bomb_info_t;


  int16_t block_x = 0;
  int16_t block_y = 0;
  int16_t block_size = 20;
  uint16_t block_speed = 4;
  uint32_t pre_time_buzzer;
  bomb_info_t bomb_info[10];

  image_t img_fighter;
  image_t img_bullet;
  image_t img_bomb;


  for (int i = 0; i < 10; i++)
  {
    bomb_info[i].is_enable = false;
  }

  img_fighter = lcdCreateImage(&fighter, 0, 0, 0, 0);
  img_bullet = lcdCreateImage(&bullet, 0, 0, 0, 0);
  img_bomb = lcdCreateImage(&bomb, 0, 0, 0, 0);


  pre_time_buzzer = millis();
  while(!is_exit)
  {
    // if (buttonGetPressed(0))
    // {
    //   break;
    // }

    if (lcdDrawAvailable())
    {
      lcdClearBuffer(black);

      lcdDrawImage(&img_fighter, block_x, block_y);


      lcdDrawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, white);


      for (int i=0; i<10; i++)
      {
        if (bomb_info[i].is_enable)
        {
          bomb_info[i].x += bomb_info[i].speed;
          if (bomb_info[i].x > LCD_WIDTH)
          {
            bomb_info[i].is_enable = false;
          }

          if (bomb_info[i].type == 0)
          {
            lcdDrawImage(&img_bullet, bomb_info[i].x, bomb_info[i].y);
          }
          if (bomb_info[i].type == 1)
          {
            lcdDrawImage(&img_bomb, bomb_info[i].x, bomb_info[i].y);
          }
        }
      }


      lcdRequestDraw();
    }
  }

  is_exit = false;  
}

app_info_t *gameGetAppInfo(void)
{
  return &app_info;
}

bool eventReceive(event_t *p_event)
{
  if (p_event->code == EVENT_UI_APP_EXIT)
  {
    is_exit = true;
  }

  return true;
}