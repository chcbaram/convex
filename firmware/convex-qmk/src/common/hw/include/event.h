#ifndef EVENT_H_
#define EVENT_H_


#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef _USE_HW_EVENT


typedef enum
{
  EVENT_TYPE_DATA   = 0x01,
  EVENT_TYPE_INFO   = 0x02,
  EVENT_TYPE_TO_BUS = 0x10,
  EVENT_TYPE_FR_BUS = 0x20,
} EventType_t;

typedef struct 
{
  EventCode_t code;
  uint8_t     type;
  uint32_t    data;
  const char *p_name;
} event_t;

typedef bool (*event_func_t)(event_t *);


#define eventPub(event_code, event_data)        eventPubFunc(#event_code, EVENT_TYPE_DATA, event_code, event_data)
#define eventPubInfo(event_code, event_data)    eventPubFunc(#event_code, EVENT_TYPE_INFO, event_code, event_data)
#define eventPubBus(event_code, event_data)     eventPubFunc(#event_code, EVENT_TYPE_DATA | EVENT_TYPE_TO_BUS, event_code, event_data)
#define eventPubBusInfo(event_code, event_data) eventPubFunc(#event_code, EVENT_TYPE_INFO | EVENT_TYPE_TO_BUS, event_code, event_data)
#define eventSub(sub_func)                      eventSubFunc(#sub_func, sub_func)


bool eventInit(void);
bool eventIsInit(void);
bool eventUpdate(void);
bool eventGet(event_t *p_event);
bool eventAvailble(void);
bool eventSubFunc(const char *p_name, event_func_t sub_func);
bool eventPubFunc(const char *p_name, EventType_t type, EventCode_t event_code, uint32_t event_data);
bool eventFunc(event_t *evt);
bool eventRun(event_t *p_event);

#endif

#ifdef __cplusplus
}
#endif

#endif