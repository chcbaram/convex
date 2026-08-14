#!/usr/bin/env python3
"""CONVEX LCD(284 x 76) 크기의 예제 GIF 를 만든다.

정지 이미지 하나와 오디오 스펙트럼 애널라이저 애니메이션 하나를 만든다.

외부 라이브러리 없이 GIF89a 를 직접 인코딩한다. 인터넷에서 받은 이미지는
라이선스가 걸리고 해상도도 맞춰야 하지만, 직접 만들면 둘 다 없다.

사용법:
    python3 tools/make_sample_gif.py [출력폴더]
"""

import sys
from pathlib import Path

WIDTH  = 284
HEIGHT = 76


# ---------------------------------------------------------------- LZW
def lzw_encode(data: bytes, min_code_size: int) -> bytes:
    clear = 1 << min_code_size
    end   = clear + 1

    table = {bytes([i]): i for i in range(clear)}
    next_code = end + 1
    code_size = min_code_size + 1

    out = bytearray()
    bit_buf = 0
    bit_cnt = 0

    def emit(code):
        nonlocal bit_buf, bit_cnt
        bit_buf |= code << bit_cnt
        bit_cnt += code_size
        while bit_cnt >= 8:
            out.append(bit_buf & 0xFF)
            bit_buf >>= 8
            bit_cnt -= 8

    emit(clear)
    cur = b""
    for byte in data:
        nxt = cur + bytes([byte])
        if nxt in table:
            cur = nxt
            continue
        emit(table[cur])
        table[nxt] = next_code
        next_code += 1
        if next_code > (1 << code_size):
            if code_size < 12:
                code_size += 1
            else:
                emit(clear)
                table = {bytes([i]): i for i in range(clear)}
                next_code = end + 1
                code_size = min_code_size + 1
        cur = bytes([byte])

    if cur:
        emit(table[cur])
    emit(end)

    if bit_cnt:
        out.append(bit_buf & 0xFF)

    # 255 바이트 이하 서브블록으로 쪼갠다
    packed = bytearray()
    for i in range(0, len(out), 255):
        chunk = out[i:i + 255]
        packed.append(len(chunk))
        packed += chunk
    packed.append(0)
    return bytes(packed)


# ---------------------------------------------------------------- GIF
def gif(frames, palette, delay_cs=0, loop=True) -> bytes:
    """frames: 인덱스 바이트열 리스트. palette: (r,g,b) 리스트(최대 256)."""
    bits = max(1, (len(palette) - 1).bit_length())
    table_size = 1 << bits

    out = bytearray(b"GIF89a")
    out += WIDTH.to_bytes(2, "little") + HEIGHT.to_bytes(2, "little")
    out += bytes([0xF0 | (bits - 1), 0, 0])          # GCT 있음, 색 깊이 8

    for i in range(table_size):
        out += bytes(palette[i]) if i < len(palette) else b"\x00\x00\x00"

    if len(frames) > 1 and loop:
        out += b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00"

    for f in frames:
        if len(frames) > 1:
            out += b"\x21\xF9\x04\x00" + delay_cs.to_bytes(2, "little") + b"\x00\x00"
        out += b"\x2C\x00\x00\x00\x00"
        out += WIDTH.to_bytes(2, "little") + HEIGHT.to_bytes(2, "little")
        out += b"\x00"
        out += bytes([bits])
        out += lzw_encode(f, bits)

    out += b"\x3B"
    return bytes(out)


# ------------------------------------------------------------ 팔레트
def build_palette():
    """검정 -> 파랑 -> 청록 -> 흰색 그라디언트 32단계."""
    pal = []
    for i in range(32):
        t = i / 31
        if t < 0.5:
            u = t / 0.5
            pal.append((int(6 + 20 * u), int(10 + 90 * u), int(24 + 190 * u)))
        else:
            u = (t - 0.5) / 0.5
            pal.append((int(26 + 229 * u), int(100 + 155 * u), int(214 + 41 * u)))
    return pal


# -------------------------------------------------------------- 그림
def make_static():
    """가운데가 밝은 가로 그라디언트 위에 격자와 강조선."""
    buf = bytearray(WIDTH * HEIGHT)
    cx, cy = WIDTH / 2, HEIGHT / 2

    for y in range(HEIGHT):
        for x in range(WIDTH):
            dx = (x - cx) / cx
            dy = (y - cy) / cy
            v = 1.0 - (dx * dx * 0.85 + dy * dy * 0.55)
            v = max(0.0, min(1.0, v))

            c = int(v * 26)
            if x % 24 == 0 or y % 19 == 0:      # 격자
                c = min(31, c + 4)
            if abs(y - HEIGHT // 2) <= 1:       # 가운데 강조선
                c = min(31, c + 6)
            buf[y * WIDTH + x] = c
    return bytes(buf)


# 스펙트럼 애널라이저 팔레트
#   0        배경
#   1        격자
#   2..2+N-1 막대 색 (아래 초록 -> 노랑 -> 위 빨강)
#   마지막   피크 표시
BAR_STEPS = 24
IDX_BG    = 0
IDX_GRID  = 1
IDX_BAR   = 2
IDX_PEAK  = IDX_BAR + BAR_STEPS


def build_spectrum_palette():
    pal = [(8, 10, 14), (24, 28, 36)]

    for i in range(BAR_STEPS):
        t = i / (BAR_STEPS - 1)          # 0 = 아래, 1 = 위
        if t < 0.6:
            u = t / 0.6                   # 초록 -> 노랑
            pal.append((int(40 + 200 * u), int(200 + 20 * u), int(70 - 40 * u)))
        else:
            u = (t - 0.6) / 0.4           # 노랑 -> 빨강
            pal.append((int(240 + 15 * u), int(220 - 160 * u), int(30 + 10 * u)))

    pal.append((235, 240, 245))           # 피크
    return pal


def make_anim(nframes=30, bars=32):
    """오디오 스펙트럼 애널라이저. 막대가 오르내리고 피크가 천천히 내려온다."""
    import math

    gap    = 2
    bar_w  = (WIDTH - gap * (bars - 1)) // bars
    left   = (WIDTH - (bar_w * bars + gap * (bars - 1))) // 2

    # 막대마다 주기가 다른 사인을 겹친다. 주기를 정수로 두어야 한 바퀴가
    # 매끄럽게 이어진다. 낮은 대역일수록 크게 흔들린다.
    waves = []
    for b in range(bars):
        lo = 1.0 - b / (bars - 1)                     # 저음쪽 가중치
        waves.append((
            (1, 0.37 * b, 0.34 + 0.30 * lo),
            (2, 0.61 * b, 0.22),
            (3, 0.11 * b, 0.14),
            (5, 0.83 * b, 0.08),
        ))

    def level(b, t):
        v = 0.30 + 0.22 * (1.0 - b / (bars - 1))      # 기본 높이
        for k, ph, amp in waves[b]:
            v += amp * math.sin(2 * math.pi * (k * t + ph))
        return max(0.02, min(1.0, v))

    # 피크는 이전 프레임에 의존한다. 한 바퀴 먼저 돌려 초기값을 맞춘 뒤
    # 기록해야 마지막 프레임에서 첫 프레임으로 튀지 않는다.
    peak = [0.0] * bars
    frames = []

    for pass_no in range(2):
        for f in range(nframes):
            t = f / nframes
            lv = [level(b, t) for b in range(bars)]

            for b in range(bars):
                peak[b] = lv[b] if lv[b] >= peak[b] else max(lv[b], peak[b] - 0.045)

            if pass_no == 0:
                continue

            buf = bytearray(WIDTH * HEIGHT)

            for y in range(0, HEIGHT, 6):             # 가로 격자
                row = y * WIDTH
                for x in range(WIDTH):
                    buf[row + x] = IDX_GRID

            for b in range(bars):
                x0 = left + b * (bar_w + gap)
                h  = int(lv[b] * HEIGHT)
                py = HEIGHT - 1 - int(peak[b] * (HEIGHT - 1))

                for y in range(HEIGHT - h, HEIGHT):
                    t_col = (HEIGHT - 1 - y) / (HEIGHT - 1)
                    c = IDX_BAR + min(BAR_STEPS - 1, int(t_col * BAR_STEPS))
                    row = y * WIDTH
                    for x in range(x0, x0 + bar_w):
                        buf[row + x] = c

                for x in range(x0, x0 + bar_w):       # 피크 선
                    buf[py * WIDTH + x] = IDX_PEAK

            frames.append(bytes(buf))

    return frames


def main():
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("firmware/release/samples")
    out_dir.mkdir(parents=True, exist_ok=True)

    pal = build_palette()

    still = out_dir / "sample-image.gif"
    still.write_bytes(gif([make_static()], pal))

    anim = out_dir / "sample-anim.gif"
    anim.write_bytes(gif(make_anim(), build_spectrum_palette(), delay_cs=7))

    for p in (still, anim):
        print("%-40s %8d B" % (p, p.stat().st_size))


if __name__ == "__main__":
    main()
