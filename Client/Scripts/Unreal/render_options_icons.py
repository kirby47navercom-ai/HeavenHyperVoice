"""Render the bundled OFL Font Awesome glyphs into portable UI textures (Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "SourceArt/UI/Options"
GLYPHS = {
    "Resume": "\uf04b", "Settings": "\uf013", "Characters": "\uf0c0",
    "Logout": "\uf08b", "Menu": "\uf0c9", "Back": "\uf060",
    "Profile": "\uf1b0", "Compass": "\uf14e",
}

if __name__ == "__main__":
    destination = SOURCE / "Icons"
    destination.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype(str(SOURCE / "FontAwesome.ttf"), 384)
    for name, glyph in GLYPHS.items():
        icon = Image.new("RGBA", (512, 512))
        draw = ImageDraw.Draw(icon)
        left, top, right, bottom = draw.textbbox((0, 0), glyph, font=font)
        draw.text(((512 - right + left) / 2 - left, (512 - bottom + top) / 2 - top),
                  glyph, font=font, fill="white")
        icon.resize((128, 128), Image.Resampling.LANCZOS).save(destination / f"{name}.png")
