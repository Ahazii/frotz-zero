#!/usr/bin/env python3
"""Add matching Debian development packages to the template SDK without installing them."""
import pathlib,subprocess,hashlib,json
root=pathlib.Path(__file__).resolve().parents[1]
items=[('libsdl2-dev','libs/libsdl2','2.32.4+dfsg-1'),('libsdl2-mixer-dev','libs/libsdl2-mixer','2.8.1+dfsg-2'),('libsdl2-ttf-dev','libs/libsdl2-ttf','2.24.0+dfsg-2'),('libarchive-dev','liba/libarchive','3.7.4-4+deb13u1')]
lock=[]
target=root/'cmake/sdk-packages.lock.json'
expected=json.loads(target.read_text()) if target.exists() else None
for name,folder,version in items:
    url=f'https://deb.debian.org/debian/pool/main/{folder}/{name}_{version}_arm64.deb';p=root/'.cache/sdk-debs'/f'{name}.deb';p.parent.mkdir(parents=True,exist_ok=True)
    if not p.exists():subprocess.run(['curl','--fail','-sSL',url,'-o',str(p)],check=True)
    sha=hashlib.sha256(p.read_bytes()).hexdigest()
    if expected is not None and not any(x['url']==url and x['sha256']==sha for x in expected):
        raise SystemExit('SDK package checksum mismatch: '+name)
    subprocess.run(['dpkg-deb','-x',str(p),str(root/'.cache/sdk')],check=True)
    lock.append({'name':name,'version':version,'architecture':'arm64','url':url,'sha256':sha})
if target.exists():
    if json.loads(target.read_text())!=lock:raise SystemExit('SDK package checksums changed')
else:target.write_text(json.dumps(lock,indent=2)+'\n')
print('Verified matching ARM64 development packages:',*[x['name'] for x in lock])
