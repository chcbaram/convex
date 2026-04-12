#include "launcher.h"
#include "quantum.h"

#include "app/game/game.h"
#include "app/clock/clock.h"
#include "app/test/test.h"
#include "app/calc/calc.h"


#define APP_MAX_CNT     8




static void uiInit(void);
static void uiDeInit(void);
static void uiEvent(lv_event_t * e);
static void uiThread(void const *arg);
static void btnThread(void const *arg);
static bool eventReceive(event_t *p_event);


extern lv_indev_t * indev_keypad;

static uint8_t     app_cnt = 0;
static app_info_t *p_app_info[APP_MAX_CNT];
static uint8_t     req_run_id = APP_ID_NONE;
static uint8_t     cur_run_id = APP_ID_NONE;
static lv_obj_t   *main_disp  = NULL;
static bool        is_ready   = false;
static bool        is_app_run = false;
static app_args_t  app_args   = {
  .is_exit = false,
};

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

  
  p_app_info[app_cnt++] = clockGetAppInfo();
  p_app_info[app_cnt++] = testGetAppInfo();
  p_app_info[app_cnt++] = calcGetAppInfo();
  
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

app_info_t *uiGetAppInfo(uint16_t id)
{
  app_info_t *p_info = NULL;
  
  for (int i=0; i<app_cnt; i++)
  {
    if (p_app_info[i]->id == id)
    {
      p_info = p_app_info[i];
      break;
    }
  }

  return p_info;
}

static int8_t get_app_index_by_id(uint8_t id)
{
  for (int i = 0; i < app_cnt; i++)
  {
    if (p_app_info[i]->id == id) 
      return i;
  }
  return -1;
}

uint8_t uiGetPrevAppId(uint8_t current_id)
{
  int8_t idx = get_app_index_by_id(current_id);
  if (idx <= 0) 
  {
    // 현재 첫 번째 앱이거나 찾지 못한 경우 마지막 앱으로 이동
    return p_app_info[app_cnt - 1]->id; 
  }
  return p_app_info[idx - 1]->id;
}

uint8_t uiGetNextAppId(uint8_t current_id)
{
  int8_t idx = get_app_index_by_id(current_id);
  if (idx == -1) 
    return p_app_info[0]->id; // 못찾으면 첫번째
  return p_app_info[(idx + 1) % app_cnt]->id;
}

void uiReqAppExit(void)
{
  app_args.is_exit = true;
}

void launcherUpdate(void)
{
  if (req_run_id == cur_run_id)
  {
    lvglUpdate();
    return;
  }

  // 1. 현재 앱 종료 처리
  app_info_t *p_cur = uiGetAppInfo(cur_run_id);
  if (p_cur != NULL)
  {
    logPrintf("Exit App: %s\n", p_cur->name);
    uiDeInit(); // LVGL UI 제거
  }

  // 2. 새 앱 정보 확인
  app_info_t *p_req = uiGetAppInfo(req_run_id);
  if (p_req != NULL)
  {
    logPrintf("Enter App: %s\n", p_req->name);

    cur_run_id = req_run_id; // ID 동기화
    is_app_run = true;

    // 3. 새 앱 실행
    if (p_req->init) 
      p_req->init();
    if (p_req->run_func) 
    {
      app_args.is_exit = false;
      p_req->run_func(&app_args); // 여기서 앱이 block 될 수 있음
    }

    is_app_run = false;

    // 4. 앱 종료 후 런처(메인 메뉴) UI 복구
    uiInit();
  }
}

bool qmk_process_keys(uint16_t keycode, keyrecord_t *record)
{
  bool is_calc_mode = false;
  bool is_matrix_mode = false;


  is_calc_mode   = (cur_run_id == APP_ID_CALC);
  is_matrix_mode = (cur_run_id == APP_ID_MATRIX);


  logPrintf("is_calc : %d\n", is_calc_mode);
  logPrintf("keycode : 0x%X, %d\n", keycode, record->event.pressed);

  if (record->event.pressed)
  {
    bool is_ui_key = true;

    switch(keycode)
    {
      case UI_KC_APP_P:
        req_run_id = uiGetNextAppId(cur_run_id);        
        break;

      case UI_KC_APP_M:
        req_run_id = uiGetPrevAppId(cur_run_id);        
        break;

      case UI_KC_CLK:
        req_run_id = APP_ID_CLOCK;        
        break;

      case UI_KC_CAL:
        req_run_id = APP_ID_CALC;        
        break;

      case UI_KC_MTX:
        req_run_id = APP_ID_MATRIX;        
        break;

      default:
        is_ui_key = false;
        break;
    }    

    if (is_ui_key)
    {
      if (req_run_id != cur_run_id)
      {
        uiReqAppExit();
      }
      return false;
    }
  }

  if (is_calc_mode)
  {
    if (record->event.pressed)
    {
      if (calcSetKeycode(keycode))
      {
        return false;
      }
    } 
  }

  if (is_matrix_mode)
  {
    return false;
  }

  return true; 
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
    // for (int i=0; i<BTN_MAX; i++)
    // {
    //   switch(btn_info[i].state)
    //   {
    //     case 0:
    //       if (keysGetPressed(btn_info[i].row, btn_info[i].col))
    //       {
    //         btn_info[i].pre_time = millis();
    //         btn_info[i].state = 1;
    //       } 
    //       break;

    //     case 1:
    //       if (!keysGetPressed(btn_info[i].row, btn_info[i].col))
    //       {
    //         btn_info[i].state = 0;
    //       }
    //       if (millis()-btn_info[i].pre_time >= 50)
    //       {
    //         btn_info[i].pressed = true;
    //         btn_info[i].pressed_event = true;
    //         btn_info[i].pre_time = millis();
    //         btn_info[i].state = 2;
    //       } 
    //       break;

    //     case 2:
    //       if (!keysGetPressed(btn_info[i].row, btn_info[i].col))
    //       {
    //         btn_info[i].pre_time = millis();
    //         btn_info[i].state = 3;
    //       }
    //       break;

    //     case 3:
    //       if (keysGetPressed(btn_info[i].row, btn_info[i].col))
    //       {
    //         btn_info[i].state = 2;
    //       }
    //       if (millis()-btn_info[i].pre_time >= 50)
    //       {
    //         btn_info[i].pressed = false;
    //         btn_info[i].state = 0;
    //       } 
    //       break;        
    //   }
    // }

    // if (btn_info[BTN_APP].pressed_event)
    // {       
    //   is_req_run = true;
    //   req_run_id = (cur_run_id + 1) % APP_ID_MAX;

    //   if (is_app_run)
    //     eventPub(EVENT_UI_APP_EXIT, 1);
    // } 

    // if (btn_info[BTN_MENU].pressed_event)
    // {       
    //   if (is_app_run)
    //     eventPub(EVENT_UI_APP_EXIT, 1);
    // } 

    // for (int i=0; i<BTN_MAX; i++)
    // {
    //   btn_info[i].pressed_event = false;
    // }    
    delay(1);
  }
}

void uiEvent(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);

  logPrintf("key event  %d\n", code);
  logPrintf("id  %d\n", (int)lv_event_get_user_data(e));

  req_run_id = (int)lv_event_get_user_data(e);
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

void uiThread(void const *arg)
{
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