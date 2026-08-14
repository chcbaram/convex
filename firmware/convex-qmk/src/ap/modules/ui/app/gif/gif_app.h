#ifndef GIF_APP_H_
#define GIF_APP_H_


#include "ap_def.h"



app_info_t *gifGetAppInfo(void);
bool        gifSetKeycode(uint16_t keycode);

// 지정한 슬롯을 화면에 띄운다. 같은 슬롯이어도 다시 읽으므로 방금 전송한
// GIF 를 바로 보여주는 데도 쓴다. 앱이 실행 중이 아니면 다음에 켤 때 적용된다.
void        gifReqSlot(uint8_t slot);

#endif