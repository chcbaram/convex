#include "clock.h"


static void appInit(void);
static void appMain(void);
static void uiInit(void);

static app_info_t app_info = {
    .name = "CLOCK",
    .init = appInit,
    .run_func = appMain,
};


app_info_t *clockGetAppInfo(void)
{
  return &app_info;
}

void appInit(void)
{
  uiInit();
}

void appMain(void)
{
  uint32_t pre_time = millis();

  while(millis()-pre_time < 3000)
  {
    lvglUpdate();
    delay(1);    
  }
}

// 폰트 선언
LV_FONT_DECLARE(convex32);
LV_FONT_DECLARE(convex24);
LV_FONT_DECLARE(convex20);
LV_FONT_DECLARE(convex16);


// 색상 및 스타일 상수
#define COLOR_NEON_PINK   lv_color_hex(0xFF00B9)
#define BG_COLOR          lv_color_hex(0x151820) // 어두운 그레이톤
#define LETTER_SPACE_DATE 1
#define LETTER_SPACE_TIME 2
#define COLON_GAP_PX      2                      // left ↔ ':' 간격
#define RIGHT_GAP_PX      2                      // ':' ↔ right 간격


// 전역 UI 핸들(접두사 없음)
static lv_obj_t   *root             = NULL;
static lv_obj_t   *date_label       = NULL;
static lv_obj_t   *time_anchor      = NULL;
static lv_obj_t   *time_left_label  = NULL; // "AM 10"
static lv_obj_t   *time_colon_label = NULL; // ":" 전용, 점멸
static lv_obj_t   *time_right_label = NULL; // "05"
static lv_timer_t *clock_timer      = NULL;



// 레이아웃 옵션: 정렬 + 오프셋 + 루트 패딩
typedef struct {
	lv_align_t date_align;
	int16_t    date_ofs_x;
	int16_t    date_ofs_y;

	lv_align_t time_align;
	int16_t    time_ofs_x;
	int16_t    time_ofs_y;

	int16_t    root_pad_left;
	int16_t    root_pad_top;
	int16_t    root_pad_right;
	int16_t    root_pad_bottom;
} clock_layout_opts_t;

// 기본 레이아웃(첨부 이미지 유사)
static clock_layout_opts_t layout = {
	.date_align     = LV_ALIGN_TOP_LEFT,
	.date_ofs_x     = 8,
	.date_ofs_y     = 6,

	.time_align     = LV_ALIGN_LEFT_MID, // 날짜 아래 왼쪽 정렬 느낌
	.time_ofs_x     = 8,
	.time_ofs_y     = 18,

	.root_pad_left  = 0,
	.root_pad_top   = 0,
	.root_pad_right = 0,
	.root_pad_bottom= 0,
};

// 스타일
static lv_style_t style_date, style_time;
static bool style_inited = false;

// 12시간 표기(left/right 분리)
static void fmt_time_12h_split(int hour24, int min,
                               char *left, size_t left_sz,
                               char *right, size_t right_sz)
{
	const char *ampm = (hour24 >= 12) ? "PM" : "AM";
	int h12 = hour24 % 12;
	if (h12 == 0) h12 = 12;

	snprintf(left,  left_sz,  "%s %02d", ampm, h12); // "AM 10"
	snprintf(right, right_sz, "%02d", min);          // "05"
}

// 레이아웃 적용: left를 기준으로 colon/right를 이어붙임
static void apply_layout(void)
{
	if (!root) return;

	lv_obj_set_style_pad_left(root,   layout.root_pad_left,   0);
	lv_obj_set_style_pad_top(root,    layout.root_pad_top,    0);
	lv_obj_set_style_pad_right(root,  layout.root_pad_right,  0);
	lv_obj_set_style_pad_bottom(root, layout.root_pad_bottom, 0);

	// 날짜
	if (date_label) {
		lv_obj_align(date_label, layout.date_align, layout.date_ofs_x, layout.date_ofs_y);
	}

	// 시간 3분할
	if (time_left_label && time_colon_label && time_right_label) {
		// 1) left 기준 배치
		lv_obj_align(time_left_label, layout.time_align, layout.time_ofs_x, layout.time_ofs_y);

		// 2) left의 위치/폭을 기준으로 colon/right를 가로로 이어 배치
		lv_obj_update_layout(time_left_label);
		int left_x = lv_obj_get_x(time_left_label);
		int left_y = lv_obj_get_y(time_left_label);
		int left_w = lv_obj_get_width(time_left_label);

		lv_obj_set_pos(time_colon_label, left_x + left_w + COLON_GAP_PX, left_y);
		lv_obj_update_layout(time_colon_label);
		int colon_x = lv_obj_get_x(time_colon_label);
		int colon_w = lv_obj_get_width(time_colon_label);

		lv_obj_set_pos(time_right_label, colon_x + colon_w + RIGHT_GAP_PX, left_y);
	}
}

// 타이머 콜백: 날짜 갱신 + 콜론 점멸 + 폭 변화 재정렬
static void clock_update_cb(lv_timer_t *timer)
{
	LV_UNUSED(timer);
	rtc_info_t info;
	rtcGetInfo(&info);

	// 날짜: YYYY.MM.DD
	char date_buf[32];
	snprintf(date_buf, sizeof(date_buf),
	         "%04d.%02d.%02d",
	         (int)info.date.year + 2000, info.date.month, info.date.day);
	lv_label_set_text(date_label, date_buf);

	// 시간 분할 텍스트 갱신
	char left_buf[16], right_buf[8];
	fmt_time_12h_split(info.time.hours, info.time.minutes,
	                   left_buf, sizeof(left_buf),
	                   right_buf, sizeof(right_buf));
	lv_label_set_text(time_left_label, left_buf);
	lv_label_set_text(time_right_label, right_buf);

	// 콜론 점멸: 짝수초 표시, 홀수초 숨김
	if ((info.time.seconds % 2) == 0) {
		lv_obj_clear_flag(time_colon_label, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(time_colon_label, LV_OBJ_FLAG_HIDDEN);
	}

	// 텍스트 폭이 바뀌었을 수 있으므로 재배치
	apply_layout();
}

/* ========= 공개 API ========= */

// 레이아웃 변경(런타임 즉시 반영)
void uiClockSetLayout(const clock_layout_opts_t *opts)
{
	if (!opts) return;
	layout = *opts;
	apply_layout();
}

// UI 생성
void uiInit(void)
{
	// 루트 컨테이너
	root = lv_obj_create(lv_screen_active());
	lv_obj_set_size(root, LCD_WIDTH, LCD_HEIGHT);
	lv_obj_set_style_bg_color(root, BG_COLOR, 0);
	lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
	lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  // 루트 생성 후
  time_anchor = lv_obj_create(root);
  lv_obj_set_size(time_anchor, 1, 1);
  lv_obj_set_style_bg_opa(time_anchor, LV_OPA_0, 0); // 보이지 않게
  lv_obj_clear_flag(time_anchor, LV_OBJ_FLAG_SCROLLABLE);

	// 스타일 준비
	if (!style_inited) {
		lv_style_init(&style_date);
		lv_style_set_text_color(&style_date, COLOR_NEON_PINK);
		lv_style_set_text_font(&style_date, &convex24);   // 날짜용 폰트
		lv_style_set_bg_opa(&style_date, LV_OPA_0);

		lv_style_init(&style_time);
		lv_style_set_text_color(&style_time, COLOR_NEON_PINK);
		lv_style_set_text_font(&style_time, &convex32);   // 시간용 폰트
		lv_style_set_bg_opa(&style_time, LV_OPA_0);

		style_inited = true;
	}

	// 날짜 라벨(루트에 직접 배치)
	date_label = lv_label_create(root);
	lv_obj_add_style(date_label, &style_date, 0);
	lv_label_set_text(date_label, "0000.00.00");
	lv_obj_set_style_text_letter_space(date_label, LETTER_SPACE_DATE, 0);
	lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_LEFT, 0);

	// 시간 3분할 라벨(루트에 직접 배치)
	time_left_label = lv_label_create(root);
	lv_obj_add_style(time_left_label, &style_time, 0);
	lv_obj_set_style_text_letter_space(time_left_label, LETTER_SPACE_TIME, 0);
	lv_obj_set_style_text_align(time_left_label, LV_TEXT_ALIGN_LEFT, 0);
	lv_label_set_text(time_left_label, "PM 00");

	time_colon_label = lv_label_create(root);
	lv_obj_add_style(time_colon_label, &style_time, 0);
	lv_obj_set_style_text_letter_space(time_colon_label, LETTER_SPACE_TIME, 0);
	lv_obj_set_style_text_align(time_colon_label, LV_TEXT_ALIGN_LEFT, 0);
	lv_label_set_text(time_colon_label, ":");

	time_right_label = lv_label_create(root);
	lv_obj_add_style(time_right_label, &style_time, 0);
	lv_obj_set_style_text_letter_space(time_right_label, LETTER_SPACE_TIME, 0);
	lv_obj_set_style_text_align(time_right_label, LV_TEXT_ALIGN_LEFT, 0);
	lv_label_set_text(time_right_label, "00");

	// 초기 배치
	apply_layout();

	// 첫 갱신 및 타이머 시작(1초 주기)
	clock_update_cb(NULL);
	clock_timer = lv_timer_create(clock_update_cb, 1000, NULL);

  clock_layout_opts_t preset = {
    .date_align      = LV_ALIGN_TOP_LEFT,
    .date_ofs_x      = 8,
    .date_ofs_y      = 8,
    .time_align      = LV_ALIGN_RIGHT_MID,
    .time_ofs_x      = 0,
    .time_ofs_y      = 16,
    .root_pad_left   = 0,
    .root_pad_top    = 0,
    .root_pad_right  = 0,
    .root_pad_bottom = 0,
  };
  uiClockSetLayout(&preset);
}

// UI 제거
void ui_clock_destroy(void)
{
	if (clock_timer) {
		lv_timer_del(clock_timer);
		clock_timer = NULL;
	}
	if (root) {
		lv_obj_delete(root);
		root = NULL;
		date_label = NULL;
		time_left_label = NULL;
		time_colon_label = NULL;
		time_right_label = NULL;
	}
}

/* ===== 사용 예시 =====
int main(void)
{
	// LVGL 초기화, 디스플레이/입력 드라이버 설정 ...
	uiInit();

	// 첨부 이미지 유사 레이아웃 프리셋
	clock_layout_opts_t preset = {
		.date_align = LV_ALIGN_TOP_LEFT,  .date_ofs_x = 8,  .date_ofs_y = 6,
		.time_align = LV_ALIGN_LEFT_MID,  .time_ofs_x = 8,  .time_ofs_y = 18,
		.root_pad_left = 0, .root_pad_top = 0, .root_pad_right = 0, .root_pad_bottom = 0,
	};
	uiClockSetLayout(&preset);

	while (1) {
		lv_timer_handler();
		// delay...
	}
	return 0;
}
*/
