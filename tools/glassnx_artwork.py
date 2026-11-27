#!/usr/bin/env python3
"""Interactive SteamGridDB hero artwork importer for GlassNX (runs on a PC)."""
import argparse
import io
import json
import os
from pathlib import Path
import re
import sys
from urllib.parse import quote, urlsplit
from urllib.request import Request, urlopen, HTTPRedirectHandler, build_opener

API = "https://www.steamgriddb.com/api/v2"
API_HOST = "www.steamgriddb.com"
CDN_HOSTS = {"cdn2.steamgriddb.com", "cdn.steamgriddb.com", "cdn.cloudflare.steamstatic.com"}
MAX_IMAGE = 12 * 1024 * 1024

def checked_url(url, allowed):
    p = urlsplit(url)
    if p.scheme != "https" or p.hostname not in allowed or p.username or p.password or p.port not in (None,443):
        raise ValueError("Unapproved HTTPS host")
    return url

class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

def request(url, hosts, limit, key=None):
    checked_url(url, hosts)
    headers = {"User-Agent":"GlassNX-Artwork/0.12"}
    if key:
        headers["Authorization"] = "Bearer " + key
    with build_opener(NoRedirect()).open(Request(url,headers=headers),timeout=20) as response:
        result = response.read(limit+1)
    if len(result)>limit:
        raise ValueError("Response exceeds size limit")
    return result

def api(path,key):
    obj=json.loads(request(API+path,{API_HOST},2*1024*1024,key))
    if not obj.get("success") or not isinstance(obj.get("data"),list):
        raise ValueError("SteamGridDB API rejected the request")
    return obj["data"]

def title_id(value):
    if not re.fullmatch(r"[0-9a-fA-F]{16}",value):
        raise ValueError("Expected a 16-digit hexadecimal Switch title ID")
    return value.lower()

def titles(path):
    rows=[]
    for line in path.read_text(encoding="utf-8").splitlines():
        tid,sep,name=line.partition("\t")
        if sep and name:
            rows.append((title_id(tid),name))
    return rows

def choose(items,describe):
    if not items: raise ValueError("No matching entries")
    for i,item in enumerate(items,1):
        print(f"{i:2}. {describe(item)}")
    value=input("Select a number (Enter cancels): ").strip()
    if not value: raise KeyboardInterrupt
    n=int(value)
    if not 1<=n<=len(items): raise ValueError("Invalid selection")
    return items[n-1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sd-root",type=Path,required=True,help="SD card root or local staging directory")
    p.add_argument("--title-id",help="16-digit Switch title ID")
    p.add_argument("--query",help="Search title (required with --title-id)")
    args=p.parse_args()
    key=os.environ.get("STEAMGRIDDB_API_KEY","").strip()
    if not key: raise ValueError("Set STEAMGRIDDB_API_KEY locally; never commit the key")
    root=args.sd_root / "glassnx"
    if args.title_id:
        tid=title_id(args.title_id)
        if not args.query: raise ValueError("--query is required with --title-id")
        name=args.query
    else:
        tid,name=choose(titles(root/"titles.tsv"),lambda row:f"{row[1]} ({row[0]})")
        name=args.query or name
    games=api("/search/autocomplete/"+quote(name,safe=""),key)
    game=choose(games[:20],lambda g:f"{g.get('name','?')} (SteamGridDB ID {g.get('id')})")
    gid=int(game["id"])
    if gid<=0: raise ValueError("Invalid game ID")
    heroes=api(f"/heroes/game/{gid}?types=static&nsfw=false&humor=false",key)
    heroes=[h for h in heroes if h.get("url") and not h.get("nsfw") and not h.get("humor")]
    hero=choose(heroes[:20],lambda h:f"Artwork {h.get('id')} | {h.get('width','?')}x{h.get('height','?')} | score {h.get('score','?')} | {h.get('url')}")
    raw=request(hero["url"],CDN_HOSTS,MAX_IMAGE) # never send API key to CDN
    from PIL import Image, ImageOps
    Image.MAX_IMAGE_PIXELS=20_000_000
    with Image.open(io.BytesIO(raw)) as im:
        if im.width*im.height>20_000_000: raise ValueError("Artwork too large")
        art=ImageOps.exif_transpose(im).convert("RGB")
        if art.width < 1130 or art.height < 405:
            raise ValueError("Hero must retain at least 1130 x 405 pixels after center cropping")
        art=ImageOps.fit(art,(1130,405),method=Image.Resampling.LANCZOS,centering=(.5,.5))
        dest=root/"art"/(tid+".hero")
        dest.parent.mkdir(parents=True,exist_ok=True)
        if dest.exists() and input(f"Replace {dest.name}? [y/N] ").strip().lower()!="y": return
        temp=dest.with_suffix(".jpg.tmp")
        try:
            art.save(temp,format="JPEG",quality=87,optimize=True)
            if temp.stat().st_size>8*1024*1024: raise ValueError("Output too large")
            os.replace(temp,dest)
        finally:
            if temp.exists(): temp.unlink()
    print(f"Saved {dest}. Restart GlassNX or change featured title to reload.")
    print("SteamGridDB artwork:",hero.get("id"),"by",(hero.get("author") or {}).get("name","unknown"))

if __name__=="__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("Cancelled.")
    except Exception as exc:
        print(f"Artwork import failed: {type(exc).__name__}: {exc}",file=sys.stderr)
        sys.exit(1)
