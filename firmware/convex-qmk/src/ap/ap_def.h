#ifndef AP_DEF_H_
#define AP_DEF_H_


#include "hw.h"



#define UI_KC_APP_P     QK_KB_0
#define UI_KC_APP_M     QK_KB_1
#define UI_KC_CLK       QK_KB_2
#define UI_KC_CAL       QK_KB_3
#define UI_KC_MTX       QK_KB_4
#define UI_KC_CLR       QK_KB_5
#define UI_KC_DEL       QK_KB_6


enum
{
  APP_ID_NONE,
  APP_ID_CLOCK,
  APP_ID_MATRIX,
  APP_ID_CALC,
  APP_ID_GIF,
  APP_ID_MAX
};

typedef struct
{
  bool is_exit;
} app_args_t;

typedef struct
{
  uint16_t id;

  char name[32];
  
  lv_image_dsc_t *img;

  void (*init)(void);
  void (*run_func)(app_args_t *p_args);
} app_info_t;


// 공통으로 필요한 모듈 
//
#include "module.h"
#include "port/lcd_port.h"

#endif