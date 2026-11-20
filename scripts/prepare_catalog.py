#!/usr/bin/env python3
"""Compile the public offline catalogue; no network or private story inputs."""
import json,pathlib,re,sys
root=pathlib.Path(__file__).resolve().parents[1]
data=json.loads((root/'assets/story-catalog.json').read_text(encoding='utf-8'))
assert data['schema']==1
rows=[];seen=set()
for item in data['stories']:
    key=(item['version'],item['release'],item['serial'],item['checksum'])
    assert key not in seen and key[0] in (3,4,5,6) and 0<=key[1]<=65535
    assert len(key[2])==6 and re.fullmatch('[0-9a-f]{4}',key[3])
    seen.add(key)
    strings=[item[k] for k in ('serial','checksum','title','edition','summary','source')]
    assert all(isinstance(v,str) and '\x00' not in v for v in strings)
    rows.append('    {'+str(key[0])+','+str(key[1])+','+','.join(json.dumps(v,ensure_ascii=False) for v in strings)+'},')
out=pathlib.Path(sys.argv[1]);out.parent.mkdir(parents=True,exist_ok=True)
out.write_text('// Generated from assets/story-catalog.json.\nstatic const CatalogEntry catalog_entries[] = {\n'+'\n'.join(rows)+'\n};\n',encoding='utf-8')
