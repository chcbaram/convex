#include "update_app.h"
#include "launcher/launcher.h"
#include "fwupdate.h"


#define COLOR_BG      lv_color_hex(0x151820)
#define COLOR_TITLE   lv_color_hex(0xFFFFFF)
#define COLOR_INFO    lv_color_hex(0x9AA4B2)
#define COLOR_BAR     lv_color_hex(0x1C7ED6)
#define COLOR_BAR_BG  lv_color_hex(0x2C3240)
#define COLOR_OK      lv_color_hex(0x2F9E44)
#define COLOR_ERR     lv_color_hex(0xE03131)

// 끝난 화면을 잠깐 보여주고 원래 앱으로 돌아간다. 호스트가 슬롯 화면을
// 요청하면(SHOW) 그 전에 빠져나가므로 이 시간은 그때 쓰이지 않는다.
#define DONE_HOLD_MS  1500


static void appInit(void);
static void appMain(app_args_t *p_args);
static void uiInit(void);
static void uiDeInit(void);
static void uiDraw(fwupdate_status_t *p_status);

static app_info_t app_info = {
  .id       = APP_ID_UPDATE,
  .name     = "UPDATE",
  .init     = appInit,
  .run_func = appMain,
};

LV_FONT_DECLARE(convex20);
LV_FONT_DECLARE(convex16);

static lv_obj_t *root        = NULL;
static lv_obj_t *title_label = NULL;
static lv_obj_t *info_label  = NULL;
static lv_obj_t *bar         = NULL;


app_info_t *updateGetAppInfo(void)
{
  return &app_info;
}

void appInit(void)
{
}

void appMain(app_args_t *p_args)
{
  fwupdate_status_t status;
  uint32_t done_time = 0;

  uiInit();

  while (!p_args->is_exit)
  {
    fwupdateGetStatus(&status);
    uiDraw(&status);

    if (status.state == FWUPDATE_STATE_DONE || status.state == FWUPDATE_STATE_ERROR)
    {
      if (done_time == 0)
      {
        done_time = millis();
      }
      else if (millis() - done_time >= DONE_HOLD_MS)
      {
        uiReqAppBack();
      }
    }
    else
    {
      done_time = 0;
    }

    lvglUpdate();
    delay(20);
  }

  uiDeInit();
}

void uiInit(void)
{
  root = lv_obj_create(lv_screen_active());
  lv_obj_set_size(root, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(root, COLOR_BG, 0);
  lv_obj_set_style_pad_all(root, 0, 0);
  lv_obj_set_style_border_width(root, 0, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  title_label = lv_label_create(root);
  lv_obj_set_style_text_font(title_label, &convex20, 0);
  lv_obj_set_style_text_color(title_label, COLOR_TITLE, 0);
  lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 6);
  lv_label_set_text(title_label, "");

  bar = lv_bar_create(root);
  lv_obj_set_size(bar, LCD_WIDTH - 40, 10);
  lv_obj_align(bar, LV_ALIGN_CENTER, 0, 6);
  lv_obj_set_style_bg_color(bar, COLOR_BAR_BG, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, COLOR_BAR, LV_PART_INDICATOR);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);

  info_label = lv_label_create(root);
  lv_obj_set_style_text_font(info_label, &convex16, 0);
  lv_obj_set_style_text_color(info_label, COLOR_INFO, 0);
  lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_label_set_text(info_label, "");
}

void uiDeInit(void)
{
  if (root != NULL)
  {
    lv_obj_delete(root);
    root        = NULL;
    title_label = NULL;
    info_label  = NULL;
    bar         = NULL;
  }
}

void uiDraw(fwupdate_status_t *p_status)
{
  if (root == NULL) return;

  if (p_status->target == FWUPDATE_TARGET_FIRM)
    lv_label_set_text(title_label, "FIRMWARE UPDATE");
  else if (p_status->is_erase)
    lv_label_set_text_fmt(title_label, "SLOT %d ERASE", p_status->slot + 1);
  else
    lv_label_set_text_fmt(title_label, "SLOT %d SAVE", p_status->slot + 1);

  switch (p_status->state)
  {
    case FWUPDATE_STATE_ERASE:
      lv_bar_set_value(bar, p_status->percent, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(bar, COLOR_BAR, LV_PART_INDICATOR);
      lv_obj_set_style_text_color(info_label, COLOR_INFO, 0);
      lv_label_set_text_fmt(info_label, "Erasing... %d %%", p_status->percent);
      break;

    case FWUPDATE_STATE_WRITE:
      lv_bar_set_value(bar, p_status->percent, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(bar, COLOR_BAR, LV_PART_INDICATOR);
      lv_obj_set_style_text_color(info_label, COLOR_INFO, 0);
      lv_label_set_text_fmt(info_label, "%d %%   Do not unplug", p_status->percent);
      break;

    case FWUPDATE_STATE_DONE:
      lv_bar_set_value(bar, 100, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(bar, COLOR_OK, LV_PART_INDICATOR);
      lv_obj_set_style_text_color(info_label, COLOR_OK, 0);
      lv_label_set_text(info_label, "Done");
      break;

    case FWUPDATE_STATE_ERROR:
      lv_obj_set_style_bg_color(bar, COLOR_ERR, LV_PART_INDICATOR);
      lv_obj_set_style_text_color(info_label, COLOR_ERR, 0);
      lv_label_set_text_fmt(info_label, "Error 0x%02X", p_status->err);
      break;

    default:
      lv_label_set_text(info_label, "");
      break;
  }
}
