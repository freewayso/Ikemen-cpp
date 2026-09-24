#!/usr/bin/env python3
"""Generate a small pixel-art fire-cannon sprite pack (PNG, stdlib only)."""
import math
import os
import struct
import zlib

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "data", "cannon"))


def write_png(path, w, h, rgba):
    def chunk(tag, data):
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = b""
    row = w * 4
    for y in range(h):
        raw += b"\x00" + bytes(rgba[y * row : (y + 1) * row])
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def splat(px, w, h, cx, cy, rx, ry, r, g, b, a, power=1.6):
    x0 = max(0, int(cx - rx - 1))
    x1 = min(w - 1, int(cx + rx + 1))
    y0 = max(0, int(cy - ry - 1))
    y1 = min(h - 1, int(cy + ry + 1))
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            nx = (x + 0.5 - cx) / max(0.2, rx)
            ny = (y + 0.5 - cy) / max(0.2, ry)
            d = (nx * nx + ny * ny) ** 0.5
            if d >= 1:
                continue
            t = (1 - d) ** power
            i = (y * w + x) * 4
            src_a = a * t
            dst_a = px[i + 3] / 255.0
            out_a = src_a + dst_a * (1 - src_a)
            if out_a <= 0.001:
                continue
            def mix(sc, dc):
                return max(0, min(255, int((sc * src_a + dc * dst_a * (1 - src_a)) / out_a)))
            px[i] = mix(r, px[i])
            px[i + 1] = mix(g, px[i + 1])
            px[i + 2] = mix(b, px[i + 2])
            px[i + 3] = max(0, min(255, int(out_a * 255)))


def shot_frame(n, total=8):
    w, h = 56, 32
    px = bytearray(w * h * 4)
    t = n / max(1, total - 1)
    wobble = math.sin(n * 1.7) * 1.4
    del t
    # trailing fire (left) + cannon ball (right)
    for k in range(6, 0, -1):
        u = k / 6.0
        splat(px, w, h, 10 + (1 - u) * 18, 16 + wobble * (1 - u), 10 + u * 4, 5 + u * 3,
              180, 20, 8, int(40 + 50 * u), 1.2)
        splat(px, w, h, 14 + (1 - u) * 16, 16 + wobble * 0.6, 8, 4,
              255, 90, 10, int(50 + 70 * u), 1.4)
    splat(px, w, h, 38, 16 + wobble * 0.3, 13, 11, 160, 18, 0, 230, 1.3)
    splat(px, w, h, 40, 16, 10, 8, 255, 70, 8, 240, 1.4)
    splat(px, w, h, 42, 15.5, 7, 5.5, 255, 170, 30, 255, 1.6)
    splat(px, w, h, 44, 15, 3.5, 2.8, 255, 250, 200, 255, 1.8)
    # muzzle spark on later frames
    if n % 2:
        splat(px, w, h, 50, 14, 3, 2, 255, 255, 180, 200, 1.5)
    return w, h, 10, 16, px


def boom_frame(n, total=6):
    w, h = 48, 48
    px = bytearray(w * h * 4)
    u = n / max(1, total - 1)
    rad = 8 + u * 16
    splat(px, w, h, 24, 24, rad, rad * 0.92, 120, 12, 0, int(220 * (1 - u * 0.4)), 1.1)
    splat(px, w, h, 24, 24, rad * 0.72, rad * 0.68, 255, 80, 10, int(240 * (1 - u * 0.3)), 1.3)
    splat(px, w, h, 24, 23, rad * 0.42, rad * 0.4, 255, 200, 40, 255, 1.5)
    splat(px, w, h, 24, 22, 4 + (1 - u) * 3, 3, 255, 255, 230, 255, 1.7)
    for i in range(6):
        ang = i * math.pi / 3 + n * 0.4
        splat(px, w, h, 24 + math.cos(ang) * rad * 0.7, 24 + math.sin(ang) * rad * 0.7,
              3 + u * 2, 2 + u, 255, 120, 20, int(180 * (1 - u)), 1.4)
    return w, h, 24, 24, px


def muzzle_frame(n, total=4):
    w, h = 40, 28
    px = bytearray(w * h * 4)
    stretch = 8 + n * 4
    splat(px, w, h, 8 + n * 2, 14, stretch, 8 - n * 0.6, 200, 30, 0, 220, 1.2)
    splat(px, w, h, 12 + n * 2, 14, stretch * 0.7, 5, 255, 120, 20, 240, 1.4)
    splat(px, w, h, 16 + n * 2, 13.5, 6, 3, 255, 240, 160, 255, 1.7)
    return w, h, 4, 14, px


def main():
    os.makedirs(ROOT, exist_ok=True)
    meta = []
    for i in range(8):
        w, h, ax, ay, px = shot_frame(i)
        name = f"shot_{i:02d}.png"
        write_png(os.path.join(ROOT, name), w, h, px)
        meta.append(f"{name} {w} {h} {ax} {ay}")
    for i in range(6):
        w, h, ax, ay, px = boom_frame(i)
        name = f"boom_{i:02d}.png"
        write_png(os.path.join(ROOT, name), w, h, px)
        meta.append(f"{name} {w} {h} {ax} {ay}")
    for i in range(4):
        w, h, ax, ay, px = muzzle_frame(i)
        name = f"muzzle_{i:02d}.png"
        write_png(os.path.join(ROOT, name), w, h, px)
        meta.append(f"{name} {w} {h} {ax} {ay}")
    with open(os.path.join(ROOT, "pack.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(meta) + "\n")
    print("wrote", ROOT, "frames", len(meta))


if __name__ == "__main__":
    main()
