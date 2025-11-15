#include "ap_def.h"


static void eventThread(void const *arg);




static bool init(void)
{
  bool ret;

  ret = threadCreate("event", eventThread, NULL, _HW_DEF_THREAD_MODULE_PRI, _HW_DEF_THREAD_MODULE_STACK);
  assert(ret);

  logPrintf("[%s] eventThreadInit()\n", ret ? "OK":"E_");
  return ret;
}

void eventThread(void const *arg)
{
  while(1)
  {
    eventUpdate();
    delay(1);
  }
}

MODULE_DEF(event) 
{
  .name = "event",
  .priority = MODULE_PRI_LOW,
  .init = init
};