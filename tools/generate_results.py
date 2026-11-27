#!/usr/bin/env python3
"""Generate upstream arc result tables using the checked-out, pinned submodules."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
arc=root/"arc"
sys.path.insert(0,str(arc))
import arc_db_gen
import arc_cpp_gen
headers=sorted((root/"libs/Atmosphere-libs/libvapours/include/vapours/results").glob("*_results.hpp"))
if not headers:
    raise SystemExit("Pinned Atmosphere-libs result headers missing; initialize submodules")
defaults=sorted(p for p in (arc/"default_rc_hpps").iterdir() if p.is_file())
if not defaults:
    raise SystemExit("arc default result headers missing")
custom=root/"libs/uCommon/include/ul/ul_Results.rc.hpp"
out=root/"libs/uCommon/include/ul/ul_Results.gen.hpp"
# The upstream parser strips the ams:: prefix only for Atmosphere input.
# Keep that code path, replacing only its remote enumeration and reader.
arc_db_gen.query_ams_rc_hpps=lambda: [str(p) for p in headers]
arc_db_gen.load_remote_rc_hpp=lambda p: Path(p).read_text(encoding="utf-8")
arc_db_gen.generate_dbs(True,[str(p) for p in defaults]+[str(custom)])
arc_cpp_gen.generate_cpp("rc","UL",str(out))
if not out.is_file() or out.stat().st_size==0:
    raise SystemExit("Result header generation failed")
