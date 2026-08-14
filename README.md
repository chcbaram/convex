# CONVEX

STM32H743 기반 QMK 키보드. 키보드 기능에 더해 가로로 긴 LCD(284 × 76)를 달고
GIF 를 재생할 수 있고, 펌웨어와 GIF 를 웹브라우저에서 바로 기기에 전송할 수 있다.

## 웹페이지

### https://chcbaram.github.io/convex/

브라우저에서 USB 로 연결된 기기에 바로 전송한다. 파일을 내려받아 복사하지 않아도 된다.

| 탭 | 하는 일 |
|---|---|
| **GIF 슬롯** | 슬롯 4개의 현재 상태(이름·종류·해상도·크기)를 보고, 미리보기로 확인한 뒤 원하는 슬롯에 저장 |
| **펌웨어** | 저장소에 올라온 펌웨어 목록에서 골라 설치. 현재 기기 버전과 비교해 준다 |
| **설정** | RTC 시간을 PC 시간으로 동기화하거나 직접 입력 |

**Chrome 또는 Edge 가 필요하다** (WebHID 사용). Safari · Firefox 는 지원하지 않으므로,
그 경우 `.uf2` 파일을 내려받아 부트로더 드라이브에 복사하는 기존 방식을 쓰면 된다.

> 웹 전송은 앱 펌웨어 `V260814R1` 이상에서 동작한다. 그 이전 펌웨어라면 웹페이지가
> 이를 알려주며, `.uf2` 로 한 번 업데이트하면 이후부터는 웹에서 바로 된다.

## 구성

| 폴더 | 내용 |
|---|---|
| `firmware/convex-boot` | UF2 부트로더. USB 대용량 저장장치로 인식되어 `.uf2` 를 받는다 |
| `firmware/convex-qmk` | 앱 펌웨어. QMK + LVGL GIF 플레이어 + 웹 업데이트 |
| `firmware/release` | 배포용 펌웨어 바이너리와 예제 GIF |
| `hardware` | 회로도 |
| `tools` | 릴리스 생성, 예제 GIF 생성 스크립트 |
| `index.html` | 위 웹페이지 |

## 하드웨어

- STM32H743VIT6 (2MB Flash, dual bank)
- 외부 QSPI 플래시 W25Q128 (16MB) — 펌웨어 업데이트 영역과 GIF 슬롯 4개
- LCD 284 × 76
- 6 × 22 키 매트릭스, SK6812 RGB
- USB Type-C

## 빌드

`arm-none-eabi-gcc` 와 CMake 가 필요하다.

```bash
cd firmware/convex-qmk
cmake -S . -B build -DKEYBOARD_PATH=keyboards/convex
cmake --build build -j8
```

부트로더는 `firmware/convex-boot` 에서 `-DKEYBOARD_PATH` 없이 같은 방식으로 빌드한다.

빌드 결과를 배포용으로 옮기고 목록을 갱신하려면:

```bash
python3 tools/make_release.py firmware/convex-qmk/build --note "변경 내용"
```
