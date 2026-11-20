#!/usr/bin/env python3
"""Safely inspect/extract personal development media without changing originals.
SPDX-License-Identifier: GPL-2.0-or-later
"""
import argparse, hashlib, json, pathlib, stat, zipfile
def extract(archive,dest):
    dest=pathlib.Path(dest).resolve();dest.mkdir(parents=True,exist_ok=True)
    records=[]
    with zipfile.ZipFile(archive) as z:
        total=0
        for info in z.infolist():
            normalized=info.filename.replace('\\','/')
            path=pathlib.PurePosixPath(normalized)
            if path.is_absolute() or '..' in path.parts or ':' in normalized or stat.S_ISLNK(info.external_attr>>16):
                raise ValueError('Unsafe archive entry: '+info.filename)
            target=dest.joinpath(*path.parts).resolve()
            if not target.is_relative_to(dest):raise ValueError('Archive path escapes destination')
            total+=info.file_size
            if info.file_size>32*1024*1024 or total>256*1024*1024:raise ValueError('Archive size limit exceeded')
            if info.is_dir():continue
            records.append((info,target))
        result=[]
        for info,target in records:
            data=z.read(info);digest=hashlib.sha256(data).hexdigest()
            if target.exists():
                if target.is_symlink() or hashlib.sha256(target.read_bytes()).hexdigest()!=digest:raise ValueError('Existing file differs: '+str(target))
            else:
                target.parent.mkdir(parents=True,exist_ok=True)
                with target.open('xb') as f:f.write(data)
            result.append({'filename':info.filename,'bytes':len(data),'sha256':digest})
    return result
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('archive');p.add_argument('--destination',default='dev-data/archive');p.add_argument('--report',default='local/archive-inventory.json');a=p.parse_args()
    records=extract(a.archive,a.destination);r=pathlib.Path(a.report);r.parent.mkdir(parents=True,exist_ok=True);r.write_text(json.dumps(records,indent=2)+'\n');print(f'{len(records)} original files preserved and safely extracted into {a.destination}')
