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
// 2 : SHOW(슬롯 화면 전환) / READ(슬롯 내용 읽기) 추가
#define FWUPDATE_PROTO_VER        2


typedef enum
{
  FWUPDATE_CMD_INFO   = 0x01,   // 지원 여부/버전 조회
  FWUPDATE_CMD_BEGIN  = 0x02,   // target, slot, total_size -> 대상 영역 erase
  FWUPDATE_CMD_DATA   = 0x03,   // offset + 26바이트 (응답 없음)
  FWUPDATE_CMD_END    = 0x04,   // crc16 -> flash 재독 검증 + 태그 기록
  FWUPDATE_CMD_COMMIT = 0x05,   // (FIRM) 업데이트 모드로 리셋
  FWUPDATE_CMD_STATUS = 0x06,   // 진행 상태 조회
  FWUPDATE_CMD_SLOT   = 0x07,   // 슬롯 하나의 현재 상태 조회
  FWUPDATE_CMD_RTC    = 0x08,   // RTC 읽기/쓰기
  FWUPDATE_CMD_SHOW   = 0x09,   // 지정한 슬롯을 기기 화면에 띄운다
  FWUPDATE_CMD_READ   = 0x0A,   // 슬롯에 저장된 GIF 를 읽어 온다
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


// 슬롯 데이터 앞 32바이트 헤더 배치
//   [0..15]  이름 (UTF-8, NUL 패딩)      <- 구버전에는 "GIF_IMAGE"/"GIF_MOVIE"
//   [16..19] 파일 크기 u32               <- gif_app.c 가 읽는다. 위치 고정
//   [20..23] 포맷 매직 "SLT1"            <- 있으면 이름 필드를 신뢰한다
//   [24]     종류 (0=이미지, 1=동영상)
//   [25..31] 예약
//   [32..]   원본 GIF                    <- gif_app.c 가 읽는다. 위치 고정
//
// 구버전 데이터는 [20..23] 이 0 이라 매직이 맞지 않는다. 그때는 이름을
// 알 수 없는 것으로 보고, 종류만 [0..15] 의 옛 문자열에서 되살린다.
#define FWUPDATE_SLOT_NAME_SIZE   16
#define FWUPDATE_SLOT_MAGIC_OFS   20
#define FWUPDATE_SLOT_KIND_OFS    24

// SLOT 응답 배치 (리포트 [3] 부터)
//   [3]      슬롯 번호
//   [4]      플래그 : bit0 사용중, bit1 동영상, bit2 이름 있음
//   [5..8]   파일 크기 u32
//   [9..10]  GIF 가로 u16
//   [11..12] GIF 세로 u16
//   [13..28] 이름 16바이트
#define FWUPDATE_SLOT_F_USED      (1<<0)
#define FWUPDATE_SLOT_F_MOVIE     (1<<1)
#define FWUPDATE_SLOT_F_NAMED     (1<<2)

// SHOW 요청 : [2] 슬롯 번호. 슬롯 앱으로 전환하고 그 슬롯을 다시 읽어 띄운다.
//             방금 전송한 GIF 를 바로 확인하는 데도 쓴다.

// READ 요청 : [2] 슬롯 번호, [3..6] 오프셋 u32 (헤더 32바이트를 뺀 GIF 기준)
// READ 응답 : [3] 슬롯, [4..7] 오프셋, [8] 이번에 실은 바이트 수, [9..31] 데이터
//   호스트가 이어붙여 원본 GIF 를 그대로 되살린다. 슬롯 이름만 바꾸는 것도
//   NOR 플래시라 헤더만 고쳐 쓸 수 없어, 읽어서 다시 보내는 식으로 한다.
#define FWUPDATE_READ_DATA_OFS    9
#define FWUPDATE_READ_DATA_SIZE   (FWUPDATE_REPORT_SIZE - FWUPDATE_READ_DATA_OFS)

// RTC 요청 : [2] 동작(0=읽기, 1=쓰기), 쓰기면 [3..8] 에 연월일시분초
// RTC 응답 : [3..9] 연, 월, 일, 요일(0=일), 시, 분, 초
//   연도는 2000 년 기준 2자리다. 요일은 장치가 날짜에서 계산하므로 보내지 않는다.
#define FWUPDATE_RTC_OP_GET       0
#define FWUPDATE_RTC_OP_SET       1


// USB(ISR) 에서 호출된다. 리포트를 큐에 넣기만 하고 즉시 반환한다.
// 우리 리포트면 true 를 반환한다. (VIA 자동 에코를 막기 위함)
bool fwupdateHandleReport(uint8_t *p_data, uint8_t length);

// 업데이트 진행 중인지. QSPI 를 함께 쓰는 GIF 재생 쪽에서 확인한다.
bool fwupdateIsBusy(void);


#ifdef __cplusplus
}
#endif

#endif
