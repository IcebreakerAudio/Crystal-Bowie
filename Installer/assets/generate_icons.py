#!/usr/bin/env python3
"""Generate placeholder installer art from a single source logo.

Run once (and again whenever assets/icon-source.png is replaced with real branding):

    python Installer/assets/generate_icons.py

Regenerates, from assets/icon-source.png:
  - Windows/icon.ico            (multi-resolution, used as the Inno Setup wizard/uninstaller icon)
  - macOS/resources/AppIcon.icns
  - macOS/resources/background.png (productbuild wizard background)

Reusing this for a future project: replace assets/icon-source.png with real square (1024x1024+)
artwork and re-run this script — nothing else in Installer/ needs to change.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

INSTALLER_DIR = Path(__file__).resolve().parent.parent
ASSETS_DIR = INSTALLER_DIR / "assets"
SOURCE_LOGO = ASSETS_DIR / "icon-source.png"
WINDOWS_ICO = INSTALLER_DIR / "Windows" / "icon.ico"
MACOS_ICNS = INSTALLER_DIR / "macOS" / "resources" / "AppIcon.icns"
MACOS_BACKGROUND = INSTALLER_DIR / "macOS" / "resources" / "background.png"

LOGO_SIZE = 1024
BACKGROUND_SIZE = (620, 418)
BAND_WIDTH = 200

BG_COLOR = (28, 30, 36)
ACCENT_COLOR = (214, 168, 92)
PANEL_COLOR = (245, 244, 240)


def _load_bold_font(size: int) -> ImageFont.FreeTypeFont:
    for candidate in (
        "C:/Windows/Fonts/arialbd.ttf",
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    ):
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default(size=size)


def generate_source_logo() -> Image.Image:
    """A simple placeholder mark: solid dark square, accent-colored ring, initials."""
    img = Image.new("RGBA", (LOGO_SIZE, LOGO_SIZE), BG_COLOR + (255,))
    draw = ImageDraw.Draw(img)

    margin = LOGO_SIZE // 10
    draw.ellipse(
        [margin, margin, LOGO_SIZE - margin, LOGO_SIZE - margin],
        outline=ACCENT_COLOR,
        width=LOGO_SIZE // 40,
    )

    font = _load_bold_font(LOGO_SIZE // 3)
    text = "CB"
    bbox = draw.textbbox((0, 0), text, font=font)
    text_w, text_h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text(
        ((LOGO_SIZE - text_w) / 2 - bbox[0], (LOGO_SIZE - text_h) / 2 - bbox[1]),
        text,
        fill=PANEL_COLOR,
        font=font,
    )

    img.save(SOURCE_LOGO)
    print(f"wrote {SOURCE_LOGO}")
    return img


def generate_windows_ico(logo: Image.Image) -> None:
    WINDOWS_ICO.parent.mkdir(parents=True, exist_ok=True)
    logo.save(
        WINDOWS_ICO,
        format="ICO",
        sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    print(f"wrote {WINDOWS_ICO}")


def generate_macos_icns(logo: Image.Image) -> None:
    MACOS_ICNS.parent.mkdir(parents=True, exist_ok=True)
    try:
        logo.save(MACOS_ICNS, format="ICNS")
        print(f"wrote {MACOS_ICNS}")
    except (OSError, ValueError) as exc:
        # Pillow's ICNS writer is cross-platform, but if it ever can't run here, fall back to a
        # .iconset folder + the one-line command that finishes the job on a real Mac.
        iconset_dir = MACOS_ICNS.with_suffix(".iconset")
        iconset_dir.mkdir(parents=True, exist_ok=True)
        for size in (16, 32, 128, 256, 512):
            logo.resize((size, size), Image.LANCZOS).save(iconset_dir / f"icon_{size}x{size}.png")
            logo.resize((size * 2, size * 2), Image.LANCZOS).save(
                iconset_dir / f"icon_{size}x{size}@2x.png"
            )
        print(
            f"Pillow could not write {MACOS_ICNS} directly ({exc}). "
            f"Wrote {iconset_dir} instead — on a Mac, finish with:\n"
            f"  iconutil -c icns {iconset_dir} -o {MACOS_ICNS}"
        )


def generate_macos_background(logo: Image.Image) -> None:
    MACOS_BACKGROUND.parent.mkdir(parents=True, exist_ok=True)
    img = Image.new("RGB", BACKGROUND_SIZE, PANEL_COLOR)
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, BAND_WIDTH, BACKGROUND_SIZE[1]], fill=BG_COLOR)

    mark = logo.convert("RGBA").resize((BAND_WIDTH - 60, BAND_WIDTH - 60), Image.LANCZOS)
    paste_x = (BAND_WIDTH - mark.width) // 2
    paste_y = (BACKGROUND_SIZE[1] - mark.height) // 2
    img.paste(mark, (paste_x, paste_y), mark)

    img.save(MACOS_BACKGROUND)
    print(f"wrote {MACOS_BACKGROUND}")


def main() -> None:
    logo = generate_source_logo()
    generate_windows_ico(logo)
    generate_macos_icns(logo)
    generate_macos_background(logo)


if __name__ == "__main__":
    main()
