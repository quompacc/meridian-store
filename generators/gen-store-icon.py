#!/usr/bin/env python3
"""Cut the alpha channel for a Meridian icon from its single flat-background
render in assets/ (store, tray-update and the launcher/menu mark all use this).

The artwork ships as one opaque PNG: a rounded tile on a flat field — pure black
for the store/update marks, white for the compass. A global "key out the bg"
would punch holes in the tile, whose own backdrop is near-black navy. So instead
we flood-fill the background inward from the image border: only background-colour
pixels *connected to the edge* become transparent; the enclosed tile (and any
bright detail inside it) stays opaque. Pick the field with --bg {black,white}.

Downscale is premultiplied so the rim doesn't bleed a fringe; the white key
additionally feathers + un-mattes the rim against white so no light halo remains.

Deterministic. Usage: gen-store-icon.py [--src P] [--out FILE] [--size N] [--bg C] [--thresh T]
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)


def flood_background(mask):
    """Boolean mask of edge-connected `mask` pixels (scanline flood, 4-conn)."""
    H, W = mask.shape
    out = np.zeros((H, W), dtype=bool)
    stack = []
    for x in range(W):
        if mask[0, x]:
            stack.append((0, x))
        if mask[H - 1, x]:
            stack.append((H - 1, x))
    for y in range(H):
        if mask[y, 0]:
            stack.append((y, 0))
        if mask[y, W - 1]:
            stack.append((y, W - 1))
    while stack:
        y, x = stack.pop()
        if out[y, x] or not mask[y, x]:
            continue
        xl = x
        while xl > 0 and mask[y, xl - 1] and not out[y, xl - 1]:
            xl -= 1
        xr = x
        while xr < W - 1 and mask[y, xr + 1] and not out[y, xr + 1]:
            xr += 1
        out[y, xl:xr + 1] = True
        for ny in (y - 1, y + 1):
            if 0 <= ny < H:
                row, orow = mask[ny], out[ny]
                nx = xl
                while nx <= xr:
                    if row[nx] and not orow[nx]:
                        stack.append((ny, nx))
                        while nx <= xr and row[nx] and not orow[nx]:
                            nx += 1
                    nx += 1
    return out


def premultiplied_resize(rgb, alpha, size):
    a = alpha.astype(np.float64) / 255.0
    pm = (rgb.astype(np.float64) * a[..., None])
    pmI = Image.fromarray(np.clip(pm, 0, 255).astype(np.uint8), "RGB").resize((size, size), Image.LANCZOS)
    aI = Image.fromarray(alpha, "L").resize((size, size), Image.LANCZOS)
    pmA = np.asarray(pmI, dtype=np.float64)
    aA = np.asarray(aI, dtype=np.float64) / 255.0
    rgb2 = np.where(aA[..., None] > 1e-3, pmA / np.maximum(aA[..., None], 1e-3), 0)
    return Image.fromarray(np.dstack([np.clip(rgb2, 0, 255), aA * 255]).astype(np.uint8), "RGBA")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=os.path.join(REPO, "assets", "meridian-store_icon_new.png"))
    ap.add_argument("--out", default=os.path.join(REPO, "dist", "branding", "meridian-store.png"))
    ap.add_argument("--size", type=int, default=512, help="output edge in px (0 = native)")
    ap.add_argument("--bg", choices=("black", "white"), default="black",
                    help="flat background colour to key out (store/update render on black, menu on white)")
    ap.add_argument("--thresh", type=int, default=24, help="how close to the bg colour still counts as background")
    args = ap.parse_args()

    rgb = np.asarray(Image.open(args.src).convert("RGB"), dtype=np.uint8)
    # Per pixel: distance from the flat background colour. Low = background-like.
    bgness = (255 - rgb.min(axis=2)) if args.bg == "white" else rgb.max(axis=2)
    outside = flood_background(bgness < args.thresh)

    if args.bg == "white":
        # A hard white key leaves a light halo at the tile rim (edge pixels are
        # tile·a + white·(1-a), neither near-white nor opaque). Feather the mask,
        # then un-matte that band against white so the recovered colour is the
        # tile's — no halo survives the premultiplied downscale.
        from PIL import ImageFilter
        inside = Image.fromarray(np.where(outside, 0, 255).astype(np.uint8), "L")
        a = np.asarray(inside.filter(ImageFilter.GaussianBlur(2.0)), np.float64) / 255.0
        a3 = a[..., None]
        F = np.where(a3 > 0.004, (rgb.astype(np.float64) - (1 - a3) * 255.0) / np.maximum(a3, 1e-3), 0.0)
        rgb = np.clip(F, 0, 255).astype(np.uint8)
        alpha = np.clip(a * 255, 0, 255).astype(np.uint8)
    else:
        alpha = np.where(outside, 0, 255).astype(np.uint8)

    icon = premultiplied_resize(rgb, alpha, args.size) if args.size else \
        Image.fromarray(np.dstack([rgb, alpha]), "RGBA")
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    icon.save(args.out)
    a = np.asarray(icon)[..., 3]
    print(f"wrote {args.out}  ({icon.width}x{icon.height}, "
          f"opaque {(a > 250).mean()*100:.0f}%, transparent {(a < 5).mean()*100:.0f}%)")


if __name__ == "__main__":
    sys.exit(main())
