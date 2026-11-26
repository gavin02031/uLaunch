#!/usr/bin/env python3
"""GlassNX v0.1: deterministic offline glass assets; preserves upstream texture sizes."""
import math
import struct
import sys
import zlib
from pathlib import Path

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

def clamp(v):
    return min(255, max(0, int(v)))

def png(path, pixel):
    original = path.read_bytes()
    if original[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not PNG: {path}")
    w, h = struct.unpack(">II", original[16:24])
    if not (0 < w <= 4096 and 0 < h <= 2160):
        raise ValueError(f"unexpected dimensions: {path}: {w}x{h}")
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(pixel(x, y, w, h))
    header = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
    print(f"GlassNX: {path.name}: {w}x{h}")

def wallpaper(x, y, w, h):
    u, v = x / w, y / h
    # Smooth pastel pools, deep enough for white text contrast.
    pink = math.exp(-((u-.12)**2*7+(v-.84)**2*5))
    blue = math.exp(-((u-.86)**2*8+(v-.25)**2*5))
    lilac = math.exp(-((u-.48)**2*7+(v-.42)**2*6))
    wave = v - (.38 + .27*math.sin(u*3.6+.35))
    wave2 = v - (.92 - .46*u + .09*math.sin(u*5))
    sheen = 17*math.exp(-abs(wave)*52) + 14*math.exp(-abs(wave2)*45)
    band = 14*math.exp(-wave*wave*34) + 10*math.exp(-wave2*wave2*45)
    return (clamp(60+94*pink+18*blue+35*lilac+sheen+band),
            clamp(70+42*pink+65*blue+24*lilac+sheen+band),
            clamp(121+48*pink+69*blue+48*lilac+sheen+band),255)

def signed_distance(x, y, w, h, radius, inset):
    qx = abs(x+.5-w*.5) - (w*.5-inset-radius)
    qy = abs(y+.5-h*.5) - (h*.5-inset-radius)
    return math.hypot(max(qx,0),max(qy,0)) + min(max(qx,qy),0) - radius

def glass(x, y, w, h, outline=False, subtle=False):
    radius = min(38, w*.16, h*.38)
    d = signed_distance(x,y,w,h,radius,3)
    v=y/h
    if outline:
        # Transparent middle keeps game covers readable. Focus glow only.
        a = 220*math.exp(-((d+4)/2.7)**2) + 95*math.exp(-((d+4)/10)**2)
        return (219,230,255,clamp(a))
    coverage = min(1,max(0,.5-d))
    rim = math.exp(-abs(d+2)*.47)
    bevel = math.exp(-abs(d+7)*.15)
    base = (38 if subtle else 78) + 20*(1-v)
    a = coverage*(base+rim*117+bevel*19)
    return (clamp(184+55*rim),clamp(195+48*rim),clamp(240+15*rim),clamp(a))

def main(root):
    png(root/"Background.png", wallpaper)
    for name in ("Default","Application","Folder","Homebrew"):
        png(root/"Main"/"TopMenuBackground"/(name+".png"),glass)
    for name in ("Main/InputBarBackground.png","Settings/InputBarBackground.png"):
        png(root/name,glass)
    png(root/"Main"/"EntryMenuBackground.png",lambda x,y,w,h:glass(x,y,w,h,subtle=True))
    png(root/"Main"/"OverIcon"/"Cursor.png",lambda x,y,w,h:glass(x,y,w,h,outline=True))

if __name__ == "__main__":
    if len(sys.argv)!=2:
        raise SystemExit("usage: glassnx_assets.py THEME_UI_DIR")
    main(Path(sys.argv[1]))
