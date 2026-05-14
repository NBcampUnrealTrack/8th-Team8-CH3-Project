#!/usr/bin/env python3
"""Checkerboard-ish 배경을 제거하고 맨발 발자국(+스플래터)만 알파 채널로 남긴 RGBA PNG 생성."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image

DEFAULT_SRC = Path(
    r"C:\Users\TaitoP\.cursor\projects\e-Unreal-git-8th-Team8-CH3-Project\assets"
    r"\c__Users_TaitoP_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images"
    r"_image-cbf51dcd-2ba5-449a-a958-6fc3ea3de22e.png"
)


def keyed_rgba(rgb: Image.Image, t0: float = 118.0, t1: float = 195.0) -> Image.Image:
    """발자국은 거의 순백이라 min(R,G,B)가 높고, 회색 검은 무늬 바닥은 낮음 — 그 간격으로 소프트 누끼."""
    arr = np.asarray(rgb.convert("RGB"), dtype=np.float32)
    mn = np.minimum(np.minimum(arr[..., 0], arr[..., 1]), arr[..., 2])
    a01 = np.clip((mn - t0) / (t1 - t0), 0.0, 1.0)
    alpha_u8 = np.clip(np.round(a01 * 255.0), 0, 255).astype(np.uint8)
    rgb_u8 = np.clip(np.round(arr), 0, 255).astype(np.uint8)
    return Image.merge("RGBA", (*(Image.fromarray(rgb_u8[..., i]) for i in range(3)), Image.fromarray(alpha_u8)))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("-i", "--input", type=Path, default=DEFAULT_SRC, help="RGB PNG 경로")
    ap.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "Content" / "Textures" / "T_FootprintBareTrail_Keyed_RGBA.png",
    )
    ap.add_argument("--t0", type=float, default=118.0, help="전부 투명이 되게 하는 min(R,G,B) 하한 근처")
    ap.add_argument("--t1", type=float, default=195.0, help="불투명에 도달하는 min(R,G,B) 상한")
    args = ap.parse_args()

    if not args.input.is_file():
        print(f"Input not found: {args.input}", file=sys.stderr)
        return 1

    rgb = Image.open(args.input).convert("RGB")
    out = keyed_rgba(rgb, t0=args.t0, t1=args.t1)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    out.save(args.output, "PNG", compress_level=6)

    bbox = out.getchannel("A").getbbox()
    print(f"Wrote {args.output} alpha bbox ~ {bbox}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
