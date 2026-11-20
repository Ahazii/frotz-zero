#!/usr/bin/env python3
"""Local-only startup/input smoke pass, never a full-playability certificate.
Every private binary, screenshot and transcript stays under ignored local/.
"""
import os,pathlib,subprocess,json,time,argparse
root=pathlib.Path(__file__).resolve().parents[1];os.chdir(root)
binary=root/'build/desktop/frotz-zero';private=root/'local/private-smoke';private.mkdir(parents=True,exist_ok=True)
args=argparse.ArgumentParser();args.add_argument('--inventory',type=pathlib.Path,default=root/'local/story-inventory.json');args.add_argument('--v6-only',action='store_true');args=args.parse_args()
inventory=json.loads(args.inventory.read_text());rows=[]
if args.v6_only:inventory=[s for s in inventory if s['version']==6]
deeper_path=root/'local/private-restores/results.json'
deeper={r['filename']:r for r in json.loads(deeper_path.read_text())} if deeper_path.exists() else {}
for s in inventory:
    stem=s['filename'];folder=private/stem;folder.mkdir(exist_ok=True)
    drive=folder/'drive.txt';drive.write_text('wait 250\ncapture '+str(folder/'startup.png')+'\ntext look\nkey Return\nwait 250\ntext look\nkey Return\nwait 250\ncapture '+str(folder/'after-input.png')+'\nquit\n')
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(folder/'config'),XDG_DATA_HOME=str(folder/'data'))
    start=time.monotonic()
    try:
        result=subprocess.run([str(binary),'--engine',str(root/'stories'/stem),'--drive',str(drive)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=8)
        (folder/'engine.log').write_bytes(result.stdout);status=result.returncode
    except subprocess.TimeoutExpired as e:
        status='timeout';(folder/'engine.log').write_bytes(e.stdout or b'')
    report_path=folder/'data/frotz-zero/stories'/s['sha256']/'working/drive-report.json';report=json.loads(report_path.read_text()) if report_path.exists() else {}
    missing='Companion graphics absent; private resource-dependent rendering/audio pending' if s['version']==6 and not s.get('pictures') else ('Sound resources absent' if not s.get('sound') and ('lurkinghorror' in stem or 'sherlock' in stem) else '')
    text=report.get('transcript','')
    outcome='startup/input smoke completed' if status==0 else 'failed or stopped: '+str(status)
    if 'Screen too narrow' in text:outcome='stopped by game: screen too narrow'
    unresolved='Full playability, complete puzzles and hardware not verified.'
    if s['version']==6:unresolved+=' Resource association and startup do not prove full v6 playability.'
    if s['findings']:unresolved+=' '+s['findings']
    actions='Header validation; render startup; attempt LOOK twice (may be consumed by introductory prompts); orderly process exit'
    if stem in deeper:actions+='; '+deeper[stem]['actions'];outcome+='; separate multi-turn/save/restore check passed'
    rows.append({'filename':stem,'version':s['version'],'tested_actions':actions,'outcome':outcome,'returncode':status,'seconds':round(time.monotonic()-start,2),'checkpoint_written':bool(report.get('safe_resume')),'missing_resources':missing,'unresolved':unresolved})
    print(stem+': '+outcome,flush=True)
(private/'compatibility.json').write_text(json.dumps(rows,indent=2)+'\n')
md='# Local private collection compatibility smoke results\n\nThis is a startup/input smoke pass, not proof of full playability. Screenshots and transcripts remain local.\n\n| Filename | Version | Tested actions | Outcome | Missing resources | Unresolved |\n|---|---:|---|---|---|---|\n'
for r in rows:md+='| '+' | '.join(str(r[k]).replace('|','/') for k in ('filename','version','tested_actions','outcome','missing_resources','unresolved'))+' |\n'
(private/'COMPATIBILITY.md').write_text(md)
print('Local report:',private/'COMPATIBILITY.md')
