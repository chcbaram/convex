#include "keys.h"


#ifdef _USE_HW_KEYS
#include "button.h"
#include "cli.h"


#define MATRIX_ROWS   6
#define MATRIX_COLS   22


#if CLI_USE(HW_KEYS)
static void cliCmd(cli_args_t *args);
#endif
static bool keysInitGpio(void);


static uint16_t row_wr_buf[MATRIX_ROWS] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
static uint32_t col_rd_buf[MATRIX_ROWS] = {0,};




bool keysInit(void)
{
  keysInitGpio();

#if CLI_USE(HW_KEYS)
  cliAdd("keys", cliCmd);
#endif

  return true;
}

bool keysInitGpio(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  // ROWS
  //
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Pin   = GPIO_PIN_0 |
                        GPIO_PIN_1 |
                        GPIO_PIN_2 |
                        GPIO_PIN_3 |
                        GPIO_PIN_4 |
                        GPIO_PIN_5;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);


  // COLS
  //
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Pin   = GPIO_PIN_0 |
                        GPIO_PIN_1 |
                        GPIO_PIN_2 |
                        GPIO_PIN_3 |
                        GPIO_PIN_4 |
                        GPIO_PIN_5 |
                        GPIO_PIN_6 |
                        GPIO_PIN_7 |
                        GPIO_PIN_8 |
                        GPIO_PIN_9 |
                        GPIO_PIN_10 |
                        GPIO_PIN_11 |
                        GPIO_PIN_12 |
                        GPIO_PIN_13 |
                        GPIO_PIN_14 |
                        GPIO_PIN_15;
  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_6 |
                        GPIO_PIN_10 |
                        GPIO_PIN_13;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_8 |
                        GPIO_PIN_9;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  return true;
}

bool keysIsBusy(void)
{
  return false;
}

void keysDelay(void)
{
  volatile uint32_t cnt;

  for (cnt=0; cnt<1000; cnt++)
  {
    //
  }
}

bool keysUpdate(void)
{
  for (int i=0; i<MATRIX_ROWS; i++)
  {
    GPIOE->ODR = row_wr_buf[i];
    keysDelay();

    uint32_t data;

    data = GPIOD->IDR;
    data |= (GPIOC->IDR & (1 << 6)) ? (1 << 16) : 0;
    data |= (GPIOC->IDR & (1 << 10)) ? (1 << 17) : 0;
    data |= (GPIOC->IDR & (1 << 13)) ? (1 << 18) : 0;
    data |= (GPIOE->IDR & (1 << 6)) ? (1 << 19) : 0;
    data |= (GPIOB->IDR & (1 << 8)) ? (1 << 20) : 0;
    data |= (GPIOB->IDR & (1 << 9)) ? (1 << 21) : 0;

    col_rd_buf[i] = data;
  }

  return true;
}

bool keysReadBuf(uint8_t *p_data, uint32_t length)
{
  return true;
}

bool keysReadColsBuf(uint32_t *p_data, uint32_t rows_cnt)
{
  memcpy(p_data, col_rd_buf, rows_cnt * sizeof(uint32_t));
  return true;
}

bool keysGetPressed(uint16_t row, uint16_t col)
{
  bool     ret = false;
  uint32_t col_bit;  

  col_bit = col_rd_buf[row];

  if (col_bit & (1<<col))
  {
    ret = true;
  }

  return ret;
}

#if CLI_USE(HW_KEYS)
void cliCmd(cli_args_t *args)
{
  bool ret = false;



  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliShowCursor(false);


    while(cliKeepLoop())
    {
      keysUpdate();
      delay(10);

      cliPrintf("     ");
      for (int cols=0; cols<MATRIX_COLS; cols++)
      {
        cliPrintf("%02d ", cols);
      }
      cliPrintf("\n");

      for (int rows=0; rows<MATRIX_ROWS; rows++)
      {
        cliPrintf("%02d : ", rows);

        for (int cols=0; cols<MATRIX_COLS; cols++)
        {
          if (keysGetPressed(rows, cols))
            cliPrintf("O  ");
          else
            cliPrintf("_  ");
        }
        cliPrintf("\n");
      }
      cliMoveUp(MATRIX_ROWS+1);
    }
    cliMoveDown(MATRIX_ROWS+1);

    cliShowCursor(true);
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("keys info\n");
  }
}
#endif

#endif