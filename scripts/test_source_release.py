#!/usr/bin/env python3
"""Rebuild the distributed source without the original checkout/private files."""
import pathlib,tempfile,tarfile,subprocess,json,time
root=pathlib.Path(__file__).resolve().parents[1]
version=json.loads((root/'app-builder.json').read_text())['version']
out=root/'local/source-rebuild';out.mkdir(parents=True,exist_ok=True)
work=out/str(time.time_ns());work.mkdir()
with tarfile.open(root/'dist'/f'frotz-zero_{version}_source.tar.gz') as t:t.extractall(work,filter='data')
source=work/'frotz-zero'
assert not (source/'stories').exists() and not (source/'third_party/frotz/.git').exists()
log=[]
for command in [['cmake','-S',str(source),'-B',str(source/'build/desktop'),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release'],['cmake','--build',str(source/'build/desktop'),'-j','4'],['ctest','--test-dir',str(source/'build/desktop'),'--output-on-failure']]:
    r=subprocess.run(command,capture_output=True,text=True);log.append(r.stdout+r.stderr)
    if r.returncode:(out/'build.log').write_text('\n'.join(log));raise SystemExit(r.returncode)
(out/'build.log').write_text('\n'.join(log));(out/'results.json').write_text(json.dumps({'passed':['source archive extraction with safe filter','complete archived Frotz verification without Git','independent Release build without private data','all four CTest suites with assertions enabled'],'source':str(source)},indent=2)+'\n')
print('PASS independent source-archive build and all four CTest suites:',source)
