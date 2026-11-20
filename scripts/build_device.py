#!/usr/bin/env python3
"""Build with the pinned official SDK, then assemble the allowlisted package."""
import pathlib,subprocess,hashlib,json
root=pathlib.Path(__file__).resolve().parents[1]
def run(*args):subprocess.run(list(map(str,args)),cwd=root,check=True)
if not (root/'third_party/frotz/src/common/main.c').exists():run('python3','scripts/fetch_dependencies.py')
sdk=json.loads((root/'dependencies.lock.json').read_text())['sdk']
archive=root/'.cache/sdk_bsp.tar.gz';archive.parent.mkdir(exist_ok=True)
if not archive.exists():run('curl','--fail','--location','--retry','2',sdk['url'],'--output',archive)
with archive.open('rb') as f:
    if hashlib.file_digest(f,'sha256').hexdigest()!=sdk['sha256']:raise SystemExit('SDK checksum mismatch; archive preserved for inspection')
stamp=root/'.cache/sdk/.verified-sha256'
if not stamp.exists() or stamp.read_text().strip()!=sdk['sha256']:
    run('python3','scripts/extract_sdk.py');stamp.write_text(sdk['sha256']+'\n')
run('python3','scripts/complete_sdk.py')
run('cmake','-S','.', '-B','build/arm64','-G','Ninja','-DCMAKE_TOOLCHAIN_FILE=cmake/arm64.cmake','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF')
run('cmake','--build','build/arm64','-j','4')
run('python3','scripts/package.py')
