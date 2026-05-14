#!/usr/bin/env python3
"""Compose multiple footprint stamps into one 1024x1024 RGBA PNG for decals."""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image

# Original single footprint (heavy tread)
SRC = Path(
    r"C:\Users\TaitoP\.cursor\projects\e-Unreal-git-8th-Team8-CH3-Project\assets"
    r"\c__Users_TaitoP_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images"
    r"_T_Heavy_01-489ad7de-3872-4e6c-8036-6fbfd8a371d0.png"
)
OUT_DIR = Path(__file__).resolve().parents[1] / "Content" / "Textures"
OUT_FILE = OUT_DIR / "T_FootprintTrail_Multiple_1K.png"
SIZE = 1024


def foot_stamp_rgba(src_rgb: Image.Image, luminance_floor: int = 42) -> Image.Image:
    """White RGB + alpha. 밝은 배경 PNG는 명도 역전. 가장자리 스플래터로 bbox가 커지는 문제는 fg 퍼센타일 박스로 줄임."""
    rgb = src_rgb.convert("RGB")
    g = rgb.convert("L")
    arr = np.asarray(g, dtype=np.uint8)
    # 배경이 밝게 차지하면 발자국 픽셀을 밝히기 위해 역전
    if float((arr >= 240).mean()) > 0.5:
        arr = 255 - arr

    fg = arr > 128
    if not np.any(fg):
        fg = arr > luminance_floor
    if not np.any(fg):
        fg = arr > 0
    ys, xs = np.where(fg)
    qy_lo, qy_hi = np.percentile(ys, [8.0, 92.0])
    qx_lo, qx_hi = np.percentile(xs, [8.0, 92.0])
    pad = 20
    top = max(0, int(np.floor(qy_lo)) - pad)
    bottom = min(arr.shape[0], int(np.ceil(qy_hi)) + pad + 1)
    left = max(0, int(np.floor(qx_lo)) - pad)
    right = min(arr.shape[1], int(np.ceil(qx_hi)) + pad + 1)

    cropped = arr[top:bottom, left:right]
    alpha_u8 = np.clip(cropped.astype(np.uint16), 0, 255).astype(np.uint8)
    h_, w_ = alpha_u8.shape
    rgb255 = Image.new("L", (w_, h_), 255)
    return Image.merge("RGBA", (rgb255, rgb255, rgb255, Image.fromarray(alpha_u8)))


def scale_to_height(im: Image.Image, target_h: int) -> Image.Image:
    w, h = im.size
    if h <= 0:
        return im
    nw = max(1, int(round(w * (target_h / h))))
    return im.resize((nw, target_h), Image.Resampling.LANCZOS)


def rotated_rgba(im: Image.Image, deg: float) -> Image.Image:
    return im.rotate(deg, resample=Image.Resampling.BICUBIC, expand=True, fillcolor=(0, 0, 0, 0))


def paste_center(canvas: Image.Image, layer: Image.Image, cx: int, cy: int) -> None:
    w, h = layer.size
    x = int(round(cx - w / 2))
    y = int(round(cy - h / 2))
    canvas.alpha_composite(layer, (x, y))


def main() -> int:
    if not SRC.is_file():
        print(f"Source not found: {SRC}", file=sys.stderr)
        return 1

    src_im = Image.open(SRC).convert("RGB")
    stamp_full = foot_stamp_rgba(src_im)

    placements: list[tuple[float, float, float, int]] = [
        (140.0, SIZE - 110.0, -14.0, 268),
        (260.0, SIZE - 255.0, -6.0, 258),
        (405.0, SIZE - 400.0, 5.0, 248),
        (540.0, SIZE - 530.0, -18.0, 238),
        (675.0, SIZE - 650.0, 14.0, 226),
        (810.0, SIZE - 770.0, -11.0, 218),
        (920.0, SIZE - 895.0, 22.0, 208),
        (SIZE - 190.0, SIZE - 150.0, -9.0, 262),
        (SIZE - 320.0, SIZE - 300.0, 8.0, 242),
        (SIZE - 85.0, SIZE - 480.0, -21.0, 232),
        (980.0, SIZE - 360.0, 12.0, 222),
    ]

    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    for cx, cy, deg, tgt_h in placements:
        lp = rotated_rgba(scale_to_height(stamp_full, tgt_h), deg)
        paste_center(canvas, lp, cx, cy)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    canvas.save(OUT_FILE, "PNG", compress_level=6)

    aa = canvas.split()[3]
    bbox = aa.getbbox()
    print(f"Wrote {OUT_FILE} footprint alpha bbox ~ {bbox}")

    assert canvas.size == (SIZE, SIZE)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
