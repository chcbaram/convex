#ifndef NVS_H_
#define NVS_H_

#ifdef __cplusplus
extern "C" {
#endif


#include "hw_def.h"

#ifdef _USE_HW_NVS

typedef struct
{
  const char *p_name;
  void       *p_data;
  uint32_t    length;
} nvs_cfg_t;


bool nvsInit(void);
bool nvsIsInit(void);

bool nvsIsExist(const char *p_name);
bool nvsSet(const char *p_name, void *p_data, uint32_t length);
bool nvsGet(const char *p_name, void *p_data, uint32_t length);
bool nvsLen(const char *p_name, uint32_t *p_length);
bool nvsSave(nvs_cfg_t *p_cfg);
bool nvsLoad(nvs_cfg_t *p_cfg);

#endif


#ifdef __cplusplus
}
#endif

#endif 