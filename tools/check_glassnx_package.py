import sys, zipfile
from pathlib import PurePosixPath
p=sys.argv[1]
required=["glassnx/bin/uSystem/exefs.nsp","glassnx/bin/uMenu/main","glassnx/bin/uMenu/main.npdm","glassnx/bin/uMenu/romfs.bin","glassnx/bin/uLoader/applet/main","glassnx/bin/uLoader/applet/main.npdm","glassnx/bin/uLoader/application/main","glassnx/bin/uLoader/application/main.npdm","switch/GlassNXManager.nro","glassnx/BUILD.txt","atmosphere/contents/0100000000001000/exefs.nsp"]
with zipfile.ZipFile(p) as z:
    names=z.namelist()
    assert len(names)==len(set(names)) and len(names)<=300
    assert all(not n.startswith("/") and ".." not in PurePosixPath(n).parts and "\\" not in n for n in names)
    assert all(n in names and 0<z.getinfo(n).file_size<=64*1024*1024 for n in required)
    assert sum(z.getinfo(n).file_size for n in required)<=128*1024*1024
    assert z.read(required[0])==z.read(required[-1])
    assert z.read("glassnx/BUILD.txt").startswith(b"GlassNX v")
    assert z.testzip() is None
print("GlassNX update package validated")
