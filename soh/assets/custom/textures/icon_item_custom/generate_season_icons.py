"""Generates the four Rod of Seasons wheel icons (Skijer's NEI).

32x32 RGBA32, one glyph per season, tinted with the season's identity colour — the same palette
sSeasonColor holds in mods/extended_inventory.c. Keep the two in sync by hand: this script is the
art, that table is what the flame and the cell read.

Run from this directory: python generate_season_icons.py
"""

import math

from PIL import Image, ImageDraw

SIZE = 32
SS = 8  # supersample factor; the glyphs are drawn big and boxed down so the edges stay soft

# Season identity colours — mirror of sSeasonColor in mods/extended_inventory.c.
SEASONS = {
    "Spring": (255, 183, 213),
    "Summer": (255, 205, 70),
    "Autumn": (230, 120, 50),
    "Winter": (150, 215, 255),
}

OUTLINE = (30, 24, 40, 255)


def shade(color, factor):
    return tuple(max(0, min(255, int(c * factor))) for c in color)


def draw_spring(d, c, r):
    """Five-petal blossom."""
    cx = cy = SIZE * SS / 2
    petal = r * 0.46
    for i in range(5):
        a = math.radians(-90 + i * 72)
        px = cx + math.cos(a) * r * 0.52
        py = cy + math.sin(a) * r * 0.52
        d.ellipse([px - petal, py - petal, px + petal, py + petal], fill=c + (255,), outline=OUTLINE, width=SS)
    core = r * 0.24
    d.ellipse([cx - core, cy - core, cx + core, cy + core], fill=shade(c, 0.65) + (255,), outline=OUTLINE, width=SS)


def draw_summer(d, c, r):
    """Sun with eight rays."""
    cx = cy = SIZE * SS / 2
    for i in range(8):
        a = math.radians(i * 45)
        x0 = cx + math.cos(a) * r * 0.62
        y0 = cy + math.sin(a) * r * 0.62
        x1 = cx + math.cos(a) * r * 1.02
        y1 = cy + math.sin(a) * r * 1.02
        d.line([x0, y0, x1, y1], fill=c + (255,), width=int(r * 0.2))
    disc = r * 0.56
    d.ellipse([cx - disc, cy - disc, cx + disc, cy + disc], fill=c + (255,), outline=OUTLINE, width=SS)


def quad(p0, p1, p2, steps=24):
    """Quadratic bezier, sampled — PIL has no curve primitive."""
    pts = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        pts.append(
            (
                u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0],
                u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1],
            )
        )
    return pts


def draw_autumn(d, c, r):
    """A falling leaf: two bezier flanks meeting at tip and stem, with a midrib."""
    cx = cy = SIZE * SS / 2
    tip = (cx, cy - r * 0.98)
    base = (cx, cy + r * 0.52)
    # Control points sit outside the silhouette, which is what gives the flanks their belly.
    right = quad(tip, (cx + r * 1.16, cy - r * 0.30), base)
    left = quad(base, (cx - r * 1.16, cy - r * 0.30), tip)
    d.polygon(right + left, fill=c + (255,), outline=OUTLINE)

    rib = shade(c, 0.5) + (255,)
    d.line([tip, (base[0], base[1] + r * 0.42)], fill=rib, width=int(r * 0.12))
    # Veins, angled the way they leave a real midrib.
    for t, span in ((0.30, 0.42), (0.52, 0.50), (0.74, 0.38)):
        y = tip[1] + (base[1] - tip[1]) * t
        for side in (-1, 1):
            d.line([cx, y, cx + side * r * span, y + r * 0.20], fill=rib, width=int(r * 0.07))


def draw_winter(d, c, r):
    """Six-spoke snowflake with branches."""
    cx = cy = SIZE * SS / 2
    arm = int(r * 0.16)
    for i in range(6):
        a = math.radians(i * 60)
        ex = cx + math.cos(a) * r * 0.95
        ey = cy + math.sin(a) * r * 0.95
        d.line([cx, cy, ex, ey], fill=c + (255,), width=arm)
        # two branches per spoke, at the classic 60 degrees off the arm
        for side in (-1, 1):
            bx = cx + math.cos(a) * r * 0.58
            by = cy + math.sin(a) * r * 0.58
            ba = a + side * math.radians(60)
            d.line(
                [bx, by, bx + math.cos(ba) * r * 0.3, by + math.sin(ba) * r * 0.3],
                fill=c + (255,),
                width=int(arm * 0.75),
            )
    hub = r * 0.16
    d.ellipse([cx - hub, cy - hub, cx + hub, cy + hub], fill=c + (255,))


GLYPHS = {
    "Spring": draw_spring,
    "Summer": draw_summer,
    "Autumn": draw_autumn,
    "Winter": draw_winter,
}


def main():
    for name, color in SEASONS.items():
        img = Image.new("RGBA", (SIZE * SS, SIZE * SS), (0, 0, 0, 0))
        GLYPHS[name](ImageDraw.Draw(img), color, SIZE * SS * 0.42)
        img = img.resize((SIZE, SIZE), Image.LANCZOS)
        out = f"gItemIconSeason{name}Tex.rgba32.png"
        img.save(out)
        print("wrote", out)


if __name__ == "__main__":
    main()
