#include "qmk.h"
#include "qmk/port/port.h"

#ifdef _USE_HW_RTOS
#define lock()      xSemaphoreTake(mutex_lock, portMAX_DELAY);
#define unLock()    xSemaphoreGive(mutex_lock);

static SemaphoreHandle_t mutex_lock;
#else
#define lock()      
#define unLock()    
#endif

static bool qmkInit(void);
static void qmkUpdate(void);
static void qmkThread(void const *arg);
static void cliQmk(cli_args_t *args);
static void idle_task(void);

static bool is_suspended = false;

MODULE_DEF(qmk) 
{
  .name = "qmk",
  .priority = MODULE_PRI_LOW,
  .init = qmkInit,
};





bool qmkInit(void)
{
  bool ret;

  #ifdef _USE_HW_RTOS
  mutex_lock = xSemaphoreCreateMutex();
  #endif

  eeprom_init();
  via_hid_init();

  keyboard_setup();
  keyboard_init();

  
  is_suspended = usbIsSuspended();

  logPrintf("[  ] qmkInit()\n");
  logPrintf("     MATRIX_ROWS : %d\n", MATRIX_ROWS);
  logPrintf("     MATRIX_COLS : %d\n", MATRIX_COLS);
  logPrintf("     DEBOUNCE    : %d\n", DEBOUNCE);

  ret = threadCreate("qmk", qmkThread, NULL, _HW_DEF_THREAD_MODULE_PRI, _HW_DEF_THREAD_MODULE_STACK);
  assert(ret);

  logPrintf("[%s] qmkThreadInit()\n", ret ? "OK":"E_");

  cliAdd("qmk", cliQmk);
  return ret;
}

void qmkLock(void)
{
  lock();
}

void qmkUnLock(void)
{
  unLock();
}

void qmkUpdate(void)
{
  keyboard_task();
  eeprom_task();
  idle_task();
}

void keyboard_post_init_user(void)
{
#ifdef KILL_SWITCH_ENABLE
  kill_switch_init();
#endif
#ifdef KKUK_ENABLE
  kkuk_init();
#endif
}

__attribute__((weak)) 
bool qmk_process_keys(uint16_t keycode, keyrecord_t *record)
{
  return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record)
{
#ifdef KILL_SWITCH_ENABLE
  kill_switch_process(keycode, record);
#endif
#ifdef KKUK_ENABLE
  kkuk_process(keycode, record);
#endif
  bool ret;

  ret = qmk_process_keys(keycode, record);

  return ret;
}

void idle_task(void)
{
  bool is_suspended_cur;

  is_suspended_cur = usbIsSuspended();
  if (is_suspended_cur != is_suspended)
  {
    if (is_suspended_cur)
    {
      suspend_power_down();
      eventPub(EVENT_QMK_SUSPEND, 1);
    }
    else
    {
      suspend_wakeup_init();
      eventPub(EVENT_QMK_RESUME, 1);
    }

    is_suspended = is_suspended_cur;
  }

#ifdef KKUK_ENABLE
  kkuk_idle();
#endif
}

void qmkThread(void const *arg)
{
  while(1)
  {
    qmkUpdate();
    delay(1);
  }
}

void cliQmk(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 2 && args->isStr(0, "clear") && args->isStr(1, "eeprom"))
  {
    eeconfig_init();
    cliPrintf("Clearing EEPROM\n");
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("qmk info\n");
    cliPrintf("qmk clear eeprom\n");
  }
}