#include "ws2812.h"



#ifdef _USE_HW_WS2812
#include "cli.h"

#define BIT_PERIOD (130) // 1300ns, 80Mhz
#define BIT_HIGH   (70)  // 700ns
#define BIT_LOW    (35)  // 350ns
#define BIT_ZERO   (50)
#define BIT_TYPE   uint16_t

#define BRIGHT_MAX (130)



bool is_init = false;


typedef struct
{
  TIM_HandleTypeDef *h_timer;  
  uint32_t channel;
  uint16_t led_cnt;
} ws2812_t;

__attribute__((section(".non_cache")))
static BIT_TYPE bit_buf[BIT_ZERO + 24*(HW_WS2812_MAX_CH+1)];


ws2812_t ws2812;
static TIM_HandleTypeDef htim3;
static DMA_HandleTypeDef hdma_tim3_ch1;


#if CLI_USE(HW_WS2812)
static void cliCmd(cli_args_t *args);
#endif
static bool ws2812InitHw(void);





bool ws2812Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};


  memset(bit_buf, 0, sizeof(bit_buf));
  
  ws2812.h_timer = &htim3;
  ws2812.channel = TIM_CHANNEL_1;

  // Timer 
  //
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();

  htim3.Instance               = TIM3;
  htim3.Init.Prescaler         = 0; // 100MHz / (1) = 100Mhz -> 10ns
  htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim3.Init.Period            = BIT_PERIOD-1;
  htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.RepetitionCounter = 0;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode       = TIM_OCMODE_PWM1;
  sConfigOC.Pulse        = 0;
  sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode  = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel        = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime         = 0;
  sBreakDeadTimeConfig.BreakState       = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter      = 0;
  sBreakDeadTimeConfig.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim3, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }

  ws2812InitHw();


  ws2812.led_cnt = WS2812_MAX_CH;
  is_init = true;

  for (int i=0; i<WS2812_MAX_CH; i++)
  {
    ws2812SetColor(i, WS2812_COLOR_OFF);
  }
  ws2812Refresh();

#if CLI_USE(HW_WS2812)
  cliAdd("ws2812", cliCmd);
#endif
  return true;
}

bool ws2812InitHw(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};


  __HAL_RCC_GPIOA_CLK_ENABLE();
  /**TIM3 GPIO Configuration
  PA6     ------> TIM3_CH1
  */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* TIM3 DMA Init */
  /* TIM3_CH1 Init */
  hdma_tim3_ch1.Instance                 = DMA1_Stream2;
  hdma_tim3_ch1.Init.Request             = DMA_REQUEST_TIM3_CH1;
  hdma_tim3_ch1.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdma_tim3_ch1.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_tim3_ch1.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_tim3_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_tim3_ch1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
  hdma_tim3_ch1.Init.Mode                = DMA_NORMAL;
  hdma_tim3_ch1.Init.Priority            = DMA_PRIORITY_LOW;
  hdma_tim3_ch1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma_tim3_ch1) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_LINKDMA(&htim3,hdma[TIM_DMA_ID_CC1],hdma_tim3_ch1);

  // HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);
  // HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);

  return true;
}

bool ws2812Refresh(void)
{
  HAL_DMA_IRQHandler(&hdma_tim3_ch1);
  HAL_TIM_PWM_Stop_DMA(ws2812.h_timer, ws2812.channel);
  HAL_TIM_PWM_Start_DMA(ws2812.h_timer, ws2812.channel,  (const uint32_t *)bit_buf, sizeof(bit_buf)/sizeof(BIT_TYPE));
  return true;
}

void ws2812SetColor(uint32_t ch, uint32_t color)
{
  BIT_TYPE r_bit[8];
  BIT_TYPE g_bit[8];
  BIT_TYPE b_bit[8];
  uint8_t  red;
  uint8_t  green;
  uint8_t  blue;
  uint32_t offset;
  uint32_t color_sum;
  uint32_t color_max = BRIGHT_MAX;

  if (ch >= WS2812_MAX_CH)
    return;
  
  red   = (color >> 16) & 0xFF;
  green = (color >> 8) & 0xFF;
  blue  = (color >> 0) & 0xFF;

  color_sum = red + green + blue;
  if (color_sum > BRIGHT_MAX)
  {
    color_max = BRIGHT_MAX * BRIGHT_MAX / color_sum;
  }
  red   = (red * color_max) / 255;
  green = (green * color_max) / 255;
  blue  = (blue * color_max) / 255;

  for (int i=0; i<8; i++)
  {
    if (red & (1<<7))
    {
      r_bit[i] = BIT_HIGH;
    }
    else
    {
      r_bit[i] = BIT_LOW;
    }
    red <<= 1;

    if (green & (1<<7))
    {
      g_bit[i] = BIT_HIGH;
    }
    else
    {
      g_bit[i] = BIT_LOW;
    }
    green <<= 1;

    if (blue & (1<<7))
    {
      b_bit[i] = BIT_HIGH;
    }
    else
    {
      b_bit[i] = BIT_LOW;
    }
    blue <<= 1;
  }

  offset = BIT_ZERO;

  memcpy(&bit_buf[offset + ch*24 + 8*0], g_bit, 8*sizeof(BIT_TYPE));
  memcpy(&bit_buf[offset + ch*24 + 8*1], r_bit, 8*sizeof(BIT_TYPE));
  memcpy(&bit_buf[offset + ch*24 + 8*2], b_bit, 8*sizeof(BIT_TYPE));
}

void DMA1_Stream2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_tim3_ch1);
}

#if CLI_USE(HW_WS2812)
void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("ws2812 led cnt : %d\n", WS2812_MAX_CH);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "test"))
  {
    uint32_t color[6] = {WS2812_COLOR_RED,
                         WS2812_COLOR_OFF,
                         WS2812_COLOR_GREEN,
                         WS2812_COLOR_OFF,
                         WS2812_COLOR_BLUE,
                         WS2812_COLOR_OFF};

    uint8_t color_idx = 0;
    uint32_t pre_time;


    pre_time = millis();
    while(cliKeepLoop())
    {
      if (millis()-pre_time >= 500)
      {
        pre_time = millis();
        
        for (int i=0; i<WS2812_MAX_CH; i++)
        {      
          ws2812SetColor(i, color[color_idx]);
        }
        ws2812Refresh();
        color_idx = (color_idx + 1) % 6;
      }
      
      cliLoopIdle();
    }

    for (int i=0; i<WS2812_MAX_CH; i++)
    {
      ws2812SetColor(i, WS2812_COLOR_OFF);
    }
    ws2812Refresh();

    ret = true;
  }

  if (args->argc >= 2 && args->isStr(0, "test"))
  {
    uint32_t color = WS2812_COLOR_OFF;

    if (args->isStr(1, "w"))
      color = WS2812_COLOR_WHITE;
    if (args->isStr(1, "r"))
      color = WS2812_COLOR_RED;
    if (args->isStr(1, "g"))
      color = WS2812_COLOR_GREEN;
    if (args->isStr(1, "b"))
      color = WS2812_COLOR_BLUE;

    for (int i=0; i<WS2812_MAX_CH; i++)
    {            
      ws2812SetColor(i, color);
    }
    ws2812Refresh();

    while(cliKeepLoop())
    {
      cliLoopIdle();
    }

    for (int i=0; i<WS2812_MAX_CH; i++)
    {
      ws2812SetColor(i, WS2812_COLOR_OFF);
    }
    ws2812Refresh();

    ret = true;
  }


  if (args->argc == 5 && args->isStr(0, "color"))
  {
    uint8_t  ch;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    ch    = (uint8_t)args->getData(1);
    red   = (uint8_t)args->getData(2);
    green = (uint8_t)args->getData(3);
    blue  = (uint8_t)args->getData(4);

    ws2812SetColor(ch, WS2812_COLOR(red, green, blue));
    ws2812Refresh();

    while(cliKeepLoop())
    {
    }
    ws2812SetColor(0, 0);
    ws2812Refresh();
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("ws2812 info\n");
    cliPrintf("ws2812 test\n");
    cliPrintf("ws2812 test w:r:g:b:o\n");
    cliPrintf("ws2812 color ch r g b\n");
  }
}
#endif

#endif