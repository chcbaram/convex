#ifndef HW_DEF_H_
#define HW_DEF_H_



#include "bsp.h"
#include <time.h>

#define _DEF_FIRMWATRE_VERSION    "V251005R1"
#define _DEF_BOARD_NAME           "CONVEX-BOOT"


#define _USE_HW_MICROS
#define _USE_HW_FLASH
#define _USE_HW_YMODEM
#define _USE_HW_LOADER
#define _USE_HW_QSPI


#define _USE_HW_LED
#define      HW_LED_MAX_CH          1

#define _USE_HW_UART
#define      HW_UART_MAX_CH         1
#define      HW_UART_CH_SWD         _DEF_UART1
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

// #define _USE_HW_KEYS
// #define      HW_KEYS_MAX_CH         HW_BUTTON_MAX_CH
// #define      HW_KEYS_PRESS_MAX      8

#define _USE_HW_SPI
#define      HW_SPI_MAX_CH          1

#define _USE_HW_I2C
#define      HW_I2C_MAX_CH          1

#define _USE_HW_EEPROM
#define         EEPROM_CHIP_BL24C256

#define _USE_HW_RTC
#define      HW_RTC_BOOT_MODE       RTC_BKP_DR3
#define      HW_RTC_RESET_BITS      RTC_BKP_DR4

#define _USE_HW_RESET
#define      HW_RESET_BOOT          1

#define _USE_HW_GPIO
#define      HW_GPIO_MAX_CH         GPIO_PIN_MAX

#define _USE_HW_LCD
#define      HW_LCD_LVGL            1
#define      HW_LCD_LOGO            0
#define _USE_HW_ST7789
#define      HW_LCD_WIDTH           284
#define      HW_LCD_HEIGHT          76


#define FLASH_SIZE_TAG              0x400
#define FLASH_SIZE_VEC              0x400
#define FLASH_SIZE_VER              0x400
#define FLASH_SIZE_FIRM             (512*1024)

#define FLASH_ADDR_BOOT             0x08000000
#define FLASH_ADDR_FIRM             0x08020000
#define FLASH_ADDR_UPDATE           0x08100000


#define KBD_NAME                    "CONVEX"
#define MATRIX_ROWS                 5
#define MATRIX_COLS                 16



//-- CLI
//
#define _USE_CLI_HW_BUTTON          1
#define _USE_CLI_HW_I2C             1
#define _USE_CLI_HW_FLASH           1
#define _USE_CLI_HW_RESET           1
#define _USE_CLI_HW_KEYS            1
#define _USE_CLI_HW_LOADER          1
#define _USE_CLI_HW_EEPROM          1
#define _USE_CLI_HW_QSPI            1
#define _USE_CLI_HW_GPIO            1


typedef enum
{
  LCD_CS,
  LCD_RESET,
  LCD_RS,
  LCD_BL,
  GPIO_PIN_MAX
} GpioPinName_t;

#endif
