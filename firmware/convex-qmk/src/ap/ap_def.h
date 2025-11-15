#ifndef AP_DEF_H_
#define AP_DEF_H_


#include "hw.h"



typedef struct
{
  char name[32];
  
  lv_image_dsc_t *img;

  void (*init)(void);
  void (*run_func)(void);
} app_info_t;


// 공통으로 필요한 모듈 
//
#include "module.h"


#endif