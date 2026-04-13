#include "calc.h"
#include "quantum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// --- 폰트 선언 ---
LV_FONT_DECLARE(neo);
LV_FONT_DECLARE(convex32);
LV_FONT_DECLARE(convex24);
LV_FONT_DECLARE(convex20);
LV_FONT_DECLARE(convex16);

// --- 디자인 설정 ---
#define COLOR_ACCENT   lv_color_hex(0xFF00B9)
#define COLOR_BG       lv_color_hex(0x151820)
#define COLOR_TEXT_DIM lv_color_hex(0x666666)

// --- 계산기 상태 관리 ---
typedef struct
{
  char   current_str[64];  // 현재 입력 문자열
  char   history_str[128]; // 상단 수식 문자열
  double result;           // 중간 계산 결과
  char   last_op;          // 마지막 연산자 (+, -, *, /)
  bool   is_new_input;     // 새 숫자를 입력할 차례인지 여부
} calc_state_t;

static calc_state_t state;

// --- 전역 UI 핸들 ---
static lv_obj_t *root          = NULL;
static lv_obj_t *label_history = NULL;
static lv_obj_t *label_main    = NULL;

static lv_style_t style_main;
static lv_style_t style_history;
static bool       style_inited = false;
static bool       is_pressed   = false;
static uint8_t    key_data     = ' ';

// --- 계산 로직 함수 ---

static void reset_calc()
{
  memset(&state, 0, sizeof(calc_state_t));
  strcpy(state.current_str, "0");
  state.is_new_input = true;
}

static void format_commas(const char *src, char *dest, size_t dest_size)
{
  int len     = strlen(src);
  int dot_pos = strchr(src, '.') ? (strchr(src, '.') - src) : len; // 소수점 위치 확인
  int j       = 0;

  for (int i = 0; i < len; i++)
  {
    // 1. 소수점 이전이며, 3자리마다 콤마 삽입 (단, 시작 지점은 제외)
    if (i < dot_pos && i > 0 && (dot_pos - i) % 3 == 0 && src[i - 1] != '-')
    {
      dest[j++] = ',';
    }
    dest[j++] = src[i];

    // 버퍼 오버플로우 방지
    if (j >= dest_size - 1) break;
  }
  dest[j] = '\0';
}

static void update_ui_labels()
{
  if (!label_main || !label_history) return;

  char display_buf[64];                       // 콤마가 포함된 문자열을 담을 임시 버퍼
  format_commas(state.current_str, display_buf, sizeof(display_buf));

  lv_label_set_text(label_history, state.history_str);
  lv_label_set_text(label_main, display_buf); // 콤마 처리된 텍스트 설정

  // --- 폰트 자동 크기 조절 (display_buf 기준) ---
  const lv_font_t *font_sizes[] = {&convex32, &convex24, &convex20, &convex16};
  uint8_t          font_idx     = 0;
  lv_point_t       size;
  int32_t          max_width = 265;

  for (font_idx = 0; font_idx < 4; font_idx++)
  {
    // 실제 콤마가 포함된 문자열의 길이를 측정합니다.
    lv_txt_get_size(&size, display_buf, font_sizes[font_idx],
                    0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x <= max_width) break;
  }
  if (font_idx >= 4) font_idx = 3;

  lv_obj_set_style_text_font(label_main, font_sizes[font_idx], 0);
}

static void perform_operation(double current_val)
{
  switch (state.last_op)
  {
    case '+':
      state.result += current_val;
      break;
    case '-':
      state.result -= current_val;
      break;
    case 'x':
      state.result *= current_val;
      break;
    case '/':
      if (current_val != 0) state.result /= current_val;
      break;
    default:
      state.result = current_val;
      break;
  }
}

// 외부(키패드 등)에서 호출할 입력 프로세서
void calc_input_char(char c)
{
  if (c >= '0' && c <= '9')
  {
    // 숫자 입력 로직 (기존과 동일)
    if (state.is_new_input || strcmp(state.current_str, "0") == 0)
    {
      if (state.is_new_input && state.history_str[0] == '=')
      {
        state.history_str[0] = '\0';
      }
      snprintf(state.current_str, sizeof(state.current_str), "%c", c);
      state.is_new_input = false;
    }
    else
    {
      if (strlen(state.current_str) < 14) strncat(state.current_str, &c, 1);
    }
  }
  else if (c == 'B')                    // 백스페이스 로직 추가
  {
    size_t len = strlen(state.current_str);
    if (len > 0 && !state.is_new_input) // 새 입력 상태가 아닐 때만 지움
    {
      if (len == 1)
      {
        strcpy(state.current_str, "0");
        state.is_new_input = true;      // 마지막 글자를 지우면 '0'으로 초기화 및 대기
      }
      else
      {
        state.current_str[len - 1] = '\0';
      }
    }
  }
  else if (c == '.')
  {
    if (!strchr(state.current_str, '.'))
    {
      strcat(state.current_str, ".");
      state.is_new_input = false;
    }
  }  
  else if (c == '+' || c == '-' || c == 'x' || c == '/')
  {
    double val = atof(state.current_str);
    perform_operation(val);
    state.last_op = c;

    // --- 히스토리에 콤마 적용 ---
    char temp_num[32];
    char comma_num[64];
    snprintf(temp_num, sizeof(temp_num), "%.10g", state.result); // 현재까지 결과값
    format_commas(temp_num, comma_num, sizeof(comma_num));       // 콤마 추가

    // "1,234 +" 형태로 히스토리 저장
    snprintf(state.history_str, sizeof(state.history_str), "%s %c", comma_num, c);

    state.is_new_input = true;
  }
  else if (c == '=')
  {
    double val = atof(state.current_str);
    perform_operation(val);

    // 결과값을 메인 문자열에 저장 -> label_main에 출력됨
    snprintf(state.current_str, sizeof(state.current_str), "%.10g", state.result);

    // 히스토리는 비워서 결과만 강조
    // state.history_str[0] = '\0';
    snprintf(state.history_str, sizeof(state.history_str), "%c", c);

    state.last_op        = 0;
    state.is_new_input   = true;
  }
  else if (c == 'C')
  {
    reset_calc();
  }

  update_ui_labels();
}

// --- UI 초기화 ---
static void uiInit(void)
{
  root = lv_obj_create(lv_screen_active());
  lv_obj_set_size(root, 284, 76);
  lv_obj_set_style_bg_color(root, COLOR_BG, 0);
  lv_obj_set_style_pad_all(root, 4, 0); // 내부 여백 살짝 부여
  lv_obj_set_style_border_width(root, 0, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  if (!style_inited)
  {
    lv_style_init(&style_main);
    lv_style_set_text_color(&style_main, COLOR_ACCENT);
    lv_style_set_text_font(&style_main, &convex32);

    lv_style_init(&style_history);
    lv_style_set_text_color(&style_history, COLOR_TEXT_DIM);
    lv_style_set_text_font(&style_history, &convex20);
    style_inited = true;
  }

  // 1. 수식 이력 라벨 (상단 배치)
  // 좌측 태그를 없앴으므로 너비를 270 이상으로 확장
  label_history = lv_label_create(root);
  lv_obj_add_style(label_history, &style_history, 0);
  lv_obj_set_width(label_history, 270);
  lv_label_set_text(label_history, "");
  lv_obj_align(label_history, LV_ALIGN_TOP_RIGHT, -2, 2);
  lv_obj_set_style_text_align(label_history, LV_TEXT_ALIGN_RIGHT, 0);

  // 2. 메인 입력/결과 라벨 (하단 배치)
  label_main = lv_label_create(root);
  lv_obj_add_style(label_main, &style_main, 0);
  lv_obj_set_width(label_main, 270);
  lv_label_set_text(label_main, "0");
  lv_obj_align(label_main, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
  lv_obj_set_style_text_align(label_main, LV_TEXT_ALIGN_RIGHT, 0);

  reset_calc();
}

static void uiDeInit(void)
{
  if (root)
  {
    lv_obj_delete(root);
    root          = NULL;
    label_history = NULL;
    label_main    = NULL;
  }
}

// --- 메인 루프 및 인터페이스 ---

static void calcInit(void)
{
  uiInit();
}

bool calcSetKeycode(uint16_t keycode)
{
  bool ret = true;


  switch (keycode)
  {
    case KC_KP_0:
      key_data = '0';
      break;

    case KC_KP_1 ... KC_KP_9:
      key_data = '1' + (keycode - KC_KP_1);
      break;

    case KC_KP_DOT:
      key_data = '.';
      break;

    case KC_KP_PLUS:
      key_data = '+';
      break;

    case KC_KP_MINUS:
      key_data = '-';
      break;
    
    case KC_KP_SLASH:
      key_data = '/';
      break;

    case KC_KP_ASTERISK:
      key_data = 'x';
      break;

    case KC_KP_ENTER:
      key_data = '=';
      break;

    case UI_KC_DEL:   
      key_data = 'B';
      break;

    case UI_KC_CLR:
      key_data = 'C';
      break;

    default:
      ret = false;
      break;
  }

  if (ret)
  {
    is_pressed = true;
  }
  return ret;
}

void calcMain(app_args_t *p_args)
{
  RGB rgb;
  lv_color_t color;

  while (!p_args->is_exit)
  {
    if (is_pressed)
    {
      is_pressed = false;
      calc_input_char(key_data);
    }

    lv_timer_handler(); // LVGL 9 핸들러
    delay(5);
  
    if (via_qmk_lcd_is_update(COLOR_TYPE_CALC))
    {
      rgb = hsv_to_rgb(via_qmk_lcd_get_color(COLOR_TYPE_CALC));
      color.red   = rgb.r;
      color.green = rgb.g;
      color.blue  = rgb.b;
      lv_style_set_text_color(&style_main, color);   
      update_ui_labels();
    }
  }
  uiDeInit();
}

app_info_t *calcGetAppInfo(void)
{
  static app_info_t info = {
    .id       = APP_ID_CALC,
    .name     = "CALC",
    .init     = calcInit,
    .run_func = calcMain,
  };
  return &info;
}