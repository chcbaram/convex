#include "ap_def.h"


static void      cliThread(void const *arg);
static uint8_t   cli_ch    = HW_UART_CH_SWD;
static uint32_t  cli_baud  = 115200;



static bool init(void)
{
  bool ret;

  cliOpen(cli_ch, cli_baud);  

  ret = threadCreate("cli", cliThread, NULL, _HW_DEF_THREAD_CLI_PRI, _HW_DEF_THREAD_CLI_STACK);
  assert(ret);

  logPrintf("[%s] cliThreadInit()\n", ret ? "OK":"E_");
  return ret;
}

void cliThread(void const *arg)
{
  uint8_t cli_ch = HW_UART_CH_CLI; 


  while(1)
  {
    if (usbIsOpen() && usbGetType() == USB_CON_CLI)
    {
      cli_ch = HW_UART_CH_USB;
    }
    else
    {
      cli_ch = HW_UART_CH_CLI;
    }
    if (cli_ch != cliGetPort())
    {
      if (cli_ch == HW_UART_CH_USB)
        logPrintf("\nCLI To USB\n");
      else
        logPrintf("\nCLI To UART\n");
      cliOpen(cli_ch, 0);
    }
    cliMain();
    delay(5);
  }
}

MODULE_DEF(cli) 
{
  .name = "cli",
  .priority = MODULE_PRI_LOW,
  .init = init
};