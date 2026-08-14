#ifndef LAUNCHER_H_
#define LAUNCHER_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "ap_def.h"



bool launcherInit(void);
void launcherUpdate(void);

// 지정한 앱으로 전환한다. 모르는 id 는 무시한다.
void uiReqApp(uint8_t app_id);

// 업데이트 화면처럼 잠깐 끼어든 앱에서 원래 앱으로 돌아간다.
void uiReqAppBack(void);


#ifdef __cplusplus
}
#endif

#endif
