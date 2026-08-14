/*
 * The MIT License (MIT)
 *
 * Copyright (c) Microsoft Corporation
 * Copyright (c) Ha Thach for Adafruit Industries
 * Copyright (c) Henry Gabryjelski
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include "uf2.h"
#include "usb.h"
#include "boot/boot.h"


//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
#define BPB_SECTOR_SIZE           (512)

// QSPI(W25Q128) 는 block erase 단위가 64KB 다. UF2 블록(256B)마다 지우는 것이 아니라
// 섹터 단위로 "이번 전송에서 이미 지웠는지"를 비트맵으로 관리해서 딱 한 번만 지운다.
// UF2 스펙상 호스트는 블록을 임의 순서로, 중복해서 보낼 수 있기 때문에
// 도착 순서에 의존하는 erase 는 이미 기록한 영역을 날려버릴 수 있다.
#define UF2_QSPI_ADDR             FLASH_ADDR_UPDATE
#define UF2_QSPI_SIZE             (16*1024*1024)
#define UF2_ERASE_SECTOR_SIZE     (64*1024)
#define UF2_ERASE_SECTOR_MAX      (UF2_QSPI_SIZE / UF2_ERASE_SECTOR_SIZE)

// 호스트가 남은 FAT/디렉터리 기록과 SYNCHRONIZE CACHE 를 끝낼 시간.
// 기존 300ms 로는 Windows 에서 복사 도중 장치가 사라져 오류가 나는 경우가 있다.
#define UF2_COMPLETE_WAIT_MS      (1000)
#define UF2_JUMP_WAIT_MS          (300)


static uint32_t   _flash_size;
static uint32_t   _flash_len   = 0;    // 기록된 영역의 최대 끝 오프셋
static bool       is_jump_req  = false;
static bool       is_done_req  = false;
static bool       is_slot      = false;
static uint8_t    slot_num     = 0;
static bool       is_tr_active = false;
static uint32_t   tr_family    = 0;
static uint8_t    erase_map[UF2_ERASE_SECTOR_MAX / 8];
static uf2_info_t uf2_info;


static bool uf2_flash_is_blank(uint32_t addr, uint32_t size);
static bool uf2_flash_erase_once(uint32_t flash_addr, uint32_t len);
static bool uf2_flash_calc_crc(uint32_t addr, uint32_t size, uint16_t *p_crc);
static bool uf2_flash_write(uint32_t addr, void const *data, uint32_t len);
static bool uf2_flash_write_gif(uint8_t slot, uint32_t addr, void const *data, uint32_t len);
static bool uf2_flash_flush(void);
static bool uf2_flash_flush_slot(uint8_t slot);
static void uf2_transfer_reset(WriteState *state);
static void uf2_show_error(const char *info_str);


static inline bool is_uf2_block(UF2_Block const *bl)
{
  return (bl->magicStart0 == UF2_MAGIC_START0) &&
         (bl->magicStart1 == UF2_MAGIC_START1) &&
         (bl->magicEnd == UF2_MAGIC_END) &&
         (bl->flags & UF2_FLAG_FAMILYID) &&
         !(bl->flags & UF2_FLAG_NOFLASH);
}




void uf2Init(void)
{
  _flash_size = FLASH_SIZE_FIRM;

  uf2_info.state = 0;
  uf2_info.percent = 0;

  memset(erase_map, 0, sizeof(erase_map));
}

void uf2Update(void)
{
  static uint8_t state = 0;
  static uint32_t pre_time;
  uint16_t err_code;

  switch(state)
  {
    case 0:
      if (is_done_req)
      {
        pre_time = millis();
        state = 1;
      }
      else if (is_jump_req)
      {
        pre_time = millis();
        state = 3;
      }
      break;

    case 1:
      if (millis()-pre_time >= UF2_COMPLETE_WAIT_MS)
      {
        if (is_slot)
        {
          pre_time = millis();
          state = 3;
        }
        else
        {
          // bootUpdateFirm() 은 내부 플래시 erase/write 로 수 초가 걸린다.
          // 그동안 USB 가 응답하지 못하므로 호스트가 장치 분리를 정상적으로
          // 인식하도록 먼저 연결을 끊는다.
          usbDisconnect();
          state = 2;
        }
      }
      break;

    case 2:
      err_code = bootUpdateFirm();
      logPrintf("[%s] bootUpdateFirm()\n", err_code == OK ? "OK":"E_");

      if (err_code == OK)
      {
        pre_time = millis();
        state = 3;
      }
      else
      {
        // 실패해도 벽돌이 되지는 않는다. USB 를 다시 붙여 재복사할 수 있게 한다.
        usbConnect();
        is_done_req = false;
        state = 0;
      }
      break;

    case 3:
      if (millis()-pre_time >= UF2_JUMP_WAIT_MS)
      {
        is_done_req = false;
        is_jump_req = false;
        bootJumpFirm();
        state = 0;
      }
      break;
  }
}

void uf2GetInfo(uf2_info_t *p_info)
{
  *p_info = uf2_info;
}

void uf2RequestJump(void)
{
  is_jump_req = true;
}

void uf2_flash_complete(WriteState *state)
{
  (void)state;

  if (is_done_req)
    return;

  uf2_info.state = 2;

  // 실제 펌웨어 갱신/점프는 USB 콜백이 아닌 uf2Update() 에서 처리한다.
  is_done_req = true;
}

/*------------------------------------------------------------------*/
/* Write UF2
 *------------------------------------------------------------------*/

/**
 * Write an uf2 block wrapped by 512 sector.
 * @return number of bytes processed, only 4 following values
 *  -2 : flash write failed, host should be notified (UF2_RET_ERR)
 *  -1 : if not an uf2 block (UF2_RET_NOT_UF2)
 * 512 : write is successful (BPB_SECTOR_SIZE == 512)
 *   0 : is busy with flashing, tinyusb stack will call write_block again with the same parameters later on
 */
int uf2_write_block(uint32_t block_no, uint8_t *data, WriteState *state)
{
  (void)block_no;
  UF2_Block *bl = (void *)data;
  bool is_new_block = true;
  bool ret;


  if (!is_uf2_block(bl))
    return UF2_RET_NOT_UF2;


  if (bl->familyID == BOARD_UF2_FAMILY_ID)
  {
    // 펌웨어
  }
  else if (bl->familyID >= BOARD_UF2_FAMILY_ID_SLOT_S && bl->familyID <= BOARD_UF2_FAMILY_ID_SLOT_E)
  {
    // 슬롯
  }
  else
  {
    // TODO family matches VID/PID
    logPrintf("familyID Not Match\n");
    logPrintf("  0x%X:0x%X\n", BOARD_UF2_FAMILY_ID, bl->familyID);
    return UF2_RET_NOT_UF2;
  }

  // 새 전송 판정. familyID 가 바뀌었거나 총 블록 수가 달라지면 다른 파일이다.
  // 여기서 erase 비트맵과 진행 상태를 함께 초기화해야 두 번째 파일도 정상 처리된다.
  if (is_tr_active == false ||
      bl->familyID != tr_family ||
      (bl->numBlocks > 0 && state->numBlocks > 0 && bl->numBlocks != state->numBlocks))
  {
    uf2_transfer_reset(state);
    is_tr_active = true;
    tr_family    = bl->familyID;

    if (bl->familyID == BOARD_UF2_FAMILY_ID)
    {
      is_slot = false;
    }
    else
    {
      is_slot  = true;
      slot_num = bl->familyID - BOARD_UF2_FAMILY_ID_SLOT_S;
    }
  }

  if (bl->numBlocks > 0 && bl->numBlocks < MAX_BLOCKS)
  {
    state->numBlocks = bl->numBlocks;
  }

  // 호스트가 같은 블록을 다시 보내는 경우가 있다. 다시 기록하면 길이/CRC 가
  // 두 번 반영되고 erase 까지 다시 걸릴 수 있으므로 이미 받은 블록은 건너뛴다.
  if (state->numBlocks > 0 && bl->blockNo < MAX_BLOCKS)
  {
    uint8_t const  mask = 1 << (bl->blockNo % 8);
    uint32_t const pos  = bl->blockNo / 8;

    if (state->writtenMask[pos] & mask)
    {
      is_new_block = false;
    }
  }

  if (is_new_block)
  {
    if (is_slot)
      ret = uf2_flash_write_gif(slot_num, bl->targetAddr, bl->data, bl->payloadSize);
    else
      ret = uf2_flash_write(bl->targetAddr, bl->data, bl->payloadSize);

    if (ret != true)
    {
      if (uf2_info.state != 3)
      {
        logPrintf("[E_] uf2_write_block()\n");
        logPrintf("         addr : 0x%X\n", bl->targetAddr);

        uf2_show_error(is_slot ? "ERR_BOOT_SLOT_SIZE":"ERR_BOOT_FLASH_WRITE");
      }

      uf2_info.state = 3;
      return UF2_RET_ERR;
    }
  }

  uf2_info.state = 1;

  //------------- Update written blocks -------------//
  if (state->numBlocks > 0 && bl->blockNo < MAX_BLOCKS)
  {
    uint8_t const  mask = 1 << (bl->blockNo % 8);
    uint32_t const pos  = bl->blockNo / 8;

    if (!(state->writtenMask[pos] & mask))
    {
      state->writtenMask[pos] |= mask;
      state->numWritten++;
    }

    // flush last blocks
    if (state->numWritten >= state->numBlocks)
    {
      if (is_slot)
        uf2_flash_flush_slot(slot_num);
      else
        uf2_flash_flush();
    }

    uf2_info.percent = (state->numWritten * 100)/state->numBlocks;

    lcd_req_info_t info;
    info.mode = LCD_INFO_MODE_PROGRESS;
    if (is_slot)
      info.title_str = "Copy...SLOT";
    else
      info.title_str = "Copy...FIRM";
    info.percent = uf2_info.percent;
    lcdUpdate(false, &info);
  }

  return BPB_SECTOR_SIZE;
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool uf2_flash_is_blank(uint32_t addr, uint32_t size)
{
  uint8_t buf[256];


  for (uint32_t i = 0; i < size; i += sizeof(buf))
  {
    uint32_t rd_len = size - i;

    if (rd_len > sizeof(buf))
      rd_len = sizeof(buf);

    if (flashRead(addr + i, buf, rd_len) != true)
      return false;

    for (uint32_t j = 0; j < rd_len; j++)
    {
      if (buf[j] != 0xFF)
        return false;
    }
  }
  return true;
}

bool uf2_flash_erase_once(uint32_t flash_addr, uint32_t len)
{
  uint32_t offset;
  uint32_t sector_s;
  uint32_t sector_e;


  if (flash_addr < UF2_QSPI_ADDR || (flash_addr + len) > (UF2_QSPI_ADDR + UF2_QSPI_SIZE))
  {
    return false;
  }

  offset   = flash_addr - UF2_QSPI_ADDR;
  sector_s = offset / UF2_ERASE_SECTOR_SIZE;
  sector_e = (offset + len - 1) / UF2_ERASE_SECTOR_SIZE;

  for (uint32_t i = sector_s; i <= sector_e; i++)
  {
    uint8_t  const mask = 1 << (i % 8);
    uint32_t const pos  = i / 8;
    uint32_t sector_addr;

    if (erase_map[pos] & mask)
      continue;

    erase_map[pos] |= mask;

    sector_addr = UF2_QSPI_ADDR + i * UF2_ERASE_SECTOR_SIZE;

    // 이미 지워져 있으면 건너뛴다. 64KB erase 는 150ms 이상 걸리므로
    // 새 칩/삭제 직후에는 blank 검사(수 ms)가 훨씬 싸다.
    if (uf2_flash_is_blank(sector_addr, UF2_ERASE_SECTOR_SIZE))
      continue;

    if (flashErase(sector_addr, UF2_ERASE_SECTOR_SIZE) != true)
    {
      logPrintf("[E_] flashErase(0x%X)\n", sector_addr);
      return false;
    }
  }

  return true;
}

bool uf2_flash_calc_crc(uint32_t addr, uint32_t size, uint16_t *p_crc)
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

bool uf2_flash_write(uint32_t addr, void const *data, uint32_t len)
{
  uint32_t flash_addr;


  if ((addr + len) > _flash_size)
  {
    return false;
  }

  flash_addr = FLASH_ADDR_UPDATE + FLASH_SIZE_TAG + addr;

  if (uf2_flash_erase_once(flash_addr, len) != true)
  {
    return false;
  }

  if (flashWrite(flash_addr, (uint8_t *)data, len) != true)
  {
    logPrintf("[E_] flashWrite 0x%X, %d\n", flash_addr, len);
    return false;
  }

  // 블록이 순서대로 오지 않을 수 있으므로 누적이 아니라 최대 끝 오프셋으로 관리한다.
  if ((addr + len) > _flash_len)
  {
    _flash_len = addr + len;
  }

  return true;
}

bool uf2_flash_write_gif(uint8_t slot, uint32_t addr, void const *data, uint32_t len)
{
  uint32_t flash_addr;


  // 실제 기록은 태그 영역 뒤에서 시작하므로 경계 검사에도 태그 크기를 포함해야
  // 다음 슬롯을 침범하지 않는다.
  if ((addr + len + FLASH_SIZE_TAG) > FLASH_SIZE_SLOT)
  {
    return false;
  }

  flash_addr = (FLASH_ADDR_UPDATE_SLOT + slot * FLASH_SIZE_SLOT) + FLASH_SIZE_TAG + addr;

  if (uf2_flash_erase_once(flash_addr, len) != true)
  {
    return false;
  }

  if (flashWrite(flash_addr, (uint8_t *)data, len) != true)
  {
    logPrintf("[E_] flashWrite 0x%X, %d\n", flash_addr, len);
    return false;
  }

  if ((addr + len) > _flash_len)
  {
    _flash_len = addr + len;
  }

  return true;
}

bool uf2_flash_flush(void)
{
  firm_tag_t firm_tag;
  uint16_t   crc = 0;


  // CRC 는 도착 순서/중복에 영향받지 않도록 기록이 끝난 플래시를 다시 읽어서 계산한다.
  // bootVerifyUpdate() 가 하는 계산과 동일해야 한다.
  if (uf2_flash_calc_crc(FLASH_ADDR_UPDATE + FLASH_SIZE_TAG, _flash_len, &crc) != true)
  {
    logPrintf("[E_] uf2_flash_calc_crc()\n");
    return false;
  }

  firm_tag.magic_number = TAG_MAGIC_NUMBER;
  firm_tag.fw_addr      = FLASH_SIZE_TAG;
  firm_tag.fw_size      = _flash_len;
  firm_tag.fw_crc       = crc;

  cliPrintf("\n");
  cliPrintf("fw addr : 0x%X\n", firm_tag.fw_addr);
  cliPrintf("fw size : %d B\n", firm_tag.fw_size);
  cliPrintf("fw crc  : 0x%X\n", firm_tag.fw_crc);

  if (flashWrite(FLASH_ADDR_UPDATE, (uint8_t *)&firm_tag, sizeof(firm_tag_t)) != true)
  {
    logPrintf("[E_] uf2_flash_flush()\n");
    return false;
  }

  bootVerifyUpdate();

  return true;
}

bool uf2_flash_flush_slot(uint8_t slot)
{
  firm_tag_t firm_tag;
  uint32_t   flash_addr;
  uint16_t   crc = 0;


  flash_addr = FLASH_ADDR_UPDATE_SLOT + slot * FLASH_SIZE_SLOT;

  if (uf2_flash_calc_crc(flash_addr + FLASH_SIZE_TAG, _flash_len, &crc) != true)
  {
    logPrintf("[E_] uf2_flash_calc_crc()\n");
    return false;
  }

  firm_tag.magic_number = TAG_MAGIC_NUMBER;
  firm_tag.fw_addr      = FLASH_SIZE_TAG;
  firm_tag.fw_size      = _flash_len;
  firm_tag.fw_crc       = crc;

  cliPrintf("\n");
  cliPrintf("slot    : %d\n", slot);
  cliPrintf("fw addr : 0x%X\n", firm_tag.fw_addr);
  cliPrintf("fw size : %d B\n", firm_tag.fw_size);
  cliPrintf("fw crc  : 0x%X\n", firm_tag.fw_crc);

  if (flashWrite(flash_addr, (uint8_t *)&firm_tag, sizeof(firm_tag_t)) != true)
  {
    logPrintf("[E_] uf2_flash_flush_slot()\n");
    return false;
  }

  return true;
}

void uf2_transfer_reset(WriteState *state)
{
  memset(state, 0, sizeof(WriteState));
  memset(erase_map, 0, sizeof(erase_map));

  _flash_len = 0;

  uf2_info.state   = 0;
  uf2_info.percent = 0;
}

void uf2_show_error(const char *info_str)
{
  lcd_req_info_t lcd_info;

  lcd_info.mode      = LCD_INFO_MODE_ERROR;
  lcd_info.title_str = "ERROR";
  lcd_info.info_str  = info_str;
  lcdUpdate(true, &lcd_info);
}
