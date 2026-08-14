#include "spectrum_app.h"
#include "quantum.h"


// 막대는 매트릭스 열 하나에 하나씩 둔다. 284 / 22 = 12.9 라 폭 11 + 간격 2 로 맞는다.
#define BAR_CNT       MATRIX_COLS
#define BAR_GAP       2
#define BAR_W         ((LCD_WIDTH - BAR_GAP * (BAR_CNT - 1)) / BAR_CNT)
#define BAR_LEFT      ((LCD_WIDTH - (BAR_W * BAR_CNT + BAR_GAP * (BAR_CNT - 1))) / 2)

#define BAR_MIN_H     2       // 아무것도 안 눌러도 바닥선은 남긴다
#define PEAK_H        2

// 누를 때 튀어오르는 높이 (%). 아래 열일수록 높다.
#define HIT_MIN_PCT   35
#define HIT_MAX_PCT   100

// 프레임마다 내려오는 양 (%). 피크는 더 천천히 내려온다.
#define FALL_PCT      5
#define PEAK_FALL_PCT 2
#define FRAME_MS      20

#define COLOR_BG      lv_color_hex(0x08090E)
#define COLOR_LOW     lv_color_hex(0x28C846)
#define COLOR_MID     lv_color_hex(0xF0DC1E)
#define COLOR_HIGH    lv_color_hex(0xFF3C28)
#define COLOR_PEAK    lv_color_hex(0xEBF0F5)


static void appInit(void);
static void appMain(app_args_t *p_args);
static void uiInit(void);
static void uiDeInit(void);
static void uiDraw(void);

static app_info_t app_info = {
  .id       = APP_ID_SPECTRUM,
  .name     = "SPECTRUM",
  .init     = appInit,
  .run_func = appMain,
};

static lv_obj_t *root = NULL;
static lv_obj_t *bar_obj[BAR_CNT];
static lv_obj_t *peak_obj[BAR_CNT];

// 키 스레드가 쓰고 UI 스레드가 읽는다. 한 바이트씩이라 잠금은 두지 않는다.
static volatile uint8_t level[BAR_CNT];
static volatile uint8_t peak[BAR_CNT];


app_info_t *spectrumGetAppInfo(void)
{
  return &app_info;
}

void spectrumSetKey(uint8_t row, uint8_t col)
{
  uint8_t hit;

  if (col >= BAR_CNT || row >= MATRIX_ROWS)
    return;

  // 아래 열일수록 높게. 위 열은 숫자·기능키라 잔잔하고, 스페이스가 제일 크게 튄다.
  hit = HIT_MIN_PCT + (HIT_MAX_PCT - HIT_MIN_PCT) * row / (MATRIX_ROWS - 1);

  if (hit > level[col])
    level[col] = hit;
  if (hit > peak[col])
    peak[col] = hit;
}

void appInit(void)
{
  for (int i = 0; i < BAR_CNT; i++)
  {
    level[i] = 0;
    peak[i]  = 0;
  }
}

void appMain(app_args_t *p_args)
{
  uiInit();

  while (!p_args->is_exit)
  {
    for (int i = 0; i < BAR_CNT; i++)
    {
      level[i] = (level[i] > FALL_PCT) ? (level[i] - FALL_PCT) : 0;

      if (peak[i] > level[i])
        peak[i] = (peak[i] > PEAK_FALL_PCT) ? (peak[i] - PEAK_FALL_PCT) : 0;
      else
        peak[i] = level[i];
    }

    uiDraw();
    lvglUpdate();
    delay(FRAME_MS);
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
  lv_obj_set_style_radius(root, 0, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < BAR_CNT; i++)
  {
    int x = BAR_LEFT + i * (BAR_W + BAR_GAP);

    bar_obj[i] = lv_obj_create(root);
    lv_obj_set_size(bar_obj[i], BAR_W, BAR_MIN_H);
    lv_obj_set_pos(bar_obj[i], x, LCD_HEIGHT - BAR_MIN_H);
    lv_obj_set_style_bg_color(bar_obj[i], COLOR_LOW, 0);
    lv_obj_set_style_border_width(bar_obj[i], 0, 0);
    lv_obj_set_style_radius(bar_obj[i], 0, 0);
    lv_obj_set_style_pad_all(bar_obj[i], 0, 0);
    lv_obj_clear_flag(bar_obj[i], LV_OBJ_FLAG_SCROLLABLE);

    peak_obj[i] = lv_obj_create(root);
    lv_obj_set_size(peak_obj[i], BAR_W, PEAK_H);
    lv_obj_set_pos(peak_obj[i], x, LCD_HEIGHT - PEAK_H);
    lv_obj_set_style_bg_color(peak_obj[i], COLOR_PEAK, 0);
    lv_obj_set_style_border_width(peak_obj[i], 0, 0);
    lv_obj_set_style_radius(peak_obj[i], 0, 0);
    lv_obj_set_style_pad_all(peak_obj[i], 0, 0);
    lv_obj_clear_flag(peak_obj[i], LV_OBJ_FLAG_SCROLLABLE);
  }
}

void uiDeInit(void)
{
  if (root != NULL)
  {
    lv_obj_delete(root);
    root = NULL;
  }
}

void uiDraw(void)
{
  if (root == NULL) return;

  for (int i = 0; i < BAR_CNT; i++)
  {
    uint8_t lv_pct = level[i];
    uint8_t pk_pct = peak[i];

    int h = (LCD_HEIGHT * lv_pct) / 100;
    if (h < BAR_MIN_H) h = BAR_MIN_H;

    lv_obj_set_height(bar_obj[i], h);
    lv_obj_set_y(bar_obj[i], LCD_HEIGHT - h);

    if (lv_pct >= 80)
      lv_obj_set_style_bg_color(bar_obj[i], COLOR_HIGH, 0);
    else if (lv_pct >= 55)
      lv_obj_set_style_bg_color(bar_obj[i], COLOR_MID, 0);
    else
      lv_obj_set_style_bg_color(bar_obj[i], COLOR_LOW, 0);

    int py = LCD_HEIGHT - ((LCD_HEIGHT * pk_pct) / 100) - PEAK_H;
    if (py < 0) py = 0;
    if (py > LCD_HEIGHT - PEAK_H) py = LCD_HEIGHT - PEAK_H;

    lv_obj_set_y(peak_obj[i], py);
  }
}
