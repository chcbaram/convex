#include "launcher.h"

#include "app/game/game.h"
#include "app/clock/clock.h"
#include "app/test/test.h"


enum
{
  APP_ID_CLOCK,
  APP_ID_TEST,
  // APP_ID_CALC,
  APP_ID_MAX
};



#define APP_MAX_CNT     8



static void uiInit(void);
static void uiDeInit(void);
static void uiEvent(lv_event_t * e);
static void uiThread(void const *arg);
static void btnThread(void const *arg);
static bool eventReceive(event_t *p_event);


extern lv_indev_t * indev_keypad;

static uint8_t app_cnt = 0;
static app_info_t *p_app_info[APP_MAX_CNT];
static bool is_req_run = false;
static uint8_t req_run_id = 0;
static uint8_t cur_run_id = 0;
static lv_obj_t *main_disp = NULL;
static bool is_ready = false;
static bool is_app_run = false;


LV_FONT_DECLARE(neo);
LV_FONT_DECLARE(convex32);
LV_FONT_DECLARE(convex24);
LV_FONT_DECLARE(convex20);
LV_FONT_DECLARE(convex16);

MODULE_DEF(launcher) 
{
  .name = "launcher",
  .priority = MODULE_PRI_LOW,
  .init = launcherInit,
  .event_cb = eventReceive,
};



bool launcherInit(void)
{
  bool ret;

  p_app_info[APP_ID_CLOCK] = clockGetAppInfo();
  p_app_info[APP_ID_TEST]  = testGetAppInfo();
  // p_app_info[APP_ID_CALC]  = gameGetAppInfo();

  app_cnt = APP_ID_MAX;

  lvglInit();

  uiInit();

  ret = threadCreate("ui", uiThread, NULL, _HW_DEF_THREAD_MODULE_PRI, _HW_DEF_THREAD_MODULE_STACK);
  assert(ret);
  ret = threadCreate("ui_btn", btnThread, NULL, _HW_DEF_THREAD_MODULE_PRI, _HW_DEF_THREAD_MODULE_STACK);
  assert(ret);

  logPrintf("[%s] uiThreadInit()\n", ret ? "OK":"E_");  
  return true;
}

bool eventReceive(event_t *p_event)
{
  if (p_event->code == EVENT_UI_READY)
  {
    is_ready = true;
  }
  return true;
}

void launcherUpdate(void)
{
  if (is_req_run)
  {
    cur_run_id = req_run_id;
    if (p_app_info[cur_run_id] != NULL)
    {
      uiDeInit();

      logPrintf("app run : %s\n", p_app_info[cur_run_id]->name);      
      is_req_run = false;
      is_app_run = true;
      p_app_info[cur_run_id]->init();
      p_app_info[cur_run_id]->run_func();      
      is_app_run = false;
      uiInit();
    }
  }
  else
  {
    lvglUpdate();
  }
}

void uiThread(void const *arg)
{
  is_req_run = true;
  req_run_id = APP_ID_CLOCK;

  while(1)
  {
    if (is_ready)
    {      
      launcherUpdate();
    }
    delay(5);
  }
}

void btnThread(void const *arg)
{
  enum
  {
    BTN_APP,
    BTN_MENU,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_UP,
    BTN_DOWN,
    BTN_ENTER,
    BTN_MAX
  };

  typedef struct
  {
    bool     is_use;
    uint8_t  state;
    uint32_t pre_time;
    uint8_t  row;
    uint8_t  col;
    bool     pressed;
    bool     pressed_event;
  } btn_info_t;

  
  btn_info_t btn_info[BTN_MAX];
  
  memset(btn_info, 0, sizeof(btn_info));
  
  btn_info[BTN_APP].is_use = true;
  btn_info[BTN_APP].row = 0;
  btn_info[BTN_APP].col = 17;

  btn_info[BTN_MENU].is_use = true;
  btn_info[BTN_MENU].row = 0;
  btn_info[BTN_MENU].col = 16;

  while(1)
  {    
    for (int i=0; i<BTN_MAX; i++)
    {
      switch(btn_info[i].state)
      {
        case 0:
          if (keysGetPressed(btn_info[i].row, btn_info[i].col))
          {
            btn_info[i].pre_time = millis();
            btn_info[i].state = 1;
          } 
          break;

        case 1:
          if (!keysGetPressed(btn_info[i].row, btn_info[i].col))
          {
            btn_info[i].state = 0;
          }
          if (millis()-btn_info[i].pre_time >= 50)
          {
            btn_info[i].pressed = true;
            btn_info[i].pressed_event = true;
            btn_info[i].pre_time = millis();
            btn_info[i].state = 2;
          } 
          break;

        case 2:
          if (!keysGetPressed(btn_info[i].row, btn_info[i].col))
          {
            btn_info[i].pre_time = millis();
            btn_info[i].state = 3;
          }
          break;

        case 3:
          if (keysGetPressed(btn_info[i].row, btn_info[i].col))
          {
            btn_info[i].state = 2;
          }
          if (millis()-btn_info[i].pre_time >= 50)
          {
            btn_info[i].pressed = false;
            btn_info[i].state = 0;
          } 
          break;        
      }
    }

    if (btn_info[BTN_APP].pressed_event)
    {       
      is_req_run = true;
      req_run_id = (cur_run_id + 1) % APP_ID_MAX;

      if (is_app_run)
        eventPub(EVENT_UI_APP_EXIT, 1);
    } 

    if (btn_info[BTN_MENU].pressed_event)
    {       
      if (is_app_run)
        eventPub(EVENT_UI_APP_EXIT, 1);
    } 

    for (int i=0; i<BTN_MAX; i++)
    {
      btn_info[i].pressed_event = false;
    }    
    delay(1);
  }
}

void uiEvent(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);

  logPrintf("key event  %d\n", code);
  logPrintf("id  %d\n", (int)lv_event_get_user_data(e));

  req_run_id = (int)lv_event_get_user_data(e);
  is_req_run = true; 
}

void uiInit(void)
{
	lv_theme_t * th = lv_theme_default_init(NULL,
		lv_palette_main(LV_PALETTE_PURPLE),
		lv_palette_main(LV_PALETTE_RED),
		true,
		LV_FONT_DEFAULT);
	lv_disp_set_theme(NULL, th);

  main_disp = lv_obj_create(lv_screen_active());
  lv_obj_set_size(main_disp, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_scroll_snap_x(main_disp, LV_SCROLL_SNAP_CENTER);
  lv_obj_set_scroll_snap_y(main_disp, LV_SCROLL_SNAP_CENTER);
  lv_obj_set_flex_flow(main_disp, LV_FLEX_FLOW_ROW);
  lv_obj_align(main_disp, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_scrollbar_mode(main_disp, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_dir(main_disp, LV_DIR_HOR);

  // lv_obj_set_style_text_font(main_disp, &neo, LV_PART_MAIN);
  lv_obj_set_style_text_font(main_disp, &convex20, LV_PART_MAIN);

  lv_group_t * g = lv_group_create();

  for (int i = 0; i < app_cnt; i++)
  {
    lv_obj_t *btn = lv_button_create(main_disp);
    lv_obj_set_size(btn, 150, lv_pct(80));

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text_fmt(label, p_app_info[i]->name);
    lv_obj_center(label);
    lv_group_add_obj(g, btn);

    lv_obj_add_event_cb(btn, uiEvent, LV_EVENT_CLICKED, (void *)i);
    // lv_obj_set_style_text_font(label, &neo, LV_PART_MAIN);    
  }
  lv_indev_set_group(indev_keypad, g);

  lv_obj_scroll_to_view(lv_obj_get_child(main_disp, 0), LV_ANIM_OFF);
  lv_group_focus_obj(lv_obj_get_child(main_disp, 0));
  lv_obj_update_snap(main_disp, LV_ANIM_ON);
}

void uiDeInit(void)
{
  lv_obj_delete(main_disp);
  main_disp = NULL;
}