#include "launcher.h"
#include "quantum.h"

#include "app/clock/clock.h"
#include "app/test/test.h"
#include "app/calc/calc.h"
#include "app/gif/gif_app.h"


#define APP_MAX_CNT     8





static void uiThread(void const *arg);
static bool eventReceive(event_t *p_event);


static uint8_t     app_cnt = 0;
static app_info_t *p_app_info[APP_MAX_CNT];
static uint8_t     req_run_id = APP_ID_NONE;
static uint8_t     cur_run_id = APP_ID_NONE;
static bool        is_ready   = false;
static bool        is_app_run = false;
static bool        is_qmk_suspend = false;
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
  p_app_info[app_cnt++] = calcGetAppInfo();
  p_app_info[app_cnt++] = gifGetAppInfo();  
  p_app_info[app_cnt++] = testGetAppInfo();  
  
  lvglInit();

  ret = threadCreate("ui", uiThread, NULL, _HW_DEF_THREAD_MODULE_PRI, _HW_DEF_THREAD_MODULE_STACK);
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

  if (p_event->code == EVENT_QMK_SUSPEND)
  {
    is_qmk_suspend = true;
    lcdDisplayOffDimming(500);
  }

  if (p_event->code == EVENT_QMK_RESUME)
  {    
    if (is_qmk_suspend)
    {
      is_qmk_suspend = false;
      lcdDisplayOnDimming(500);
    }
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
    // uiDeInit(); // LVGL UI 제거
  }

  // 2. 새 앱 정보 확인
  app_info_t *p_req = uiGetAppInfo(req_run_id);
  if (p_req != NULL)
  {
    logPrintf("Enter App: %s\n", p_req->name);

    cur_run_id = req_run_id; // ID 동기화
    is_app_run = true;


    eepromWriteByte(HW_EEPROM_INIT_APP, cur_run_id);

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
    // uiInit();
  }
}

bool qmk_process_keys(uint16_t keycode, keyrecord_t *record)
{
  uint8_t bl_level;

  // logPrintf("keycode : 0x%X, %d\n", keycode, record->event.pressed);

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

      case UI_KC_SLOT:
        req_run_id = APP_ID_SLOT;        
        break;

      case UI_KC_BL_P:
        bl_level = constrain(lcdGetBackLight() + 5, 8, 100);
        lcdSetBackLight(bl_level);
        lcdSaveCfg();
        break;

      case UI_KC_BL_M:
        if (lcdGetBackLight() > 8)
        {        
          bl_level = constrain(lcdGetBackLight() - 5, 8, 100);
          lcdSetBackLight(bl_level);
          lcdSaveCfg();
        }
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

  if (record->event.pressed)
  {
    if (calcSetKeycode(keycode))
    {
      return false;
    }
    if (gifSetKeycode(keycode))
    {
      return false;
    }    
  } 

  if (cur_run_id == APP_ID_MATRIX)
  {
    return false;
  }

  return true; 
}

void uiThread(void const *arg)
{
  uint8_t init_id;

  req_run_id = APP_ID_CLOCK;

  eepromReadByte(HW_EEPROM_INIT_APP, &init_id);
  if (uiGetAppInfo(init_id) != NULL)
  {
    req_run_id = init_id;
  }

  via_qmk_lcd_init();

  while(1)
  {
    if (is_ready)
    {      
      launcherUpdate();
    }
    delay(5);
  }
}