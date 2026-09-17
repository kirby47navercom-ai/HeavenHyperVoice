"""타이틀 로고 레이어를 글자·장식 단위로 잘라 UI 리소스 폴더에 넣는다.

원본은 output/imagegen/heaven-hypervoice-layers-refined 이다. 모든 원본이 2172×724 같은 캔버스라
그대로 겹치면 로고가 되지만, 한 장씩 텍스처로 쓰기엔 빈 여백이 크다. 그래서 보이는 부분만 잘라 두고,
잘린 위치(로고 캔버스 기준 x, y)는 출력으로 알려 준다. 배치 값은 Client/UI/Images/Frontend/README.md.

실행:
    python Client/Scripts/Art/Frontend/crop_logo_layers.py
"""

from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[4]
SOURCE_ROOT = PROJECT_ROOT / "output" / "imagegen" / "heaven-hypervoice-layers-refined"
OUTPUT_ROOT = PROJECT_ROOT / "Client" / "UI" / "Images" / "Frontend"

# 겹침 순서(아래 → 위)대로 적는다. 이름은 텍스처 이름이 된다.
LAYERS = [
    ("Logo_Wave", "00_background.png"),
    ("Logo_Glyph1_Po", "01_1_po.png"),
    ("Logo_Glyph2_Ket", "01_2_ket.png"),
    ("Logo_Glyph3_Mon", "01_3_mon.png"),
    ("Logo_Glyph4_Seu", "01_4_seu.png"),
    ("Logo_Glyph5_Teo", "01_5_teo.png"),
    ("Logo_Subtitle", "02_subtitle.png"),
    ("Logo_FeatherLeft", "individual_effects/feather_left.png"),
    ("Logo_FeatherRight", "individual_effects/feather_right.png"),
    ("Logo_Shout", "03_shout.png"),
    ("Logo_StarLeft", "individual_effects/star_left.png"),
    ("Logo_StarRight", "individual_effects/star_right.png"),
]

# 글로우 끝이 잘리지 않게 보이는 영역보다 조금 넉넉히 자른다.
PADDING = 4


def main():
    for name, file_name in LAYERS:
        image = Image.open(SOURCE_ROOT / file_name).convert("RGBA")
        left, top, right, bottom = image.getchannel("A").getbbox()
        box = (
            max(0, left - PADDING),
            max(0, top - PADDING),
            min(image.width, right + PADDING),
            min(image.height, bottom + PADDING),
        )
        image.crop(box).save(OUTPUT_ROOT / f"{name}.png", optimize=True)
        print(f"{name}: x={box[0]} y={box[1]} w={box[2] - box[0]} h={box[3] - box[1]}")


if __name__ == "__main__":
    main()
