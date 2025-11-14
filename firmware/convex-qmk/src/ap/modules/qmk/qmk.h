#ifndef QMK_H_
#define QMK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ap_def.h"


#include "quantum.h"


void qmkLock(void);
void qmkUnLock(void);

#ifdef __cplusplus
}
#endif

#endif