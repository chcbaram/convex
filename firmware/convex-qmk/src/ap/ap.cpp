#include "ap.h"
#include "launcher/launcher.h"




void apInit(void)
{   
  ledOn(_DEF_LED1);

  moduleInit();
  threadBegin();
  
  logBoot(false);
}

void apMain(void)
{
  uint32_t pre_time;
  bool is_led_on = true;
  
  lcdLogoOn();
  lcdDisplayOnDimming(1000);
  delay(1000);
  
  lcdClear(black);
  delay(100);

  eventPub(EVENT_UI_READY, 1);

  pre_time = millis();
  while(1)
  {
    if (is_led_on && millis()-pre_time >= 500)
    {
      is_led_on = false;
      ledOff(_DEF_LED1);
    }
    delay(1);
  }
}

