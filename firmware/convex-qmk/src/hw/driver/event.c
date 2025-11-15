#include "event.h"



#ifdef _USE_HW_EVENT
#include "qbuffer.h"
#include "cli.h"


#ifdef _USE_HW_RTOS
#define lock()      xSemaphoreTake(mutex_lock, portMAX_DELAY);
#define unLock()    xSemaphoreGive(mutex_lock);
static SemaphoreHandle_t mutex_lock;
#else
#define lock()
#define unLock()    
#endif


#define EVENT_Q_MAX       HW_EVENT_Q_MAX
#define EVENT_NODE_MAX    HW_EVENT_NODE_MAX


typedef struct
{
  int32_t count;
  event_func_t event_func[EVENT_NODE_MAX];
  const char *func_name;
} event_node_t;


#ifdef _USE_HW_CLI
static void cliEvent(cli_args_t *args);
#endif

static bool is_init = false;
static uint8_t log_bits = EVENT_TYPE_DATA;
static qbuffer_t event_q;
static event_t event_buf[EVENT_Q_MAX];
static event_node_t event_node;




bool eventInit(void)
{
  event_node.count = 0;
  for (int i=0; i<EVENT_NODE_MAX; i++)
  {
    event_node.event_func[i] = NULL;
  }
  is_init = qbufferCreateBySize(&event_q, (uint8_t *)event_buf, sizeof(event_t), EVENT_Q_MAX);

  #ifdef _USE_HW_RTOS
  mutex_lock = xSemaphoreCreateMutex();
  #endif

#ifdef _USE_HW_CLI
  cliAdd("event", cliEvent);
#endif  
  return is_init;
}

bool eventIsInit(void)
{
  return is_init;
}

bool eventSubFunc(const char *p_name, event_func_t sub_func)
{
  bool ret;

  if (event_node.count < EVENT_NODE_MAX)
  {
    lock();
    event_node.event_func[event_node.count] = sub_func;
    event_node.func_name = p_name;
    event_node.count++;
    unLock();
    ret = true;
  }
  else
  {
    ret = false;
  }

  return ret;  
}

bool eventAvailble(void)
{
  if (is_init != true)
    return false;

  if (qbufferAvailable(&event_q) > 0)
    return true;
  else
    return false;
}

bool eventPubFunc(const char *p_name, EventType_t event_type, EventCode_t event_code, uint32_t event_data)
{
  bool ret;
  event_t event_msg;
  uint32_t isrm = 0;

  if (is_init != true)
    return false;

  event_msg.code   = event_code;
  event_msg.type   = (uint8_t)event_type;
  event_msg.data   = event_data;
  event_msg.p_name = p_name;

  if (xPortIsInsideInterrupt() == pdTRUE)
  {    
    isrm = taskENTER_CRITICAL_FROM_ISR();
  }
  else
  {
    lock();
  }
  ret = qbufferWrite(&event_q, (uint8_t *)&event_msg, 1);
  if (xPortIsInsideInterrupt() == pdTRUE)
  {    
    taskEXIT_CRITICAL_FROM_ISR(isrm);
  }
  else
  {
    unLock();
  }

  return ret;
}

bool eventGet(event_t *p_event)
{
  bool ret;

  if (is_init != true)
    return false;

  ret = qbufferRead(&event_q, (uint8_t *)p_event, 1);

  return ret;
}

bool eventUpdate(void)
{

  if (is_init != true)
    return false;


  while(eventAvailble())
  {
    event_t evt;

    if (eventGet(&evt) == true)
    {
      if (log_bits & evt.type)
      {
        logPrintf("[  ] Event %s:%d\n", evt.p_name, evt.data);
      }

      for (int i=0; i<event_node.count; i++)
      {
        if (event_node.event_func[i] != NULL)
        {
          event_node.event_func[i](&evt);
        }
      }
    }
  }

  return true;
}  

bool eventRun(event_t *p_event)
{
  for (int i=0; i<event_node.count; i++)
  {
    if (event_node.event_func[i] != NULL)
    {
      event_node.event_func[i](p_event);
    }
  }

  return true;
}  

#ifdef _USE_HW_CLI
void cliEvent(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("init : %s\n", is_init ? "True":"False");
    cliPrintf("log  : 0x%02X\n", log_bits);
    cliPrintf("node : %d\n", event_node.count);
    for (int i=0; i<event_node.count; i++)
    {
      cliPrintf("       %d - %s\n",i, event_node.func_name);
    }
    ret = true;
  }

  if (args->argc >= 1 && args->isStr(0, "log"))
  { 
    if (args->argc == 1)
    {
      cliPrintf("EVENT_TYPE_DATA = 0x01\n");
      cliPrintf("EVENT_TYPE_INFO = 0x02\n");
    }
    if (args->argc == 2)
    {
      log_bits = args->getData(1);
    }
    cliPrintf("log  : 0x%02X\n", log_bits);
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("event info\n");
    cliPrintf("event log [bits]\n");
  }
}
#endif

#endif