#include "ap.h"
#include "uf2/uf2.h"
#include "boot/boot.h"




static void bootUp(void);
static void cliUpdate(void);



void apInit(void)
{  
  #ifdef _USE_HW_CLI
  cliOpen(HW_UART_CH_CLI, 115200);
  cliLogo();
  #endif

  bootUp();

  lcdInit();
  uf2Init();
  usbInit();

  bootInit();

  lcdClearBuffer(black);
  lcdPrintfRect(0, 0, LCD_WIDTH, LCD_HEIGHT, white, 32,
                LCD_ALIGN_H_CENTER | LCD_ALIGN_V_CENTER,
                "CONVEX BOOT");
  lcdUpdateDraw();

  lcdSetBackLight(100);
}

void apMain(void)
{
  uint32_t pre_time;  

  pre_time = millis();  
  while(1)
  {
    if (millis()-pre_time >= 100)
    {
      pre_time = millis();     
      ledToggle(_DEF_LED1);    
    }

    cliUpdate();
    usbUpdate();
    uf2Update();
    lcdUpdate(false, NULL);
  }
}

void cliLoopIdle(void)
{
  usbUpdate();
  uf2Update();
}

void lcdUpdate(bool req, lcd_req_info_t *p_req_info)
{
  static uint32_t pre_time = 0;


  if (req || (lcdDrawAvailable() && millis()-pre_time >= 100))
  {
    pre_time = millis();

    if (req)
    {
      lcdUpdateDraw();
    }
    


    rtc_time_t rtc_time;
    rtc_date_t rtc_date;
    const char *week_str[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    rtcGetTime(&rtc_time);
    rtcGetDate(&rtc_date);

    uf2_info_t info;

    uf2GetInfo(&info);
    
    if (info.state == 0)
    {
      lcdClearBuffer(black);  

      lcdDrawFillRect(0, 0, LCD_WIDTH, 20, green);
      lcdPrintfRect(0, 0, LCD_WIDTH, 20, black, 16, LCD_ALIGN_H_CENTER|LCD_ALIGN_V_CENTER, 
                    "%02d-%d-%02d %s  %02d:%02d:%02d", 
                    rtc_date.year, rtc_date.month, rtc_date.day, week_str[rtc_date.week],
                    rtc_time.hours, rtc_time.minutes, rtc_time.seconds
                  );
      lcdPrintfRect(0, 20, LCD_WIDTH, LCD_HEIGHT - 20, white, 32, LCD_ALIGN_H_CENTER|LCD_ALIGN_V_CENTER, 
                    "BARAM-BOOT");

      lcdRequestDraw();                    
    }
    else if (p_req_info != NULL)
    {
      lcdClearBuffer(black);  

      lcdDrawFillRect(0, 0, LCD_WIDTH, 20, green);
      lcdPrintfRect(0, 0, LCD_WIDTH, 20, black, 16, LCD_ALIGN_H_CENTER|LCD_ALIGN_V_CENTER, 
                    p_req_info->title_str);

      if (p_req_info->mode == LCD_INFO_MODE_PROGRESS)
      {
        lcdPrintf(0, 25, white, "%3d%%", p_req_info->percent);
        
        lcdDrawRect(0, 40, LCD_WIDTH, 24, white);
        lcdDrawFillRect(3, 42, p_req_info->percent * (LCD_WIDTH - 6) / 100, 20, white);      
      }
      else
      {
        lcdPrintfRect(0, 20, LCD_WIDTH, LCD_HEIGHT - 20, white, 16, LCD_ALIGN_H_CENTER|LCD_ALIGN_V_CENTER, 
                      p_req_info->info_str);    
      }

      lcdRequestDraw();
    }    
  }
}

void cliUpdate(void)
{
  #ifdef _USE_HW_CLI
  cliMain();
  #endif
}

void bootUp(void)
{
  static bool is_run_fw = true;
  static bool is_update_fw = false;

  uint32_t boot_param;
  uint16_t err_code;

  boot_param = resetGetBootMode();


  if (boot_param & (1<<MODE_BIT_BOOT))
  {
    boot_param &= ~(1<<MODE_BIT_BOOT);
    resetSetBootMode(boot_param);    
    is_run_fw = false;
  }

  #ifdef _USE_HW_KEYS
  uint32_t pre_time = millis();
  while(millis()-pre_time <= 5)
  {
    keysUpdate();
  }
  if (keysGetPressed(0, 0))
  {
    is_run_fw = false;
  }
  #endif
  
  if (boot_param & (1<<MODE_BIT_UPDATE))
  {
    boot_param &= ~(1<<MODE_BIT_UPDATE);
    resetSetBootMode(boot_param);
    
    is_run_fw = true;
    is_update_fw = true;
  }
  logPrintf("\n");

  if (is_update_fw)
  {
    logPrintf("[  ] bootUpdateFirm()\n");
    err_code = bootUpdateFirm();
    if (err_code == OK)
      logPrintf("[OK]\n");
    else
      logPrintf("[E_] err : 0x%04X\n", err_code);    
  }

  if (is_run_fw)
  {  
    err_code = bootJumpFirm();
    if (err_code != OK)
    {
      logPrintf("[  ] bootJumpFirm()\n");
      logPrintf("[E_] err : 0x%04X\n", err_code);
      if (bootVerifyUpdate() == OK)
      {
        delay(500);
        logPrintf("[  ] retry update\n");
        if (bootUpdateFirm() == OK)
        {
          err_code = bootJumpFirm();
          if (err_code != OK)
            logPrintf("[E_] err : 0x%04X\n", err_code);
        }
      }      
    }
  }

  logPrintf("\n");
  logPrintf("Boot Mode..\n"); 
}
