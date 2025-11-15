/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "lcd.h"
#include "button.h"
#include "keys.h"


static void keypad_init(void);
static void keypad_read(lv_indev_t * indev, lv_indev_data_t * data);
static uint32_t keypad_get_key(void);


lv_indev_t * indev_keypad;


void lv_port_indev_init(void)
{

  /*------------------
   * Keypad
   * -----------------*/

  /*Initialize your keypad or keyboard if you have*/
  keypad_init();

  /*Register a keypad input device*/
  indev_keypad = lv_indev_create();
  lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(indev_keypad, keypad_read);
}

/*------------------
 * Keypad
 * -----------------*/

static void keypad_init(void)
{
}

enum
{
  KEY_NONE,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_ENTER,
};


static void keypad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
  static uint32_t last_key = 0;


  uint32_t act_key = keypad_get_key();
  if (act_key > 0)
  {
    data->state = LV_INDEV_STATE_PRESSED;

    switch (act_key)
    {
    case KEY_LEFT:
      act_key = LV_KEY_PREV;
      break;
    case KEY_RIGHT:
      act_key = LV_KEY_NEXT;
      break;
    case KEY_ENTER:
      act_key = LV_KEY_ENTER;
      break;
    }
    last_key = act_key;
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }

  data->key = last_key;
}

/*Get the currently being pressed key.  0 if no key is pressed*/
static uint32_t keypad_get_key(void)
{
  uint32_t ret = KEY_NONE;
  
  // if (adcRead(_DEF_CH1) >= 3000)
  // {
  //   ret = KEY_RIGHT;
  // }
  // if (adcRead(_DEF_CH1) <= 1000)
  // {
  //   ret = KEY_LEFT;
  // }
  // if (buttonGetPressed(2))
  // {
  //   ret = KEY_ENTER;
  // }
  if (keysGetPressed(5, 15))
  {
    ret = KEY_LEFT;
  }
  if (keysGetPressed(5, 17))
  {
    ret = KEY_RIGHT;
  }
  if (keysGetPressed(5, 21))
  {
    ret = KEY_ENTER;
  }

  return ret;
}
