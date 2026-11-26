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

def png(path, pixel, size=None):
    if size is None:
        original = path.read_bytes()
        if original[:8] != b"\x89PNG\r\n\x1a\n":
            raise ValueError(f"not PNG: {path}")
        w, h = struct.unpack(">II", original[16:24])
    else:
        w, h = size
        path.parent.mkdir(parents=True, exist_ok=True)
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

def wallpaper(x,y,w,h):
    u,v=x/w,y/h
    pink=math.exp(-((u-.11)**2*10+(v-.78)**2*6))
    blue=math.exp(-((u-.89)**2*9+(v-.26)**2*6))
    violet=math.exp(-((u-.47)**2*7+(v-.11)**2*8))
    r=39+65*pink+25*violet+13*blue
    g=41+25*pink+22*violet+58*blue
    b=85+36*pink+60*violet+62*blue
    # Overlapping curved, translucent sheets with a directional specular seam.
    for i in range(3):
        center=(.22+i*.29)+.17*math.sin(u*4.4+i*.85)-.16*u
        d=v-center
        body=math.exp(-((d-.048)/.115)**2)
        seam=math.exp(-((d+.008)/.0065)**2)
        soft=math.exp(-((d+.013)/.033)**2)
        shadow=math.exp(-((d-.123)/.045)**2)
        strength=.65+.35*math.sin(u*2.9+i*.5)**2
        r+=strength*(body*(22+i*4)+seam*39+soft*13-shadow*12)
        g+=strength*(body*(20+i*6)+seam*42+soft*12-shadow*12)
        b+=strength*(body*31+seam*39+soft*12-shadow*9)
    return clamp(r),clamp(g),clamp(b),255

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
    rim = math.exp(-abs(d+1)*1.05)
    bevel = math.exp(-abs(d+4)*.35)
    base = (36 if subtle else 62) + 16*(1-v)
    a = coverage*(base+rim*82*(1-.65*v)+bevel*14)
    return (clamp(108+110*rim),clamp(119+110*rim),clamp(183+61*rim),clamp(a))

def landscape(x,y,w,h):
    u,v=x/w,y/h
    # Original illustrated twilight lake and mountain silhouette, no borrowed assets.
    sky=min(1,v/.64)
    r=64+107*sky;g=78+34*sky;b=153+32*sky
    moon=math.hypot((u-.77)*w/h,v-.18)
    if moon<.13:
        glow=max(0,min(1,(.13-moon)*130))
        r=r*(1-glow)+234*glow;g=g*(1-glow)+218*glow;b=b*(1-glow)+250*glow
    if v<.6:
        cloud=math.exp(-((v-.35-.034*math.sin(u*22))/.022)**2)*(.5+.5*math.sin(u*15)**2)
        r+=40*cloud;g+=20*cloud;b+=19*cloud
    def ridge(t):
        return .67-.28*max(0,1-abs(t-.48)/.32)-.07*math.exp(-((t-.13)/.13)**2)+.017*math.sin(t*34)
    horizon=.68
    rv=v if v<=horizon else horizon-(v-horizon)*.72
    if rv>ridge(u) and rv<horizon:
        snow=max(0,min(1,(ridge(u)+.075-rv)*35)) if abs(u-.48)<.21 else 0
        r=57+130*snow;g=73+116*snow;b=123+107*snow
    if v>=horizon:
        shimmer=(.5+.5*math.sin(v*410+math.sin(u*63)*2))**9
        a=.65+(v-horizon)*.6
        r=r*.56+40+shimmer*42;g=g*.52+35+shimmer*25;b=b*.6+55
        r*=a;g*=a;b*=a
    # Subtle darkening under centered lock-screen text.
    shade=1-.15*math.exp(-((u-.5)*2.8)**2)
    return clamp(r*shade),clamp(g*shade),clamp(b*shade),255

def main(root):
    png(root/"Background.png", wallpaper)
    png(root/"GlassNX"/"Lockscreen.png",landscape,(1920,1080))
    png(root/"GlassNX"/"Unlock.png",glass,(640,140))
    png(root/"GlassNX"/"Alert.png",glass,(1200,108))
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
