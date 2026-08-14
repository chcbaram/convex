#include "via_hid.h"
#include "raw_hid.h"
#include "fwupdate.h"


#define USE_VIA_HID_PRINT   0



#if USE_VIA_HID_PRINT == 1
static const char *command_id_str[] =
{
  [id_get_protocol_version]                 = "id_get_protocol_version",
  [id_get_keyboard_value]                   = "id_get_keyboard_value",
  [id_set_keyboard_value]                   = "id_set_keyboard_value",
  [id_dynamic_keymap_get_keycode]           = "id_dynamic_keymap_get_keycode",
  [id_dynamic_keymap_set_keycode]           = "id_dynamic_keymap_set_keycode",
  [id_dynamic_keymap_reset]                 = "id_dynamic_keymap_reset",
  [id_custom_set_value]                     = "id_custom_set_value",
  [id_custom_get_value]                     = "id_custom_get_value",
  [id_custom_save]                          = "id_custom_save",
  [id_eeprom_reset]                         = "id_eeprom_reset",
  [id_bootloader_jump]                      = "id_bootloader_jump",
  [id_dynamic_keymap_macro_get_count]       = "id_dynamic_keymap_macro_get_count",
  [id_dynamic_keymap_macro_get_buffer_size] = "id_dynamic_keymap_macro_get_buffer_size",
  [id_dynamic_keymap_macro_get_buffer]      = "id_dynamic_keymap_macro_get_buffer",
  [id_dynamic_keymap_macro_set_buffer]      = "id_dynamic_keymap_macro_set_buffer",
  [id_dynamic_keymap_macro_reset]           = "id_dynamic_keymap_macro_reset",
  [id_dynamic_keymap_get_layer_count]       = "id_dynamic_keymap_get_layer_count",
  [id_dynamic_keymap_get_buffer]            = "id_dynamic_keymap_get_buffer",
  [id_dynamic_keymap_set_buffer]            = "id_dynamic_keymap_set_buffer",
  [id_dynamic_keymap_get_encoder]           = "id_dynamic_keymap_get_encoder",
  [id_dynamic_keymap_set_encoder]           = "id_dynamic_keymap_set_encoder",
  [id_unhandled]                            = "id_unhandled",
};

static void via_hid_print(uint8_t *data, uint8_t length, bool is_resp);
#endif


static bool via_hid_receive(uint8_t *data, uint8_t length);


void via_hid_init(void)
{
  usbHidSetViaReceiveFunc(via_hid_receive);
}

void raw_hid_send(uint8_t *data, uint8_t length)
{
  
}

bool via_hid_receive(uint8_t *data, uint8_t length)
{
  // 웹 업데이트 리포트(0xC0)는 VIA 명령이 아니다. 큐로 넘기고 자동 응답은 막는다.
  // DATA 는 리포트마다 응답이 필요없고, 응답 큐가 20ms 마다 하나씩만 빠져 넘치기 때문이다.
  if (fwupdateHandleReport(data, length))
  {
    return false;
  }

  #if USE_VIA_HID_PRINT == 1
  via_hid_print(data, length, true);
  #endif
  raw_hid_receive(data, length);

  return true;
}

#if USE_VIA_HID_PRINT == 1
void via_hid_print(uint8_t *data, uint8_t length, bool is_resp)
{
  uint8_t *command_id   = &(data[0]);
  uint8_t *command_data = &(data[1]);  
  uint8_t data_len = length - 1;


  logPrintf("[%s] id : 0x%02X, 0x%02X, len %d,  %s",
            is_resp == true ? "OK" : "  ",
            *command_id,
            command_data[0],
            length,
            command_id_str[*command_id]);

  if (is_resp == true)
  {
    logPrintf("\n");
    return;
  }

  for (int i=0; i<data_len; i++)
  {
    if (i%8 == 0)
      logPrintf("\n     ");
    logPrintf("0x%02X ", command_data[i]);
  }
  logPrintf("\n");  
}
#endif