#include "quantum.h"



enum via_qmk_rtc_value {
    id_qmk_rtc_date_yy      = 1,
    id_qmk_rtc_date_mm      = 2,
    id_qmk_rtc_date_dd      = 3,
    id_qmk_rtc_time_hh      = 4,
    id_qmk_rtc_time_mm      = 5,
    id_qmk_rtc_time_ss      = 6,
};


static void via_qmk_get_value(uint8_t *data);
static void via_qmk_set_value(uint8_t *data);



void via_qmk_rtc(uint8_t *data, uint8_t length)
{
  // data = [ command_id, channel_id, value_id, value_data ]
  uint8_t *command_id        = &(data[0]);
  uint8_t *value_id_and_data = &(data[2]);

  switch (*command_id)
  {
    case id_custom_set_value:
      {
        via_qmk_set_value(value_id_and_data);
        break;
      }
    case id_custom_get_value:
      {
        via_qmk_get_value(value_id_and_data);
        break;
      }
    case id_custom_save:
      {
        break;
      }
    default:
      {
        *command_id = id_unhandled;
        break;
      }
  }
}

void via_qmk_get_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);
  rtc_info_t rtc_info;


  rtcGetInfo(&rtc_info);


  switch (*value_id)
  {
    case id_qmk_rtc_date_yy:
      {
        value_data[0] = rtc_info.date.year - 24;
        break;
      }    
    case id_qmk_rtc_date_mm:
      {
        value_data[0] = rtc_info.date.month - 1;
        break;
      }         
    case id_qmk_rtc_date_dd:
      {
        value_data[0] = rtc_info.date.day - 1;
        break;
      }
    case id_qmk_rtc_time_hh:
      {
        value_data[0] = rtc_info.time.hours;
        break;
      }      
    case id_qmk_rtc_time_mm:
      {
        value_data[0] = rtc_info.time.minutes;
        break;
      }      
    case id_qmk_rtc_time_ss:
      {
        value_data[0] = rtc_info.time.seconds;
        break;
      }      
  }
}

void via_qmk_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);
  rtc_info_t rtc_info;


  rtcGetInfo(&rtc_info);


  switch (*value_id)
  {
    case id_qmk_rtc_date_yy:
      {
        rtc_info.date.year = value_data[0] + 24;
        break;
      }    
    case id_qmk_rtc_date_mm:
      {
        rtc_info.date.month = value_data[0] + 1;
        break;
      }         
    case id_qmk_rtc_date_dd:
      {
        rtc_info.date.day = value_data[0] + 1;
        break;
      }
    case id_qmk_rtc_time_hh:
      {
        rtc_info.time.hours = value_data[0];
        break;
      }      
    case id_qmk_rtc_time_mm:
      {
        rtc_info.time.minutes = value_data[0];
        break;
      }      
    case id_qmk_rtc_time_ss:
      {
        rtc_info.time.seconds = value_data[0];
        break;
      }      
  }

  rtcSetDate(&rtc_info.date);
  rtcSetTime(&rtc_info.time);
}
