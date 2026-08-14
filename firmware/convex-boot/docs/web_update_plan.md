# 웹페이지 → 앱 경유 펌웨어 업데이트 / SLOT 저장 검토

질문: "앱에서 웹페이지를 통해 펌웨어 업데이트와 SLOT 데이터 저장을 지원하고 싶다.
부트로더가 아니라 앱에만 추가하면 될 것 같은데 맞나?"

**결론: 맞다. 부트로더 수정 없이 앱(`convex-qmk`) + 웹페이지만으로 구현 가능하다.**
그리고 이 방식은 이미 출하된 보드에도 그대로 적용된다 — 부트로더의 MSC/UF2 문제를
전부 우회하기 때문에, 출하품 대응의 실질적인 해법이다.

구현은 이 브랜치가 아니라 별도 브랜치에서 진행한다.

---

## 1. 왜 부트로더 수정이 필요 없는가

부트로더가 이미 "QSPI 업데이트 영역을 읽어 내부 플래시에 적용"하는 경로를 갖고 있고,
그 트리거가 RTC 백업 레지스터에 있기 때문이다.

`convex-boot/src/ap/ap.c` `bootUp()`:

```c
if (boot_param & (1<<MODE_BIT_UPDATE))
{
  boot_param &= ~(1<<MODE_BIT_UPDATE);
  resetSetBootMode(boot_param);
  is_run_fw    = true;
  is_update_fw = true;
}
...
if (is_update_fw)
{
  err_code = bootUpdateFirm();     // CRC 검증 -> 내부 플래시 erase/write -> 재검증
}
```

즉 앱이 할 일은 **QSPI 업데이트 영역 채우기 + 태그 쓰기 + `MODE_BIT_UPDATE` 세팅 후 리셋**이 전부다.
MSC·UF2·FAT·파일 복사는 전혀 개입하지 않는다.

앱 쪽 부품도 이미 다 있다:

| 필요한 것 | 이미 있는 것 | 위치 |
|---|---|---|
| QSPI 업데이트 영역 기록 + 태그 생성 | `loaderDownToFlash()` (UART/YMODEM 판) | `convex-qmk/src/hw/driver/loader.c` |
| 업데이트 모드로 리셋 | `resetSetBootMode(1<<MODE_BIT_UPDATE)` + `resetToReset()` | `convex-qmk/src/hw/driver/reset.c:163` |
| 슬롯 영역 읽기/재생 | `gif_app.c` + LVGL FS 드라이버 | `convex-qmk/.../app/gif/gif_app.c` |
| 플래시 추상화 | `flashErase/flashWrite/flashRead` (QSPI 자동 라우팅) | `convex-qmk/src/hw/driver/flash.c` |

바꿔야 할 것은 **전송 계층뿐**이다. YMODEM/UART → 웹 브라우저.

---

## 2. 동작 흐름

### 2-1. 펌웨어 업데이트

```
웹페이지                    앱(convex-qmk)                     부트로더
   |                            |                                |
   |-- BEGIN(FIRM, size) ------>| QSPI 업데이트 영역 erase        |
   |<-- ACK ---------------------|                                |
   |-- DATA(offset, payload) -->| flashWrite(0x90000400+offset)   |
   |        ... 반복 ...         |                                |
   |-- END(crc) --------------->| flash 재독 -> CRC 검증          |
   |                            | firm_tag_t 를 0x90000000 에 기록 |
   |<-- RESULT(ok/err) ----------|                                |
   |-- COMMIT ----------------->| resetSetBootMode(MODE_BIT_UPDATE)|
   |                            | resetToReset() ---------------->| bootUpdateFirm()
   |                            |                                 | CRC 검증
   |                            |                                 | 내부 플래시 복사
   |                            |                                 | bootJumpFirm()
```

**안전성**: 앱은 QSPI(외부 플래시)에만 쓴다. 실행 중인 펌웨어는 내부 플래시에 그대로 있으므로
전송이 중간에 끊겨도 벽돌이 되지 않는다. 부트로더도 `bootUpdateFirm()` 에서 CRC 를 먼저 검증하고,
실패하면 기존 펌웨어로 그냥 부팅한다.

### 2-2. SLOT 데이터 저장

부트로더가 아예 개입하지 않으므로 더 간단하다.

```
웹 -- BEGIN(SLOT n, size) --> 앱: 슬롯 전체(2MB) erase
    -- DATA(offset, ...) ---> 앱: flashWrite(slot_base + 0x400 + offset)
    -- END(crc) ------------> 앱: flash 재독 CRC 검증 -> 결과 회신
```

리셋도 필요 없다. `gif_app` 의 `slot_run` 을 `SLOT_MAX_CH` 로 되돌리면 다음 진입 때 다시 읽는다.

헤더 규약은 지금 `index.html` 이 만드는 것과 동일하게 유지한다
(0..15 매직 문자열, 16..19 파일 크기 LE, 32부터 원본). 기존 UF2 경로와 호환된다.

---

## 3. 전송 계층 선택 (Windows / macOS / Linux 모두 지원해야 함)

이 보드는 이미 **raw HID(VIA)** 와 **CDC** 인터페이스를 모두 노출한다
(`convex-qmk/src/hw/driver/usb/usb_cmp/usbd_cmp.c`, `hw_def.h` 의 `_USE_HW_CDC`).
새 USB 인터페이스를 추가하지 않아도 된다.

| 방식 | OS 지원 | 브라우저 | 드라이버/권한 | 속도 |
|---|---|---|---|---|
| **WebHID** (raw HID, usage page 0xFF60) | Win / macOS / Linux 모두 | Chrome·Edge (Safari·Firefox ✗) | Win·macOS 무설정. **Linux 는 udev 규칙 필요** | report 64B 기준 full-speed 1ms → 실효 수십 KB/s |
| **WebSerial** (CDC) | Win / macOS / Linux 모두 | Chrome·Edge (Safari·Firefox ✗) | Win10+ 자동(usbser). macOS 무설정. **Linux 는 dialout 그룹 또는 udev** | bulk 64B×여러 패킷 → 수백 KB/s |
| WebUSB (vendor 인터페이스 신설) | Win 은 WinUSB 바인딩 필요(WCID 디스크립터) | Chrome·Edge | Linux udev 필요 | 가장 빠름 |

### 권장

**WebHID 를 기본, WebSerial 을 대용량용으로** 두는 조합을 권한다.

- WebHID 는 VIA 가 정확히 같은 인터페이스로 세 OS에서 동작하는 것이 검증돼 있고,
  Linux udev 규칙도 키보드 커뮤니티에서 이미 표준화된 형태가 있다.
- 다만 raw HID 는 report 크기가 작아 1.7MB 펌웨어 전송에 수십 초가 걸린다.
  펌웨어와 대용량 GIF 는 CDC(WebSerial)가 현실적이다.
- WebUSB 는 Windows 에서 WinUSB 바인딩을 위해 WCID(MS OS 2.0) 디스크립터를 추가해야 해서
  "세 OS 무설정" 목표에 오히려 불리하다. 권하지 않는다.

### 반드시 챙길 것

1. **HTTPS 필수** — WebHID/WebSerial 은 secure context 에서만 동작한다.
   지금처럼 GitHub Pages 로 서비스하면 충족된다. `file://` 로 열면 동작하지 않는다.
2. **Safari / Firefox 미지원** — 웹페이지에서 `navigator.hid`/`navigator.serial` 존재 여부를
   먼저 검사하고, 없으면 "Chrome 또는 Edge 를 사용하세요 + 기존 UF2 다운로드" 대체 경로를 안내한다.
   기존 UF2 생성기를 지우지 말고 fallback 으로 남겨둘 것.
3. **Linux 권한 안내** — udev 규칙(`/etc/udev/rules.d/`) 예시를 페이지에 함께 제공한다.
   이게 없으면 Linux 에서 장치가 목록에 뜨지 않는다. 세 OS 지원에서 가장 흔한 실패 지점이다.
4. **사용자 제스처 필요** — `requestDevice()` 는 클릭 등 사용자 조작 안에서만 호출 가능하다.

---

## 4. 프로토콜 설계 권고

부트로더 UF2 경로에서 문제가 됐던 것들을 처음부터 배제하는 형태로 만든다.

| 규칙 | 이유 |
|---|---|
| 모든 DATA 패킷에 **절대 오프셋**을 넣는다 | 순서/중복에 무관해진다. UF2 경로의 B-1~B-3 이 전부 이 문제였다 |
| erase 는 **BEGIN 에서 대상 영역 전체를 한 번에** | 블록 단위 on-demand erase 는 순서가 뒤바뀌면 기록한 데이터를 지운다 |
| END 에서 **기기가 flash 를 다시 읽어 CRC 검증** 후 결과 회신 | MSC 로는 불가능했던 "성공/실패를 사용자에게 알리기"가 된다 |
| 모든 명령에 응답(ACK/NAK + 상태 코드) | 실패를 성공으로 보고하던 A-4 문제가 구조적으로 사라진다 |
| 진행률을 LCD 에 표시 | 기존 `lcd_req_info_t` / `LCD_INFO_MODE_PROGRESS` 재사용 |

명령 예시:

```
BEGIN  : target(FIRM|SLOT), slot_no, total_size          -> ACK / ERR_SIZE, ERR_BUSY
DATA   : offset(u32), len(u16), payload                  -> ACK / ERR_WRITE
END    : crc16(u16)                                      -> ACK / ERR_CRC
COMMIT : (FIRM 일 때만) 업데이트 모드로 리셋              -> (응답 후 리셋)
STATUS : 진행 상태 조회                                   -> state, percent, err_code
```

---

## 5. 앱 구현 시 주의할 점

1. **QSPI XIP 모드** — `qspiRead/Write/Erase` 에 `assert(qspiGetXipMode() == false)` 가 있다.
   GIF 재생 경로와 기록 경로가 QSPI 를 동시에 건드리지 않도록 락 또는 상태 배타 처리가 필요하다.
2. **erase 블로킹** — 64KB erase 가 150ms 이상 걸린다. 슬롯 2MB 전체면 32회 = 약 5초.
   `_USE_HW_RTOS` 가 켜져 있으므로 별도 스레드에서 진행하고, 키 스캔/USB/LCD 는 계속 돌게 한다.
   한 번에 다 지우지 말고 청크로 나눠 진행률을 갱신하는 편이 사용자 경험상 낫다.
3. **런타임 동적 할당 금지** — 고정 크기 수신 버퍼를 쓴다.
4. **기존 UF2 경로와 공존** — 부트로더 MSC 경로는 그대로 남는다.
   웹 방식이 안정화되면 사용자 안내에서 UF2 드래그 방식을 보조 수단으로 내리는 것을 권한다.

---

## 6. 이 방식으로도 해결되지 않는 것

- **부트로더 자체 업데이트는 여전히 불가.** `loaderDownToFlash()` 가 임의 주소를 받으므로
  앱에 `FLASH_ADDR_BOOT` 대상 명령을 추가하는 것은 기술적으로 가능하지만,
  기록 도중 전원이 끊기면 복구 수단이 없다(벽돌). 안전하게 하려면 2단계 부트로더 구조가 필요하다.
  현 시점에서는 권하지 않는다.
- 부트로더 모드로 진입해 UF2 를 드래그하는 기존 경로의 버그는 출하품에 그대로 남는다.
  → `uf2_msc_review.md` 의 C 절 참고. 웹 방식을 기본 경로로 안내해 회피한다.
