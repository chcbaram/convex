#ifndef UPDATE_APP_H_
#define UPDATE_APP_H_


#include "ap_def.h"


// 웹(WebHID)에서 펌웨어나 슬롯을 쓰는 동안 진행률을 보여주는 화면.
// 사용자가 고르는 앱이 아니라 fwupdate 가 필요할 때 띄운다.
app_info_t *updateGetAppInfo(void);

#endif
