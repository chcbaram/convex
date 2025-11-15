#include "nvs.h"
#include "fs.h"

#ifdef _USE_HW_NVS


static bool is_init = false;
static fs_t nvs_fs;




bool nvsInit(void)
{
  bool ret = false;

  if (fsIsInit() == true)
  {
    is_init = true;
  }

  ret = is_init;

  logPrintf("[%s] nvsInit()\n", ret ? "OK" : "E_");

  return ret;
}

bool nvsIsInit(void)
{
  return is_init;
}

bool nvsIsExist(const char *p_name)
{
  bool ret = false;

  if (is_init != true) return false;

  if (fsIsExist(p_name) == true)
  {
    ret = true;
  }

  return ret;
}

bool nvsSet(const char *p_name, void *p_data, uint32_t length)
{
  bool ret = false;
  int32_t file_len;

  do
  {
    if (fsFileOpen(&nvs_fs, p_name) != true)
      break;

    file_len = fsFileWrite(&nvs_fs, p_data, length);

    if (fsFileClose(&nvs_fs) != true)
      break;

    if (file_len == length)
      ret = true;
  } while (0);
  
  return ret;
}

bool nvsGet(const char *p_name, void *p_data, uint32_t length)
{
  bool ret = false;
  int32_t file_len;

  do
  {
    if (fsIsExist(p_name) != true)
      break;

    if (fsFileOpen(&nvs_fs, p_name) != true)
      break;

    file_len = fsFileRead(&nvs_fs, p_data, length);

    if (fsFileClose(&nvs_fs) != true)
      break;

    if (file_len == length)
      ret = true;
  } while (0);
  
  return ret;
}

bool nvsLoad(nvs_cfg_t *p_cfg)
{
  bool ret = true;
  uint32_t cfg_len = 0;

  assert(p_cfg->p_name != NULL);
  assert(p_cfg->p_data != NULL);
  assert(p_cfg->length > 0);

  ret = nvsIsExist(p_cfg->p_name);
  if (!ret)
  {
    return false;    
  }

  ret = nvsLen(p_cfg->p_name, &cfg_len);
  if (ret)
  {
    ret = nvsGet(p_cfg->p_name, p_cfg->p_data, cfg_len);
  }
  
  return ret;
}

bool nvsSave(nvs_cfg_t *p_cfg)
{
  bool ret;
  
  assert(p_cfg->p_name != NULL);
  assert(p_cfg->p_data != NULL);
  assert(p_cfg->length > 0);

  ret = nvsSet(p_cfg->p_name, p_cfg->p_data, p_cfg->length);

  return ret;
}

bool nvsLen(const char *p_name, uint32_t *p_length)
{
  bool ret = false;
  int32_t file_len = 0;

  do
  {
    if (fsIsExist(p_name) != true)
      break;

    if (fsFileOpen(&nvs_fs, p_name) != true)
      break;

    file_len = fsFileSize(&nvs_fs);

    if (fsFileClose(&nvs_fs) != true)
      break;

    ret = true;
  } while (0);
  
  if (file_len < 0)
  {
    file_len = 0;
  }
  *p_length = file_len;

  return ret;
}
#endif