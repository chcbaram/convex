# UF2 부트로더 MSC / 파일시스템 검토

대상: `convex-boot` (TinyUSB MSC + UF2), `convex-qmk` GIF 슬롯 재생, 루트 `index.html` UF2 생성기

---

## 0. 결론 요약

| 구분 | 판정 |
|---|---|
| FAT16 디스크 이미지(BPB/FAT/루트 디렉터리) 기하 | **정상**. LBA 배치와 read10 응답이 일치 |
| Windows 파일 전송 실패 가능성 | **있음**. SCSI 미처리 명령 STALL, eject 시 CSW 미응답, 300ms 후 강제 점프 |
| UF2 → flash 기록 로직 | **취약**. 블록 도착 순서/중복 재전송에 대한 방어가 없음 |
| SLOT 2~4 전용 버그 | **없음**(주소·범위 계산은 슬롯 1~4 모두 정상). 다만 아래 B-4 / C-2 때문에 "재기록되는 슬롯"에서 실패 확률이 올라감 |

---

## A. Windows에서 파일 전송이 안 될 수 있는 원인

### A-1. SYNCHRONIZE CACHE(0x35)에 STALL 응답 — 우선순위 높음
`src/hw/driver/usb/usb_msc.c:288-307`, `.../tinyusb/class/msc/msc_device.c:847`

TinyUSB가 내부 처리하는 SCSI 명령은 TEST_UNIT_READY / START_STOP_UNIT /
PREVENT_ALLOW_MEDIUM_REMOVAL / READ_CAPACITY_10 / READ_FORMAT_CAPACITY /
INQUIRY / MODE_SENSE_6 / REQUEST_SENSE 뿐이다. 그 외는 `tud_msc_scsi_cb()`로 내려오는데
현재 구현은 `default:` 하나뿐이라 **무조건 ILLEGAL REQUEST + `-1`** 을 반환한다
(→ `fail_scsi_op()` → CSW FAILED + 엔드포인트 STALL).

Windows는 볼륨 flush / 안전 제거 시 `SYNCHRONIZE CACHE(10) = 0x35`를 보낼 수 있고,
`MODE SELECT(6) = 0x15`, `MODE SENSE(10) = 0x5A`도 보낼 수 있다.
이때 STALL이 나가면 "지연된 쓰기 실패" 계열 오류나 복사 실패로 이어질 수 있다.

**수정안**
```c
switch (scsi_cmd[0])
{
  case SCSI_CMD_SYNCHRONIZE_CACHE_10: // 0x35
    resplen = 0;                      // 캐시가 없으므로 그냥 성공
    break;

  default:
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    resplen = -1;
    break;
}
```

### A-2. eject 시 CSW를 보내기 전에 펌웨어로 점프
`src/ap/modules/uf2/uf2_msc.c:46-66`

```c
else { logPrintf("ejected\n"); bootJumpFirm(); }   // ← 콜백 안에서 바로 점프
```
`bootJumpFirm()`은 `bspDeInit()` 후 되돌아오지 않는다. 즉 START_STOP_UNIT의
**상태(CSW)가 호스트로 전송되기 전에** USB가 죽는다 → Windows "장치를 제거하는 중 오류".

**수정안**: 콜백에서는 플래그만 세우고 `uf2Update()`에서 수백 ms 뒤 점프
(기존 `is_jump_fw` 경로 재사용).

### A-3. 마지막 블록 후 300ms 만에 점프 / 콜백 안에서 수 초짜리 작업
`src/ap/modules/uf2/uf2.c:236-260`, `uf2_msc.c:69-87`

- Windows는 데이터 섹터를 다 쓴 뒤에도 FAT/디렉터리 섹터를 더 쓰고 SYNC까지 보낸다.
  300ms는 짧아서 그 사이 연결이 끊기면 탐색기가 복사 오류를 띄운다.
- 펌웨어 경로는 `tud_msc_write10_complete_cb()` → `uf2_flash_complete()` → `bootUpdateFirm()`으로
  내부 플래시 1.75MB erase + write + verify를 **콜백 안에서** 수행한다. 그동안 `tud_task()`가
  돌지 않아 수 초간 USB 무응답 → Windows 타임아웃/장치 사라짐.

**수정안**: complete 콜백에서는 요청 플래그만 세우고 실제 업데이트는 메인 루프에서 수행.

### A-4. 기록 실패를 호스트에 성공으로 보고
`src/ap/modules/uf2/uf2_msc.c:19-30`

```c
while (count < bufsize) {
  if (0 == uf2_write_block(lba, buffer, &_wr_state)) break;   // -1은 break 안 함
  lba++; buffer += 512; count += 512;
}
return count;   // 항상 bufsize
```
`uf2_write_block()`이 `-1`(슬롯 크기 초과, erase 실패 등)을 반환해도 그대로 진행하고
`bufsize`를 반환하므로 Windows에는 100% 성공으로 보인다. 사용자는 LCD 에러로만 알 수 있다.

**수정안**: UF2 블록이 아닌 경우(-1)와 처리 실패(-2 등)를 구분해서, 실패 시 `-1`을 반환해
CSW FAILED가 나가도록 한다.

### A-5. 디스크 이미지 / 디스크립터 사소한 문제

- `usb_msc.c:104` README.TXT 디렉터리 엔트리 크기가 **114 하드코딩**인데
  `usb_msc.c:216-235`에서 실제로 만드는 내용은 80~100바이트다 → 파일 끝에 NUL 패딩이 붙는다.
- `usb_msc.c:216-234` `snprintf(&readme_txt[index], sizeof(readme_txt), ...)` —
  세 번째 인자가 남은 크기(`sizeof - index`)가 아니다. 현재 값으로는 넘치지 않지만 잘못된 사용.
  `firm_ver_t.name_str[32] / version_str[32]`는 NUL 종료 보장이 없다(`src/common/def.h:127-133`).
- `usb_msc.c:73-74` 총 섹터 32768(<0x10000)인데 BPB_TotSec16=0, BPB_TotSec32에 값이 있다.
  FAT 스펙상 이 경우 TotSec16을 써야 한다. Windows는 관대하지만 검사 도구는 경고할 수 있다.
- `usb_msc.c:32,128-140,155-173` `ejected` / weak `tud_msc_start_stop_cb` / weak `tud_msc_capacity_cb`는
  `uf2_msc.c`의 strong 심볼에 가려 죽은 코드다.
- `usb_desc.c:246-256,155` MSC 인터페이스 문자열 인덱스를 5로 참조하지만 CDC가 꺼져 있어
  `string_desc_arr`는 5개(0~4)뿐 → GET_STRING(5)에 NULL 반환(STALL). 무해하나 정리 필요.
- `usb_desc.c:50-53` MSC 단일 인터페이스인데 `bDeviceClass = MISC/IAD`. 순수 MSC면 0x00 권장.

FAT 기하 자체는 정상이다:
reserved 1 + FAT 2×128 + root 32 = **289**, README 클러스터 2 = LBA 289로 `read10`과 일치하고
클러스터 수 32479는 FAT16 유효 범위(4085~65524) 안이다.

---

## B. UF2 → flash 기록 로직 (가장 위험한 부분)

UF2 스펙은 **호스트가 블록을 임의 순서로, 중복해서 보낼 수 있다**고 명시한다.
현재 구현은 "0번부터 순차적으로 한 번씩 온다"를 가정한다.

### B-1. 블록 중복 수신 시 CRC/길이가 두 번 누적 — 우선순위 높음
`src/ap/modules/uf2/uf2.c:92-123` vs `uf2.c:344-354`

```c
uf2_flash_write(state, bl->targetAddr, bl->data, bl->payloadSize);  // 무조건 실행
...
_flash_crc  = utilCalcCRC(_flash_crc, data, len);
_flash_len += len;
...
// 중복 방지 체크는 '나중에'만 있다
if (!(state->writtenMask[pos] & mask)) { ...; state->numWritten++; }
```
같은 LBA가 재전송되면 flash 쓰기는 같은 값이라 무해하지만 `_flash_len` / `_flash_crc`가
두 번 누적된다 → 태그의 `fw_size`/`fw_crc`가 어긋남 → `bootVerifyUpdate()` 실패
(`ERR_BOOT_UPDATE_CRC`).

### B-2. CRC를 "도착 순서"로 누적
`uf2.c:121`

`bootVerifyUpdate()`는 flash를 **선형으로** 읽어 CRC를 계산하는데(`boot.c:56-74`),
기록 측은 블록 도착 순서로 누적한다. 순서가 뒤바뀌면 값이 달라져 업데이트가 실패한다.

**B-1/B-2 공통 수정안**: `writtenMask`로 신규 블록인지 먼저 판정한 뒤 flash 기록,
CRC/길이는 flush 시점에 **flash를 다시 읽어** 계산한다.

### B-3. erase 조건이 순차 도착을 가정
`uf2.c:106-113`

```c
if (addr == 0 || (flash_addr % 0x10000) == 0)
  flashErase(flash_addr, len);   // 64KB 섹터 1개 erase
```
- 0x10000 경계 블록이 **늦게** 도착하면 이미 기록된 64KB가 통째로 지워진다.
- `addr == 0` 블록이 재전송되면 첫 64KB가 지워진다.

**수정안**: "이번 세션에서 이미 지운 64KB 섹터" 비트맵을 두고 최초 1회만 erase.

### B-4. `uf2_flash_is_blank()`가 사실상 동작하지 않음 — 슬롯 손상의 직접 원인
`uf2.c:50-68`

```c
size = sector_size - (addr % sector_size);          // ① 인자 size를 덮어씀
for (uint32_t i = 0; i < size; i += sizeof(uint32_t))
{
  flashRead(addr, (uint8_t *)&data, sizeof(uint32_t));   // ② addr이 증가하지 않음
  if (data != 0xffffffff) return false;
}
```
① 호출자가 넘긴 길이를 무시하고, ② 루프 안에서 주소를 올리지 않아 **선두 4바이트만**
반복해서 읽는다. 게다가 여기서 쓰는 `sector_size`는 4KB인데 실제 `flashErase()`는
64KB 단위로 지운다(`qspi.c:246` `W25Q128FV_SECTOR_SIZE = 0x10000`).

결과:
- 이전 데이터의 **선두 4바이트가 우연히 0xFFFFFFFF**면 "blank"로 판정 → erase 생략 →
  지워지지 않은 영역에 덮어쓰기 → 비트가 AND되어 **데이터 손상**.
  GIF 데이터는 서브블록 길이 0xFF가 자주 나와 실제로 걸릴 수 있다.
- 성능: 256바이트 블록마다 최대 1024회의 QSPI 4바이트 read를 수행 → 전송 속도 급락.

**수정안**
```c
static bool uf2_flash_is_blank(uint32_t addr, uint32_t size)
{
  uint8_t buf[64];

  for (uint32_t i = 0; i < size; i += sizeof(buf))
  {
    uint32_t len = (size - i) > sizeof(buf) ? sizeof(buf) : (size - i);

    if (flashRead(addr + i, buf, len) != true)
      return false;

    for (uint32_t j = 0; j < len; j++)
    {
      if (buf[j] != 0xFF)
        return false;
    }
  }
  return true;
}
```
다만 B-3과 마찬가지로 **"섹터별 erase 완료 비트맵"** 방식으로 바꾸는 편이 더 안전하고 빠르다.

### B-5. 슬롯 경계 검사 off-by-0x400
`uf2.c:161-166`

```c
if ((addr + len) > FLASH_SIZE_SLOT) return false;
flash_addr = (FLASH_ADDR_UPDATE_SLOT + slot * FLASH_SIZE_SLOT) + FLASH_SIZE_TAG + addr;
```
검사는 `addr + len`인데 실제 기록은 `+ FLASH_SIZE_TAG(0x400)` 뒤라 슬롯 끝에서
최대 0x400 바이트가 다음 슬롯을 침범한다. `(addr + len + FLASH_SIZE_TAG) > FLASH_SIZE_SLOT`가 맞다.
(family 최대값 0xFFFF0015 = slot 5는 QSPI 끝 16MB를 넘어가 `qspiWrite()`가 실패한다.)

### B-6. 한 세션에서 2개 이상 UF2 복사 불가
`uf2.c:329-342`, `uf2_msc.c:7`

`_wr_state`가 리셋되지 않아 두 번째 UF2에서 `state->numBlocks = 0xffffffff`가 되어
flush/complete가 영원히 안 된다. 보통 첫 파일 후 리부팅되므로 드러나지 않지만,
점프가 실패하는 상황에서는 문제가 된다.

### B-7. QSPI erase 폴링 타임아웃이 잘못된 상수
`src/hw/driver/qspi.c:650-687`, `src/common/hw/include/qspi/w25q128fv.h:84-85`

`BSP_QSPI_Erase_Sector()`는 `SECTOR_ERASE_CMD(0xD8, 64KB)`를 보내면서 완료 폴링은
`W25Q128FV_SUBSECTOR_ERASE_MAX_TIME(800ms)`을 쓴다. 64KB block erase의 스펙 max는
`W25Q128FV_SECTOR_ERASE_MAX_TIME(3000ms)`이다. 800ms를 넘기면 `QSPI_ERROR`로 떨어져
그 블록 기록이 실패한다. (함수 이름도 실제 명령과 반대로 붙어 있다:
`Erase_Block`이 4KB SUBSECTOR, `Erase_Sector`가 64KB SECTOR)

---

## C. SLOT 2~4 지정 시 다운로드 문제

### 슬롯 번호 자체에 기인한 버그는 없음 (확인 완료)

| 항목 | 확인 결과 |
|---|---|
| `index.html:35-38` familyID | 0xFFFF0010~0013 → SLOT 1~4 |
| `uf2.c:295-298` slot_num | `familyID - 0xFFFF0010` = 0~3 ✔ |
| 부트로더 기록 주소 | 0x90400000 + n×2MB + 0x400 → SLOT4 = 0x90A00400 ✔ |
| 펌웨어 재생 주소 `gif_app.c:13-16` | 동일 계산 ✔ (부트로더와 일치) |
| QSPI 용량 | 칩 ID 0xEF/0x40/0x18 = W25Q128 16MB. SLOT4 끝 0x90BFFFFF는 범위 내 ✔ |
| QSPI 주소 폭 | read/write/erase 모두 24-bit(16MB 커버) ✔ |
| 헤더 규약 | HTML은 offset 16에 4바이트 size, 데이터는 +32부터. `gif_app.c:72,76`과 일치 ✔ |

### 그래도 SLOT 2~4가 실패하는 것으로 보일 수 있는 실제 원인

1. **B-4 (`uf2_flash_is_blank` 버그)** — 가장 유력.
   처음 쓰는 슬롯(전체 0xFF)은 항상 성공하지만, **이미 GIF가 들어있던 슬롯에 다시 쓰면**
   64KB 섹터 erase가 누락될 수 있다. 여러 번 테스트한 슬롯일수록 확률이 올라가고,
   GIF가 64KB보다 크면 반드시 여러 섹터에 걸치므로 위험이 커진다.

2. **슬롯 삭제가 64KB만 지움** — `convex-qmk/src/.../gif_app.c:269`
   ```c
   flashErase(SLOT_ADDRS[slot_run_req], 10);   // 64KB 섹터 1개만 erase
   ```
   헤더만 날아가서 "빈 슬롯"으로 보이지만 뒤쪽 데이터는 그대로 남는다.
   그 상태에서 새 GIF를 쓰면 1번 위험이 그대로 발동한다.
   → 슬롯 전체(2MB) 또는 최소한 이전 파일 크기만큼 지워야 한다.

3. **슬롯 용량 초과가 조용히 실패** — 슬롯당 상한은 2MB − 0x400 − 32바이트다.
   초과하면 `uf2.c:299-316`에서 LCD에 `ERR_BOOT_SLOT_SIZE`만 뜨고,
   **A-4** 때문에 Windows에는 복사 성공으로 표시된다. GIF 동영상은 쉽게 넘길 수 있다.

4. **B-7 (erase 타임아웃 800ms)** — 실패 시 그 슬롯 기록이 깨진다. 슬롯 번호와 무관.

5. **HTML의 매직 문자열이 검증되지 않음** — `index.html:75-77`이 `GIF_IMAGE`/`GIF_MOVIE`를
   헤더 앞 16바이트에 넣지만, `gif_app.c`는 offset 16의 size만 읽고 매직을 확인하지 않는다.
   잘못된 슬롯/파일이 걸러지지 않는다.

6. **`flash_fs_open()`에 크기 상한 검사 없음** — `gif_app.c:72-73`은 `0`/`0xFFFFFFFF`만 거른다.
   `load_gif_from_slot()`은 `< FLASH_SIZE_SLOT`을 검사하므로 일관성을 맞추는 편이 좋다.

---

## D. 수정 내역 (`fix/uf2-msc-robustness` 브랜치)

부트로더(`convex-boot`) 부터 반영했다. 앱(`convex-qmk`) / 웹페이지 수정도 같은 브랜치에서 이어서 진행한다.

| # | 항목 | 상태 | 반영 위치 |
|---|---|---|---|
| 1 | B-3/B-4 erase 를 64KB 섹터 비트맵 기반 "전송당 1회"로 교체, `uf2_flash_is_blank()` 정상 구현 | 완료 | `uf2.c` `uf2_flash_erase_once()` / `uf2_flash_is_blank()` |
| 2 | B-1/B-2 CRC·길이를 도착 순서/중복과 무관하게 산출 (flush 시 flash 재독) | 완료 | `uf2.c` `uf2_flash_calc_crc()` / `uf2_flash_flush*()` |
| 3 | B-1 중복 블록은 `writtenMask` 로 걸러 재기록하지 않음 | 완료 | `uf2.c` `uf2_write_block()` |
| 4 | B-6 새 전송 감지 시 상태 전체 초기화 (2개 이상 파일 연속 복사) | 완료 | `uf2.c` `uf2_transfer_reset()` |
| 5 | B-5 슬롯 경계 검사에 `FLASH_SIZE_TAG` 포함 | 완료 | `uf2.c` `uf2_flash_write_gif()` |
| 6 | A-1 `SYNCHRONIZE CACHE(10/16)` 성공 응답 | 완료 | `usb_msc.c` `tud_msc_scsi_cb()` |
| 7 | A-2 eject 시 CSW 전송 후 점프 (요청 플래그 + 메인 루프 처리) | 완료 | `uf2_msc.c` / `uf2.c` `uf2RequestJump()` |
| 8 | A-3 `bootUpdateFirm()` 을 USB 콜백 밖으로, 완료 대기 1000ms, 긴 작업 전 `tud_disconnect()` | 완료 | `uf2.c` `uf2Update()` / `usb.c` `usbDisconnect()` |
| 9 | A-4 기록 실패 시 write10 에서 오류 반환 | 완료 | `uf2_msc.c` / `UF2_RET_ERR` |
| 10 | B-7 64KB erase 폴링 타임아웃을 `SECTOR_ERASE_MAX_TIME(3000ms)` 로 | 완료 | `qspi.c` `BSP_QSPI_Erase_Sector()` |
| 11 | A-5 README 크기를 실제 길이와 동기화, `snprintf` 남은 크기 전달, 비종료 문자열 방어 | 완료 | `usb_msc.c` `msc_make_readme()` |
| 12 | A-5 BPB 총 섹터를 16bit 필드로 (스펙 준수) | 완료 | `usb_msc.c` boot sector |
| 13 | C-2 슬롯 삭제 시 슬롯 전체 erase | 예정 | `convex-qmk` `gif_app.c` |
| 14 | C-3 웹 생성기에서 슬롯 용량 초과 차단 | 예정 | `index.html` |

빌드 확인: `arm-none-eabi-gcc 13.3.1`, 경고 0, text 133,360 B (부트 영역 256KB 이내).

### 출하된 보드에 대한 한계

출하품은 부트로더를 갱신할 수 없다. `loaderDownToFlash()` 는 임의 주소를 받지만 CLI 는
`FLASH_ADDR_UPDATE`(QSPI) 만 노출하고 boot 영역(`0x08000000`) 기록 경로는 없다.
따라서 위 수정은 **차기 리비전/재고 보드에만** 적용된다.
출하품 대응은 `docs/web_update_plan.md` 의 "앱 경유 업데이트"로 우회한다.

---

## E. OS별 동작 검토 (Windows / macOS / Linux)

세 OS 모두에서 동작해야 하므로, 각 OS가 MSC 에 요구하는 동작을 따로 확인했다.

### 공통 (수정 전 → 후)

| 항목 | Windows | macOS | Linux |
|---|---|---|---|
| `SYNCHRONIZE CACHE(0x35)` | flush/제거 시 전송. 실패하면 지연 쓰기 오류 | unmount 시 전송 | `sync`/umount 시 전송. 실패하면 dmesg 에 `Synchronize Cache(10) failed` | 
| → 수정 후 | 성공 응답 → 오류 사라짐 | 동일 | 동일 |
| UF2 블록 도착 순서 | 대체로 순차, 재시도 시 중복 가능 | **덩어리 단위로 순서가 뒤바뀌는 사례가 알려져 있음** | 페이지 캐시 flush 순서에 따라 뒤바뀔 수 있음 | 
| → 수정 후 | 섹터 erase 비트맵 + `writtenMask` + 재독 CRC 로 순서/중복 무관 | 동일 | 동일 |
| eject / unmount | "안전하게 제거" → `START STOP UNIT` | 디스크 꺼내기 → 동일 | `eject` → 동일 |
| → 수정 후 | CSW 응답 후 점프 → 제거 오류 없음 | "디스크가 올바르게 분리되지 않음" 경고 사라짐 | 동일 |
| 볼륨에 메타데이터 기록 | `System Volume Information`, `IndexerVolumeGuid` | `.fseventsd`, `.Spotlight-V100`, `.Trashes` | 없음(대체로) |
| → 처리 | UF2 블록이 아니므로 무시하고 성공 응답. 읽기는 항상 초기 상태 → 재시도 루프 없음 | 동일 | 동일 |

### OS별 개별 확인

- **Windows**: MSC 단일 인터페이스인데 `bDeviceClass` 가 `MISC/IAD` 다. USBSTOR 는 인터페이스 디스크립터로
  바인딩하므로 실동작에는 문제가 없으나, 순수 MSC 라면 `0x00` 이 맞다. (미수정, 위험 대비 이득이 작음)
- **macOS**: Disk Arbitration 이 마운트 시 `READ CAPACITY` / `MODE SENSE(6)` 를 보낸다. 둘 다 tinyusb 내장 처리.
  `MODE SENSE(10)` / `READ CAPACITY(16)` 는 미처리(STALL) 지만, 실패 시 6바이트/10바이트 명령으로
  폴백하는 것이 정상 동작이라 문제되지 않는다.
- **Linux**: `usb-storage` 는 `TEST UNIT READY` / `INQUIRY` / `READ CAPACITY(10)` / `MODE SENSE(6)` /
  `PREVENT ALLOW MEDIUM REMOVAL` 만 쓰며 전부 처리된다. FAT 드라이버는 `TotSec16` 이 0 이 아니면
  그것을 쓰므로 이번 BPB 수정과 정합한다.
- 세 OS 모두 FAT16 판정은 클러스터 수(32479, 유효 범위 4085~65524)로 하며 동일하게 FAT16 으로 인식한다.
