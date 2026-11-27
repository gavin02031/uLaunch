"""Original antialiased GlassNX icon family. No external artwork."""
from pathlib import Path
from PIL import Image, ImageDraw
import math
PALETTE={
"home":(120,156,232),"games":(69,165,183),"apps":(139,139,230),"settings":(114,133,174),
"wifi":(89,170,233),"bluetooth":(131,147,236),"sun":(222,174,99),"volume":(109,169,223),
"lock":(157,138,215),"power":(190,118,158),"bell":(204,139,182),"moon":(142,134,213),
"album":(97,159,220),"battery":(103,185,166),"storage":(120,147,186),"clock":(112,147,211),
"star":(211,169,102),"profile":(130,153,230),"browser":(77,160,212),"palette":(194,127,184),
"amiibo":(165,121,212),"mii":(178,131,196),"folder":(110,159,232),"search":(122,159,219),
"check":(91,187,160),"edit":(132,160,222)}
def symbol(kind,n=96):
    size=n*3; im=Image.new("RGBA",(size,size));d=ImageDraw.Draw(im);k=size/24;c=(249,251,255,255)
    def xy(p):return tuple(round(v*k) for v in p)
    def line(*pts):
        w=max(2,round(1.65*k));d.line([xy(p) for p in pts],fill=c,width=w,joint="curve")
        for p in (pts[0],pts[-1]):
            x,y=xy(p);r=w/2;d.ellipse((x-r,y-r,x+r,y+r),fill=c)
    def ring(x,y,r):d.ellipse(xy((x-r,y-r,x+r,y+r)),outline=c,width=max(2,round(1.65*k)))
    def dot(x,y,r=1):d.ellipse(xy((x-r,y-r,x+r,y+r)),fill=c)
    def box(x,y,w,h,r=2):d.rounded_rectangle(xy((x,y,x+w,y+h)),radius=round(r*k),outline=c,width=max(2,round(1.65*k)))
    def arc(x,y,r,a,b):d.arc(xy((x-r,y-r,x+r,y+r)),a,b,fill=c,width=max(2,round(1.65*k)))
    def ray(a,r1,r2):
        a=math.radians(a);line((12+r1*math.cos(a),12+r1*math.sin(a)),(12+r2*math.cos(a),12+r2*math.sin(a)))
    if kind=="home":line((3,11),(12,3),(21,11));box(5,10,14,11);line((12,16),(12,21))
    elif kind=="games":box(2.5,6,19,13,4);line((7,10),(7,15));line((4.5,12.5),(9.5,12.5));dot(16,10);dot(19,13);dot(13,13);dot(16,16)
    elif kind=="apps":
        for x in (3,13):
            for y in (3,13):box(x,y,8,8,2.5)
    elif kind=="settings":
        ring(12,12,6.8);ring(12,12,2.8)
        for a in range(0,360,45):ray(a,7,10.2)
    elif kind=="wifi":
        for r in (5,9,13):arc(12,19,r,224,316)
        dot(12,20,1.2)
    elif kind=="bluetooth":line((12,2),(12,22),(19,16),(5,5));line((12,2),(19,8),(5,19))
    elif kind=="sun":
        ring(12,12,4.7)
        for a in range(0,360,45):ray(a,7,10)
    elif kind=="volume":box(3,9,4,6,1);line((7,9),(12,5),(12,19),(7,15));arc(12,12,5,-55,55);arc(12,12,9,-55,55)
    elif kind=="lock":box(5,10,14,11,3);arc(12,8,4.5,180,360);line((7.5,8),(7.5,10));line((16.5,8),(16.5,10));dot(12,15,1.2)
    elif kind=="power":arc(12,12,9,-55,235);line((12,2),(12,11))
    elif kind=="bell":arc(12,10,6,180,360);line((6,10),(6,15),(3,19),(21,19),(18,15),(18,10));arc(12,19,3,0,180)
    elif kind=="moon":arc(12,12,9,38,285);arc(17,7,7.8,85,205)
    elif kind=="album":box(3,4,18,16,3);line((4,18),(9,11),(14,16),(18,12),(21,15));dot(16,8,1.5)
    elif kind=="battery":box(2,6,18,12,3);line((22,10),(22,14));d.rounded_rectangle(xy((5,9,14,15)),radius=round(k),fill=c)
    elif kind=="storage":box(3,5,18,14,3);line((5,14),(19,14));dot(17,17,.8)
    elif kind=="clock":ring(12,12,9);line((12,6),(12,12),(17,15))
    elif kind=="star":
        pts=[(12+(9.5 if i%2==0 else 4.5)*math.cos(math.radians(-90+i*36)),12+(9.5 if i%2==0 else 4.5)*math.sin(math.radians(-90+i*36))) for i in range(10)]
        line(*pts,pts[0])
    elif kind=="profile":ring(12,7,4);arc(12,22,8,180,360);line((4,22),(20,22))
    elif kind=="browser":ring(12,12,9.5);line((8,16),(15,6),(16,15),(8,16));dot(12,12,.8)
    elif kind=="palette":
        arc(12,12,9,35,330);arc(17,17,4,180,360)
        for p in [(7,8),(12,6),(17,8),(7,15)]:dot(*p,1.2)
    elif kind=="amiibo":arc(12,12,9,15,165);arc(12,12,9,195,345);dot(12,12,2.5);dot(12,3,1);dot(12,21,1)
    elif kind=="mii":ring(12,12,9);dot(8.5,9,1);dot(15.5,9,1);arc(12,13,4.5,15,165)
    elif kind=="folder":box(3,7,18,13,3);line((3,7),(3,4),(10,4),(13,7));line((6,11),(18,11))
    elif kind=="search":ring(10,10,6.5);line((15,15),(21,21))
    elif kind=="check":ring(12,12,9);line((7,12),(10,15),(17,8))
    elif kind=="edit":line((5,19),(6,14),(17,3),(21,7),(10,18),(5,19));line((14,6),(18,10))
    return im.resize((n,n),Image.Resampling.LANCZOS)
def glass_icon(kind,n=256):
    im=Image.new("RGBA",(n,n));p=im.load();rgb=PALETTE[kind]
    for y in range(n):
        v=(y+.5)/n
        for x in range(n):
            u=(x+.5)/n;qx=abs(u-.5)-.32;qy=abs(v-.5)-.32
            sd=math.hypot(max(qx,0),max(qy,0))+min(max(qx,qy),0)-.15
            a=max(0,min(1,.5-sd*n))
            if not a:continue
            edge=math.exp(-abs(sd+.009)*85);inner=math.exp(-abs(sd+.035)*38)
            highlight=math.exp(-((u-.26)**2+(v-.12)**2)/.045);low=math.exp(-((u-.78)**2+(v-.85)**2)/.10)
            light=min(.78,.12+.49*edge*(1-.55*v)+.19*inner+.23*highlight+.09*low)
            shade=.76+.24*(1-v)
            p[x,y]=tuple(round(max(0,min(255,ch*shade*(1-light)+255*light))) for ch in rgb)+(round(a*255),)
    lens=Image.new("RGBA",im.size);d=ImageDraw.Draw(lens);m=round(n*.19)
    d.ellipse((m,m,n-m,n-m),fill=(19,31,65,46),outline=(235,247,255,105),width=max(1,n//100))
    im=Image.alpha_composite(im,lens);glyph=symbol(kind,round(n*.51));off=(n-glyph.width)//2
    im.alpha_composite(glyph,(off,off));return im
def generate(root):
    root=Path(root)
    for folder in ("Glyphs","Icons"):(root/"GlassNX"/folder).mkdir(parents=True,exist_ok=True)
    for kind in PALETTE:
        symbol(kind).save(root/"GlassNX"/"Glyphs"/(kind+".png"))
        glass_icon(kind).save(root/"GlassNX"/"Icons"/(kind+".png"))
    names={"Album":"album","Amiibo":"amiibo","Controllers":"games","MiiEdit":"mii","Power":"power","Settings":"settings","Themes":"palette","WebBrowser":"browser","Folder":"folder"}
    for folder in ("EntryIcon","QuickIcon"):
        for name,kind in names.items():
            path=root/"Main"/folder/(name+".png")
            if path.exists():
                with Image.open(path) as old:size=old.size
                src=root/"GlassNX"/("Icons" if folder=="EntryIcon" else "Glyphs")/(kind+".png")
                with Image.open(src) as img:img.resize(size,Image.Resampling.LANCZOS).save(path)
    for name,kind in (("SettingEditableIcon","edit"),("SettingNonEditableIcon","settings")):
        path=root/"Settings"/(name+".png")
        if path.exists():
            with Image.open(path) as old:size=old.size
            symbol(kind).resize(size,Image.Resampling.LANCZOS).save(path)
