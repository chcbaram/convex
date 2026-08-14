#ifndef FWUPDATE_H_
#define FWUPDATE_H_

#include "ap_def.h"

#ifdef __cplusplus
extern "C" {
#endif


//--------------------------------------------------------------------+
// 웹페이지(WebHID) -> 앱 펌웨어/슬롯 업데이트 프로토콜
//
// VIA raw HID(usage page 0xFF60, 32바이트 리포트)를 그대로 재사용한다.
// VIA 명령 ID 는 0x15 까지와 0xFF 만 쓰므로 0xC0 은 충돌하지 않는다.
//
//   리포트 구조 (32바이트)
//     [0]      FWUPDATE_REPORT_ID (0xC0)
//     [1]      cmd
//     [2..31]  payload (30바이트)
//
// DATA 는 절대 오프셋을 실어 보내므로 순서/중복에 영향받지 않는다.
// (부트로더 UF2 경로에서 문제가 됐던 부분을 처음부터 배제한다)
//--------------------------------------------------------------------+
#define FWUPDATE_REPORT_ID        0xC0
#define FWUPDATE_REPORT_SIZE      32
#define FWUPDATE_HEAD_SIZE        2
#define FWUPDATE_PAYLOAD_SIZE     (FWUPDATE_REPORT_SIZE - FWUPDATE_HEAD_SIZE)

// DATA : [2..5] offset(u32 LE), [6..31] data
#define FWUPDATE_DATA_OFFSET      6
#define FWUPDATE_DATA_SIZE        (FWUPDATE_REPORT_SIZE - FWUPDATE_DATA_OFFSET)

// 프로토콜 버전. 웹페이지가 INFO 응답으로 지원 여부를 판별한다.
#define FWUPDATE_PROTO_VER        1


typedef enum
{
  FWUPDATE_CMD_INFO   = 0x01,   // 지원 여부/버전 조회
  FWUPDATE_CMD_BEGIN  = 0x02,   // target, slot, total_size -> 대상 영역 erase
  FWUPDATE_CMD_DATA   = 0x03,   // offset + 26바이트 (응답 없음)
  FWUPDATE_CMD_END    = 0x04,   // crc16 -> flash 재독 검증 + 태그 기록
  FWUPDATE_CMD_COMMIT = 0x05,   // (FIRM) 업데이트 모드로 리셋
  FWUPDATE_CMD_STATUS = 0x06,   // 진행 상태 조회
  FWUPDATE_CMD_SLOT   = 0x07,   // 슬롯 하나의 현재 상태 조회
} FwUpdateCmd_t;

typedef enum
{
  FWUPDATE_TARGET_FIRM = 0,
  FWUPDATE_TARGET_SLOT = 1,
} FwUpdateTarget_t;

typedef enum
{
  FWUPDATE_STATE_IDLE = 0,
  FWUPDATE_STATE_ERASE,
  FWUPDATE_STATE_WRITE,
  FWUPDATE_STATE_DONE,
  FWUPDATE_STATE_ERROR,
} FwUpdateState_t;

typedef enum
{
  FWUPDATE_OK          = 0x00,
  FWUPDATE_ERR_CMD     = 0x01,   // 알 수 없는 명령
  FWUPDATE_ERR_SEQ     = 0x02,   // BEGIN 없이 DATA/END
  FWUPDATE_ERR_TARGET  = 0x03,   // 잘못된 target/slot
  FWUPDATE_ERR_SIZE    = 0x04,   // 대상 영역보다 큼
  FWUPDATE_ERR_ERASE   = 0x05,
  FWUPDATE_ERR_WRITE   = 0x06,
  FWUPDATE_ERR_READ    = 0x07,
  FWUPDATE_ERR_CRC     = 0x08,   // 기록된 내용의 CRC 불일치
  FWUPDATE_ERR_BUSY    = 0x09,
} FwUpdateErr_t;


// SLOT 응답 배치 (리포트 [3] 부터)
//   [3]      슬롯 번호
//   [4]      사용 여부 (0/1)
//   [5..8]   파일 크기 u32
//   [9..10]  GIF 가로 u16
//   [11..12] GIF 세로 u16
//   [13..28] 매직 문자열 16바이트 (GIF_IMAGE / GIF_MOVIE)
#define FWUPDATE_SLOT_MAGIC_SIZE  16


// USB(ISR) 에서 호출된다. 리포트를 큐에 넣기만 하고 즉시 반환한다.
// 우리 리포트면 true 를 반환한다. (VIA 자동 에코를 막기 위함)
bool fwupdateHandleReport(uint8_t *p_data, uint8_t length);

// 업데이트 진행 중인지. QSPI 를 함께 쓰는 GIF 재생 쪽에서 확인한다.
bool fwupdateIsBusy(void);


#ifdef __cplusplus
}
#endif

#endif
