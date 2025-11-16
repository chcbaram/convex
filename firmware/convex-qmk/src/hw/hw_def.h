#ifndef HW_DEF_H_
#define HW_DEF_H_



#include "bsp.h"
#include QMK_KEYMAP_CONFIG_H


#define _DEF_FIRMWATRE_VERSION    "V251116R2"
#define _DEF_BOARD_NAME           "CONVEX-QMK-FW"


#define _USE_HW_MICROS
#define _USE_HW_FLASH
#define _USE_HW_YMODEM
#define _USE_HW_LOADER
#define _USE_HW_RTOS
#define _USE_HW_QSPI
#define _USE_HW_NVS
#define _USE_HW_LVGL
#define _USE_HW_CACHE


#define _USE_HW_LED
#define      HW_LED_MAX_CH          1

#define _USE_HW_UART
#define      HW_UART_MAX_CH         2
#define      HW_UART_CH_SWD         _DEF_UART1
#define      HW_UART_CH_USB         _DEF_UART2
#define      HW_UART_CH_CLI         HW_UART_CH_SWD

#define _USE_HW_CLI
#define      HW_CLI_CMD_LIST_MAX    32
#define      HW_CLI_CMD_NAME_MAX    16
#define      HW_CLI_LINE_HIS_MAX    8
#define      HW_CLI_LINE_BUF_MAX    64

#define _USE_HW_CLI_GUI
#define      HW_CLI_GUI_WIDTH       80
#define      HW_CLI_GUI_HEIGHT      24

#define _USE_HW_LOG
#define      HW_LOG_CH              HW_UART_CH_SWD
#define      HW_LOG_BOOT_BUF_MAX    2048
#define      HW_LOG_LIST_BUF_MAX    4096

#define _USE_HW_USB
#define _USE_HW_CDC
#ifdef  _USE_HW_VCOM
#define      HW_USB_LOG             0
#define      HW_USB_CMP             1
#define      HW_USB_CDC             1
#define      HW_USB_MSC             0
#define      HW_USB_HID             1
#else
#define      HW_USB_LOG             0
#define      HW_USB_CMP             0
#define      HW_USB_CDC             0
#define      HW_USB_MSC             0
#define      HW_USB_HID             1
#endif

#define _USE_HW_KEYS
#define      HW_KEYS_MAX_CH         HW_BUTTON_MAX_CH
#define      HW_KEYS_PRESS_MAX      6

#define _USE_HW_SPI
#define      HW_SPI_MAX_CH          1

#define _USE_HW_EEPROM
#define      HW_EEPROM_MAX_PAGES    32
#define      HW_EEPROM_MODE         0

#define _USE_HW_I2C
#define      HW_I2C_MAX_CH          1

#define _USE_HW_RTC
#define      HW_RTC_BOOT_MODE       RTC_BKP_DR3
#define      HW_RTC_RESET_BITS      RTC_BKP_DR4

#define _USE_HW_RESET
#define      HW_RESET_BOOT          1

#define _USE_HW_SWTIMER
#define      HW_SWTIMER_MAX_CH      8

#define _USE_HW_GPIO
#define      HW_GPIO_MAX_CH         GPIO_PIN_MAX

#define _USE_HW_EVENT
#define      HW_EVENT_Q_MAX         8
#define      HW_EVENT_NODE_MAX      64

#define _USE_HW_FS
#define      HW_FS_FLASH_OFFSET     0x90800000 
#define      HW_FS_MAX_SIZE         (8*1024*1024)

#define _USE_HW_PWM
#define      HW_PWM_MAX_CH          PWM_PIN_MAX

#define _USE_HW_LCD
#define      HW_LCD_LVGL            1
#define      HW_LCD_LOGO            1
#define _USE_HW_ST7789
#define      HW_LCD_WIDTH           284
#define      HW_LCD_HEIGHT          76


#define FLASH_SIZE_TAG              0x400
#define FLASH_SIZE_VEC              0x400
#define FLASH_SIZE_VER              0x400
#define FLASH_SIZE_FIRM             (2*1024*1024 - 256*1024)

#define FLASH_ADDR_BOOT             0x08000000
#define FLASH_ADDR_FIRM             0x08040000
#define FLASH_ADDR_UPDATE           0x90000000



//-- CLI
//
#define _USE_CLI_HW_BUTTON          1
#define _USE_CLI_HW_EEPROM          1
#define _USE_CLI_HW_I2C             1
#define _USE_CLI_HW_FLASH           1
#define _USE_CLI_HW_RESET           1
#define _USE_CLI_HW_KEYS            1
#define _USE_CLI_HW_LOADER          1
#define _USE_CLI_HW_WS2812          1
#define _USE_CLI_HW_GPIO            1
#define _USE_CLI_HW_QSPI            1


//-- RTOS
//
#define _USE_HW_THREAD
#define      HW_THREAD_MAX_CNT                  32

#define _HW_DEF_THREAD_MAIN_PRI                 osPriorityNormal
#define _HW_DEF_THREAD_MAIN_STACK               (8*1024)

#define _HW_DEF_THREAD_MODULE_PRI               osPriorityNormal
#define _HW_DEF_THREAD_MODULE_STACK             (4*1024)

#define _HW_DEF_THREAD_CLI_PRI                  osPriorityNormal
#define _HW_DEF_THREAD_CLI_STACK                (8*1024)



typedef enum
{
  LCD_CS,
  LCD_RESET,
  LCD_RS,
  LCD_BL,
  GPIO_PIN_MAX
} GpioPinName_t;

typedef enum
{
  LCD_BL_PWM,
  PWM_PIN_MAX
} PwmPinName_t;

#endif
