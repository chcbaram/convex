#include "spectrum_app.h"
#include "quantum.h"


// 막대는 매트릭스 열 하나에 하나씩 둔다. 284 / 22 = 12.9 라 폭 11 + 간격 2 로 맞는다.
#define BAR_CNT       MATRIX_COLS
#define BAR_GAP       2
#define BAR_W         ((LCD_WIDTH - BAR_GAP * (BAR_CNT - 1)) / BAR_CNT)
#define BAR_LEFT      ((LCD_WIDTH - (BAR_W * BAR_CNT + BAR_GAP * (BAR_CNT - 1))) / 2)

#define BAR_MIN_H     2       // 아무것도 안 눌러도 바닥선은 남긴다
#define PEAK_H        2

// 누를 때 튀어오르는 높이 (%). 위 열일수록 높다.
#define HIT_MIN_PCT   35
#define HIT_MAX_PCT   100

// 감쇠는 지수형이다. 선형으로 깎으면 낙하 전체가 몇 단계밖에 안 돼
// 계단처럼 보이고, 단계를 잘게 하면 이번엔 너무 느려진다.
// 현재 높이의 일정 비율씩 줄이면 처음엔 빠르고 끝에서 부드럽게 잦아든다.
#define FALL_PCT      8       // 프레임마다 현재 높이의 8%
#define FRAME_MS      10

// 피크는 예제 GIF 와 같은 속도로 내려온다. GIF 은 70ms 프레임마다 4.5%씩
// 선형으로 떨어뜨린다 = 초당 64%. 지수로 하면 끝에서 한없이 느려져
// 꼭대기에 오래 남는다. 값은 0.1% 단위다.
#define PEAK_FALL_X10 6       // 프레임(10ms)당 0.6% -> 초당 60%

// 피크는 잠깐 멈췄다 내려온다. 방금 얼마나 높았는지가 남는다.
#define PEAK_HOLD_MS  150

// 피크가 막대 끝에 붙어 있으면 흰 선이 막대 테두리에 겹쳐 깜빡이는 것처럼
// 보인다. 이만큼 떨어져 있을 때만 보여 준다.
#define PEAK_SHOW_GAP 4

#define COLOR_BG      lv_color_hex(0x08090E)
#define COLOR_LOW     lv_color_hex(0x28C846)
#define COLOR_PEAK    lv_color_hex(0xEBF0F5)

// 색은 RGB 로 섞지 않고 색상환을 돈다. 초록(120도) -> 빨강(0도) 으로
// 색조만 바꾸고 채도는 100% 로 두면 중간이 탁해지지 않는다.
#define HUE_LOW       120     // 낮은 막대 : 초록
#define HUE_HIGH      0       // 높은 막대 : 빨강
#define VAL_TIP       100     // 막대 끝 밝기
#define VAL_ROOT      55      // 막대 뿌리 밝기


static void appInit(void);
static void appMain(app_args_t *p_args);
static void uiInit(void);
static void uiDeInit(void);
static void uiDraw(void);
static lv_color_t levelColor(uint8_t pct);

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
static volatile uint8_t  level[BAR_CNT];
static volatile uint16_t peak[BAR_CNT];        // 0.1% 단위 (0 ~ 1000)
static volatile uint32_t peak_time[BAR_CNT];   // 피크가 갱신된 시각


app_info_t *spectrumGetAppInfo(void)
{
  return &app_info;
}

void spectrumSetKey(uint8_t row, uint8_t col)
{
  uint8_t hit;

  if (col >= BAR_CNT || row >= MATRIX_ROWS)
    return;

  // 위 열일수록 높게. 숫자·기능키가 크게 튀고 스페이스 쪽이 잔잔하다.
  hit = HIT_MIN_PCT + (HIT_MAX_PCT - HIT_MIN_PCT) * (MATRIX_ROWS - 1 - row) / (MATRIX_ROWS - 1);

  // 낮은 키를 눌렀으면 내려가는 것도 바로 보여 준다. 누를 때마다 반응이
  // 있어야 하고, 직전에 높았던 값은 피크 선이 들고 있다.
  level[col] = hit;

  if ((uint16_t)hit * 10 >= peak[col])
  {
    peak[col]      = (uint16_t)hit * 10;
    peak_time[col] = millis();
  }
}

void appInit(void)
{
  for (int i = 0; i < BAR_CNT; i++)
  {
    level[i]     = 0;
    peak[i]      = 0;
    peak_time[i] = 0;
  }
}

void appMain(app_args_t *p_args)
{
  uiInit();

  while (!p_args->is_exit)
  {
    for (int i = 0; i < BAR_CNT; i++)
    {
      uint8_t step;

      if (level[i] > 0)
      {
        step = (level[i] * FALL_PCT) / 100;
        if (step == 0) step = 1;              // 비율만 쓰면 낮은 값이 안 내려온다
        level[i] = (level[i] > step) ? (level[i] - step) : 0;
      }

      if (peak[i] > (uint16_t)level[i] * 10)
      {
        if (millis() - peak_time[i] >= PEAK_HOLD_MS)
        {
          peak[i] = (peak[i] > PEAK_FALL_X10) ? (peak[i] - PEAK_FALL_X10) : 0;
        }
      }
      else
      {
        peak[i] = (uint16_t)level[i] * 10;
      }
    }

    uiDraw();

    // LVGL 기본 주기(LV_DEF_REFR_PERIOD 33ms)를 따르면 감쇠 세 번에 한 번만
    // 그려져 뚝뚝 끊겨 보인다. 이 앱은 매 프레임 직접 그려 50fps 를 지킨다.
    // 바뀐 것이 없으면 LVGL 이 알아서 아무것도 하지 않으므로, 가만히 있을
    // 때는 부담이 없다.
    lv_refr_now(NULL);

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
    lv_obj_set_style_bg_grad_dir(bar_obj[i], LV_GRAD_DIR_VER, 0);
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
    lv_obj_add_flag(peak_obj[i], LV_OBJ_FLAG_HIDDEN);
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

// 높이를 색조로 바꾼다. 50% 면 60도라 정확히 노랑, 75% 면 30도라 주황이다.
static uint16_t levelHue(uint8_t pct)
{
  if (pct > 100) pct = 100;
  return HUE_LOW - ((HUE_LOW - HUE_HIGH) * pct / 100);
}

lv_color_t levelColor(uint8_t pct)
{
  return lv_color_hsv_to_rgb(levelHue(pct), 100, VAL_TIP);
}

void uiDraw(void)
{
  if (root == NULL) return;

  for (int i = 0; i < BAR_CNT; i++)
  {
    uint8_t lv_pct = level[i];
    uint8_t pk_pct = peak[i] / 10;

    int h = (LCD_HEIGHT * lv_pct) / 100;
    if (h < BAR_MIN_H) h = BAR_MIN_H;

    lv_obj_set_height(bar_obj[i], h);
    lv_obj_set_y(bar_obj[i], LCD_HEIGHT - h);

    // 끝은 밝게, 뿌리는 같은 색조를 유지한 채 어둡게. 회색을 섞으면 탁해진다.
    uint16_t hue = levelHue(lv_pct);

    lv_obj_set_style_bg_color(bar_obj[i], lv_color_hsv_to_rgb(hue, 100, VAL_TIP), 0);
    lv_obj_set_style_bg_grad_color(bar_obj[i], lv_color_hsv_to_rgb(hue, 100, VAL_ROOT), 0);

    if (pk_pct > lv_pct + PEAK_SHOW_GAP)
    {
      int py = LCD_HEIGHT - ((LCD_HEIGHT * pk_pct) / 100) - PEAK_H;
      if (py < 0) py = 0;
      if (py > LCD_HEIGHT - PEAK_H) py = LCD_HEIGHT - PEAK_H;

      lv_obj_set_y(peak_obj[i], py);
      lv_obj_clear_flag(peak_obj[i], LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      lv_obj_add_flag(peak_obj[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}
