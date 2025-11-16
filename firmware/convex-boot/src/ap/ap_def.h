#ifndef AP_DEF_H_
#define AP_DEF_H_


#include "hw.h"


//--------------------------------------------------------------------+
// USB UF2
//--------------------------------------------------------------------+





enum {
  STATE_BOOTLOADER_STARTED = 0,///< STATE_BOOTLOADER_STARTED
  STATE_USB_PLUGGED,           ///< STATE_USB_PLUGGED
  STATE_USB_UNPLUGGED,         ///< STATE_USB_UNPLUGGED
  STATE_WRITING_STARTED,       ///< STATE_WRITING_STARTED
  STATE_WRITING_FINISHED,      ///< STATE_WRITING_FINISHED
};


typedef enum 
{
  LCD_INFO_MODE_PROGRESS,
  LCD_INFO_MODE_INFO,
  LCD_INFO_MODE_ERROR,
} LcdInfoMode_t;

typedef struct
{
  LcdInfoMode_t mode;
  const char   *title_str;
  const char   *info_str;
  uint8_t       percent;
  uint16_t      err_code;
} lcd_req_info_t;


void lcdUpdate(bool req, lcd_req_info_t *p_req_info);

#endif