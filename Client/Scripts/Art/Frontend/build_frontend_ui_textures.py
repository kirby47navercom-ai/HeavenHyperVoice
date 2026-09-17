"""타이틀·서버 접속·로그인 화면에 쓰는 UI 리소스 PNG를 만든다.

모양은 프론트엔드 리디자인 시안 기준이다. 패널과 버튼은 왼쪽 위·오른쪽 아래 두 모서리만
깎고, 테두리는 타이틀 로고 외곽선 파랑, 주 버튼은 로고 노랑이다.
9-slice 여백과 배치 값은 Client/UI/Images/Frontend/README.md 에 적었다.

타이틀 로고(TitleLogo.png)와 배경 그림(Background_SpearPillar.png)은 원본을 복사해 둔 것이라 여기서 만들지 않는다.

실행:
    python Client/Scripts/Art/Frontend/build_frontend_ui_textures.py
"""

from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter


PROJECT_ROOT = Path(__file__).resolve().parents[4]
OUTPUT_ROOT = PROJECT_ROOT / "Client" / "UI" / "Images" / "Frontend"

# 대각선 가장자리 안티앨리어싱용 슈퍼샘플 배율.
SS = 4

NAVY = (11, 42, 134)
PANEL = (245, 249, 255)
INPUT = (228, 238, 249)
INPUT_FOCUS = (241, 247, 253)
LINE = (183, 201, 226)
LINE_HOVER = (143, 167, 201)
CYAN = (43, 168, 232)
ERROR = (208, 73, 63)
YELLOW = (253, 212, 0)
YELLOW_HOVER = (255, 227, 71)
DISABLED_FILL = (213, 222, 234)
DISABLED_LINE = (143, 160, 190)
WHITE = (255, 255, 255)


def chamfer(x0, y0, x1, y1, cut):
    """왼쪽 위와 오른쪽 아래만 깎은 사각형. 로고 글자 모서리와 같은 방향이다."""
    return [(x0 + cut, y0), (x1, y0), (x1, y1 - cut), (x1 - cut, y1), (x0, y1), (x0, y0 + cut)]


def inner_cut(cut, border):
    # 테두리를 안쪽으로 border 만큼 밀었을 때 대각선에서도 같은 두께가 되는 깎임 길이.
    return cut - border * (2 - 2 ** 0.5)


def canvas(width, height, color):
    # 투명 픽셀 RGB 를 가장자리 색으로 채워 둬야 축소할 때 검은 테두리가 번지지 않는다.
    return Image.new("RGBA", (width * SS, height * SS), color + (0,))


def finish(image, width, height):
    return image.resize((width, height), Image.LANCZOS)


def inset_band(image, polygon, top, height, alpha):
    """폴리곤 안쪽에만 가로 그림자 띠를 얹는다 (입력칸 안쪽 그림자, 버튼 아래 그림자)."""
    size = image.size
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).polygon(polygon, fill=255)
    band = Image.new("L", size, 0)
    ImageDraw.Draw(band).rectangle([0, top, size[0], top + height - 1], fill=int(255 * alpha))
    shade = Image.new("RGBA", size, NAVY + (0,))
    shade.putalpha(ImageChops.multiply(band, mask))
    return Image.alpha_composite(image, shade)


def framed(width, height, cut, border, fill, line, top_shade=0, bottom_shade=0, shade_alpha=0.0):
    image = canvas(width, height, line)
    draw = ImageDraw.Draw(image)
    w, h, b = width * SS, height * SS, border * SS
    draw.polygon(chamfer(0, 0, w, h, cut * SS), fill=line + (255,))
    inner = chamfer(b, b, w - b, h - b, inner_cut(cut, border) * SS)
    if fill is None:
        # 속이 빈 테두리: 안쪽을 다시 투명하게 뚫는다.
        hole = Image.new("L", image.size, 255)
        ImageDraw.Draw(hole).polygon(inner, fill=0)
        image.putalpha(ImageChops.multiply(image.getchannel("A"), hole))
    else:
        draw.polygon(inner, fill=fill + (255,))
    if top_shade:
        image = inset_band(image, inner, b, top_shade * SS, shade_alpha)
    if bottom_shade:
        image = inset_band(image, inner, h - b - bottom_shade * SS, bottom_shade * SS, shade_alpha)
    return finish(image, width, height)


def outline_rect(size, border, color):
    image = canvas(size, size, color)
    draw = ImageDraw.Draw(image)
    s, b = size * SS, border * SS
    draw.rectangle([0, 0, s - 1, s - 1], fill=color + (255,))
    draw.rectangle([b, b, s - b - 1, s - b - 1], fill=color + (0,))
    return finish(image, size, size)


def bezier(p0, p1, p2, p3, steps=24):
    points = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        points.append((
            u ** 3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t ** 3 * p3[0],
            u ** 3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t ** 3 * p3[1],
        ))
    return points


def star_polygon(center, radius):
    """로고 안 반짝이와 같은 네 갈래 별. 24 단위 좌표계를 radius 로 키운다."""
    cx, cy = center
    k = radius / 12

    def p(x, y):
        return (cx + (x - 12) * k, cy + (y - 12) * k)

    segments = [
        (p(12, 0), p(12.8, 8), p(16, 11.2), p(24, 12)),
        (p(24, 12), p(16, 12.8), p(12.8, 16), p(12, 24)),
        (p(12, 24), p(11.2, 16), p(8, 12.8), p(0, 12)),
        (p(0, 12), p(8, 11.2), p(11.2, 8), p(12, 0)),
    ]
    points = []
    for segment in segments:
        points.extend(bezier(*segment)[:-1])
    return points


def build_star(size, scale=1.0):
    image = canvas(size, size, WHITE)
    s = size * SS
    ImageDraw.Draw(image).polygon(star_polygon((s / 2, s / 2), s / 2 * scale), fill=WHITE + (255,))
    return finish(image, size, size)


def build_sparkle(size):
    core = build_star(size, 0.72)
    glow = core.getchannel("A").filter(ImageFilter.GaussianBlur(size * 0.08))
    glow = glow.point(lambda a: int(a * 0.85))
    image = Image.new("RGBA", (size, size), WHITE + (0,))
    image.putalpha(ImageChops.lighter(glow, core.getchannel("A")))
    return image


def build_status_icon(glyph):
    size = 64
    image = framed(size, size, 12, 5, None, WHITE)
    layer = canvas(size, size, WHITE)
    draw = ImageDraw.Draw(layer)

    def r(x0, y0, x1, y1):
        return [x0 * SS, y0 * SS, x1 * SS, y1 * SS]

    if glyph == "info":
        draw.ellipse(r(27, 15, 37, 25), fill=WHITE + (255,))
        draw.rectangle(r(28, 29, 36, 49), fill=WHITE + (255,))
    elif glyph == "error":
        draw.rectangle(r(28, 15, 36, 37), fill=WHITE + (255,))
        draw.ellipse(r(27, 41, 37, 51), fill=WHITE + (255,))
    elif glyph == "check":
        draw.line([(19 * SS, 33 * SS), (28 * SS, 42 * SS), (45 * SS, 22 * SS)], fill=WHITE + (255,), width=7 * SS, joint="curve")
    return Image.alpha_composite(image, finish(layer, size, size))


def build_mic():
    size = 128
    image = canvas(size, size, WHITE)
    draw = ImageDraw.Draw(image)
    s = SS
    solid = WHITE + (255,)
    draw.ellipse([4 * s, 4 * s, 124 * s, 124 * s], outline=solid, width=6 * s)
    draw.rounded_rectangle([50 * s, 26 * s, 78 * s, 76 * s], radius=14 * s, fill=solid)
    draw.arc([38 * s, 44 * s, 90 * s, 96 * s], start=0, end=180, fill=solid, width=6 * s)
    draw.line([(64 * s, 96 * s), (64 * s, 106 * s)], fill=solid, width=6 * s)
    return finish(image, size, size)


def build_tab_tick():
    width, height = 48, 10
    image = canvas(width, height, YELLOW)
    s = SS
    ImageDraw.Draw(image).polygon([(7 * s, 0), (48 * s, 0), (41 * s, 10 * s), (0, 10 * s)], fill=YELLOW + (255,))
    return finish(image, width, height)


def build_panel_header():
    width, height = 128, 58
    image = canvas(width, height, NAVY)
    s = SS
    # 패널 테두리(5px) 안쪽에 붙으므로 깎임 길이는 패널 안쪽 깎임과 같다.
    cut = inner_cut(36, 5) * s
    ImageDraw.Draw(image).polygon([(cut, 0), (width * s, 0), (width * s, height * s), (0, height * s), (0, cut)], fill=NAVY + (255,))
    return finish(image, width, height)


def build_panel_shadow():
    size = 256
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).polygon(chamfer(40, 40, 216, 216, 36), fill=int(255 * 0.32))
    image = Image.new("RGBA", (size, size), NAVY + (0,))
    image.putalpha(mask.filter(ImageFilter.GaussianBlur(24)))
    return image


def rgba_from_arrays(rgb, alpha):
    data = np.dstack([rgb, alpha]).clip(0, 255).astype(np.uint8)
    return Image.fromarray(data, "RGBA")


def build_streak():
    width, height = 512, 24
    x = np.linspace(0, 1, width)
    along = np.interp(x, [0, 0.55, 1], [0, 1, 0])
    across = np.exp(-((np.arange(height) - (height - 1) / 2) ** 2) / (2 * 3.5 ** 2))
    alpha = across[:, None] * along[None, :] * 255
    return rgba_from_arrays(np.dstack([np.full((height, width), 255)] * 3), alpha)


def build_loading_shade():
    """로딩 화면 아래쪽 글자가 배경 위에서 읽히게 까는 남색 그라디언트. 위는 투명, 아래로 갈수록 진하다."""
    width, height = 8, 256
    t = np.linspace(0, 1, height)[:, None] * np.ones((1, width))
    alpha = 0.64 * t * t * (3 - 2 * t) * 255
    return rgba_from_arrays(np.dstack([np.full((height, width), c) for c in NAVY]), alpha)


def build_logo_flash():
    """로고 조각이 다 모인 순간 번쩍이는 흰 실루엣. 글로우가 잘리지 않게 사방 32px 키운다.

    TitleLogo.png 알파를 그대로 쓰므로 로고를 바꾸면 이 스크립트를 다시 돌린다.
    """
    pad, blur = 32, 16
    logo = Image.open(OUTPUT_ROOT / "TitleLogo.png").convert("RGBA")
    alpha = Image.new("L", (logo.width + pad * 2, logo.height + pad * 2), 0)
    alpha.paste(logo.getchannel("A"), (pad, pad))
    glow = alpha.filter(ImageFilter.GaussianBlur(blur)).point(lambda a: int(a * 0.8))
    image = Image.new("RGBA", alpha.size, WHITE + (0,))
    image.putalpha(ImageChops.lighter(alpha, glow))
    return image


def build_all():
    return {
        # 9-slice 프레임
        "Panel": framed(160, 160, 36, 5, PANEL, NAVY),
        "PanelShadow": build_panel_shadow(),
        "PanelHeader": build_panel_header(),
        "Input_Normal": framed(96, 96, 15, 3, INPUT, LINE, top_shade=4, shade_alpha=0.07),
        "Input_Hovered": framed(96, 96, 15, 3, INPUT, LINE_HOVER, top_shade=4, shade_alpha=0.07),
        "Input_Focused": framed(96, 96, 15, 3, INPUT_FOCUS, CYAN),
        "Input_Error": framed(96, 96, 15, 3, INPUT, ERROR, top_shade=4, shade_alpha=0.07),
        "ButtonPrimary_Normal": framed(128, 96, 18, 4, YELLOW, NAVY, bottom_shade=6, shade_alpha=0.18),
        "ButtonPrimary_Hovered": framed(128, 96, 18, 4, YELLOW_HOVER, NAVY, bottom_shade=6, shade_alpha=0.18),
        "ButtonPrimary_Pressed": framed(128, 96, 18, 4, YELLOW, NAVY, top_shade=5, shade_alpha=0.20),
        "ButtonPrimary_Disabled": framed(128, 96, 18, 4, DISABLED_FILL, DISABLED_LINE),
        "PromptBar": framed(96, 96, 18, 0, NAVY, NAVY),
        # 흰색 리소스는 Tint 로 색을 입힌다 (탭 묶음은 파랑, 키 표시는 청록).
        "TabGroup_Frame": outline_rect(64, 3, WHITE),
        "Chip_Frame": outline_rect(48, 2, WHITE),
        # 키보드·패드 포커스 링. 주 버튼처럼 모서리를 깎은 모양이고, 흰색이라 청록으로 Tint 한다.
        "FocusRing": framed(128, 96, 22, 3, None, WHITE),
        "TabTick": build_tab_tick(),
        "Icon_Star": build_star(64),
        "Icon_Mic": build_mic(),
        "Icon_StatusInfo": build_status_icon("info"),
        "Icon_StatusError": build_status_icon("error"),
        "Icon_StatusCheck": build_status_icon("check"),
        "Sparkle": build_sparkle(128),
        "Streak": build_streak(),
        "TitleLogo_Flash": build_logo_flash(),
        "LoadingShade": build_loading_shade(),
    }


def main():
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    for name, image in build_all().items():
        image.save(OUTPUT_ROOT / f"{name}.png", optimize=True)
        print(f"{name}.png {image.size[0]}x{image.size[1]}")


if __name__ == "__main__":
    main()
