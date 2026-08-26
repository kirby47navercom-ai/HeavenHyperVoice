"""정확한 포켓몬 원본으로 원형 HUD 얼굴 초상화를 일괄 생성한다."""

from pathlib import Path

from PIL import Image, ImageChops, ImageDraw


PROJECT_ROOT = Path(__file__).resolve().parents[4]
REFERENCE_ROOT = PROJECT_ROOT / "Client" / "Saved" / "Codex" / "PokemonPortraitRefs"
OUTPUT_ROOT = PROJECT_ROOT / "Client" / "SourceArt" / "Pokemon" / "Portraits"

OUTPUT_SIZE = 512
CONTENT_SIZE = 462
CIRCLE_INSET = 4
CIRCLE_BACKGROUND = (225, 246, 250, 255)
CIRCLE_RING = (74, 220, 231, 255)
CIRCLE_RING_WIDTH = 7


# 포켓몬마다 얼굴 위치가 달라 공통 좌표를 쓰지 않는다.
# 귀, 뿔, 머리 장식도 프로필 안에 들어오도록 종별 범위를 직접 지정했다.
PORTRAIT_JOBS = {
    "arceus": ("아르세우스", (65, 15, 300, 250)),
    "axew": ("터검니", (65, 15, 365, 315)),
    "bergmite": ("꽁어름", (35, 15, 440, 420)),
    "bulbasaur": ("이상해씨", (15, 55, 355, 395)),
    "charmander": ("파이리", (55, 15, 345, 305)),
    "chimchar": ("불꽃숭이", (85, 15, 375, 305)),
    "dialga": ("디아루가", (160, 10, 445, 295)),
    "eevee": ("이브이", (55, 10, 420, 375)),
    "giratina": ("기라티나", (105, 35, 385, 315)),
    "kricketot": ("귀뚤뚜기", (95, 10, 380, 295)),
    "pachirisu": ("파치리스", (20, 115, 305, 400)),
    "palkia": ("펄기아", (20, 45, 315, 340)),
    "pawniard": ("자망칼", (70, 35, 405, 370)),
    "pikachu": ("피카츄", (20, 20, 385, 385)),
    "piplup": ("팽도리", (75, 15, 395, 335)),
    "ralts": ("랄토스", (80, 10, 395, 325)),
    "shinx": ("꼬링크", (75, 45, 405, 375)),
    "squirtle": ("꼬부기", (65, 15, 365, 315)),
    "tinkatuff": ("벼리짱", (25, 45, 335, 355)),
    "turtwig": ("모부기", (45, 10, 370, 335)),
}


def make_antialiased_circle_mask() -> Image.Image:
    scale = 4
    large_size = OUTPUT_SIZE * scale
    inset = CIRCLE_INSET * scale
    mask = Image.new("L", (large_size, large_size), 0)
    draw = ImageDraw.Draw(mask)
    draw.ellipse(
        (inset, inset, large_size - inset - 1, large_size - inset - 1),
        fill=255,
    )
    return mask.resize((OUTPUT_SIZE, OUTPUT_SIZE), Image.Resampling.LANCZOS)


def make_antialiased_ring() -> Image.Image:
    scale = 4
    large_size = OUTPUT_SIZE * scale
    inset = CIRCLE_INSET * scale
    ring = Image.new("RGBA", (large_size, large_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(ring)
    draw.ellipse(
        (inset, inset, large_size - inset - 1, large_size - inset - 1),
        outline=CIRCLE_RING,
        width=CIRCLE_RING_WIDTH * scale,
    )
    return ring.resize((OUTPUT_SIZE, OUTPUT_SIZE), Image.Resampling.LANCZOS)


def build_portrait(
    source_name: str,
    korean_name: str,
    crop_box: tuple[int, int, int, int],
    circle_mask: Image.Image,
    circle_ring: Image.Image,
) -> Path:
    source_path = REFERENCE_ROOT / f"{source_name}.png"
    if not source_path.is_file():
        raise FileNotFoundError(f"원본 초상화를 찾지 못함: {source_path}")

    with Image.open(source_path).convert("RGBA") as source:
        cropped = source.crop(crop_box)
        cropped = cropped.resize(
            (CONTENT_SIZE, CONTENT_SIZE),
            Image.Resampling.LANCZOS,
        )

    offset = (OUTPUT_SIZE - CONTENT_SIZE) // 2
    character_layer = Image.new("RGBA", (OUTPUT_SIZE, OUTPUT_SIZE), (0, 0, 0, 0))
    character_layer.alpha_composite(cropped, (offset, offset))

    # 캐릭터 원본 픽셀은 유지하고, 원 바깥쪽 알파만 제거한다.
    circular_background = Image.new(
        "RGBA",
        (OUTPUT_SIZE, OUTPUT_SIZE),
        CIRCLE_BACKGROUND,
    )
    circular_background.putalpha(circle_mask)
    portrait = Image.alpha_composite(circular_background, character_layer)
    portrait.putalpha(ImageChops.multiply(portrait.getchannel("A"), circle_mask))
    portrait = Image.alpha_composite(portrait, circle_ring)

    # 네모 모서리가 남으면 UMG에서도 사각형으로 보이므로 저장 전에 알파를 검증한다.
    alpha = portrait.getchannel("A")
    corner_alpha = (
        alpha.getpixel((0, 0)),
        alpha.getpixel((OUTPUT_SIZE - 1, 0)),
        alpha.getpixel((0, OUTPUT_SIZE - 1)),
        alpha.getpixel((OUTPUT_SIZE - 1, OUTPUT_SIZE - 1)),
    )
    if any(value != 0 for value in corner_alpha):
        raise RuntimeError(f"{korean_name} 원형 마스크 모서리 알파 오류: {corner_alpha}")
    if alpha.getpixel((OUTPUT_SIZE // 2, OUTPUT_SIZE // 2)) != 255:
        raise RuntimeError(f"{korean_name} 원형 마스크 중심 알파 오류")

    output_dir = OUTPUT_ROOT / korean_name
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"T_{korean_name}_Portrait.png"
    portrait.save(output_path, format="PNG", optimize=True)
    return output_path


def main() -> None:
    circle_mask = make_antialiased_circle_mask()
    circle_ring = make_antialiased_ring()
    generated_paths = [
        build_portrait(
            source_name,
            korean_name,
            crop_box,
            circle_mask,
            circle_ring,
        )
        for source_name, (korean_name, crop_box) in PORTRAIT_JOBS.items()
    ]
    print(f"원형 포켓몬 얼굴 초상화 생성 완료: {len(generated_paths)}개")


if __name__ == "__main__":
    main()
