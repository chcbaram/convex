#!/usr/bin/env python3
"""'CONVEX' 로고 애니메이션 GIF 를 만든다.

글자 위를 빛줄기가 훑고 지나가고, 아래 바가 같이 반짝인다.
빛줄기가 화면 밖에서 시작해 화면 밖으로 나가므로 반복 재생이 끊겨 보이지 않는다.

크기를 인자로 받는다. LCD 크기(284x76) 가 기본이고, LCD 보다 큰 GIF 를
넣었을 때 기기가 어떻게 동작하는지 시험하려고 큰 것도 만들 수 있다.

GIF 인코더는 make_sample_gif.py 것을 그대로 쓴다.

사용법:
    python3 tools/make_sample_logo.py [출력폴더] [가로x세로 ...]
    python3 tools/make_sample_logo.py firmware/release/samples 284x76 610x354
"""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_sample_gif as enc                        # noqa: E402


FRAMES   = 48
DELAY_CS = 4                     # 40ms · 약 1.9초 한 바퀴

TEXT     = "CONVEX"
BG_N     = 16                    # 배경 색 단계
FG_N     = 64                    # 글자 색 단계
IDX_BG   = 0
IDX_FG   = BG_N

FONT = {
    "C": ("01110", "10001", "10000", "10000", "10000", "10001", "01110"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "N": ("10001", "11001", "10101", "10011", "10001", "10001", "10001"),
    "V": ("10001", "10001", "10001", "10001", "10001", "01010", "00100"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
    "X": ("10001", "10001", "01010", "00100", "01010", "10001", "10001"),
}


def lerp(a, b, t):
    return int(round(a + (b - a) * t))


def build_palette():
    """앞쪽 16칸은 배경(남색), 뒤쪽 64칸은 글자(청록 -> 흰색)."""
    pal = []
    for i in range(BG_N):                       # 배경 : 검정에 가까운 남색
        t = i / (BG_N - 1)
        pal.append((lerp(4, 26, t), lerp(7, 34, t), lerp(16, 72, t)))
    for i in range(FG_N):                       # 글자 : 청록 -> 하늘 -> 흰색
        t = i / (FG_N - 1)
        if t < 0.6:
            u = t / 0.6
            pal.append((lerp(8, 60, u), lerp(80, 190, u), lerp(120, 240, u)))
        else:
            u = (t - 0.6) / 0.4
            pal.append((lerp(60, 255, u), lerp(190, 255, u), lerp(240, 255, u)))
    return pal


class Layout:
    """가로 크기에 맞춰 글꼴 배율과 자리를 정한다. 5x7 글꼴 6글자 + 글자 사이."""

    def __init__(self, w, h):
        self.w = w
        self.h = h
        # 글자 폭이 화면의 74% 쯤 되게. 한 글자 5칸 + 사이 1칸 = 35칸.
        self.scale  = max(1, round(w * 0.74 / 35))
        self.glyph_w = 5 * self.scale
        self.gap     = self.scale
        self.text_w  = self.glyph_w * len(TEXT) + self.gap * (len(TEXT) - 1)
        self.text_h  = 7 * self.scale
        self.bar_h   = max(2, self.scale * 2 // 3)
        self.bar_pad = max(4, self.scale + 2)

        block = self.text_h + self.bar_pad + self.bar_h
        self.text_x = (w - self.text_w) // 2
        self.text_y = max(0, (h - block) // 2)
        self.bar_y  = self.text_y + self.text_h + self.bar_pad

        self.sweep_w = w * 0.134                 # 빛줄기 폭
        self.span    = w + int(w * 0.85) * 2     # 화면 밖에서 화면 밖까지


def build_mask(lo):
    """글자와 바가 차지하는 자리를 미리 그려 둔다. 프레임마다 다시 안 그린다."""
    mask = bytearray(lo.w * lo.h)               # 1 = 글자, 2 = 바
    for ci, ch in enumerate(TEXT):
        gx = lo.text_x + ci * (lo.glyph_w + lo.gap)
        for ry, row in enumerate(FONT[ch]):
            for rx, bit in enumerate(row):
                if bit != "1":
                    continue
                for y in range(lo.text_y + ry * lo.scale, lo.text_y + (ry + 1) * lo.scale):
                    base = y * lo.w + gx + rx * lo.scale
                    for x in range(base, base + lo.scale):
                        mask[x] = 1

    for y in range(lo.bar_y, min(lo.h, lo.bar_y + lo.bar_h)):
        for x in range(lo.text_x, lo.text_x + lo.text_w):
            mask[y * lo.w + x] = 2
    return mask


def make_frames(lo):
    mask   = build_mask(lo)
    frames = []
    start  = -(lo.span - lo.w) // 2
    scan   = max(2, round(lo.h / 19))            # 가로 주사선 간격

    for f in range(FRAMES):
        t     = f / FRAMES
        sweep = start + lo.span * t
        # 배경 결 : 천천히 흐르는 사선 무늬. 한 바퀴에 정확히 두 번 돈다.
        phase = 2 * math.pi * t * 2
        freq  = 14.0 / lo.w

        buf = bytearray(lo.w * lo.h)
        for y in range(lo.h):
            row  = y * lo.w
            # 위아래로 어두워지는 그라디언트
            vign = 1.0 - abs(y - lo.h / 2) / (lo.h / 2)
            dark = (y % scan == 0)
            for x in range(lo.w):
                kind = mask[row + x]

                # 사선 빛줄기까지의 거리. 아래로 갈수록 왼쪽으로 기울인다.
                d = (x + 0.55 * (lo.h - y)) - sweep
                shine = math.exp(-(d / lo.sweep_w) ** 2)

                if kind == 0:
                    v = 0.18 + 0.42 * vign
                    v += 0.10 * math.sin(phase + (x - y * 0.7) * freq)
                    v += 0.35 * shine
                    if dark:                    # 옅은 가로 주사선
                        v -= 0.12
                    idx = IDX_BG + min(BG_N - 1, max(0, int(v * BG_N)))
                elif kind == 1:
                    # 글자 안쪽은 위가 밝은 세로 그라디언트 + 빛줄기
                    g = 1.0 - (y - lo.text_y) / lo.text_h
                    v = 0.20 + 0.18 * g + 0.95 * shine
                    idx = IDX_FG + min(FG_N - 1, max(0, int(v * FG_N)))
                else:
                    v = 0.16 + 0.80 * shine
                    idx = IDX_FG + min(FG_N - 1, max(0, int(v * FG_N)))

                buf[row + x] = idx
        frames.append(bytes(buf))
    return frames


def build(w, h):
    # gif() 는 모듈 전역 크기를 본다. 만들려는 크기로 바꿔 두고 부른다.
    enc.WIDTH, enc.HEIGHT = w, h
    return enc.gif(make_frames(Layout(w, h)), build_palette(), delay_cs=DELAY_CS)


def main():
    args    = sys.argv[1:]
    out_dir = Path(args[0]) if args and "x" not in args[0] else Path("firmware/release/samples")
    sizes   = [a for a in args if "x" in a] or ["284x76"]
    out_dir.mkdir(parents=True, exist_ok=True)

    for spec in sizes:
        w, h = (int(v) for v in spec.lower().split("x"))
        name = "sample-logo.gif" if (w, h) == (284, 76) else "sample-logo-%dx%d.gif" % (w, h)
        path = out_dir / name
        path.write_bytes(build(w, h))
        print("%-44s %8d B  %d프레임 %dx%d" % (path, path.stat().st_size, FRAMES, w, h))


if __name__ == "__main__":
    main()
