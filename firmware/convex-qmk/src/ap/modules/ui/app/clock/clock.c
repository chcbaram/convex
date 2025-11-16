#include "clock.h"


static void appInit(void);
static void appMain(void);
static void uiInit(void);
static bool eventReceive(event_t *p_event);
static void ui_clock_destroy(void);

static app_info_t app_info = {
  .name     = "CLOCK",
  .init     = appInit,
  .run_func = appMain
};

static bool is_exit = false;




void appInit(void)
{
  static bool is_first = true;
  
  uiInit();

  is_exit = false;

  if (is_first)
  {
    is_first = false;
    eventSub(eventReceive);
  }
}

void appMain(void)
{
  while(!is_exit)
  {
    lvglUpdate();
    delay(5);    
  }
  is_exit = false;

  ui_clock_destroy();
}

app_info_t *clockGetAppInfo(void)
{
  return &app_info;
}

bool eventReceive(event_t *p_event)
{
  if (p_event->code == EVENT_UI_APP_EXIT)
  {
    is_exit = true;
  }

  return true;
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

	// 시간(3분할)
	if (time_left_label && time_colon_label && time_right_label && time_anchor) {
		// 1) 앵커를 원하는 위치로 정렬
		lv_obj_align(time_anchor, layout.time_align, layout.time_ofs_x, layout.time_ofs_y);

		// 2) 각 라벨 폭 업데이트
		lv_obj_update_layout(time_left_label);
		lv_obj_update_layout(time_colon_label);
		lv_obj_update_layout(time_right_label);

		int w_left  = lv_obj_get_width(time_left_label);
		int w_colon = lv_obj_get_width(time_colon_label);
		int w_right = lv_obj_get_width(time_right_label);

		int total_w = w_left + COLON_GAP_PX + w_colon + RIGHT_GAP_PX + w_right;

		// 3) 앵커 좌표
		int ax = lv_obj_get_x(time_anchor);
		int ay = lv_obj_get_y(time_anchor);

		// 4) 수평 기준을 앵커 align에 따라 결정
		//    *_RIGHT 계열이면 우측 기준, *_MID/ CENTER 면 중앙, 나머지는 좌측 기준
		int start_x = ax; // 좌측 기준 기본
		switch (layout.time_align) {
			case LV_ALIGN_TOP_RIGHT:
			case LV_ALIGN_RIGHT_MID:
			case LV_ALIGN_BOTTOM_RIGHT:
				start_x = ax - total_w; // 오른쪽 기준
				break;
			case LV_ALIGN_TOP_MID:
			case LV_ALIGN_CENTER:
			case LV_ALIGN_BOTTOM_MID:
				start_x = ax - (total_w / 2); // 중앙 기준
				break;
			default:
				// LV_ALIGN_TOP_LEFT / LEFT_MID / BOTTOM_LEFT 등은 좌측 기준
				start_x = ax;
				break;
		}

		// 5) 같은 y로 배치. 세로 정렬이 필요하면 time_anchor의 y를 align에서 맞추는 방식으로 통일
		int y = ay;

		// left
		lv_obj_set_pos(time_left_label, start_x, y);

		// colon
		int x_colon = start_x + w_left + COLON_GAP_PX;
		lv_obj_set_pos(time_colon_label, x_colon, y);

		// right
		int x_right = x_colon + w_colon + RIGHT_GAP_PX;
		lv_obj_set_pos(time_right_label, x_right, y);
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
    .date_ofs_x      = 0,
    .date_ofs_y      = 0,
    .time_align      = LV_ALIGN_RIGHT_MID,
    .time_ofs_x      = 0,
    .time_ofs_y      = 0,
    .root_pad_left   = 10,
    .root_pad_top    = 5,
    .root_pad_right  = 10,
    .root_pad_bottom = 5,
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
