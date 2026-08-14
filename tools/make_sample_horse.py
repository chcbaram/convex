#!/usr/bin/env python3
"""퍼블릭 도메인 영상을 CONVEX LCD(284 x 76) 크기의 예제 GIF 로 만든다.

원본 : Eadweard Muybridge, "Sallie Gardner at a Gallop" (1878)
       Wikimedia Commons, 퍼블릭 도메인 (저작권 만료)
       https://commons.wikimedia.org/wiki/File:Horse_in_Motion_-_Sallie_Gardner_(animation).gif

LCD 가 3.7:1 로 길어 원본을 그대로 넣으면 위아래가 잘린다. 가운데 띠만
잘라내 가로로 긴 장면으로 만든다.

Pillow 가 필요하다.
    pip install Pillow
    python3 tools/make_sample_horse.py [출력폴더]
"""

import sys
import urllib.request
from io import BytesIO
from pathlib import Path

from PIL import Image, ImageSequence

SRC_URL = ("https://upload.wikimedia.org/wikipedia/commons/0/01/"
           "Horse_in_Motion_-_Sallie_Gardner_%28animation%29.gif")

WIDTH   = 284
HEIGHT  = 76
COLORS  = 64      # 색을 줄여야 슬롯 용량과 디코딩 부담이 준다
FRAME_MS = 160    # 11프레임이라 한 바퀴 약 1.8초. 너무 빠르면 눈이 못 따라간다
CROP_Y  = 0.55    # 말이 화면 중앙보다 살짝 아래에 있다


def main() -> None:
    out_dir = Path(sys.argv[1] if len(sys.argv) > 1 else "firmware/release/samples")
    out_dir.mkdir(parents=True, exist_ok=True)

    print("download :", SRC_URL)
    # 위키미디어는 기본 User-Agent 를 막는다. 정책상 식별 가능한 값을 보낸다.
    req = urllib.request.Request(SRC_URL, headers={
        "User-Agent": "convex-sample-builder/1.0 (https://github.com/chcbaram/convex)"})
    with urllib.request.urlopen(req) as res:
        src = Image.open(BytesIO(res.read()))

    frames = []
    for f in ImageSequence.Iterator(src):
        im = f.convert("RGB")
        w, h = im.size

        band = int(w * HEIGHT / WIDTH)
        top  = int((h - band) * CROP_Y)

        im = im.crop((0, top, w, top + band)).resize((WIDTH, HEIGHT), Image.LANCZOS)
        frames.append(im.convert("P", palette=Image.ADAPTIVE, colors=COLORS))

    out = out_dir / "sample-horse.gif"
    frames[0].save(out, save_all=True, append_images=frames[1:],
                   duration=FRAME_MS, loop=0, optimize=True, disposal=2)

    print("out      : %s (%d frames, %.1f KB)" % (out, len(frames), out.stat().st_size / 1024))


if __name__ == "__main__":
    main()
