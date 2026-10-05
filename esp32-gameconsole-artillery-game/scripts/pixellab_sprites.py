"""Generate a cohesive, reproducible Tank Duel sprite set.

The default path draws every asset locally from one palette and outline rule,
then packs the PNGs to RGB565 + 1-bit masks in sprites_data.hpp. This keeps
regeneration deterministic and prevents stray fragments from AI sprite sheets.

Pass --ai to use the legacy Pixel Lab style-key workflow.

Reads PIXELLAB_API_KEY from the environment or `.local.env`.
"""

from __future__ import annotations

import argparse
import base64
import io
import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.stderr.write("pip install pillow\n")
    raise

ROOT = Path(__file__).resolve().parents[1]
PNG_DIR = ROOT / "assets" / "sprites"
HEADER = ROOT / "game" / "include" / "artillery" / "sprites_data.hpp"
API = "https://api.pixellab.ai/v2"

STYLE = (
    "cute 16-bit side-view artillery game, Worms-like cartoon tanks, "
    "chunky single black outline, medium flat shading, limited palette of "
    "burnt orange, rust, teal, leaf green, dirt brown, cream, and sunflower yellow. "
    "Same outline weight and pixel size on every object. No photorealism, "
    "no sci-fi hovercraft, no lineless art, no UI text."
)

STYLE_KEY = {
    "width": 128,
    "height": 128,
    "seed": 101,
    "description": (
        "style reference plate: a round cartoon tank hull with black treads and no cannon, "
        "a matching teal tank of the same silhouette, a fluffy cream cloud, a simple sun "
        "with short rays, a yellow cannonball, and a small orange fireburst. "
        + STYLE
    ),
}

SPRITES = [
    {
        "name": "tank_p0",
        "width": 48,
        "height": 32,
        "target_w": 32,
        "style": "key",
        "seed": 42,
        "description": (
            "side-view cartoon tank hull facing right, burnt orange and rust, "
            "round turret, black caterpillar treads with wheels, NO gun, NO cannon barrel, "
            "transparent background. " + STYLE
        ),
    },
    {
        "name": "tank_p1",
        "width": 48,
        "height": 32,
        "target_w": 32,
        "style": "tank_p0",
        "seed": 43,
        "description": (
            "the same cartoon tank hull facing right, teal and cyan body, "
            "round turret, black caterpillar treads with wheels, NO gun, NO cannon barrel, "
            "identical silhouette to an orange twin, transparent background. " + STYLE
        ),
    },
    {
        "name": "cloud",
        "width": 48,
        "height": 32,
        "target_w": 40,
        "style": "key",
        "seed": 7,
        "description": (
            "one fluffy cream pixel-art cloud with grey underside shading, "
            "chunky black outline, transparent background. " + STYLE
        ),
    },
    {
        "name": "sun",
        "width": 32,
        "height": 32,
        "target_w": 24,
        "style": "key",
        "seed": 3,
        "description": (
            "simple sunflower-yellow sun with a cream core and short triangular rays, "
            "chunky black outline, transparent background. " + STYLE
        ),
    },
    {
        "name": "shell",
        "width": 32,
        "height": 32,
        "target_w": 8,
        "style": "key",
        "seed": 11,
        "description": (
            "tiny round yellow cannonball with a cream highlight, "
            "chunky black outline, transparent background. " + STYLE
        ),
    },
    {
        "name": "boom",
        "width": 32,
        "height": 32,
        "target_w": 20,
        "style": "key",
        "seed": 19,
        "description": (
            "small cartoon explosion burst, orange fire core, yellow sparks, "
            "chunky black outline, transparent background. " + STYLE
        ),
    },
    {
        "name": "grass",
        "width": 32,
        "height": 32,
        "target_w": 12,
        "style": "key",
        "seed": 5,
        "description": (
            "tiny grass tuft, three pointed leaf-green blades, dirt at the base, "
            "chunky black outline, transparent background. " + STYLE
        ),
    },
]


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def load_local_env() -> None:
    path = ROOT / ".local.env"
    if not path.is_file():
        return
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip("'").strip('"')
        if key and key not in os.environ:
            os.environ[key] = value


def api_token() -> str:
    load_local_env()
    tok = os.environ.get("PIXELLAB_API_KEY", "").strip()
    if not tok:
        sys.stderr.write("Set PIXELLAB_API_KEY or add it to .local.env\n")
        sys.exit(2)
    return tok


def http_json(method: str, path: str, body: dict | None = None) -> dict:
    data = None if body is None else json.dumps(body).encode("utf-8")
    headers = {
        "Authorization": "Bearer " + api_token(),
        "Accept": "application/json",
        "User-Agent": "tank-duel-pixellab",
    }
    if body is not None:
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(API + path, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=180) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        err = e.read().decode("utf-8", errors="replace")
        raise SystemExit(f"{method} {path} HTTP {e.code}: {err}") from e


def decode_png_b64(blob: str) -> Image.Image:
    if blob.startswith("data:"):
        blob = blob.split(",", 1)[1]
    return Image.open(io.BytesIO(base64.b64decode(blob))).convert("RGBA")


def extract_image(payload: dict) -> Image.Image:
    image = payload.get("image") or payload
    b64 = image.get("base64") if isinstance(image, dict) else None
    if not b64 and isinstance(payload.get("images"), list) and payload["images"]:
        first = payload["images"][0]
        b64 = first.get("base64") if isinstance(first, dict) else None
        if isinstance(first, dict) and not b64:
            inner = first.get("image") or {}
            b64 = inner.get("base64")
    if not b64:
        raise SystemExit(f"no image in response keys={list(payload.keys())}")
    return decode_png_b64(b64)


def to_b64_image(im: Image.Image) -> dict:
    buf = io.BytesIO()
    im.save(buf, format="PNG")
    raw = base64.b64encode(buf.getvalue()).decode("ascii")
    return {"type": "base64", "base64": raw, "format": "png"}


def palette_image(im: Image.Image, side: int = 16) -> Image.Image:
    colors = []
    seen = set()
    pix = im.convert("RGBA").load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if a < 128:
                continue
            key = (r & 0xF8, g & 0xFC, b & 0xF8)
            if key in seen:
                continue
            seen.add(key)
            colors.append(key)
            if len(colors) >= 24:
                break
        if len(colors) >= 24:
            break
    if not colors:
        colors = [(232, 124, 36), (36, 196, 204), (124, 184, 72), (255, 214, 96)]
    pal = Image.new("RGBA", (side, side), (0, 0, 0, 255))
    draw = ImageDraw.Draw(pal)
    n = max(1, int(len(colors) ** 0.5 + 0.999))
    cw = max(1, side // n)
    for i, (r, g, b) in enumerate(colors):
        x = (i % n) * cw
        y = (i // n) * cw
        draw.rectangle([x, y, x + cw - 1, y + cw - 1], fill=(r, g, b, 255))
    return pal


def generate_pixen(description: str, width: int, height: int, seed: int) -> Image.Image:
    print(f"pixen {width}x{height} seed={seed} ...", flush=True)
    out = http_json(
        "POST",
        "/create-image-pixen",
        {
            "description": description,
            "image_size": {"width": width, "height": height},
            "no_background": True,
            "view": "side",
            "direction": "east",
            "outline": "single color black outline",
            "detail": "medium detail",
            "seed": seed,
            "enhance_prompt": False,
        },
    )
    return extract_image(out)


def generate_bitforge(
    description: str,
    width: int,
    height: int,
    seed: int,
    style: Image.Image,
    palette: Image.Image,
) -> Image.Image:
    print(f"bitforge {width}x{height} seed={seed} ...", flush=True)
    out = http_json(
        "POST",
        "/create-image-bitforge",
        {
            "description": description,
            "negative_description": "photorealistic, sci-fi, hover tank, lineless, 3d render, different art style",
            "image_size": {"width": width, "height": height},
            "no_background": True,
            "view": "side",
            "direction": "east",
            "outline": "single color black outline",
            "shading": "medium shading",
            "detail": "medium detail",
            "style_strength": 70,
            "text_guidance_scale": 8,
            "style_image": to_b64_image(style),
            "color_image": to_b64_image(palette),
            "seed": seed,
        },
    )
    return extract_image(out)


def generate_with_style(description: str, style: Image.Image, seed: int) -> Image.Image:
    print(f"generate-with-style seed={seed} ...", flush=True)
    body = {
        "description": description,
        "style_description": STYLE,
        "no_background": True,
        "seed": seed,
        "style_images": [
            {
                "image": to_b64_image(style),
                "width": style.width,
                "height": style.height,
            }
        ],
    }
    accepted = http_json("POST", "/generate-with-style-v2", body)
    job_id = accepted.get("background_job_id")
    if not job_id:
        return extract_image(accepted)
    for _ in range(48):
        time.sleep(5)
        job = http_json("GET", f"/background-jobs/{job_id}")
        status = job.get("status")
        print(f"  job {status}", flush=True)
        if status == "completed":
            return extract_image(job.get("last_response") or job)
        if status == "failed":
            raise SystemExit(f"style job failed: {job}")
    raise SystemExit(f"style job timed out: {job_id}")


def crop_alpha(im: Image.Image, pad: int = 1) -> Image.Image:
    bbox = im.getbbox()
    if bbox is None:
        return im
    x0, y0, x1, y1 = bbox
    x0 = max(0, x0 - pad)
    y0 = max(0, y0 - pad)
    x1 = min(im.width, x1 + pad)
    y1 = min(im.height, y1 + pad)
    return im.crop((x0, y0, x1, y1))


def fit_canvas(im: Image.Image, width: int, height: int) -> Image.Image:
    return im.resize((width, height), Image.NEAREST)


def fit_width(im: Image.Image, target_w: int) -> Image.Image:
    if im.width <= target_w:
        return im
    h = max(1, int(round(im.height * target_w / im.width)))
    return im.resize((target_w, h), Image.NEAREST)


def pack_sprite(im: Image.Image) -> tuple[int, int, list[int], list[int]]:
    rgb: list[int] = []
    mask: list[int] = []
    w, h = im.size
    pix = im.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            rgb.append(rgb565(r, g, b) if a >= 128 else 0)
            mask.append(1 if a >= 128 else 0)
    return w, h, rgb, mask


def c_array_u16(name: str, vals: list[int]) -> str:
    lines = [f"inline constexpr uint16_t {name}[] = {{"]
    row: list[str] = []
    for i, v in enumerate(vals):
        row.append(f"0x{v:04X}")
        if len(row) == 12 or i == len(vals) - 1:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    lines.append("};")
    return "\n".join(lines)


def c_array_mask(name: str, bits: list[int]) -> str:
    packed: list[int] = []
    acc = 0
    n = 0
    for b in bits:
        if b:
            acc |= 1 << n
        n += 1
        if n == 8:
            packed.append(acc)
            acc = 0
            n = 0
    if n:
        packed.append(acc)
    lines = [f"inline constexpr uint8_t {name}[] = {{"]
    row: list[str] = []
    for i, v in enumerate(packed):
        row.append(f"0x{v:02X}")
        if len(row) == 16 or i == len(packed) - 1:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    lines.append("};")
    return "\n".join(lines)


def hull_to_orange(im: Image.Image) -> Image.Image:
    out = im.copy()
    pix = out.load()
    w, h = out.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if a < 128 or r + g + b < 80:
                continue
            if g > r + 18 and b > r - 10:
                lum = (r + g + b) / 3.0
                pix[x, y] = (
                    min(255, int(lum * 1.65 + 20)),
                    min(255, int(lum * 0.65)),
                    min(255, int(lum * 0.22)),
                    a,
                )
    return out


ART_PALETTE = {
    "outline": (38, 35, 38, 255),
    "track": (55, 58, 61, 255),
    "track_hi": (94, 91, 82, 255),
    "orange": (205, 76, 35, 255),
    "orange_hi": (238, 117, 45, 255),
    "orange_dark": (142, 54, 35, 255),
    "teal": (39, 128, 108, 255),
    "teal_hi": (65, 164, 132, 255),
    "teal_dark": (30, 86, 78, 255),
    "cream": (247, 232, 183, 255),
    "cream_shadow": (211, 198, 157, 255),
    "yellow": (250, 190, 35, 255),
    "yellow_hi": (255, 225, 83, 255),
    "fire": (238, 83, 25, 255),
    "grass": (62, 145, 72, 255),
    "grass_hi": (102, 181, 77, 255),
    "dirt": (124, 77, 47, 255),
}


def blank(w: int, h: int) -> Image.Image:
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))


def handcrafted_tank(team: str) -> Image.Image:
    """One shared tank silhouette; only the team paint changes."""
    im = blank(38, 22)
    d = ImageDraw.Draw(im)
    p = ART_PALETTE
    base = p["orange"] if team == "orange" else p["teal"]
    hi = p["orange_hi"] if team == "orange" else p["teal_hi"]
    dark = p["orange_dark"] if team == "orange" else p["teal_dark"]

    # Turret and hull use the same one-pixel outline as every other sprite.
    d.polygon([(10, 3), (13, 1), (27, 1), (30, 3), (30, 7), (9, 7), (9, 4)],
              fill=p["outline"])
    d.rectangle((12, 2, 27, 5), fill=base)
    d.line((13, 2, 26, 2), fill=hi)
    d.rectangle((7, 6, 32, 12), fill=p["outline"])
    d.polygon([(5, 8), (31, 8), (35, 11), (32, 14), (7, 14), (3, 11)],
              fill=p["outline"])
    d.polygon([(7, 8), (30, 8), (33, 11), (31, 12), (8, 12), (5, 10)],
              fill=base)
    d.line((8, 8, 29, 8), fill=hi)
    d.line((9, 12, 30, 12), fill=dark)

    # Track pod and evenly spaced wheels.
    d.polygon([(6, 13), (32, 13), (35, 16), (32, 20), (7, 20), (3, 17)],
              fill=p["outline"])
    d.rectangle((7, 15, 31, 18), fill=p["track"])
    for cx in (9, 15, 22, 29):
        d.ellipse((cx - 2, 15, cx + 2, 19), fill=p["outline"])
        d.rectangle((cx - 1, 16, cx + 1, 18), fill=p["track_hi"])
        d.point((cx, 17), fill=dark)
    d.line((8, 20, 31, 20), fill=p["track_hi"])
    return im


def handcrafted_cloud() -> Image.Image:
    im = blank(34, 16)
    d = ImageDraw.Draw(im)
    p = ART_PALETTE
    # Joined circles avoid detached pixels and accidental neighboring objects.
    d.ellipse((1, 6, 14, 15), fill=p["outline"])
    d.ellipse((7, 2, 21, 15), fill=p["outline"])
    d.ellipse((16, 0, 28, 15), fill=p["outline"])
    d.ellipse((23, 6, 33, 15), fill=p["outline"])
    d.rectangle((6, 8, 28, 15), fill=p["outline"])
    d.ellipse((3, 7, 14, 13), fill=p["cream"])
    d.ellipse((9, 3, 21, 13), fill=p["cream"])
    d.ellipse((17, 2, 27, 13), fill=p["cream"])
    d.ellipse((23, 7, 31, 13), fill=p["cream"])
    d.rectangle((7, 8, 27, 13), fill=p["cream"])
    d.line((7, 13, 27, 13), fill=p["cream_shadow"])
    return im


def handcrafted_sun() -> Image.Image:
    im = blank(25, 25)
    d = ImageDraw.Draw(im)
    p = ART_PALETTE
    for box in ((11, 0, 13, 4), (11, 20, 13, 24), (0, 11, 4, 13), (20, 11, 24, 13),
                (3, 3, 6, 6), (18, 3, 21, 6), (3, 18, 6, 21), (18, 18, 21, 21)):
        d.rectangle(box, fill=p["outline"])
    d.ellipse((5, 5, 19, 19), fill=p["outline"])
    d.ellipse((7, 7, 17, 17), fill=p["yellow"])
    d.rectangle((9, 8, 14, 9), fill=p["yellow_hi"])
    return im


def handcrafted_shell() -> Image.Image:
    im = blank(7, 7)
    d = ImageDraw.Draw(im)
    p = ART_PALETTE
    d.ellipse((0, 0, 6, 6), fill=p["outline"])
    d.rectangle((2, 1, 4, 5), fill=p["yellow"])
    d.rectangle((1, 2, 5, 4), fill=p["yellow"])
    d.point((2, 2), fill=p["yellow_hi"])
    return im


def handcrafted_boom() -> Image.Image:
    im = blank(19, 19)
    d = ImageDraw.Draw(im)
    p = ART_PALETTE
    outer = [(9, 0), (11, 5), (16, 2), (14, 7), (18, 9), (14, 11),
             (16, 17), (11, 14), (9, 18), (7, 14), (2, 17), (4, 11),
             (0, 9), (4, 7), (2, 2), (7, 5)]
    d.polygon(outer, fill=p["outline"])
    inner = [(9, 3), (11, 7), (14, 5), (13, 9), (16, 9), (12, 11),
             (13, 14), (9, 12), (6, 15), (7, 11), (3, 9), (7, 8), (5, 5)]
    d.polygon(inner, fill=p["fire"])
    d.polygon([(9, 6), (11, 9), (9, 13), (7, 10)], fill=p["yellow"])
    d.point((9, 8), fill=p["yellow_hi"])
    return im


def handcrafted_grass() -> Image.Image:
    im = blank(11, 11)
    d = ImageDraw.Draw(im)
    p = ART_PALETTE
    d.polygon([(5, 9), (4, 2), (6, 0), (6, 8), (10, 3), (8, 9)], fill=p["outline"])
    d.polygon([(4, 9), (0, 4), (2, 3), (6, 9)], fill=p["outline"])
    d.polygon([(5, 8), (5, 3), (6, 2), (6, 8)], fill=p["grass_hi"])
    d.polygon([(6, 9), (9, 5), (8, 9)], fill=p["grass"])
    d.polygon([(4, 9), (2, 5), (3, 9)], fill=p["grass"])
    d.rectangle((2, 9, 8, 10), fill=p["dirt"])
    return im


def handcrafted_sprites() -> dict[str, Image.Image]:
    return {
        "tank_p0": handcrafted_tank("orange"),
        "tank_p1": handcrafted_tank("teal"),
        "cloud": handcrafted_cloud(),
        "sun": handcrafted_sun(),
        "shell": handcrafted_shell(),
        "boom": handcrafted_boom(),
        "grass": handcrafted_grass(),
    }


def save_handcrafted_style_assets(sprites: dict[str, Image.Image]) -> None:
    p = ART_PALETTE
    key = Image.new("RGBA", (128, 64), (115, 190, 214, 255))
    key.alpha_composite(sprites["cloud"], (7, 7))
    key.alpha_composite(sprites["sun"], (96, 5))
    key.alpha_composite(sprites["tank_p0"], (14, 38))
    key.alpha_composite(sprites["tank_p1"], (72, 38))
    key.alpha_composite(sprites["shell"], (57, 11))
    key.alpha_composite(sprites["boom"], (68, 7))
    key.save(PNG_DIR / "style_key.png")

    names = ["outline", "track", "orange", "orange_hi", "teal", "teal_hi",
             "cream", "cream_shadow", "yellow", "fire", "grass", "dirt"]
    pal = Image.new("RGBA", (48, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(pal)
    for i, name in enumerate(names):
        x = (i % 6) * 8
        y = (i // 6) * 8
        draw.rectangle((x, y, x + 7, y + 7), fill=p[name])
    pal.save(PNG_DIR / "style_palette.png")


def crop_box(im: Image.Image, box: tuple[int, int, int, int]) -> Image.Image:
    c = im.crop(box)
    bb = c.getbbox()
    return c.crop(bb) if bb else c


def assemble_from_key(key: Image.Image) -> dict[str, Image.Image]:
    tank = crop_box(key, (18, 58, 108, 126))
    return {
        "tank_p0": hull_to_orange(tank),
        "tank_p1": tank,
        "cloud": crop_box(key, (4, 4, 56, 40)),
        "sun": crop_box(key, (92, 2, 126, 40)),
        "boom": crop_box(key, (108, 56, 128, 92)),
        "shell": crop_box(key, (12, 39, 26, 53)),
    }


def emit_header(sprites: dict[str, tuple[int, int, list[int], list[int]]]) -> str:
    chunks = [
        "#pragma once",
        "",
        "// Generated by scripts/pixellab_sprites.py from the shared Tank Duel pixel-art palette.",
        "// Do not edit by hand; re-run the script if you regenerate assets/sprites.",
        "",
        "#include <cstdint>",
        "",
        "namespace artillery {",
        "",
        "struct PixelSprite {",
        "    int w;",
        "    int h;",
        "    const uint16_t* rgb;",
        "    const uint8_t* mask;",
        "};",
        "",
        "inline bool sprite_opaque(const uint8_t* mask, int i)",
        "{",
        "    return (mask[i >> 3] & (1u << (i & 7))) != 0;",
        "}",
        "",
    ]
    decls = []
    for name, (w, h, rgb, mask) in sprites.items():
        chunks.append(c_array_u16(f"kSpr_{name}_rgb", rgb))
        chunks.append("")
        chunks.append(c_array_mask(f"kSpr_{name}_mask", mask))
        chunks.append("")
        decls.append(
            f"inline constexpr PixelSprite kSpr_{name}{{{w}, {h}, kSpr_{name}_rgb, kSpr_{name}_mask}};"
        )
    chunks.extend(decls)
    chunks.append("")
    chunks.append("}  // namespace artillery")
    chunks.append("")
    return "\n".join(chunks)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--ai",
        action="store_true",
        help="use the legacy Pixel Lab generation path instead of deterministic local sprites",
    )
    parser.add_argument("--skip-generate", action="store_true", help="only convert existing PNGs")
    parser.add_argument("--regen-key", action="store_true", help="force a new style_key.png")
    parser.add_argument(
        "--bitforge",
        action="store_true",
        help="per-sprite bitforge (often copies the whole style plate; prefer default crop)",
    )
    parser.add_argument(
        "--pro-style",
        action="store_true",
        help="use generate-with-style-v2 (Pro) instead of cropping the style key",
    )
    args = parser.parse_args()
    PNG_DIR.mkdir(parents=True, exist_ok=True)

    if not args.ai and not args.skip_generate:
        images = handcrafted_sprites()
        save_handcrafted_style_assets(images)
        packed = {}
        for spec in SPRITES:
            im = images[spec["name"]]
            png_path = PNG_DIR / f"{spec['name']}.png"
            im.save(png_path)
            packed[spec["name"]] = pack_sprite(im)
            print(f"  handcrafted {spec['name']} {im.size}")
        HEADER.write_text(emit_header(packed), encoding="utf-8")
        print(f"wrote {HEADER}")
        return 0

    key_path = PNG_DIR / "style_key.png"
    pal_path = PNG_DIR / "style_palette.png"
    images: dict[str, Image.Image] = {}

    if args.skip_generate or (key_path.exists() and not args.regen_key):
        style_key = Image.open(key_path).convert("RGBA")
        print(f"reuse {key_path} {style_key.size}")
    else:
        style_key = generate_pixen(
            STYLE_KEY["description"], STYLE_KEY["width"], STYLE_KEY["height"], STYLE_KEY["seed"]
        )
        style_key.save(key_path)
        print(f"  saved {key_path} {style_key.size}")
    images["key"] = style_key
    palette = palette_image(style_key)
    palette.save(pal_path)

    packed = {}
    assembled = {} if args.skip_generate else assemble_from_key(style_key)
    for spec in SPRITES:
        png_path = PNG_DIR / f"{spec['name']}.png"
        if args.skip_generate and png_path.exists():
            im = Image.open(png_path).convert("RGBA")
        elif spec["name"] in assembled and not args.bitforge and not args.pro_style:
            im = assembled[spec["name"]]
            im.save(png_path)
            print(f"  from style_key {spec['name']} {im.size}")
        elif spec["name"] == "grass" and not args.bitforge and not args.pro_style:
            im = generate_pixen(spec["description"], spec["width"], spec["height"], spec["seed"])
            im.save(png_path)
            print(f"  saved {png_path} {im.size}")
        else:
            style_src = images.get(spec["style"], style_key)
            style_fit = fit_canvas(style_src, spec["width"], spec["height"])
            pal_fit = fit_canvas(palette, spec["width"], spec["height"])
            if args.pro_style:
                im = generate_with_style(spec["description"], style_fit, spec["seed"])
            else:
                im = generate_bitforge(
                    spec["description"],
                    spec["width"],
                    spec["height"],
                    spec["seed"],
                    style_fit,
                    pal_fit,
                )
            im.save(png_path)
            print(f"  saved {png_path} {im.size}")
        images[spec["name"]] = im
        packed_im = fit_width(crop_alpha(im), spec["target_w"])
        packed[spec["name"]] = pack_sprite(packed_im)
        print(f"  packed {spec['name']} {packed[spec['name']][0]}x{packed[spec['name']][1]}")

    HEADER.write_text(emit_header(packed), encoding="utf-8")
    print(f"wrote {HEADER}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
