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


#endif