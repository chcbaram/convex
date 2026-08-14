#!/usr/bin/env python3
"""CONVEX LCD(284 x 76) 크기의 예제 GIF 를 만든다.

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


def make_anim(nframes=24):
    """좌우로 훑고 지나가는 빛줄기."""
    frames = []
    cy = HEIGHT / 2
    span = WIDTH + 80

    for f in range(nframes):
        t = f / nframes
        # 좌우 왕복 (사인 곡선이라 끝에서 부드럽게 되돌아온다)
        head = -40 + span * (0.5 - 0.5 * __import__("math").cos(2 * 3.141592653589793 * t))
        buf = bytearray(WIDTH * HEIGHT)

        for y in range(HEIGHT):
            fy = 1.0 - abs(y - cy) / cy
            fy = max(0.0, fy) ** 1.6
            row = y * WIDTH
            for x in range(WIDTH):
                d = abs(x - head)
                v = max(0.0, 1.0 - d / 46.0) ** 2.2 * fy
                c = int(v * 31)
                if y % 19 == 0 or x % 24 == 0:
                    c = min(31, c + 2)
                buf[row + x] = c
        frames.append(bytes(buf))
    return frames


def main():
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("firmware/release/samples")
    out_dir.mkdir(parents=True, exist_ok=True)

    pal = build_palette()

    still = out_dir / "sample-image.gif"
    still.write_bytes(gif([make_static()], pal))

    anim = out_dir / "sample-anim.gif"
    anim.write_bytes(gif(make_anim(), pal, delay_cs=5))

    for p in (still, anim):
        print("%-40s %8d B" % (p, p.stat().st_size))


if __name__ == "__main__":
    main()
