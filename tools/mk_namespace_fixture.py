"""Independent volumes for native and host mount/namespace regressions."""
from pathlib import Path
from mk_v7fs import Image, DIR, REG
image=Image(256)
image.inode(4,REG|0o644,b'volume\n')
image.inode(5,REG|0o755,Path('build/v7-echo.elf').read_bytes())
image.inode(13,REG|0o755,Path('build/text-probe.elf').read_bytes())
image.directory(3,2,[])
for i in range(6,13):
    image.directory(i,2,[])
image.directory(2,2,[('inner',3),('data',4),('echo',5),('probe',13)]+[(chr(97+i-6),i) for i in range(6,13)])
Path('build/namespace.img').write_bytes(image.finish())
