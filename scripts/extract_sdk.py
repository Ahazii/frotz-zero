#!/usr/bin/env python3
"""Extract the official sysroot, rooting its absolute symlinks inside the cache."""
import pathlib,tarfile,os,hashlib
root=pathlib.Path(__file__).resolve().parents[1];archive=root/'.cache/sdk_bsp.tar.gz';dest=root/'.cache/sdk'
expected='e51b6eb803ed08f450e459efbfe62dd0341440846f3be9d01da861fe6cfdebb0'
with archive.open('rb') as f:
    if hashlib.file_digest(f,'sha256').hexdigest()!=expected:raise SystemExit('SDK checksum mismatch')
dest.mkdir(parents=True,exist_ok=True)
def safe(member,path):
    name=pathlib.PurePosixPath(member.name)
    if name.is_absolute() or '..' in name.parts:raise ValueError('Unsafe member name')
    # Only the compile/link sysroot, excluding unrelated packaged applications.
    if not (member.name.startswith(('./usr/include/','./usr/lib/aarch64-linux-gnu/','./usr/lib/gcc/','./usr/lib/pkgconfig/','./usr/share/pkgconfig/','./lib/','./lib64/')) or member.name in ('./','./usr/','./usr/lib/')):return None
    if member.issym() and member.linkname.startswith('/'):
        target=dest/member.linkname.lstrip('/')
        member=member.replace(linkname=os.path.relpath(target,(dest/member.name).parent))
    return tarfile.data_filter(member,path)
with tarfile.open(archive) as t:t.extractall(dest,filter=safe)
print('Verified SDK extracted:',dest)
