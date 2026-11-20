#!/usr/bin/env python3
"""Deterministic allowlisted .deb and complete corresponding-source archive.
Never enumerate private story, local, build or development-data directories.
"""
import pathlib,subprocess,os,shutil,tempfile,json,hashlib,tarfile,gzip,io,re
from check_public_tree import FORBIDDEN_SUFFIXES
root=pathlib.Path(__file__).resolve().parents[1]
meta=json.loads((root/'app-builder.json').read_text());version=meta['version']
author=meta['store']['author']
epoch=int(os.environ.get('SOURCE_DATE_EPOCH','1795046400'))
dist=root/'dist';dist.mkdir(exist_ok=True)
stage=pathlib.Path(tempfile.mkdtemp(prefix='fz-package-'))
def copy(src,dest,mode=0o644):
    p=stage/dest;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/src,p);p.chmod(mode)
try:
    copy('build/arm64/frotz-zero','usr/bin/frotz-zero',0o755)
    subprocess.run(['aarch64-linux-gnu-strip','--strip-unneeded',str(stage/'usr/bin/frotz-zero')],check=True)
    copy('packaging/frotz-zero.desktop','usr/share/APPLaunch/applications/frotz-zero.desktop')
    copy('assets/frotz-zero.png','usr/share/APPLaunch/share/images/frotz-zero.png')
    for name in ('LICENSE','README.md','CHANGELOG.md','CONTRIBUTING.md','THIRD_PARTY_NOTICES.md','dependencies.lock.json'):
        copy(name,'usr/share/doc/frotz-zero/'+name)
    for path in sorted((root/'docs').glob('*.md')):copy(path.relative_to(root),'usr/share/doc/frotz-zero/docs/'+path.name)
    copy('store/SUBMISSION.md','usr/share/doc/frotz-zero/store/SUBMISSION.md')
    copy('third_party/frotz/COPYING','usr/share/doc/frotz-zero/Frotz-COPYING')
    copy('third_party/frotz/AUTHORS','usr/share/doc/frotz-zero/Frotz-AUTHORS')
    copy('third_party/qrcodegen/qrcodegen.h','usr/share/doc/frotz-zero/qrcodegen-notice.h')
    copy('packaging/nlohmann-json-LICENSE.MIT','usr/share/doc/frotz-zero/nlohmann-json-LICENSE.MIT')
    control=stage/'DEBIAN';control.mkdir()
    size=sum(p.stat().st_size for p in stage.rglob('*') if p.is_file())//1024
    (control/'control').write_text(f'''Package: frotz-zero
Version: {version}
Architecture: arm64
Maintainer: {author['display_name']} <{author['email']}>
Section: games
Priority: optional
Installed-Size: {size}
Depends: libc6 (>= 2.41), libgcc-s1, libstdc++6 (>= 13), libsdl2-2.0-0 (>= 2.32.4), libsdl2-mixer-2.0-0 (>= 2.8.1), libsdl2-ttf-2.0-0 (>= 2.24.0), libfreetype6, libpng16-16t64, libjpeg62-turbo, zlib1g, libxkbcommon0, libssl3t64, fonts-dejavu-core, xkb-data, libcurl4t64, libarchive13t64, libxml2, ca-certificates
Homepage: {meta['store']['source_repo']}
Description: Z-machine adventure player for Linux Cardputer Zero
 Graphical Frotz frontend with a keyboard library, portable saves, supported
 input-boundary resume, Blorb media, and an optional online catalogue.
''')
    files=sorted(p for p in stage.rglob('*') if p.is_file() and 'DEBIAN' not in p.parts)
    (control/'md5sums').write_text(''.join(hashlib.md5(p.read_bytes()).hexdigest()+'  '+p.relative_to(stage).as_posix()+'\n' for p in files))
    for p in stage.rglob('*'):
        if p.is_dir():p.chmod(0o755)
        os.utime(p,(epoch,epoch))
    stage.chmod(0o755)
    deb=dist/f'frotz-zero_{version}_arm64.deb'
    subprocess.run(['dpkg-deb','--root-owner-group','-Zxz','-z9','--build',str(stage),str(deb)],check=True,env=dict(os.environ,SOURCE_DATE_EPOCH=str(epoch)))
finally:shutil.rmtree(stage)
# Source archive includes the pinned upstream source and every build input.
source=dist/f'frotz-zero_{version}_source.tar.gz'
roots=['src','scripts','tests','cmake','assets','packaging','docs','store','LICENSE','README.md','CHANGELOG.md','CONTRIBUTING.md','THIRD_PARTY_NOTICES.md','CMakeLists.txt','dependencies.lock.json','app-builder.json','.gitignore','.gitattributes','third_party/qrcodegen']
inputs=[]
for name in roots:
    p=root/name
    inputs.extend(p.rglob('*') if p.is_dir() else [p])
upstream=root/'third_party/frotz'
if (upstream/'.git').exists():
    tracked=[os.fsdecode(p) for p in subprocess.check_output(['git','-C',str(upstream),'ls-files','-z']).split(b'\0') if p]
else:tracked=list(json.loads((upstream/'.frotz-zero-source.json').read_text())['files'])
# Upstream's optional test-game collection contains compiled stories. It is not
# used to build the interpreter; only our original CC0 test sources are shipped.
tracked=[p for p in tracked if not p.startswith('src/test/')]
inputs.extend(upstream/p for p in tracked)
manifest={'commit':json.loads((root/'dependencies.lock.json').read_text())['frotz']['commit'],'files':{p:hashlib.sha256((upstream/p).read_bytes()).hexdigest() for p in tracked}}
with source.open('wb') as raw,gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=epoch) as compressed,tarfile.open(fileobj=compressed,mode='w') as archive:
    for p in sorted(set(inputs)):
        if not p.is_file() or '__pycache__' in p.parts or p.suffix=='.pyc':continue
        relative=p.relative_to(root)
        if any(part in ('local','stories','dev-data','.cache','build','dist','.git') for part in relative.parts):raise RuntimeError('Non-public source input: '+str(relative))
        if p.suffix.lower() in FORBIDDEN_SUFFIXES or re.fullmatch(r'\.z[1-8]',p.suffix.lower()):raise RuntimeError('Story/media/save/archive input: '+str(relative))
        if p.suffix.lower()=='.inf' and relative.parent!=pathlib.Path('tests/fixtures'):raise RuntimeError('Non-original test story source: '+str(relative))
        info=archive.gettarinfo(str(p),arcname='frotz-zero/'+relative.as_posix());info.uid=info.gid=0;info.uname=info.gname='root';info.mtime=epoch;info.mode=0o644
        with p.open('rb') as f:archive.addfile(info,f)
    body=(json.dumps(manifest,sort_keys=True,indent=2)+'\n').encode()
    info=tarfile.TarInfo('frotz-zero/third_party/frotz/.frotz-zero-source.json');info.size=len(body);info.uid=info.gid=0;info.uname=info.gname='root';info.mtime=epoch;info.mode=0o644;archive.addfile(info,io.BytesIO(body))
(dist/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in (deb,source)))
print('Built',deb,'and corresponding source',source)
