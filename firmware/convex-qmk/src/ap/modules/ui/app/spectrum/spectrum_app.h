#ifndef SPECTRUM_APP_H_
#define SPECTRUM_APP_H_


#include "ap_def.h"


app_info_t *spectrumGetAppInfo(void);

// 키를 누를 때마다 호출한다. 막대 위치는 매트릭스 col, 높이는 row 로 정한다.
// 타자를 막지 않도록 값만 남기고 그리기는 UI 스레드가 한다.
void        spectrumSetKey(uint8_t row, uint8_t col);

#endif
