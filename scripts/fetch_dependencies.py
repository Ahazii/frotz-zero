#!/usr/bin/env python3
"""Fetch exact source revisions; never reset an existing changed checkout."""
import json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
lock=json.loads((root/'dependencies.lock.json').read_text())
item=lock['frotz'];dest=root/'third_party/frotz'
if not (dest/'.git').exists():
    dest.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(['git','clone','--no-checkout',item['url'],str(dest)],check=True)
else:
    changed=subprocess.check_output(['git','-C',str(dest),'-c','core.autocrlf=true','-c','core.fileMode=false','status','--porcelain'],text=True)
    if changed.strip():raise SystemExit('Existing Frotz checkout has changes; preserve/review them before fetching.')
subprocess.run(['git','-C',str(dest),'fetch','origin',item['commit']],check=True)
subprocess.run(['git','-C',str(dest),'checkout','--detach',item['commit']],check=True)
print('Pinned Frotz ready:',item['commit'])
