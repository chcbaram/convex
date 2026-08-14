#include "module.h"
#include "fwupdate.h"
#include "util_core.h"


#define FWUPDATE_SECTOR_SIZE      (64*1024)      // QSPI(W25Q128) block erase 단위
#define FWUPDATE_PAGE_SIZE        (256)          // QSPI page program 단위
#define FWUPDATE_SLOT_MAX_CH      (4)            // gif_app.c 의 SLOT_MAX_CH 와 같아야 한다
#define FWUPDATE_QUEUE_LEN        (128)          // 32B * 128 = 4KB

// 슬롯당 실제 데이터 상한 (태그 영역 제외)
#define FWUPDATE_SLOT_DATA_MAX    (FLASH_SIZE_SLOT - FLASH_SIZE_TAG)


typedef struct
{
  uint8_t buf[FWUPDATE_REPORT_SIZE];
} fwupdate_report_t;


static void fwupdateThread(void const *arg);
static void fwupdate_process(uint8_t *p_data);
static void fwupdate_cmd_info(void);
static void fwupdate_cmd_begin(uint8_t *p_data);
static void fwupdate_cmd_data(uint8_t *p_data);
static void fwupdate_cmd_end(uint8_t *p_data);
static void fwupdate_cmd_commit(void);
static void fwupdate_cmd_status(void);
static bool fwupdate_erase(uint32_t addr, uint32_t size);
static bool fwupdate_page_store(uint32_t addr, uint8_t *p_data, uint32_t length);
static bool fwupdate_page_flush(void);
static bool fwupdate_calc_crc(uint32_t addr, uint32_t size, uint16_t *p_crc);
static bool fwupdate_write_tag(void);
static void fwupdate_send_resp(uint8_t cmd, uint8_t err, uint8_t *p_data, uint8_t length);


static qbuffer_t         report_q;
static fwupdate_report_t report_q_buf[FWUPDATE_QUEUE_LEN];

static uint8_t  state       = FWUPDATE_STATE_IDLE;
static uint8_t  last_err    = FWUPDATE_OK;
static uint8_t  tr_target   = FWUPDATE_TARGET_FIRM;
static uint8_t  tr_slot     = 0;
static uint32_t tr_base     = 0;   // 데이터 시작 주소 (태그 다음)
static uint32_t tr_size     = 0;   // 전체 크기
static uint32_t tr_written  = 0;
static bool     is_commit   = false;

static uint8_t  page_buf[FWUPDATE_PAGE_SIZE];
static uint32_t page_addr   = 0;
static bool     page_dirty  = false;




static bool init(void)
{
  bool ret;

  qbufferCreateBySize(&report_q, (uint8_t *)report_q_buf, sizeof(fwupdate_report_t), FWUPDATE_QUEUE_LEN);

  ret = threadCreate("fwupdate", fwupdateThread, NULL, _HW_DEF_THREAD_MODULE_PRI, _HW_DEF_THREAD_MODULE_STACK);
  assert(ret);

  logPrintf("[%s] fwupdateInit()\n", ret ? "OK":"E_");
  return ret;
}

bool fwupdateHandleReport(uint8_t *p_data, uint8_t length)
{
  fwupdate_report_t report;

  if (length < FWUPDATE_REPORT_SIZE)
    return false;

  if (p_data[0] != FWUPDATE_REPORT_ID)
    return false;

  // USB 인터럽트 안이므로 큐에만 넣고 즉시 빠져나온다.
  // 플래시 erase/write 는 fwupdateThread() 에서 처리한다.
  memcpy(report.buf, p_data, FWUPDATE_REPORT_SIZE);
  qbufferWrite(&report_q, (uint8_t *)&report, 1);

  return true;
}

bool fwupdateIsBusy(void)
{
  return (state == FWUPDATE_STATE_ERASE || state == FWUPDATE_STATE_WRITE);
}

void fwupdateThread(void const *arg)
{
  fwupdate_report_t report;

  while(1)
  {
    while (qbufferAvailable(&report_q) > 0)
    {
      qbufferRead(&report_q, (uint8_t *)&report, 1);
      fwupdate_process(report.buf);
    }

    if (is_commit)
    {
      // 응답이 호스트에 전달될 시간을 준 뒤 리셋한다.
      delay(200);
      resetSetBootMode(1<<MODE_BIT_UPDATE);
      resetToReset();
    }

    delay(2);
  }
}

void fwupdate_process(uint8_t *p_data)
{
  switch(p_data[1])
  {
    case FWUPDATE_CMD_INFO:
      fwupdate_cmd_info();
      break;

    case FWUPDATE_CMD_BEGIN:
      fwupdate_cmd_begin(p_data);
      break;

    case FWUPDATE_CMD_DATA:
      fwupdate_cmd_data(p_data);
      break;

    case FWUPDATE_CMD_END:
      fwupdate_cmd_end(p_data);
      break;

    case FWUPDATE_CMD_COMMIT:
      fwupdate_cmd_commit();
      break;

    case FWUPDATE_CMD_STATUS:
      fwupdate_cmd_status();
      break;

    default:
      fwupdate_send_resp(p_data[1], FWUPDATE_ERR_CMD, NULL, 0);
      break;
  }
}

void fwupdate_cmd_info(void)
{
  uint8_t buf[FWUPDATE_PAYLOAD_SIZE - 1];
  uint32_t index = 0;

  memset(buf, 0, sizeof(buf));

  buf[index++] = FWUPDATE_PROTO_VER;
  buf[index++] = FWUPDATE_SLOT_MAX_CH;

  uint32_t firm_max = FLASH_SIZE_FIRM;
  uint32_t slot_max = FWUPDATE_SLOT_DATA_MAX;

  memcpy(&buf[index], &firm_max, 4);  index += 4;
  memcpy(&buf[index], &slot_max, 4);  index += 4;

  // 남은 자리에 보드 이름과 펌웨어 버전을 실어 보낸다.
  index += snprintf((char *)&buf[index], sizeof(buf)-index, "%s", KBD_NAME) + 1;
  if (index < sizeof(buf))
  {
    index += snprintf((char *)&buf[index], sizeof(buf)-index, "%s", _DEF_FIRMWATRE_VERSION);
  }

  fwupdate_send_resp(FWUPDATE_CMD_INFO, FWUPDATE_OK, buf, sizeof(buf));
}

void fwupdate_cmd_begin(uint8_t *p_data)
{
  uint8_t  target;
  uint8_t  slot;
  uint32_t size;
  uint32_t erase_addr;
  uint32_t erase_size;
  uint8_t  err = FWUPDATE_OK;


  target = p_data[2];
  slot   = p_data[3];
  memcpy(&size, &p_data[4], 4);

  do
  {
    if (target == FWUPDATE_TARGET_FIRM)
    {
      if (size == 0 || size >= FLASH_SIZE_FIRM)
      {
        err = FWUPDATE_ERR_SIZE;
        break;
      }
      tr_base = FLASH_ADDR_UPDATE + FLASH_SIZE_TAG;
    }
    else if (target == FWUPDATE_TARGET_SLOT)
    {
      if (slot >= FWUPDATE_SLOT_MAX_CH)
      {
        err = FWUPDATE_ERR_TARGET;
        break;
      }
      if (size == 0 || size > FWUPDATE_SLOT_DATA_MAX)
      {
        err = FWUPDATE_ERR_SIZE;
        break;
      }
      tr_base = FLASH_ADDR_UPDATE_SLOT + (slot * FLASH_SIZE_SLOT) + FLASH_SIZE_TAG;
    }
    else
    {
      err = FWUPDATE_ERR_TARGET;
      break;
    }

    tr_target  = target;
    tr_slot    = slot;
    tr_size    = size;
    tr_written = 0;
    page_dirty = false;
    page_addr  = 0;

    state = FWUPDATE_STATE_ERASE;

    // 태그 영역까지 포함해 한 번에 지운다. 블록 단위 on-demand erase 는
    // 순서가 뒤바뀌면 이미 쓴 데이터를 지우므로 쓰지 않는다.
    erase_addr = tr_base - FLASH_SIZE_TAG;
    erase_size = FLASH_SIZE_TAG + size;

    if (fwupdate_erase(erase_addr, erase_size) != true)
    {
      err = FWUPDATE_ERR_ERASE;
      break;
    }

    state = FWUPDATE_STATE_WRITE;
  } while(0);

  if (err != FWUPDATE_OK)
  {
    state    = FWUPDATE_STATE_ERROR;
    last_err = err;
    logPrintf("[E_] fwupdate begin : 0x%02X\n", err);
  }

  fwupdate_send_resp(FWUPDATE_CMD_BEGIN, err, NULL, 0);
}

void fwupdate_cmd_data(uint8_t *p_data)
{
  uint32_t offset;
  uint32_t length;


  if (state != FWUPDATE_STATE_WRITE)
  {
    // DATA 는 응답이 없다. 오류는 STATUS 로 확인한다.
    state    = FWUPDATE_STATE_ERROR;
    last_err = FWUPDATE_ERR_SEQ;
    return;
  }

  memcpy(&offset, &p_data[2], 4);

  if (offset >= tr_size)
    return;

  length = tr_size - offset;
  if (length > FWUPDATE_DATA_SIZE)
    length = FWUPDATE_DATA_SIZE;

  if (fwupdate_page_store(tr_base + offset, &p_data[FWUPDATE_DATA_OFFSET], length) != true)
  {
    state    = FWUPDATE_STATE_ERROR;
    last_err = FWUPDATE_ERR_WRITE;
    return;
  }

  tr_written += length;
}

void fwupdate_cmd_end(uint8_t *p_data)
{
  uint16_t host_crc;
  uint16_t flash_crc = 0;
  uint8_t  err = FWUPDATE_OK;


  memcpy(&host_crc, &p_data[2], 2);

  do
  {
    if (state != FWUPDATE_STATE_WRITE)
    {
      err = (state == FWUPDATE_STATE_ERROR) ? last_err : FWUPDATE_ERR_SEQ;
      break;
    }

    if (fwupdate_page_flush() != true)
    {
      err = FWUPDATE_ERR_WRITE;
      break;
    }

    // 기록된 플래시를 다시 읽어 검증한다. 호스트에 성공/실패를 정확히 알릴 수 있다.
    if (fwupdate_calc_crc(tr_base, tr_size, &flash_crc) != true)
    {
      err = FWUPDATE_ERR_READ;
      break;
    }

    if (flash_crc != host_crc)
    {
      logPrintf("[E_] fwupdate crc : 0x%04X != 0x%04X\n", flash_crc, host_crc);
      err = FWUPDATE_ERR_CRC;
      break;
    }

    if (fwupdate_write_tag() != true)
    {
      err = FWUPDATE_ERR_WRITE;
      break;
    }
  } while(0);

  if (err == FWUPDATE_OK)
  {
    state = FWUPDATE_STATE_DONE;
    logPrintf("[OK] fwupdate end : %d B\n", tr_size);
  }
  else
  {
    state    = FWUPDATE_STATE_ERROR;
    last_err = err;
  }

  fwupdate_send_resp(FWUPDATE_CMD_END, err, NULL, 0);
}

void fwupdate_cmd_commit(void)
{
  uint8_t err = FWUPDATE_OK;

  if (state != FWUPDATE_STATE_DONE || tr_target != FWUPDATE_TARGET_FIRM)
  {
    err = FWUPDATE_ERR_SEQ;
  }

  fwupdate_send_resp(FWUPDATE_CMD_COMMIT, err, NULL, 0);

  if (err == FWUPDATE_OK)
  {
    is_commit = true;
  }
}

void fwupdate_cmd_status(void)
{
  uint8_t buf[6];
  uint8_t percent = 0;

  if (tr_size > 0)
  {
    percent = (tr_written * 100) / tr_size;
    if (percent > 100)
      percent = 100;
  }

  buf[0] = state;
  buf[1] = percent;
  memcpy(&buf[2], &tr_written, 4);

  fwupdate_send_resp(FWUPDATE_CMD_STATUS, last_err, buf, sizeof(buf));
}

void fwupdate_cmd_slot(uint8_t *p_data)
{
  uint8_t  slot = p_data[2];
  uint8_t  buf[26];
  uint8_t  head[48];
  uint32_t slot_addr;
  uint32_t file_size = 0;
  uint16_t width  = 0;
  uint16_t height = 0;
  bool     is_used = false;


  memset(buf, 0, sizeof(buf));

  if (slot >= FWUPDATE_SLOT_MAX_CH)
  {
    fwupdate_send_resp(FWUPDATE_CMD_SLOT, FWUPDATE_ERR_TARGET, NULL, 0);
    return;
  }

  slot_addr = FLASH_ADDR_UPDATE_SLOT + (slot * FLASH_SIZE_SLOT) + FLASH_SIZE_TAG;

  // 헤더 32바이트와 원본 GIF 앞부분을 한 번에 읽는다.
  //   헤더 : [0..15] 매직, [16..19] 파일 크기
  //   GIF  : [32..34] "GIF", [38..39] 가로, [40..41] 세로 (논리 화면 크기)
  if (flashRead(slot_addr, head, sizeof(head)) == true)
  {
    memcpy(&file_size, &head[16], 4);

    if (file_size > 0 && file_size != 0xFFFFFFFF && file_size <= FWUPDATE_SLOT_DATA_MAX)
    {
      is_used = true;

      if (head[32] == 'G' && head[33] == 'I' && head[34] == 'F')
      {
        memcpy(&width,  &head[38], 2);
        memcpy(&height, &head[40], 2);
      }
    }
    else
    {
      file_size = 0;
    }
  }

  buf[0] = slot;
  buf[1] = 0;
  memcpy(&buf[2], &file_size, 4);
  memcpy(&buf[6], &width,  2);
  memcpy(&buf[8], &height, 2);

  if (is_used)
  {
    buf[1] |= FWUPDATE_SLOT_F_USED;

    if (memcmp(&head[FWUPDATE_SLOT_MAGIC_OFS], "SLT1", 4) == 0)
    {
      // 새 포맷 : 이름과 종류를 그대로 쓴다.
      buf[1] |= FWUPDATE_SLOT_F_NAMED;
      if (head[FWUPDATE_SLOT_KIND_OFS] != 0)
        buf[1] |= FWUPDATE_SLOT_F_MOVIE;

      memcpy(&buf[10], head, FWUPDATE_SLOT_NAME_SIZE);
    }
    else
    {
      // 구버전 : 이름은 없고 [0..15] 의 옛 문자열에서 종류만 되살린다.
      if (memcmp(head, "GIF_MOVIE", 9) == 0)
        buf[1] |= FWUPDATE_SLOT_F_MOVIE;
    }
  }

  fwupdate_send_resp(FWUPDATE_CMD_SLOT, FWUPDATE_OK, buf, sizeof(buf));
}

void fwupdate_cmd_rtc(uint8_t *p_data)
{
  rtc_time_t rtc_time;
  rtc_date_t rtc_date;
  uint8_t    buf[7];
  uint8_t    err = FWUPDATE_OK;


  if (p_data[2] == FWUPDATE_RTC_OP_SET)
  {
    rtc_date.year   = p_data[3];
    rtc_date.month  = p_data[4];
    rtc_date.day    = p_data[5];
    rtc_date.week   = 0;          // 날짜에서 계산되므로 값은 의미가 없다
    rtc_time.hours  = p_data[6];
    rtc_time.minutes= p_data[7];
    rtc_time.seconds= p_data[8];

    if (rtc_date.month < 1 || rtc_date.month > 12 ||
        rtc_date.day   < 1 || rtc_date.day   > 31 ||
        rtc_time.hours > 23 || rtc_time.minutes > 59 || rtc_time.seconds > 59)
    {
      fwupdate_send_resp(FWUPDATE_CMD_RTC, FWUPDATE_ERR_TARGET, NULL, 0);
      return;
    }

    if (rtcSetDate(&rtc_date) != true || rtcSetTime(&rtc_time) != true)
    {
      err = FWUPDATE_ERR_WRITE;
    }
  }

  // 읽기든 쓰기든 현재 값을 되돌려 준다. 쓰기 직후 확인에 쓸 수 있다.
  if (rtcGetTime(&rtc_time) != true || rtcGetDate(&rtc_date) != true)
  {
    fwupdate_send_resp(FWUPDATE_CMD_RTC, FWUPDATE_ERR_READ, NULL, 0);
    return;
  }

  buf[0] = rtc_date.year;
  buf[1] = rtc_date.month;
  buf[2] = rtc_date.day;
  buf[3] = rtc_date.week;
  buf[4] = rtc_time.hours;
  buf[5] = rtc_time.minutes;
  buf[6] = rtc_time.seconds;

  fwupdate_send_resp(FWUPDATE_CMD_RTC, err, buf, sizeof(buf));
}

bool fwupdate_erase(uint32_t addr, uint32_t size)
{
  uint32_t sector_s;
  uint32_t sector_e;

  sector_s = addr / FWUPDATE_SECTOR_SIZE;
  sector_e = (addr + size - 1) / FWUPDATE_SECTOR_SIZE;

  for (uint32_t i = sector_s; i <= sector_e; i++)
  {
    if (flashErase(i * FWUPDATE_SECTOR_SIZE, FWUPDATE_SECTOR_SIZE) != true)
    {
      logPrintf("[E_] flashErase(0x%X)\n", i * FWUPDATE_SECTOR_SIZE);
      return false;
    }
  }

  return true;
}

bool fwupdate_page_store(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  uint32_t index = 0;

  // QSPI 는 page program 이 0.4ms 이상 걸린다. 리포트(26B)마다 쓰면
  // 속도도 손해고 같은 페이지를 여러 번 프로그램하게 되므로 페이지 단위로 모은다.
  while (index < length)
  {
    uint32_t cur_addr = addr + index;
    uint32_t cur_page = cur_addr - (cur_addr % FWUPDATE_PAGE_SIZE);
    uint32_t cur_off  = cur_addr - cur_page;
    uint32_t cur_len  = FWUPDATE_PAGE_SIZE - cur_off;

    if (cur_len > (length - index))
      cur_len = length - index;

    if (page_dirty && page_addr != cur_page)
    {
      if (fwupdate_page_flush() != true)
        return false;
    }

    if (!page_dirty)
    {
      // 0xFF 로 채워두면 기록하지 않은 자리는 erase 상태 그대로 남는다.
      memset(page_buf, 0xFF, sizeof(page_buf));
      page_addr  = cur_page;
      page_dirty = true;
    }

    memcpy(&page_buf[cur_off], &p_data[index], cur_len);
    index += cur_len;
  }

  return true;
}

bool fwupdate_page_flush(void)
{
  bool ret = true;

  if (page_dirty)
  {
    ret = flashWrite(page_addr, page_buf, FWUPDATE_PAGE_SIZE);
    if (ret != true)
    {
      logPrintf("[E_] flashWrite(0x%X)\n", page_addr);
    }
    page_dirty = false;
  }

  return ret;
}

bool fwupdate_calc_crc(uint32_t addr, uint32_t size, uint16_t *p_crc)
{
  uint8_t  buf[256];
  uint16_t crc = 0;
  uint32_t index = 0;


  while (index < size)
  {
    uint32_t rd_len = size - index;

    if (rd_len > sizeof(buf))
      rd_len = sizeof(buf);

    if (flashRead(addr + index, buf, rd_len) != true)
      return false;

    crc = utilCalcCRC(crc, buf, rd_len);
    index += rd_len;
  }

  *p_crc = crc;
  return true;
}

bool fwupdate_write_tag(void)
{
  firm_tag_t firm_tag;
  uint16_t   crc = 0;

  if (fwupdate_calc_crc(tr_base, tr_size, &crc) != true)
    return false;

  firm_tag.magic_number = TAG_MAGIC_NUMBER;
  firm_tag.fw_addr      = FLASH_SIZE_TAG;
  firm_tag.fw_size      = tr_size;
  firm_tag.fw_crc       = crc;

  return flashWrite(tr_base - FLASH_SIZE_TAG, (uint8_t *)&firm_tag, sizeof(firm_tag_t));
}

void fwupdate_send_resp(uint8_t cmd, uint8_t err, uint8_t *p_data, uint8_t length)
{
  uint8_t buf[FWUPDATE_REPORT_SIZE];

  memset(buf, 0, sizeof(buf));

  buf[0] = FWUPDATE_REPORT_ID;
  buf[1] = cmd;
  buf[2] = err;

  if (p_data != NULL && length > 0)
  {
    if (length > (FWUPDATE_REPORT_SIZE - 3))
      length = FWUPDATE_REPORT_SIZE - 3;

    memcpy(&buf[3], p_data, length);
  }

  usbHidSendReportVia(buf, sizeof(buf));
}

MODULE_DEF(fwupdate)
{
  .name = "fwupdate",
  .priority = MODULE_PRI_LOW,
  .init = init
};
