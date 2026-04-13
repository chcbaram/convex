#include "lcd_port.h"
#include "color.h"
#include "eeconfig.h"





typedef union
{
  uint32_t raw;

  struct PACKED
  {
    uint8_t enable : 2;
    uint8_t mode   : 6;
    HSV     hsv;
  };
} color_config_t;

_Static_assert(sizeof(color_config_t) == sizeof(uint32_t), "EECONFIG out of spec.");

enum via_qmk_lcd_value {
    id_qmk_lcd_brightness   = 1,
    id_qmk_lcd_date_color   = 2,
    id_qmk_lcd_time_color   = 3,  
    id_qmk_lcd_calc_color   = 4,
};

static void via_qmk_get_value(uint8_t *data);
static void via_qmk_set_value(uint8_t *data);
static void via_qmk_save(uint8_t *data);
static HSV  rgb_to_hsv(RGB rgb);
static void lcd_set_pwm(uint8_t level);
static bool eventReceive(event_t *p_event);

static color_config_t color_config[COLOR_TYPE_MAX];
static bool           color_is_ready[COLOR_TYPE_MAX];
static bool           is_ui_ready = false;


EECONFIG_DEBOUNCE_HELPER(color_lcd,  EECONFIG_USER_COLOR_LCD, color_config[COLOR_TYPE_LCD]);
EECONFIG_DEBOUNCE_HELPER(color_time, EECONFIG_USER_COLOR_TIME, color_config[COLOR_TYPE_TIME]);
EECONFIG_DEBOUNCE_HELPER(color_date, EECONFIG_USER_COLOR_DATE, color_config[COLOR_TYPE_DATE]);
EECONFIG_DEBOUNCE_HELPER(color_calc, EECONFIG_USER_COLOR_CALC, color_config[COLOR_TYPE_CALC]);




void via_qmk_lcd_init(void)
{
  eventSubFunc("via_qmk_lcd", eventReceive);

  eeconfig_init_color_lcd();
  eeconfig_init_color_time();
  eeconfig_init_color_date();
  eeconfig_init_color_calc();

  if (color_config[COLOR_TYPE_LCD].mode != 1)
  {
    color_config[COLOR_TYPE_LCD].mode   = 1;
    color_config[COLOR_TYPE_LCD].enable = true;
    color_config[COLOR_TYPE_LCD].hsv.v  = 100;
    eeconfig_flush_color_lcd(true);
  }   

  if (color_config[COLOR_TYPE_TIME].mode != 1)
  {
    RGB rgb;

    rgb.r = 0xFF;
    rgb.g = 0x00;
    rgb.b = 0xB9;
    color_config[COLOR_TYPE_TIME].mode   = 1;
    color_config[COLOR_TYPE_TIME].enable = true;
    color_config[COLOR_TYPE_TIME].hsv    = rgb_to_hsv(rgb);
    eeconfig_flush_color_time(true);
  }    

  if (color_config[COLOR_TYPE_DATE].mode != 1)
  {
    RGB rgb;

    rgb.r = 0xFF;
    rgb.g = 0x00;
    rgb.b = 0xB9;
    color_config[COLOR_TYPE_DATE].mode   = 1;
    color_config[COLOR_TYPE_DATE].enable = true;
    color_config[COLOR_TYPE_DATE].hsv    = rgb_to_hsv(rgb);
    eeconfig_flush_color_date(true);
  }  
  
  if (color_config[COLOR_TYPE_CALC].mode != 1)
  {
    RGB rgb;

    rgb.r = 0xFF;
    rgb.g = 0x00;
    rgb.b = 0xB9;
    color_config[COLOR_TYPE_CALC].mode   = 1;
    color_config[COLOR_TYPE_CALC].enable = true;
    color_config[COLOR_TYPE_CALC].hsv    = rgb_to_hsv(rgb);
    eeconfig_flush_color_calc(true);
  }   


  color_is_ready[COLOR_TYPE_CALC] = true;
}

bool eventReceive(event_t *p_event)
{
  if (p_event->code == EVENT_UI_READY)
  {
    is_ui_ready = true;
    lcd_set_pwm(color_config[COLOR_TYPE_LCD].hsv.v); 
  }
  return true;
}

bool via_qmk_lcd_is_update(uint8_t type)
{
  bool ret;

  ret = color_is_ready[type];
  color_is_ready[type] = false;

  return ret;
}

HSV  via_qmk_lcd_get_color(uint8_t type)
{
  return color_config[type].hsv;
}

void via_qmk_lcd(uint8_t *data, uint8_t length)
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
        via_qmk_save(value_id_and_data);
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

  switch (*value_id)
  {
    case id_qmk_lcd_brightness:
      {
        value_data[0] = color_config[COLOR_TYPE_LCD].hsv.v;        
        break;
      }    
    case id_qmk_lcd_time_color:
      {
        value_data[0] = color_config[COLOR_TYPE_TIME].hsv.h;
        value_data[1] = color_config[COLOR_TYPE_TIME].hsv.s;
        break;
      }
    case id_qmk_lcd_date_color:
      {
        value_data[0] = color_config[COLOR_TYPE_DATE].hsv.h;
        value_data[1] = color_config[COLOR_TYPE_DATE].hsv.s;
        break;
      }
    case id_qmk_lcd_calc_color:
      {
        value_data[0] = color_config[COLOR_TYPE_CALC].hsv.h;
        value_data[1] = color_config[COLOR_TYPE_CALC].hsv.s;
        break;
      }      
  }
}

void via_qmk_set_value(uint8_t *data)
{
  // data = [ value_id, value_data ]
  uint8_t *value_id   = &(data[0]);
  uint8_t *value_data = &(data[1]);

  switch (*value_id)
  {
    case id_qmk_lcd_brightness:
      {
        color_config[COLOR_TYPE_LCD].hsv.v = value_data[0];
        color_is_ready[COLOR_TYPE_LCD] = true;        
        lcd_set_pwm(color_config[COLOR_TYPE_LCD].hsv.v);        
        break;
      }
    case id_qmk_lcd_time_color:
      {
        color_config[COLOR_TYPE_TIME].hsv.h = value_data[0];
        color_config[COLOR_TYPE_TIME].hsv.s = value_data[1];
        color_is_ready[COLOR_TYPE_TIME] = true;
        break;
      }
    case id_qmk_lcd_date_color:
      {
        color_config[COLOR_TYPE_DATE].hsv.h = value_data[0];
        color_config[COLOR_TYPE_DATE].hsv.s = value_data[1];
        color_is_ready[COLOR_TYPE_DATE] = true;
        break;
      }
    case id_qmk_lcd_calc_color:
      {
        color_config[COLOR_TYPE_CALC].hsv.h = value_data[0];
        color_config[COLOR_TYPE_CALC].hsv.s = value_data[1];
        color_is_ready[COLOR_TYPE_CALC] = true;
        break;
      }      
  }
}

void via_qmk_save(uint8_t *data)
{
  eeconfig_flush_color_lcd(true);
  eeconfig_flush_color_time(true);
  eeconfig_flush_color_date(true);
  eeconfig_flush_color_calc(true);
}

HSV rgb_to_hsv(RGB rgb)
{
  HSV     hsv;
  uint8_t rgb_min, rgb_max, delta;

  rgb_min = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
  rgb_max = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

  hsv.v = rgb_max; // Value
  delta = rgb_max - rgb_min;

  if (rgb_max == 0 || delta == 0)
  {
    hsv.h = 0;
    hsv.s = 0;
    return hsv;
  }

  // Saturation
  hsv.s = 255 * (uint16_t)delta / rgb_max;

  // Hue
  if (rgb_max == rgb.r)
    hsv.h = 0 + 43 * (rgb.g - rgb.b) / delta;
  else if (rgb_max == rgb.g)
    hsv.h = 85 + 43 * (rgb.b - rgb.r) / delta;
  else
    hsv.h = 171 + 43 * (rgb.r - rgb.g) / delta;

  return hsv;
}

void lcd_set_pwm(uint8_t level)
{
  lcdSetBackLight(constrain(level, 8, 100));  
}