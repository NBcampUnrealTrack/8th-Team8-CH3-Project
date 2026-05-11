"""Generate PNG: black text on fully transparent background (knock-out / 누끼).

UE Masked 머티리얼의 Opacity Mask에 쓰기 좋게 알파를 이진화(경계 정리)합니다.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT = Path(r"C:\Windows\Fonts\malgun.ttf")
OUT = Path(__file__).resolve().parents[1] / "assets" / "quarantine_rules_text.png"

# 알파가 이 값 미만이면 완전 투명, 이상이면 불투명 검정 (가장자리 프린지 제거)
ALPHA_CUTOFF = 40


def knock_out_edges(rgba: Image.Image, cutoff: int = ALPHA_CUTOFF) -> None:
    """In-place: 배경은 (0,0,0,0), 글자는 (0,0,0,255)로 정리."""
    px = rgba.load()
    w, h = rgba.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < cutoff:
                px[x, y] = (0, 0, 0, 0)
            else:
                px[x, y] = (0, 0, 0, 255)


def main() -> None:
    title = "격리 구역 출입 통제  방역 수칙"
    body_lines = [
        "1. 개인 보호구(PPE) 착용 후 출입합니다.",
        "2. 마스크 착용 및 손 소독을 상시 준수합니다.",
        "3. 허가된 구역 외 무단 출입을 금지합니다.",
        "4. 발열·호흡 곤란 등 증상 시 즉시 신고합니다.",
        "5. 음식물 및 불필요한 개인 물품 반입을 금지합니다.",
        "6. 사용 후 폐기물은 지정 수거함에 배출합니다.",
        "",
        "※ 미준수 시 안내에 따라 구역에서 퇴거 조치될 수 있습니다.",
    ]

    w, h = 1800, 1000
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    title_font = ImageFont.truetype(str(FONT), 48)
    body_font = ImageFont.truetype(str(FONT), 32)

    # 검정 글씨 (누끼 전 그리기용; 이후 knock_out_edges로 알파 정리)
    fill = (0, 0, 0, 255)

    x_margin = 80
    y = 70

    draw.text((x_margin, y), title, font=title_font, fill=fill)
    y += int(title_font.size * 1.8)

    line_gap = 14
    for line in body_lines:
        draw.text((x_margin, y), line, font=body_font, fill=fill)
        bbox = draw.textbbox((0, 0), line, font=body_font)
        y += (bbox[3] - bbox[1]) + line_gap

    knock_out_edges(img, ALPHA_CUTOFF)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT, "PNG")
    print(f"Wrote {OUT}")


if __name__ == "__main__":
    main()
