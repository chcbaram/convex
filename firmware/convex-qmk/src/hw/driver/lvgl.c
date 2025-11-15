#include "lvgl.h"

#ifdef _USE_HW_LVGL
#include "lvgl/lv_port_disp.h"
#include "lvgl/lv_port_indev.h"
#include "cli.h"


static void cliCmd(cli_args_t *args);

static bool is_init = false;
static bool is_enable = true;






bool lvglInit(void)
{

  if (is_init == true)
    return true;

  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();

  lv_tick_set_cb(millis);

  is_init = true;

  cliAdd("lvgl", cliCmd);
  return true;
}

bool lvglDeInit(void)
{
  return true;
}

bool lvglUpdate(void)
{
  if (is_init == false)
    return false;

  if (is_enable == true)
  {
    lv_task_handler();
  }
  return true;
}

void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("is_init   : %d\n", is_init);
    cliPrintf("is_enable : %d\n", is_enable);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "enable"))
  {
    is_enable = true;
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "disable"))
  {
    is_enable = false;
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("lvgl info\n");
    cliPrintf("lvgl enable\n");
    cliPrintf("lvgl disable\n");
  }
}

#endif
