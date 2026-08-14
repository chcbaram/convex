#include "uf2.h"
#include "usb.h"
#include "boot/boot.h"



static WriteState _wr_state = { 0 };



// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  (void) lun;
  (void) offset;

  uint32_t count = 0;
  while ( count < bufsize )
  {
    int ret;

    ret = uf2_write_block(lba, buffer, &_wr_state);

    // only break if write_block is busy with flashing (return 0)
    if ( ret == 0 )
      break;

    // 기록 실패는 호스트에 알려야 한다. 성공으로 응답하면 복사가 끝난 것처럼
    // 보이지만 실제로는 플래시에 아무것도 남지 않는다.
    if ( ret == UF2_RET_ERR )
      return -1;

    // Consider non-uf2 block write as successful (FAT/directory 기록)
    lba++;
    buffer += 512;
    count  += 512;
  }

  return count;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
  (void) lun;

  *block_count = CFG_UF2_NUM_BLOCKS;
  *block_size  = 512;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  (void) lun;
  (void) power_condition;

  if ( load_eject )
  {
    if (start)
    {
      // load disk storage
      logPrintf("load\n");
    }else
    {
      // unload disk storage
      // 여기서 바로 점프하면 이 명령의 CSW 를 호스트에 보내기 전에 USB 가 죽어
      // Windows 에서 "장치 제거 오류"가 뜬다. 요청만 남기고 uf2Update() 에서 처리한다.
      logPrintf("ejected\n");
      uf2RequestJump();
    }
  }

  return true;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
void tud_msc_write10_complete_cb(uint8_t lun)
{
  (void)lun;


  // abort the DFU, uf2 block failed integrity check
  if (_wr_state.aborted)
  {
    logPrintf("aboart\n");
  }
  else if (_wr_state.numBlocks)
  {
    // All block of uf2 file is complete --> complete DFU process
    if (_wr_state.numWritten >= _wr_state.numBlocks)
    {
      uf2_flash_complete(&_wr_state);
    }
  }
}
