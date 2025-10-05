#include "usb.h"

#include "tusb.h"
#include "usb_desc.h"


static void usbInitPhy(void);




bool usbInit(void)
{
  usbInitPhy();

  tusb_init();

  return true;
}

void usbDeInit(void)
{
  __HAL_RCC_USB_OTG_FS_CLK_DISABLE();  

  HAL_NVIC_DisableIRQ(OTG_FS_IRQn);  
}

void usbUpdate(void)
{
  tud_task();
}

void tud_mount_cb(void)
{
  logPrintf("tud_mount_cb()\n");
}

void tud_umount_cb(void)
{
  logPrintf("tud_umount_cb()\n");
}

void usbInitPhy(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  GPIO_InitTypeDef GPIO_InitStruct;

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_PWREx_EnableUSBVoltageDetector();

  __HAL_RCC_GPIOA_CLK_ENABLE();

  /**USB_OTG_FS GPIO Configuration
  PA11     ------> USB_OTG_FS_DM
  PA12     ------> USB_OTG_FS_DP
  */
  GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USB_OTG_FS clock enable */
  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

  // Disable VBUS sense (B device) via pin PA9
  USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

  // B-peripheral session valid override enable
  USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
  USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;  

  HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

void OTG_FS_IRQHandler(void)
{
  tud_int_handler(BOARD_TUD_RHPORT);
}