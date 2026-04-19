/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
 *
 */

#include "usb.h"
#include "flash.h"

#if CFG_TUD_MSC

// whether host does safe-eject
static bool ejected = false;

// Some MCU doesn't have enough 8KB SRAM to store the whole disk
// We will use Flash as read-only disk with board that has
// CFG_EXAMPLE_MSC_READONLY defined

#define README_CONTENTS \
"This is BARAM BOOT.\r\n\r\n" \
"\r\n" \
"NAME : " _DEF_BOARD_NAME "\r\n" \
"VER  : " _DEF_FIRMWATRE_VERSION "\r\n" 


static uint8_t readme_txt[512] = {0, };

#define RAM_BLOCK_NUM    (16)
#define DISK_BLOCK_NUM   32768  // 16MB (32768 * 512 = 16.7MB)
#define DISK_BLOCK_SIZE  512

static uint8_t msc_disk[RAM_BLOCK_NUM][DISK_BLOCK_SIZE] =
{
  //------------- Block0: Boot Sector -------------//
  // FAT magic code at offset 510-511
  {    
    0xEB, 0x3C, 0x90, 
    'M', 'S', 'D', 'O', 'S', '5', '.', '0',
    0x00, 0x02,       // Bytes per sector (512)
    0x01,             // Sectors per cluster (1)
    0x01, 0x00,       // Reserved sectors (1)
    0x02,             // FAT count (2)
    0x00, 0x02,       // Root entries (512개 -> 32섹터)
    0x00, 0x00,       // Small sectors (0)
    0xF8,             // Media descriptor
    
    // 16MB (32768 클러스터) 관리용 FAT 크기: 32768 * 2바이트 / 512 = 128섹터
    0x80, 0x00,       // [22-23] Sectors per FAT (128개 = 0x80)
    
    0x01, 0x00,       // Sectors per track
    0x01, 0x00,       // Heads
    0x00, 0x00, 0x00, 0x00, 
    
    // [32-35] Total sectors (32768 = 0x00008000)
    0x00, 0x80, 0x00, 0x00, 
    
    0x80, 0x00, 0x29, 
    0x99, 0x88, 0x77, 0x66, // Serial Number (강제 갱신용 새 번호)
    'B', 'A', 'R', 'A', 'M', ' ', 'B', 'O', 'O', 'T', ' ',
    'F', 'A', 'T', '1', '6', ' ', ' ', ' ',
    
    
    [510] = 0x55, [511] = 0xAA
  },

  //------------- Block1: FAT16 Table (Initial) -------------//
  [1] = 
  { 
    0xF8, 0xFF, // Media descriptor
    0xFF, 0xFF, // EOC (End of Cluster chain)
    0xFF, 0xFF  // Cluster 2 (README.TXT) - EOC
  },


  //------------- Block2: Root Directory -------------//
  [RAM_BLOCK_NUM-1]
  {
      // first entry is volume label
      'B' , 'A' , 'R' , 'A' , 'M' , ' ' , 'B' , 'O' , 'O' , 'T' , ' ' , 0x08, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // second entry is readme file
      'R' , 'E' , 'A' , 'D' , 'M' , 'E' , ' ' , ' ' , 'T' , 'X' , 'T' , 0x20, 0x00, 0xC6, 0x52, 0x6D,
      0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x02, 0x00,
      // sizeof(README_CONTENTS)-1, 0x00, 0x00, 0x00 // readme's files size (4 Bytes)
      114, 0x00, 0x00, 0x00 // readme's files size (4 Bytes)
  },

  // //------------- Block3: Readme Content -------------//
  // README_CONTENTS
};

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  (void) lun;

  const char vid[] = "BARAM";
  const char pid[] = "Mass Storage";
  const char rev[] = "1.0";

  memcpy(vendor_id  , vid, strlen(vid));
  memcpy(product_id , pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void) lun;

  // RAM disk is ready until ejected
  if (ejected) {
    // Additional Sense 3A-00 is NOT_FOUND
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }

  return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
__attribute__((weak)) void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
  (void) lun;

  *block_count = DISK_BLOCK_NUM;
  *block_size  = DISK_BLOCK_SIZE;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
__attribute__((weak)) bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  (void) lun;
  (void) power_condition;

  if ( load_eject )
  {
    if (start)
    {
      // load disk storage
    }else
    {
      // unload disk storage
      ejected = true;
    }
  }

  return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
__attribute__((weak)) int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  (void) lun;

  // 기본적으로 버퍼를 0으로 초기화 (데이터가 없는 영역 대응)
  memset(buffer, 0, bufsize);

  if (lba == 0) 
  {
    memcpy(buffer, msc_disk[0] + offset, bufsize);
  }
  // FAT Table 1 (LBA 1 ~ 128)
  else if (lba >= 1 && lba <= 128) 
  {
    if (lba == 1) memcpy(buffer, msc_disk[1] + offset, bufsize);
  }
  // FAT Table 2 (LBA 129 ~ 256)
  else if (lba >= 129 && lba <= 256) 
  {
    if (lba == 129) memcpy(buffer, msc_disk[1] + offset, bufsize);
  }
  // Root Directory (LBA 257 ~ 288)
  else if (lba >= 257 && lba <= 288) 
  {
    if (lba == 257) memcpy(buffer, msc_disk[RAM_BLOCK_NUM - 1] + offset, bufsize);
  }
  // README File (Cluster 2 -> LBA 289)
  else if (lba == 289)
  {
    int len;
    int index = 0;
    firm_ver_t tag_ver;
    firm_ver_t *p_tag = &tag_ver;
    
    // Readme 텍스트 버퍼 초기화
    memset(readme_txt, 0, sizeof(readme_txt));

    flashRead(FLASH_ADDR_FIRM + FLASH_SIZE_TAG + FLASH_SIZE_VEC, (uint8_t *)p_tag, sizeof(firm_ver_t));

    len = snprintf((char *)&readme_txt[index], sizeof(readme_txt), "This is BARAM BOOT.\r\n\r\n\r\n");
    index += len;
    len = snprintf((char *)&readme_txt[index], sizeof(readme_txt), "NAME : %s\r\n", _DEF_BOARD_NAME);
    index += len;
    len = snprintf((char *)&readme_txt[index], sizeof(readme_txt), "VER  : %s\r\n", _DEF_FIRMWATRE_VERSION);
    index += len;
    len = snprintf((char *)&readme_txt[index], sizeof(readme_txt), "\r\n\r\n");
    index += len;

    if (p_tag->magic_number == VERSION_MAGIC_NUMBER)
    {
      len = snprintf((char *)&readme_txt[index], sizeof(readme_txt), "NAME : %s\r\n", p_tag->name_str);
      index += len;
      len = snprintf((char *)&readme_txt[index], sizeof(readme_txt), "VER  : %s\r\n", p_tag->version_str);
    }
    else
    {
      len = snprintf((char *)&readme_txt[index], sizeof(readme_txt), "No Firmware\r\n");
    }
    index += len;

    // 전송 (offset 고려)
    if (offset < sizeof(readme_txt)) {
      uint32_t available = sizeof(readme_txt) - offset;
      uint32_t copy_size = (bufsize < available) ? bufsize : available;
      memcpy(buffer, readme_txt + offset, copy_size);
    }
  }
  // 5. 그 외의 영역 (8MB 범위 내)
  else if (lba < DISK_BLOCK_NUM)
  {
    // 이미 위에서 0으로 초기화했으므로 그대로 반환
  }

  return (int32_t) bufsize;
}

bool tud_msc_is_writable_cb (uint8_t lun)
{
  (void) lun;

#ifdef CFG_EXAMPLE_MSC_READONLY
  return false;
#else
  return true;
#endif
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
__attribute__((weak)) int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  (void) lun;

#if 0
  // out of ramdisk
  if ( lba >= DISK_BLOCK_NUM ) return -1;

#ifndef CFG_EXAMPLE_MSC_READONLY
  uint8_t* addr = msc_disk[lba] + offset;
  memcpy(addr, buffer, bufsize);
#else
  (void) lba; (void) offset; (void) buffer;
#endif
#endif

  return (int32_t) bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  // read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0])
  {
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
    break;
  }

  // return resplen must not larger than bufsize
  if ( resplen > bufsize ) resplen = bufsize;

  if ( response && (resplen > 0) )
  {
    if(in_xfer)
    {
      memcpy(buffer, response, (size_t) resplen);
    }else
    {
      // SCSI output
    }
  }

  return (int32_t) resplen;
}

#endif
