#pragma once

#include "quantum.h"


enum
{
  COLOR_TYPE_LCD,
  COLOR_TYPE_TIME,
  COLOR_TYPE_DATE,
  COLOR_TYPE_CALC,
  COLOR_TYPE_MAX
};

void via_qmk_lcd_init(void);
bool via_qmk_lcd_is_update(uint8_t type);
HSV  via_qmk_lcd_get_color(uint8_t type);
void via_qmk_lcd(uint8_t *data, uint8_t length);

