#include "pwm.h"



#ifdef _USE_HW_PWM
#include "cli.h"

#define NAME_DEF(x)  x, #x

#define PWM_TIM15_KHZ    1



typedef struct
{
  TIM_HandleTypeDef *p_htim;
  uint16_t max_value;

  PwmPinName_t        pin_name;
  const char         *p_name;
} pwm_tbl_t;


#ifdef _USE_HW_CLI
static void cliPwm(cli_args_t *args);
#endif

static bool is_init = false;

static TIM_HandleTypeDef htim15;
static uint16_t pwm_duty[PWM_MAX_CH] = {0, };

static const pwm_tbl_t pwm_tbl[PWM_MAX_CH] =
{
  {&htim15, 255, NAME_DEF(LCD_BL_PWM)},
};


static bool pwmInitTimer(void);





bool pwmInit(void)
{
  bool ret;
  
  for (int i=0; i<PWM_MAX_CH; i++)
  {
    assert(i == (int)pwm_tbl[i].pin_name);
  }

  ret = pwmInitTimer();
  is_init = ret;

  logPrintf("[%s] pwmInit()\n", ret ? "OK" : "E_");

#ifdef _USE_HW_CLI
  cliAdd("pwm", cliPwm);
#endif
  return ret;
}

bool pwmInitTimer(void)
{
  bool ret = true;  
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};


  __HAL_RCC_TIM15_CLK_ENABLE();
  

  // TIM15
  //
  htim15.Instance = TIM15;
  htim15.Init.Prescaler = (100000000 / (PWM_TIM15_KHZ*1000*256)) - 1; 
  htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim15.Init.Period = 254;
  htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  htim15.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim15) != HAL_OK)
  {
    return false;
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim15, &sClockSourceConfig) != HAL_OK)
  {
     return false;
  }  
  if (HAL_TIM_PWM_Init(&htim15) != HAL_OK)
  {
    return false;
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;  
  assert(HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1) == HAL_OK);


  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  /**TIM15 GPIO Configuration
  PA2     ------> TIM15_CH1
  */
  GPIO_InitStruct.Pin       = GPIO_PIN_2;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_TIM15;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


  HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);

  return ret;
}

bool pwmIsInit(void)
{
  return is_init;
}

void pwmWrite(uint8_t ch, uint16_t pwm_data)
{
  assert(ch < PWM_MAX_CH);
  assert(pwm_data <= pwm_tbl[ch].max_value);

  pwm_duty[ch] = constrain(pwm_data, 0, pwm_tbl[ch].max_value);

  switch(ch)
  {
    case _DEF_PWM1:
      htim15.Instance->CCR1 = pwm_duty[ch];
      break;
  }
}

uint16_t pwmRead(uint8_t ch)
{
  if (ch >= HW_PWM_MAX_CH) return 0;

  return pwm_duty[ch];
}

uint16_t pwmGetMax(uint8_t ch)
{
  if (ch >= HW_PWM_MAX_CH) return 255;

  return pwm_tbl[ch].max_value;
}



#ifdef _USE_HW_CLI
void cliPwm(cli_args_t *args)
{
  bool ret = false;
  uint8_t  ch;
  uint32_t pwm;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    for (int i=0; i<PWM_MAX_CH; i++)
    {
      cliPrintf("%02d. %-32s : %03d/%03d %-2d%%\n", 
        i, 
        pwm_tbl[i].p_name, 
        pwm_duty[i], 
        pwm_tbl[i].max_value,
        pwm_duty[i] * 100 / pwm_tbl[i].max_value);
    }
    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "set"))
  {
    ch  = (uint8_t)args->getData(1);
    pwm = (uint8_t)args->getData(2);

    ch = constrain(ch, 0, PWM_MAX_CH);

    pwmWrite(ch, pwm);
    cliPrintf("pwm ch%d %d\n", ch, pwm);
    ret = true;
  }
  
  if (args->argc == 2 && args->isStr(0, "get"))
  {
    ch = (uint8_t)args->getData(1);

    cliPrintf("pwm ch%d %d\n", ch, pwmRead(ch));
    ret = true;
  }


  if (ret == false)
  {
    cliPrintf("pwm info\n");
    cliPrintf("pwm set 0~%d 0~255 \n", PWM_MAX_CH-1);
    cliPrintf("pwm get 0~%d \n", PWM_MAX_CH-1);
  }

}
#endif

#endif