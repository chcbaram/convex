#include "quantum.h" // 필요한 시스템 헤더
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// --- 드라이브 설정 ---
#define SLOT_DRIVE_LETTER  'F'
#define SLOT_MAX_CH         4


// --- UF2 슬롯 메모리 주소 설정 (CONVEX UF2 생성기 기준) ---
#define SLOT1_ADDR (FLASH_ADDR_UPDATE_SLOT + (FLASH_SIZE_SLOT * 0) + FLASH_SIZE_TAG)
#define SLOT2_ADDR (FLASH_ADDR_UPDATE_SLOT + (FLASH_SIZE_SLOT * 1) + FLASH_SIZE_TAG)
#define SLOT3_ADDR (FLASH_ADDR_UPDATE_SLOT + (FLASH_SIZE_SLOT * 2) + FLASH_SIZE_TAG)
#define SLOT4_ADDR (FLASH_ADDR_UPDATE_SLOT + (FLASH_SIZE_SLOT * 3) + FLASH_SIZE_TAG)




// --- 디자인 설정 ---
#define COLOR_BG lv_color_hex(0x000000) 


// --- 앱 상태 관리 ---
typedef struct
{
  uint8_t   current_slot;
  lv_obj_t *gif_obj;
  bool      is_playing;
} gif_state_t;

// 파일 상태 추적을 위한 구조체
typedef struct {
    uint32_t base_addr; // 실제 GIF 데이터 시작 주소 (Header 32B 이후)
    uint32_t pos;       // 현재 읽기 위치
    uint32_t size;      // 헤더에서 읽어온 파일 크기
} flash_file_t;

static gif_state_t    state;
static lv_obj_t      *root         = NULL;
static uint8_t        slot_run     = SLOT_MAX_CH;
static uint8_t        slot_run_req = 0;
static const uint32_t SLOT_ADDRS[] = {SLOT1_ADDR, SLOT2_ADDR, SLOT3_ADDR, SLOT4_ADDR};


LV_FONT_DECLARE(convex32);




static void *flash_fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
  if (mode != LV_FS_MODE_RD) return NULL;

  int slot_idx = -1;
  if (strcmp(path, "slot1.gif") == 0)
    slot_idx = 0;
  else if (strcmp(path, "slot2.gif") == 0)
    slot_idx = 1;
  else if (strcmp(path, "slot3.gif") == 0)
    slot_idx = 2;
  else if (strcmp(path, "slot4.gif") == 0)
    slot_idx = 3;

  if (slot_idx == -1) return NULL;

  uint32_t slot_base = SLOT_ADDRS[slot_idx];
  uint32_t file_size = 0;

  // 헤더(오프셋 16)에서 4바이트 사이즈 읽기
  if (!flashRead(slot_base + 16, (uint8_t *)&file_size, 4)) return NULL;
  if (file_size == 0 || file_size == 0xFFFFFFFF) return NULL;

  flash_file_t *f = lv_malloc(sizeof(flash_file_t));
  f->base_addr    = slot_base + 32; // 헤더 32바이트 제외
  f->size         = file_size;
  f->pos          = 0;

  return f;
}

static lv_fs_res_t flash_fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
  flash_file_t *f         = file_p;
  uint32_t      remaining = f->size - f->pos;
  uint32_t      check_btr = (btr > remaining) ? remaining : btr;

  if (check_btr > 0)
  {
    if (flashRead(f->base_addr + f->pos, (uint8_t *)buf, check_btr))
    {
      f->pos += check_btr;
      *br     = check_btr;
    }
    else
    {
      return LV_FS_RES_HW_ERR;
    }
  }
  else
  {
    *br = 0;
  }
  return LV_FS_RES_OK;
}

static lv_fs_res_t flash_fs_close(lv_fs_drv_t *drv, void *file_p)
{
  lv_free(file_p);
  return LV_FS_RES_OK;
}

static lv_fs_res_t flash_fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
  flash_file_t *f = file_p;
  if (whence == LV_FS_SEEK_SET)
    f->pos = pos;
  else if (whence == LV_FS_SEEK_CUR)
    f->pos += pos;
  else if (whence == LV_FS_SEEK_END)
    f->pos = (f->size > pos) ? (f->size - pos) : 0;

  if (f->pos > f->size) f->pos = f->size;
  return LV_FS_RES_OK;
}

static lv_fs_res_t flash_fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
  flash_file_t *f = file_p;
  *pos_p          = f->pos;
  return LV_FS_RES_OK;
}

// 드라이버 등록
void flash_fs_init(void)
{
  static lv_fs_drv_t drv;
  lv_fs_drv_init(&drv);
  drv.letter   = SLOT_DRIVE_LETTER;
  drv.open_cb  = flash_fs_open;
  drv.read_cb  = flash_fs_read;
  drv.close_cb = flash_fs_close;
  drv.seek_cb  = flash_fs_seek;
  drv.tell_cb  = flash_fs_tell;
  lv_fs_drv_register(&drv);
}

// --- GIF 로드 함수 ---
static void load_gif_from_slot(uint8_t slot_idx)
{
  uint32_t slot_base;
  uint32_t file_size = 0;
  bool file_exists = false;


  if (slot_idx >= 4 || root == NULL) return;


  slot_base = SLOT_ADDRS[slot_idx];


  // 헤더에서 사이즈 읽기 시도
  if (flashRead(slot_base + 16, (uint8_t *)&file_size, 4))
  {
    if (file_size > 0 && file_size < FLASH_SIZE_SLOT)
    {
      file_exists = true;
    }
  }

  if (file_exists)
  {
    // 3-A. GIF 파일이 존재하는 경우
    char path[32];
    snprintf(path, sizeof(path), "F:slot%d.gif", slot_idx + 1);

    if (state.gif_obj)
    {
      lv_obj_delete(state.gif_obj);
      state.gif_obj = NULL;
    }    
    state.gif_obj = lv_gif_create(root);
    lv_gif_set_src(state.gif_obj, path);
    lv_obj_center(state.gif_obj);

    file_exists = lv_gif_is_loaded(state.gif_obj);
  }

  if (!file_exists)
  {
    if (state.gif_obj)
    {
      lv_obj_delete(state.gif_obj);
      state.gif_obj = NULL;
    }    
    // 3-B. GIF 파일이 없는 경우 안내 문구 표시
    state.gif_obj = lv_label_create(root);
    lv_label_set_text_fmt(state.gif_obj, "GIF SLOT %d", slot_idx + 1);

    // 스타일 설정 (가독성을 위해)
    lv_obj_set_style_text_color(state.gif_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(state.gif_obj, LV_TEXT_ALIGN_CENTER, 0);

    // 선언된 convex32 폰트 적용 (필요 시)
    lv_obj_set_style_text_font(state.gif_obj, &convex32, 0);

    lv_obj_center(state.gif_obj);
  }

  state.current_slot = slot_idx;
}

// --- UI 초기화 ---
static void uiInit(void)
{
  static bool fs_inited = false;

  if (!fs_inited)
  {
    flash_fs_init();
    fs_inited = true;
  }

  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  logPrintf("IN Free memory: %u bytes (%d%% used)\n", mon.free_size, mon.used_pct);

  root = lv_obj_create(lv_screen_active());
  lv_obj_set_size(root, LCD_WIDTH, LCD_HEIGHT); 
  lv_obj_set_style_bg_color(root, COLOR_BG, 0);
  lv_obj_set_style_pad_all(root, 0, 0);
  lv_obj_set_style_border_width(root, 0, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
}

static void uiDeInit(void)
{
  if (root)
  {
    lv_obj_delete(root);
    root          = NULL;
    state.gif_obj = NULL;
  }

  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  logPrintf("OUT Free memory: %u bytes (%d%% used)\n", mon.free_size, mon.used_pct);
}

// --- 키 입력 처리 (슬롯 변경) ---
bool gifSetKeycode(uint16_t keycode)
{
  bool ret = true;

  if (root == NULL)
  {
    return false;
  }

  switch (keycode)
  {
    case UI_KC_SLOT_NXT:
      slot_run_req = (slot_run_req + 1) % SLOT_MAX_CH;
      eepromWriteByte(HW_EEPROM_SLOT_RUN, slot_run_req);      
      break;          

    case UI_KC_SLOT_DEL:
      flashErase(SLOT_ADDRS[slot_run_req], 10);
      slot_run = SLOT_MAX_CH;
      break;

    default:
      ret = false;
      break;
  }
  return ret;
}

void gifInit(void)
{
  slot_run = SLOT_MAX_CH;

  eepromReadByte(HW_EEPROM_SLOT_RUN, &slot_run_req);
  if (slot_run_req >= SLOT_MAX_CH)
  {
    slot_run_req = 0;
  }
}

void gifMain(app_args_t *p_args)
{
  uiInit();

  while (!p_args->is_exit)
  {
    if (slot_run_req != slot_run)
    {
      logPrintf("Slot Change : %d -> %d\n", slot_run, slot_run_req);
      slot_run = slot_run_req;
      load_gif_from_slot(slot_run);
    }
    lvglUpdate();
    delay(5);
  }

  uiDeInit();
}

// --- 앱 정보 등록 ---
app_info_t *gifGetAppInfo(void)
{
  static app_info_t info = {
    .id       = APP_ID_SLOT,
    .name     = "GIF PLAYER",
    .init     = gifInit,
    .run_func = gifMain,
  };
  return &info;
}