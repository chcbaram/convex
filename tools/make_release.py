#!/usr/bin/env python3
"""빌드 결과물을 firmware/release 로 복사하고 manifest.json 을 갱신한다.

웹페이지(index.html)가 manifest.json 을 읽어 펌웨어 목록을 만들고,
선택한 항목의 .bin 을 그대로 장치에 전송한다.

사용법:
    python3 tools/make_release.py <빌드폴더> [--note "내용"] ...

    --note 를 주지 않으면 manifest 에 이미 있는 릴리즈 노트를 그대로 유지한다.
    예) python3 tools/make_release.py firmware/convex-qmk/build --note "슬롯 이름 저장"
"""

import json
import re
import shutil
import sys
from datetime import date
from pathlib import Path

ROOT        = Path(__file__).resolve().parent.parent
QMK_DIR     = ROOT / "firmware" / "convex-qmk"
RELEASE_DIR = ROOT / "firmware" / "release"
MANIFEST    = RELEASE_DIR / "manifest.json"

HW_DEF      = QMK_DIR / "src" / "hw" / "hw_def.h"
KBD_CONFIG  = QMK_DIR / "src" / "ap" / "modules" / "qmk" / "keyboards" / "convex" / "config.h"


def crc16(data: bytes) -> int:
    """펌웨어의 utilCalcCRC() 와 같은 계산 (CRC-16/BUYPASS, poly 0x8005, MSB first)."""
    table = []
    for i in range(256):
        c = i << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x8005) & 0xFFFF if (c & 0x8000) else (c << 1) & 0xFFFF
        table.append(c)

    crc = 0
    for b in data:
        crc = ((crc << 8) ^ table[((crc >> 8) ^ b) & 0xFF]) & 0xFFFF
    return crc


def grep_define(path: Path, name: str) -> str:
    m = re.search(r'#define\s+%s\s+"([^"]*)"' % name, path.read_text())
    if not m:
        raise SystemExit("%s 에서 %s 를 찾지 못했다" % (path, name))
    return m.group(1)


def main() -> None:
    args = sys.argv[1:]
    notes = []
    while "--note" in args:
        i = args.index("--note")
        if i + 1 >= len(args):
            raise SystemExit("--note 뒤에 내용이 필요하다")
        notes.append(args[i + 1])
        del args[i:i + 2]

    if len(args) != 1:
        raise SystemExit(__doc__)

    build_dir = Path(args[0]).resolve()
    version   = grep_define(HW_DEF, "_DEF_FIRMWATRE_VERSION")
    board     = grep_define(KBD_CONFIG, "KBD_NAME")

    bin_src = build_dir / "convex-qmk.bin"
    uf2_src = build_dir / ("%s-%s.uf2" % (board, version))

    if not bin_src.exists():
        raise SystemExit("%s 가 없다. 먼저 빌드할 것" % bin_src)

    RELEASE_DIR.mkdir(parents=True, exist_ok=True)

    bin_name = "%s-%s.bin" % (board, version)
    uf2_name = "%s-%s.uf2" % (board, version)

    data = bin_src.read_bytes()
    shutil.copyfile(bin_src, RELEASE_DIR / bin_name)
    if uf2_src.exists():
        shutil.copyfile(uf2_src, RELEASE_DIR / uf2_name)

    entry = {
        "version": version,
        "board":   board,
        "date":    date.today().isoformat(),
        "bin":     bin_name,
        "uf2":     uf2_name if uf2_src.exists() else None,
        "size":    len(data),
        "crc":     "0x%04X" % crc16(data),
        "notes":   notes,
    }

    manifest = {"board": board, "firmwares": []}
    if MANIFEST.exists():
        manifest = json.loads(MANIFEST.read_text())
        manifest.setdefault("firmwares", [])

    # --note 를 주지 않았으면 기존 노트를 유지한다.
    if not notes:
        prev = next((f for f in manifest["firmwares"] if f.get("version") == version), None)
        if prev:
            entry["notes"] = prev.get("notes") or ([prev["note"]] if prev.get("note") else [])

    # 같은 버전이 있으면 교체하고, 없으면 맨 앞에 넣는다 (최신이 위).
    others = [f for f in manifest["firmwares"] if f.get("version") != version]
    manifest["board"]     = board
    manifest["firmwares"] = [entry] + others

    MANIFEST.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")

    print("release  : %s" % RELEASE_DIR)
    print("board    : %s" % board)
    print("version  : %s" % version)
    print("bin      : %s (%d B)" % (bin_name, entry["size"]))
    print("crc      : %s" % entry["crc"])
    print("notes    : %d" % len(entry["notes"]))
    print("firmwares: %d" % len(manifest["firmwares"]))


if __name__ == "__main__":
    main()
